# Owner(s): ["NNC"]

import operator
import unittest
import contextlib
import math
import torch
import torch.nn.functional as F
from torch.testing import FileCheck
from typing import List
import warnings

# these needs to be set before `common_utils`
# infers `GRAPH_EXECUTOR`.
# this file **requires** these settings
# and setting them after `GRAPH_EXECUTOR` is
# inferred erroneously runs or skips
# some tests
torch._C._jit_set_profiling_executor(True)
torch._C._jit_set_profiling_mode(True)

from torch.testing._internal.common_utils import run_tests, ProfilingMode, GRAPH_EXECUTOR, \
    enable_profiling_mode_for_profiling_tests, TestCase
from torch.testing._internal.jit_utils import JitTestCase, \
    RUN_CUDA, RUN_CUDA_HALF, RUN_CUDA_MULTI_GPU, warmup_backward, set_fusion_group_inlining

from torch.testing._internal.common_methods_invocations import op_db
from torch.testing._internal.common_device_type import ops, onlyCPU, instantiate_device_type_tests

from textwrap import dedent
from itertools import product, permutations

from test_jit import backward_graph, get_lstm_inputs, get_milstm_inputs, \
    LSTMCellC, LSTMCellF, LSTMCellS, MiLSTMCell

from jit.test_fuser_common import TestFuserCommon  # noqa: F401

FUSION_GROUP = 'prim::TensorExprGroup'
LLVM_ENABLED = torch._C._llvm_enabled()

def strip_profiling_nodes(nodes):
    profiling_opcodes = set(['prim::BailoutTemplate', 'prim::BailOut'])
    return [n for n in nodes if n.kind() not in profiling_opcodes]

def warmup_forward(f, *args, profiling_count=2):
    for i in range(profiling_count):
        results = f(*args)

    return results

@contextlib.contextmanager
def texpr_reductions_enabled():
    old = torch._C._jit_set_texpr_reductions_enabled(True)
    try:
        yield
    finally:
        torch._C._jit_set_texpr_reductions_enabled(old)

@contextlib.contextmanager
def inline_fusion_groups():
    old_inlining = torch._C._debug_get_fusion_group_inlining()
    torch._C._debug_set_fusion_group_inlining(True)
    try:
        yield
    finally:
        torch._C._debug_set_fusion_group_inlining(old_inlining)

class TestTEFuser(JitTestCase):
    def setUp(self):
        self.old_cpu_fuser_state = torch._C._jit_can_fuse_on_cpu()
        self.old_must_use_cpu_state = torch._C._jit_get_te_must_use_llvm_cpu()
        self.old_gpu_fuser_state = torch._C._jit_can_fuse_on_gpu()

        torch._C._jit_override_can_fuse_on_cpu(True)
        # TODO: force LLVM. need to add it to asan, mac, windows builds + sandcastle
        # torch._C._jit_set_te_must_use_llvm_cpu(True)
        torch._C._jit_override_can_fuse_on_gpu(True)

        self.old_profiling_executor = torch._C._jit_set_profiling_executor(True)
        self.old_profiling_mode = torch._C._jit_set_profiling_mode(True)

        self.old_fusion_inlining = torch._C._debug_get_fusion_group_inlining()
        torch._C._debug_set_fusion_group_inlining(False)

        self.texpr_fuser_state = torch._C._jit_texpr_fuser_enabled()
        torch._C._jit_set_texpr_fuser_enabled(True)

        self.old_te_must_use_llvm_cpu = torch._C._jit_get_te_must_use_llvm_cpu()
        torch._C._jit_set_te_must_use_llvm_cpu(False)

        self.devices = ['cpu'] if not torch.cuda.is_available() else ['cpu', 'cuda']
        self.int_dtypes = [
            torch.int8,
            torch.int16,
            torch.int32,
            torch.int64,
            torch.bool,
        ]
        self.fp_dtypes = [
            torch.float16,
            torch.float32,
            torch.float64,
            torch.bfloat16,
        ]
        self.dtypes = self.int_dtypes + self.fp_dtypes

    def tearDown(self):
        torch._C._jit_set_profiling_executor(self.old_profiling_executor)
        torch._C._jit_set_profiling_mode(self.old_profiling_mode)

        torch._C._jit_override_can_fuse_on_gpu(self.old_gpu_fuser_state)
        torch._C._jit_override_can_fuse_on_cpu(self.old_cpu_fuser_state)
        torch._C._jit_set_te_must_use_llvm_cpu(self.old_must_use_cpu_state)
        torch._C._debug_set_fusion_group_inlining(self.old_fusion_inlining)

        torch._C._jit_set_texpr_fuser_enabled(self.texpr_fuser_state)
        torch._C._jit_set_te_must_use_llvm_cpu(self.old_te_must_use_llvm_cpu)

    def assertLastGraphAllFused(self):
        self.assertAllFused(torch.jit.last_executed_optimized_graph())

    def findFusionGroups(self, graph):
        result = []
        for n in graph.nodes():
            if n.kind() == FUSION_GROUP:
                result.append(n.g('Subgraph'))
                continue
            for block in n.blocks():
                result += self.findFusionGroups(block)
        return result

    def test_typecheck(self):
        a = torch.ones(1)

        def fused_kernel(a, b):
            return (a + b) * 2.

        scripted = self.checkScript(fused_kernel, (a, a))
        graph = scripted.graph_for(a, a)
        # double check we fused
        fusion_groups = self.findFusionGroups(graph)
        self.assertEqual(len(fusion_groups), 1)
        # we use a bigger tensor now (size 2)
        # if we won't trigger a recompilation
        # we will still create a tensor up to (size 1)
        # if the type check fails
        a = torch.ones(2)
        # shape changed if we don't trigger recompilation
        # we would compute the wrong result silently
        self.assertEqual(scripted(a, a), fused_kernel(a, a))

    def test_sum_simple(self):
        def func(x):
            x2 = x * x
            return x2.sum()

        with texpr_reductions_enabled():
            a = torch.tensor(list(x for x in range(0, 15)), dtype=torch.float, device='cpu')
            a = a.reshape(5, 3)
            scripted = self.checkScript(func, (a,))
            self.assertLastGraphAllFused()

    def test_nop(self):
        pass

    def test_sum_dim(self):
        def func(x):
            return x.sum((0, )) * 2

        def func_neg(x):
            return x.sum((-2, )) * 2

        with texpr_reductions_enabled():
            a = torch.tensor(list(x for x in range(0, 15)), dtype=torch.float, device='cpu')
            a = a.reshape(5, 3)
            scripted = self.checkScript(func, (a,))
            self.assertLastGraphAllFused()
            scripted = self.checkScript(func_neg, (a,))
            self.assertLastGraphAllFused()

    def test_sum_keepdim_cast(self):
        def func(x):
            return x.sum((0, ), keepdim=True, dtype=torch.double) * 2

        with texpr_reductions_enabled():
            a = torch.tensor(list(x for x in range(0, 15)), dtype=torch.float, device='cpu')
            a = a.reshape(5, 3)

            self.checkScript(func, (a,))
            self.assertLastGraphAllFused()

    def test_abs(self):
        for device in self.devices:
            def func(x):
                return x.abs() * 2

            a = torch.randn(5, device=device)
            scripted = self.checkScript(func, (a,))
            self.assertLastGraphAllFused()

    def test_unsqueeze_size_calculation(self):
        for device in self.devices:
            def foo(b, d):
                x = d.unsqueeze(1)
                y = x * 42.
                z = b + y
                r = z / 42.
                return r

            inputs = (torch.rand(20, 28, device=device, requires_grad=True), torch.rand(20, device=device))
            scripted = self.checkScript(foo, inputs)
            self.assertAllFused(scripted.graph_for(*inputs))

    def test_zero_element_tensors(self):
        for device in self.devices:
            def decode(sin_t, cos_t):
                theta = torch.atan2(sin_t.float(), cos_t.float())
                return theta

            sin = torch.zeros(0, device=device)
            cos = torch.zeros(0, device=device)
            inputs = [sin, cos]
            ge = self.checkScript(decode, inputs)

    def test_arg_configurations_smoke(self):
        # A smoke test to make sure we won't use the same kernel for contiguous
        # and non-contiguous arguments.
        # TODO: add optionally enabled debug counters to the fuser to verify
        #       that we really can tell the difference between configurations
        for device in self.devices:
            def f(x, y):
                z1, z2 = (x + y).chunk(2, dim=1)
                return z1 * z2

            x = torch.randn(4, 4, dtype=torch.float, device=device)
            y = torch.randn(4, 4, dtype=torch.float, device=device)
            traced_f = torch.jit.trace(f, (x, y,))
            self.assertEqual(traced_f(x.t().contiguous(), y), traced_f(x.t(), y))

    def test_broadcast(self):
        for device in self.devices:
            def scaleshift(x, scale, shift):
                return x * scale + shift

            inputs = [
                torch.randn(4, 4, dtype=torch.float, device=device),
                torch.randn(4, dtype=torch.float, device=device),
                torch.randn(4, dtype=torch.float, device=device),
            ]
            self.checkScript(scaleshift, inputs)

    @unittest.skipIf(not RUN_CUDA, "fuser requires CUDA")
    @unittest.skipIf(not RUN_CUDA_HALF, "no half support")
    @unittest.skipIf(GRAPH_EXECUTOR != ProfilingMode.LEGACY, "no half support with profiling on")
    def test_cuda_half(self):
        x = torch.randn(4, 4, dtype=torch.half, device='cuda')
        y = torch.randn(4, 4, dtype=torch.half, device='cuda')

        funcs = [
            self.fn_test_comparison_gt_lt,
            self.fn_test_relu,
            self.fn_test_exp
        ]

        # Note: Non fused inputs must be float to prevent loss of precision
        inputs = (x.float(), y.float())
        fusion_inputs = (x, y)
        for fn in funcs:
            local_inputs = [t.clone().requires_grad_() for t in inputs]
            local_fusion_inputs = [t.clone().requires_grad_() for t in fusion_inputs]

            # Verifies outputs
            fusion = torch.jit.trace(fn, local_fusion_inputs, check_trace=False)
            outputs = fn(*local_inputs)
            fusion_outputs = fusion(*local_fusion_inputs)
            outputs_half = [t.half() for t in outputs]
            self.assertEqual(outputs_half, fusion_outputs)

            # Verifies gradients
            for output, fusion_output in zip(outputs_half, fusion_outputs):
                grads = torch.autograd.grad(
                    output.float().sum(), local_inputs, allow_unused=True, retain_graph=True)
                fusion_grads = torch.autograd.grad(
                    fusion_output.sum(), local_fusion_inputs, allow_unused=True, retain_graph=True)
                grads_half = [t.half() for t in grads]
                self.assertEqual(grads_half, fusion_grads)

    def test_checks_cat_inputs(self):
        # single fusion node causes error
        with set_fusion_group_inlining(True):
            for device in self.devices:
                # We shouldn't treat cat nodes as broadcasting. All their inputs
                # need to be checked for having the same map size, before we can
                # run the kernel.
                def f(x, y):
                    return torch.cat([x + 2 * x + x ** 2, y + 4 * y + y ** 3], dim=0)

                # NOTE: y is broadcastable to x, but output of f(x, y) should have
                # shape 3x4, and not 4x4.
                x = torch.randn(2, 4, dtype=torch.float, device=device)
                y = torch.randn(1, 4, dtype=torch.float, device=device)

                scripted = self.checkScript(f, (x, y))
                self.assertEqual(scripted(x, y).shape, (3, 4))
                self.assertAllFused(scripted.graph_for(x, y))

    def test_chunk(self):
        for device in self.devices:
            def fn(x):
                a, b, c = x.chunk(3, 1)
                return a * b + c

            inputs = [torch.randn(10, 6, dtype=torch.float, device=device)]

            self.checkScript(fn, inputs)
            self.assertLastGraphAllFused()

    def test_chunk_correctness(self):
        for device in self.devices:
            def chunk_4_0(x):
                x0, x1, x2, x3 = x.chunk(4, 0)
                return x0 + x1 + x2 + x3

            def chunk_4_1(x):
                x0, x1, x2, x3 = x.chunk(4, 1)
                return x0 + x1 + x2 + x3

            def chunk_4_last(x):
                x0, x1, x2, x3 = x.chunk(4, 2)
                return x0 + x1 + x2 + x3

            fns = [chunk_4_0, chunk_4_1, chunk_4_last]
            tensors = [
                # splitSize = 1
                torch.randn(4, 4, 4, dtype=torch.float, device=device),

                # contiguous case
                torch.randn(12, 8, 16, dtype=torch.float, device=device),

                # non-contiguous case
                torch.randn(12, 8, 16, dtype=torch.float, device=device).transpose(1, 2),
            ]

            for tensor in tensors:
                for fn in fns:
                    self.checkScript(fn, [tensor])
                    self.assertLastGraphAllFused()

    def test_chunk_distributes(self):
        for device in self.devices:
            def f(x, y):
                z1, z2 = (x + y).chunk(2, dim=1)
                return z1 * z2

            x = torch.randn(4, 4, dtype=torch.float, device=device)
            y = torch.randn(4, 4, dtype=torch.float, device=device)

            ge = self.checkTrace(f, (x, y))
            graph = ge.graph_for(x, y)
            # XXX: The old fuser does broadcast_tensors but the new fuser doesn't.
            # FileCheck().check("broadcast_tensors").check('with ' + FUSION_GROUP + '_') \
            #     .check_count('ConstantChunk', 2, exactly=True).run(str(graph))
            FileCheck().check("with " + FUSION_GROUP + "_").check_count(
                "ConstantChunk", 1, exactly=True
            ).run(str(graph))

    def test_chunk_motion_deduplicates_inputs(self):
        for device in self.devices:
            def func1(x):
                z = x * x
                z0, z1 = z.chunk(2)
                return z0 * z1

            def func2(x):
                z = x * x * x
                z0, z1 = z.chunk(2)
                return z0 * z1

            inputs = [
                torch.tensor([1.1, 1.2], device=device, dtype=torch.float),
            ]
            for func in [func1, func2]:
                self.checkScript(func, inputs)
                self.assertLastGraphAllFused()

    def test_chunk_multiple(self):
        for device in self.devices:
            # The arguments are intentionally used out of order as a test to see
            # if the fusion compiler adds extra args in the correct order
            def fn(s, x, y, z):
                z1, z2 = z.chunk(2, 2)
                x1, x2, x3 = x.chunk(3, 1)
                y1, y2 = y.chunk(2, 0)
                return s + x1 + x2 + x3 + y1 + y2 + z1 + z2

            inputs = [
                torch.randn(5, 2, 3, dtype=torch.float, device=device),
                torch.randn(5, 6, 3, dtype=torch.float, device=device),
                torch.randn(10, 2, 3, dtype=torch.float, device=device),
                torch.randn(5, 2, 6, dtype=torch.float, device=device),
            ]

            ge = self.checkScript(fn, inputs)
            self.assertAllFused(ge.graph_for(*inputs))

    def test_minmax(self):
        for device in self.devices:
            def tmax(a, b):
                return torch.max(2 * a, b)

            def tmin(a, b):
                return torch.min(2 * a, b)

            a = torch.randn(4, 4, dtype=torch.float)
            b = torch.randn(4, 4, dtype=torch.float)
            nan = torch.tensor(float('nan'), dtype=torch.float)

            for f, inputs, device in product(
                    (tmax, tmin),
                    ([a, b], [a, nan], [b, nan]),
                    self.devices):
                inputs = [t.to(device) for t in inputs]
                s = self.checkScript(f, inputs)
                self.assertAllFused(s.graph_for(*inputs))

    def test_clamp(self):
        for device in self.devices:
            def func2(a, b):
                return torch.clamp(a + b, min=0, max=2)

            def funcInf(a, b):
                return torch.clamp(a + b, min=0, max=float('inf'))

            def funcNegInf(a, b):
                return torch.clamp(a + b, min=float('-inf'), max=0)

            def funcOptMin(a, b):
                return torch.clamp(a + b, max=2)

            def funcOptMax(a, b):
                return torch.clamp(a + b, min=0)

            a = torch.randn(4, 4, dtype=torch.float, device=device, requires_grad=True)
            b = torch.randn(4, 4, dtype=torch.float, device=device)
            nan = torch.tensor(float('nan'), dtype=torch.float, device=device)

            funcs = (func2, funcInf, funcNegInf, funcOptMin, funcOptMax)
            for f, inputs in product(funcs, [[a, b], [a, nan]]):
                inp1, inp2 = inputs
                s = self.checkScript(f, (inp1, inp2), profiling=ProfilingMode.PROFILING)
                self.assertAllFused(s.graph_for(inp1, inp2), except_for={'aten::size', 'aten::_size_if_not_equal'})
                c = s(inp1, inp2)
                with enable_profiling_mode_for_profiling_tests():
                    warmup_backward(c.sum())
                graph = backward_graph(s)
                self.assertAllFused(graph, except_for={'aten::Float', 'aten::_grad_sum_to_size'})

    def test_clamp_double(self):
        for device in self.devices:
            def clamp_double(x, eta: float):
                return 1 - x.clamp(eta, 1 - eta)

            x = torch.tensor([1.0, 1.0], dtype=torch.double, device=device)
            eta = 1e-9
            s = self.checkScript(clamp_double, (x, eta), profiling=ProfilingMode.PROFILING, atol=1e-10, rtol=1e-5)
            self.assertAllFused(s.graph_for(x, eta))

    def test_clamp_int(self):
        for device in self.devices:
            def clamp_int(x, eta: int):
                return x.clamp(0, eta)

            x = torch.tensor([1, 1], device=device)
            eta = 1 << 32
            s = self.checkScript(clamp_int, (x, eta), profiling=ProfilingMode.PROFILING)
            self.assertAllFused(s.graph_for(x, eta))

    def test_add_bool(self):
        sizes = [(1,), (2,), (4, 4)]
        for device, size in product(self.devices, sizes):
            def f(x, y, z):
                return x + y + z

            x = torch.randint(0, 2, size, dtype=torch.bool, device=device)
            y = torch.randint(0, 2, size, dtype=torch.bool, device=device)
            z = torch.randint(0, 2, size, dtype=torch.bool, device=device)
            ge = self.checkTrace(f, (x, y, z), inputs_require_grads=False)
            self.assertAllFused(ge.graph_for(x, y, z))

    def test_mul_bool(self):
        for device in self.devices:
            def f(x, y, z):
                return x * y * z

            x = torch.randint(0, 2, (4, 4), dtype=torch.bool, device=device)
            y = torch.randint(0, 2, (4, 4), dtype=torch.bool, device=device)
            z = torch.randint(0, 2, (4, 4), dtype=torch.bool, device=device)

            ge = self.checkTrace(f, (x, y, z), inputs_require_grads=False)
            self.assertAllFused(ge.graph_for(x, y, z))

    def test_div_bool(self):
        for device in self.devices:
            def f(x, y, z):
                return (x + y) / z

            x = torch.randint(0, 2, (4, 4), dtype=torch.bool, device=device)
            y = torch.randint(0, 2, (4, 4), dtype=torch.bool, device=device)
            z = torch.ones_like(x, dtype=torch.bool, device=device)

            ge = self.checkTrace(f, (x, y, z), inputs_require_grads=False)
            self.assertAllFused(ge.graph_for(x, y, z))

    def test_bitwise_ops(self):
        def apply(fn):
            return lambda x, y, z: fn(fn(x, y), z)

        binary_ops = [
            operator.__and__,
            operator.__or__,
            operator.__xor__,
            operator.__lshift__,
            operator.__rshift__,
        ]
        devices = self.devices
        for dtype, op, device in product(self.int_dtypes, binary_ops, devices):
            try:
                x = self.data_for(dtype, device)
                y = self.data_for(dtype, device)
                z = self.data_for(dtype, device)
                fn = apply(op)
                ref = fn(x, y, z)
            except Exception:
                # If eager mode doesn't support a dtype/op/device combo,
                # neither does the fuser.  Catch everything to avoid needing to
                # guess what errors might be thrown by eager.
                continue
            try:
                t = torch.jit.trace(fn, (x, y, z))
                self.assertEqual(ref, t(x, y, z))
                self.assertAllFused(t.graph_for(x, y, z))
            except Exception as e:
                raise RuntimeError(
                    " ".join(["Failed:", str(dtype), op.__name__, device])
                )

    def test_minmax_int_ops(self):
        def apply(fn):
            return lambda x, y, z: fn(fn(x, y), z)

        binary_ops = [
            torch.min,
            torch.max
        ]
        devices = self.devices
        for dtype, op, device in product(self.int_dtypes, binary_ops, devices):
            try:
                x = self.data_for(dtype, device)
                y = self.data_for(dtype, device)
                z = self.data_for(dtype, device)
                fn = apply(op)
                ref = fn(x, y, z)
            except Exception:
                # If eager mode doesn't support a dtype/op/device combo,
                # neither does the fuser.  Catch everything to avoid needing to
                # guess what errors might be thrown by eager.
                continue
            try:
                t = torch.jit.trace(fn, (x, y, z))
                self.assertEqual(ref, t(x, y, z))
                self.assertAllFused(t.graph_for(x, y, z))
            except Exception as e:
                raise RuntimeError(
                    " ".join(["Failed:", str(dtype), op.__name__, device])
                )

    def test_comparison_eq_ne(self):
        for device in self.devices:
            def f(x, y):
                mask = (x == 0).type_as(x)
                z = x * mask + y
                mask = (x != 0).type_as(x)
                z = z * mask + y
                return z

            x = torch.randn(4, 4, dtype=torch.float, device=device)
            y = torch.randn(4, 4, dtype=torch.float, device=device)

            ge = self.checkTrace(f, (x, y))
            self.assertAllFused(ge.graph_for(x, y))

    @staticmethod
    def fn_test_comparison_gt_lt(x, y):
        mask = (x > 0).type_as(x)
        z = x * mask + y
        mask = (x < 0).type_as(x)
        z = z * mask + y
        return z

    def test_comparison_gt_lt(self):
        for device in self.devices:
            x = torch.randn(4, 4, dtype=torch.float, device=device)
            y = torch.randn(4, 4, dtype=torch.float, device=device)

            ge = self.checkTrace(self.fn_test_comparison_gt_lt, (x, y))
            self.assertAllFused(ge.graph_for(x, y))

    def test_comparison_ge_le(self):
        for device in self.devices:
            def f(x, y):
                mask = (x >= 0).type_as(x)
                z = x * mask + y
                mask = (x <= 0).type_as(x)
                z = z * mask + y
                return z

            x = torch.randn(4, 4, dtype=torch.float, device=device)
            y = torch.randn(4, 4, dtype=torch.float, device=device)

            ge = self.checkTrace(f, (x, y))
            self.assertAllFused(ge.graph_for(x, y))
            x.requires_grad_(True)
            y.requires_grad_(True)
            self.assertAllFused(ge.graph_for(x, y), except_for=("aten::size", "prim::BroadcastSizes",
                                                                "aten::_size_if_not_equal"))

    def test_addcmul(self):
        for device in self.devices:
            t = torch.randn(1, 4, dtype=torch.float, device=device)
            t1 = torch.randn(4, 1, dtype=torch.float, device=device)
            t2 = torch.randn(1, 4, dtype=torch.float, device=device)

            def foo(t, t1, t2):
                return t.addcmul(t + 1, t2, value=0.1)

            ge = self.checkTrace(foo, (t, t1, t2), allow_unused=True)
            graph = ge.graph_for(t, t1, t2)
            fusion_groups = self.findFusionGroups(graph)
            self.assertEqual(len(fusion_groups), 1)
            FileCheck().check("aten::add(").check("aten::addcmul(").run(str(fusion_groups[0]))

    # TODO: We leak CUDA memory here because the traced graph holds onto a
    # constant-ified tensor. Since the Python-global CompilationUnit is alive
    # until the end of the process, the memory is effectively leaked.
    # Removed `_cuda` suffix from this test which disables leak-checking.
    # If this is a real problem, we'll need to revisit Torchscript Function
    # lifetimes in Python.
    def test_lerp(self):
        for device in self.devices:
            start = torch.randn(4, 1, dtype=torch.float, device=device)
            end = torch.randn(1, 4, dtype=torch.float, device=device)
            weight = torch.tensor(0.5, dtype=torch.float, device=device)

            # scalar weight overload
            def foo_weight_scalar(start, end):
                return torch.lerp(start + 1, end, 0.5)

            # tensor weight overload
            def foo_weight_tensor(start, end):
                return torch.lerp(start + 1, end, weight)

            ge_weight_scalar = self.checkTrace(foo_weight_scalar, (start, end))
            graph = ge_weight_scalar.graph_for(start, end)
            self.assertAllFused(graph)

            # TODO: uncomment when TE enables support for scalar tensors
            # ge_weight_tensor = self.checkTrace(foo_weight_tensor, (start, end))
            # graph = ge_weight_tensor.graph_for(start, end)
            # self.assertAllFused(graph)

    def test_concat(self):
        # disabling concat causes error with single concat node
        with set_fusion_group_inlining(True):
            for device in self.devices:
                hx = torch.randn(3, 20, dtype=torch.float, device=device)
                cx = torch.randn(3, 20, dtype=torch.float, device=device)

                def foo(hx, cx):
                    return torch.cat((hx + cx, hx * cx))

                ge = self.checkTrace(foo, (hx, cx))
                graph = ge.graph_for(hx, cx)
                self.assertAllFused(graph)
                # XXX: TE fuser can handle concats in a fusion group.
                # FileCheck().check("FusedConcat").check_next("return").run(str(graph))

    def test_remove_output_used_only_in_size(self):
        for device in self.devices:
            def test_fuse(a, b):
                c = a + b
                d = c + b
                return d

            scripted_f = torch.jit.script(test_fuse)
            x = torch.ones(1, requires_grad=True, device=device)
            y = torch.ones(1, requires_grad=True, device=device)
            warmup_forward(scripted_f, x, y, profiling_count=3)
            g = scripted_f.graph_for(x, y)
            diff_nodes = g.findAllNodes('prim::DifferentiableGraph')
            self.assertEqual(len(diff_nodes), 1)
            g = diff_nodes[0].g('Subgraph')
            if_nodes = [n for n in g.nodes() if n.kind() == 'prim::If']
            self.assertEqual(len(if_nodes), 1)
            # the if node and the fusion group inside it should only have one output
            self.assertEqual(len(list(if_nodes[0].outputs())), 1)

    def test_concat_invariant(self):
        for device in self.devices:
            # Invariant: the output of prim::FusedConcat may
            # not be an input to any node inside the FusionGroup.
            def fn(x, y, z):
                x1 = x + y
                y1 = x - y
                w = torch.cat([x1, y1])
                return w + z

            x = torch.randn(2, 2, dtype=torch.float, device=device)
            y = torch.randn(2, 2, dtype=torch.float, device=device)
            z = torch.randn(4, 2, dtype=torch.float, device=device)
            ge = self.checkTrace(fn, (x, y, z))
            graph = ge.graph_for(x, y, z)
            self.assertAllFused(graph, except_for={'aten::add'})
            # XXX: TE fuser can handle concats inside a fusion group.
            # FileCheck().check("FusedConcat").check_next("return").run(str(graph))

    @staticmethod
    def fn_test_exp(x, y):
        return (x + .5 * y).exp()

    def test_exp(self):
        for device in self.devices:
            x = torch.randn(4, 4, dtype=torch.float, device=device)
            y = torch.randn(4, 4, dtype=torch.float, device=device)

            ge = self.checkTrace(self.fn_test_exp, (x, y))
            self.assertAllFused(ge.graph_for(x, y))

    def test_threshold(self):
        for device in self.devices:
            def f(x):
                return torch.threshold(x, 0, -10) + x + x + x

            x = torch.tensor([-1, -0.5, 0, 1, 2, 3], device=device)
            scripted = self.checkScript(f, (x,))
            self.assertAllFused(scripted.graph_for(x))

    def test_scalar_arg(self):
        for device in self.devices:
            def fn_test_scalar_arg(x: torch.Tensor, p: float) -> torch.Tensor:
                return p * (x * x + x)

            x = torch.randn(4, 4, dtype=torch.float, device=device)
            p = 3
            scripted = self.checkScript(fn_test_scalar_arg, (x, p))
            self.assertAllFused(scripted.graph_for(x, p))

            x.requires_grad_(True)

            # use another function otherwise we will bailout
            # and won't be able to do fused checks
            def fn_test_scalar_arg_requires_grad(x: torch.Tensor, p: float) -> torch.Tensor:
                return p * (x * x + x)

            scripted = torch.jit.script(fn_test_scalar_arg_requires_grad)
            out = scripted(x, p)
            out = scripted(x, p)
            out = scripted(x, p)
            self.assertAllFused(scripted.graph_for(x, p), except_for=("aten::size", "prim::BroadcastSizes",
                                                                      "aten::_size_if_not_equal"))

    @unittest.skipIf(not RUN_CUDA, "fuser requires CUDA")
    @unittest.skipIf(not RUN_CUDA_MULTI_GPU, "needs non-zero device")
    def test_fusion_reuse_multi_gpu(self):
        def fn(x, y):
            return x * y * x * y

        inputs_cpu = [
            torch.randn(4, 4, dtype=torch.float),
            torch.randn(4, 4, dtype=torch.float),
        ]
        inputs_cuda0 = [x.cuda(0) for x in inputs_cpu]
        inputs_cuda1 = [y.cuda(1) for y in inputs_cpu]

        # Should not crash; these should compile different kernels.
        ge = self.checkScript(fn, inputs_cpu)
        self.assertAllFused(ge.graph_for(*inputs_cpu))
        ge(*inputs_cuda0)
        ge(*inputs_cuda1)

    # TODO: we're currently not checking 'device' in the type info when pulling
    # nodes into a fusion group. We should fix that and re-enable this test.
    @unittest.skipIf(not RUN_CUDA, "fuser requires CUDA")
    @unittest.skipIf(not RUN_CUDA_MULTI_GPU, "needs non-zero device")
    def test_kernel_cache_multi_gpu(self):
        def not_fusible(x):
            return x

        def fn(x, y, z):
            x_out = x * x * x * x * x  # fusion: lambda x. x * x * x * x * x
            y_out = y * y * y * y * y
            z_out = z * z * z * z * z
            return not_fusible(x_out), not_fusible(y_out), not_fusible(z_out)

        inputs = [
            torch.randn(4, 4, dtype=torch.float),
            torch.randn(4, 4, dtype=torch.float, device='cuda:0'),
            torch.randn(4, 4, dtype=torch.float, device='cuda:1'),
        ]

        prev_cache_size = torch._C._jit_debug_fuser_num_cached_kernel_specs()

        # There are 3 FusionGroups. Because they have the same graph, they
        # should reuse the same KernelSpec in the KernelSpec cache.
        ge = self.checkScript(fn, inputs)
        self.assertGraphContainsExactly(
            ge.graph_for(*inputs), FUSION_GROUP, 3, True)
        new_cache_size = torch._C._jit_debug_fuser_num_cached_kernel_specs()
        # XXX: This assumes that the same kernel isn't already used by another test
        # FIXME: Use the TE fuser's way of querying the cache.
        # self.assertEqual(new_cache_size - prev_cache_size, 1)

    @unittest.skipIf(not RUN_CUDA_MULTI_GPU, "needs non-zero device")
    def test_nonzero_device_cuda(self):
        device = 'cuda:' + str(1)
        x = torch.tensor([0.4], dtype=torch.float, device=device)
        y = torch.tensor([0.7], dtype=torch.float, device=device)

        def doit(x, y):
            return torch.sigmoid(torch.tanh(x * (x + y) + x))

        ge = self.checkTrace(doit, (x, y))
        self.assertAllFused(ge.graph_for(x, y))

    def test_lstm(self):
        for device in self.devices:
            inputs = get_lstm_inputs(device, training=True)
            module = self.checkScript(LSTMCellS, inputs)
            self.assertAllFused(module.graph_for(inputs))

    def test_lstm_concat(self):
        # single fusion node causes error
        with set_fusion_group_inlining(True):
            for device in self.devices:
                inputs = get_lstm_inputs(device)
                ge = self.checkTrace(LSTMCellC, inputs)
                graph = ge.graph_for(*inputs)
                self.assertAllFused(ge.graph_for(*inputs))
                # XXX: TE fuser can handle concats inside a fusion group.
                # FileCheck().check("FusedConcat").check_next("return").run(str(graph))

    def test_lstm_gates_permutations(self):
        for device in self.devices:
            # lstm has gates = x.mm(w_ih.t()) + hx.mm(w_hh.t()) + b_ih + b_hh.
            # Test that any permutation of this will still result in one FusionGroup.
            choices = ['x.mm(w_ih.t())', 'hx.mm(w_hh.t())', 'b_ih', 'b_hh']
            template = dedent('''
            def cell(x, hx, cx, w_ih, w_hh, b_ih, b_hh):
                gates = {} + {} + {} + {}
                ingate, forgetgate, cellgate, outgate = gates.chunk(4, 1)
                return ingate * forgetgate * cellgate * outgate
            ''')
            for permutation in permutations(choices, len(choices)):
                code = template.format(*permutation)
                scope = {}
                exec(code, globals(), scope)
                cu = torch.jit.CompilationUnit(code)

                inputs = get_lstm_inputs(device, training=False)
                self.assertEqual(cu.cell(*inputs), scope['cell'](*inputs))
                forward_graph = cu.cell.graph_for(*inputs)
                self.assertGraphContainsExactly(forward_graph, FUSION_GROUP, 1)

    # TODO: Fuser doesn't work at all when inputs require grad. Fix that
    def test_lstm_traced(self):
        for device in self.devices:
            inputs = get_lstm_inputs(device)
            ge = self.checkTrace(LSTMCellF, inputs)
            graph = ge.graph_for(*inputs)
            fusion_groups = self.findFusionGroups(graph)
            self.assertEqual(len(fusion_groups), 1)
            FileCheck().check("Chunk").check("aten::sigmoid").check("aten::tanh").run(str(fusion_groups[0]))

    def test_milstm(self):
        for device in self.devices:
            inputs = get_milstm_inputs(device, training=True)
            module = self.checkScript(MiLSTMCell, inputs)
            forward_graph = module.graph_for(*inputs)
            self.assertGraphContainsExactly(
                forward_graph, FUSION_GROUP, 1, consider_subgraphs=True)
            FileCheck().check("DifferentiableGraph").check("TupleConstruct") \
                .check_next("return").check(FUSION_GROUP).run(str(forward_graph))
            hy, cy = module(*inputs)
            warmup_backward((hy + cy).sum())

    @unittest.skipIf(not RUN_CUDA, "fuser requires CUDA")
    @unittest.skip("rand_like is not supported yet")
    def test_rand_cuda(self):
        class M(torch.jit.ScriptModule):
            __constants__ = ['d']

            def __init__(self):
                super(M, self).__init__()
                self.d = torch.device('cuda')

            @torch.jit.script_method
            def create(self, x):
                return x * x + x + torch.rand_like(x)

        x = torch.zeros([3, 4, 5], dtype=torch.float, device='cuda')
        m = M()
        out1 = m.create(x)
        out2 = m.create(x)
        self.assertNotEqual(out1, out2)
        self.assertTrue(torch.all(out1 >= 0))
        self.assertTrue(torch.all(out1 < 1))
        self.assertTrue(torch.all(out2 >= 0))
        self.assertTrue(torch.all(out2 < 1))
        self.assertAllFused(m.create.graph_for(x))

    @staticmethod
    def fn_test_relu(x, y):
        return F.relu(x + .5 * y)

    def test_relu(self):
        for device in self.devices:
            x = torch.randn(4, 4, dtype=torch.float, device=device)
            y = torch.randn(4, 4, dtype=torch.float, device=device)

            ge = self.checkTrace(self.fn_test_relu, (x, y))
            self.assertAllFused(ge.graph_for(x, y))

    def test_erf(self):
        for device in self.devices:
            def fn_test_erf(x):
                return F.relu(torch.erf(x) - torch.erfc(x))

            x = torch.randn(4, 4, dtype=torch.float, device=device)
            ge = self.checkScript(fn_test_erf, (x,), profiling=ProfilingMode.PROFILING)
            self.assertAllFused(ge.graph_for(x))
            x.requires_grad_(True)
            ge = self.checkScript(fn_test_erf, (x,), profiling=ProfilingMode.PROFILING)
            self.assertAllFused(ge.graph_for(x), except_for=("aten::size", "prim::BroadcastSizes",
                                                             "aten::_size_if_not_equal"))

    @unittest.skipIf(not RUN_CUDA, "fuser requires CUDA")
    @unittest.skip("rand_like is not supported yet")
    def test_rand_broadcast_cuda(self):
        def fn_test_rand(x, y):
            r = torch.rand_like(y)
            return r * x + x

        # If using profiling, a different function is needed to test different
        # shapes, or we'll use a cached script.
        def fn_test_rand2(x, y):
            r = torch.rand_like(y)
            return r * x * x

        x = torch.randn(4, 4, dtype=torch.float, device='cuda')
        y = torch.randn(4, 4, dtype=torch.float, device='cuda')
        script_f = torch.jit.script(fn_test_rand)
        warmup_forward(script_f, x, y)
        out = script_f(x, y)
        self.assertAllFused(script_f.graph_for(x, y))
        x.requires_grad_(True)
        out = script_f(x, y)
        self.assertAllFused(script_f.graph_for(x, y), except_for=("aten::size", "prim::BroadcastSizes",
                                                                  "aten::_size_if_not_equal"))

        # test that broadcasting random produces correct results
        x = torch.ones(4, 4, dtype=torch.float, device='cuda')
        y = torch.ones(4, dtype=torch.float, device='cuda')
        script_f = torch.jit.script(fn_test_rand2)
        warmup_forward(script_f, x, y)
        out = script_f(x, y)
        # TODO(#38095): Replace assertEqualIgnoreType. See issue #38095
        self.assertEqualIgnoreType(out[0, :] + torch.zeros(4, 4, device='cuda'), out)

    @unittest.skipIf(not RUN_CUDA, "fuser requires CUDA")
    @unittest.skip("rand_like is not supported yet")
    def test_rand_diamond(self):
        def fn_test_diamond(x, y):
            r = torch.rand_like(y)
            a = x + r
            b = y - r
            return a + b

        x = torch.randn(4, 4, dtype=torch.float, device='cuda')
        y = torch.randn(4, 4, dtype=torch.float, device='cuda')
        script_f = torch.jit.script(fn_test_diamond)
        warmup_forward(script_f, x, y)
        out = script_f(x, y)
        self.assertEqual(out, x + y)

    def test_scalar(self):
        def fn(x, y):
            return 2 * x + y

        x = torch.tensor(0.1, dtype=torch.float, device='cpu')
        y = torch.tensor(1, dtype=torch.float, device='cpu')
        ge = self.checkScript(fn, (x, y))
        self.assertAllFused(ge.graph_for(x, y))

    def test_small_constant(self):
        for device in self.devices:
            def fn_test_small_constant(x, y):
                return (1e-8 * x + 5e-9 * y) * 1e8
            x = torch.randn(4, 4, dtype=torch.float, device=device)
            y = torch.randn(4, 4, dtype=torch.float, device=device)

            ge = self.checkTrace(fn_test_small_constant, (x, y))
            self.assertAllFused(ge.graph_for(x, y))

    # Currently we don't pull constants into fusion groups, because in some
    # cases it could remove the constant from the original graph and now our
    # fusion group needs to return that constant for its other users.
    # Instead of never pulling constants into the fusion group, we should just
    # be more careful at how we rewrite its users.
    # TODO: fix that and reenable the test.
    def test_tensor_scalar_ops(self):
        for device in self.devices:
            def should_fuse(x):
                z = 3.
                y = x + z
                return x * y

            def should_fuse_scalar(x, z):
                y = x + int(z)
                return x * y

            inputs = [torch.randn(2, 2, dtype=torch.float, device=device)]
            ge = self.checkScript(should_fuse, inputs)
            graph = ge.graph_for(*inputs)
            fusion_groups = self.findFusionGroups(graph)
            self.assertEqual(len(fusion_groups), 1)
            FileCheck().check("aten::add").check("aten::mul").run(str(fusion_groups[0]))

            inputs = [
                torch.randn(2, 2, dtype=torch.float, device=device),
                torch.tensor(3., dtype=torch.float, device=device),
            ]
            ge = self.checkScript(should_fuse_scalar, inputs)
            # Check that the fused graph computes correct results when the scalar
            # input changes.
            inputs = [
                torch.randn(2, 2, dtype=torch.float, device=device),
                torch.tensor(7., dtype=torch.float, device=device),
            ]
            self.assertEqual(ge(*inputs), should_fuse_scalar(*inputs))
            # The TE fuser supports fusion of non-constant scalars
            self.assertGraphContainsExactly(
                ge.graph_for(*inputs), FUSION_GROUP, 1, consider_subgraphs=True)

    def test_where_and_typing(self):
        for device in self.devices:
            def f(x, y):
                mask = x > y
                res = torch.where(mask, x, y)
                return mask, res

            x = torch.randn(4, 4, dtype=torch.double, device=device)
            y = torch.randn(4, 4, dtype=torch.double, device=device)

            script_f = self.checkScript(f, (x, y))
            self.assertAllFused(script_f.graph_for(x, y), except_for={'prim::TupleConstruct'})

    def test_disabled(self):
        old_cpu_fuser_state = torch._C._jit_can_fuse_on_cpu()
        torch._C._jit_override_can_fuse_on_cpu(False)

        def fn(a):
            return a ** 2 + a

        x = torch.randn(4, dtype=torch.float, device="cpu")
        s = self.checkScript(fn, (x,))
        g = s.graph_for(x)
        self.assertEqual(len(self.findFusionGroups(g)), 0)

        torch._C._jit_override_can_fuse_on_cpu(old_cpu_fuser_state)

    def data_for(self, dtype, device="cuda", size=None):
        if size is None:
            v = torch.arange(1, 3, dtype=torch.float, device=device)
        else:
            v = torch.rand(*size, device=device)
        if dtype == torch.bool:
            return v > 2
        elif dtype in [torch.qint8, torch.quint8, torch.qint32]:
            return torch.quantize_per_tensor(v, 0.1, 1, dtype=dtype)
        else:
            return v.to(dtype)

    def test_torch_to(self):
        # test no op
        @torch.jit.script
        def foo(x):
            return x.to(torch.float)

        foo(torch.tensor([3.], dtype=torch.float))
        foo(torch.tensor([3.], dtype=torch.float))
        FileCheck().check_not("TensorExpr").run(torch.jit.last_executed_optimized_graph())

        # test not fusing non-const inputs
        @torch.jit.script
        def foo(x, dtype: int):
            return x.to(dtype)

        foo(torch.tensor([3.], dtype=torch.float), torch.int)
        foo(torch.tensor([3.], dtype=torch.float), torch.int)
        FileCheck().check_not("TensorExpr").run(torch.jit.last_executed_optimized_graph())

        # test not fusing to_pinned inputs
        @torch.jit.script
        def foo(x, dtype: int):
            return x.to(pin_memory=True)

        foo(torch.tensor([3.], dtype=torch.float), torch.int)
        foo(torch.tensor([3.], dtype=torch.float), torch.int)
        FileCheck().check_not("TensorExpr").run(torch.jit.last_executed_optimized_graph())


        # test across-device not supported
        if torch.cuda.is_available():
            @torch.jit.script
            def foo(x):
                return x.to(device="cuda")

            foo(torch.tensor([3.], dtype=torch.float))
            foo(torch.tensor([3.], dtype=torch.float))
            FileCheck().check_not("TensorExpr").run(torch.jit.last_executed_optimized_graph())

        sizes = [(1, 4), (4, 4)]
        # reuses cast impl, smaller dtype set for faster test
        dtypes = [
            torch.bool,
            torch.int,
            torch.float16,
            torch.float32,
            torch.float64,
        ]

        class MyMod(torch.nn.Module):
            def __init__(self, dtype):
                super(MyMod, self).__init__()
                self.dtype = dtype

            def forward(self, x):
                return x.to(self.dtype)

        bad_dtypes = []
        for dtype, output_dtype, device, size in product(dtypes, dtypes, self.devices, sizes):
            # TODO: Add back when https://github.com/pytorch/pytorch/issues/55905 is closed
            if dtype in [torch.float16, torch.bfloat16] and device == "cpu":
                continue
            if dtype == output_dtype:
                continue

            x = self.data_for(dtype, device, size=size)
            mod = MyMod(output_dtype)
            ref = mod.forward(x)
            # use freezing to make non-Tensor args to `to` constant
            mod = torch.jit.freeze(torch.jit.script(mod.eval()))
            warmup_forward(mod.forward, x)
            self.assertEqual(ref, mod.forward(x))
            self.assertLastGraphAllFused()

    @unittest.skip("Temporarily disabled")
    def test_masked_fill(self):
        dtypes = [
            torch.int8,
            torch.int16,
            torch.int32,
            torch.int64,
            # TODO: Add back when https://github.com/pytorch/pytorch/issues/55905 is closed
            # torch.float16,
            torch.float32,
            torch.float64,
            torch.bool,
        ]
        sizes = [(2,), (4, 4)]
        for self_dtype, device, scalar_val, size in product(dtypes, self.devices, [0.4, 3], sizes):
            input_v = self.data_for(self_dtype, device, size=size)
            mask = self.data_for(torch.bool, device, size=size)

            def fn(input_v, mask):
                return torch.masked_fill(input_v, mask, scalar_val)
            ref = fn(input_v, mask)
            try:
                t = torch.jit.trace(fn, (input_v, mask))
                torch.testing.assert_close(ref, t(input_v, mask))
                print(torch.jit.last_executed_optimized_graph())
                self.assertLastGraphAllFused()
            except Exception as e:
                raise RuntimeError(
                    " ".join(["Failed:", str(self_dtype), op.__name__, device, str(size)])
                )

    def test_isnan(self):
        x = torch.rand([4])
        x[0] = float('nan')
        inputs = [
            x,
            torch.tensor([float('nan'), .5])
        ]
        dtypes = [
            torch.int8,
            torch.int16,
            torch.int32,
            torch.int64,
            torch.float16,
            torch.float32,
            torch.float64,
            torch.bool,
        ]

        for inp, device, dtype in product(inputs, self.devices, dtypes):
            # TODO: Add back when https://github.com/pytorch/pytorch/issues/55905 is closed
            if dtype in [torch.float16, torch.bfloat16] and device == "cpu":
                continue
            inp = inp.to(device=device, dtype=dtype)
            try:
                f = torch.jit.trace(lambda x: x.isnan(), (inp,))
                warmup_forward(f, inp)
                self.assertEqual(f(inp), inp.isnan())
                self.assertLastGraphAllFused()
            except Exception as e:
                raise RuntimeError(
                    " ".join(["Failed:", str(dtype), 'isnan', device])
                )

    def test_unary_ops(self):
        def apply(fn):
            return lambda x: fn(x)

        unary_ops = [
            torch.lgamma,
            torch.sigmoid,
            torch.reciprocal,
            torch.neg,
            torch.relu,
            F.relu6,
            torch.log,
            torch.log10,
            torch.log1p,
            torch.log2,
            torch.exp,
            torch.expm1,
            torch.erf,
            torch.erfc,
            torch.cos,
            torch.sin,
            torch.tan,
            torch.acos,
            torch.asin,
            torch.cosh,
            torch.sinh,
            torch.atan,
            torch.tanh,
            F.hardtanh,
            F.hardsigmoid,
            F.hardswish,
            F.softplus,
            torch.sqrt,
            torch.rsqrt,
            F.gelu,
            torch.abs,
            torch.ceil,
            torch.floor,
            torch.round,
            torch.trunc,
            torch.frac,
            # TODO: broken on ROCm?
            # F.hardshrink,
            F.leaky_relu,
            lambda x: torch.threshold(x, 0, -10),
            lambda x: torch.clamp(x, -10, 10),
        ]
        gpu_only = {torch.erf, torch.erfc}
        sizes = [(1,), (2,), (4, 4)]
        for dtype, op, device, size in product(self.dtypes, unary_ops, self.devices, sizes):
            # TODO: Add back when https://github.com/pytorch/pytorch/issues/55905 is closed
            if dtype in [torch.float16, torch.bfloat16] and device == "cpu":
                continue
            if op in gpu_only and device == "cpu":
                continue
            try:
                x = self.data_for(dtype, device, size=size)
                fn = apply(op)
                ref = fn(x)
            except Exception:
                # If eager mode doesn't support a dtype/op/device combo,
                # neither does the fuser.  Catch everything to avoid needing to
                # guess what errors might be thrown by eager.
                continue
            try:
                t = torch.jit.trace(fn, (x,))
                torch.testing.assert_close(ref, t(x))
                self.assertAllFused(t.graph_for(x))
            except Exception as e:
                raise RuntimeError(
                    " ".join(["Failed:", str(dtype), op.__name__, device, str(size)])
                )

    def test_binary_ops(self):
        def apply(fn):
            return lambda x, y: fn(x, y)

        binary_ops = [
            operator.__and__,
            operator.__or__,
            operator.__xor__,
            torch.add,
            torch.sub,
            torch.mul,
            torch.min,
            torch.max,
            lambda x, y: torch.lerp(x, y, 0.5),
            torch.atan2,
            torch.div,
            torch.eq,
            torch.ne,
            torch.ge,
            torch.gt,
            torch.lt,
            torch.fmod,
            torch.remainder,
            lambda x, y: y.type_as(x),
        ]
        fp_only = [
            torch.fmod,
            torch.remainder,
        ]
        devices = self.devices
        for dtype, op, device in product(self.dtypes, binary_ops, devices):
            if dtype in [torch.float16, torch.bfloat16] and device == "cpu":
                continue
            try:
                x = self.data_for(dtype, device)
                y = self.data_for(dtype, device)
                fn = apply(op)
                ref = fn(x, y)
            except Exception:
                # If eager mode doesn't support a dtype/op/device combo,
                # neither does the fuser.  Catch everything to avoid needing to
                # guess what errors might be thrown by eager.
                continue
            try:
                t = torch.jit.trace(fn, (x, y))
                self.assertEqual(ref, t(x, y))
                if op not in fp_only or dtype.is_floating_point:
                    self.assertAllFused(t.graph_for(x, y))
            except Exception as e:
                raise RuntimeError(
                    " ".join(["Failed:", str(dtype), op.__name__, device])
                )

    def test_matmul(self):
        def fn(x, y):
            return torch.matmul(x, y)

        devices = ['cpu']  # No cuda support for ext calls yet
        sizes = [[[128, 128], [128, 128]],
                 [[10, 10], [10, 10]],
                 [[1, 16], [16, 128]],
                 [[128], [128]],
                 [[128], [128, 128]],
                 [[3], [3]],
                 [[3, 4], [4]],
                 [[10, 3, 4], [4]],
                 [[10, 3, 4], [10, 4, 5]],
                 [[10, 3, 4], [4, 5]],
                 ]

        # Only 2D x 2D matrix multiply is supported. For non-supported sizes we
        # still want to run results verification to test that we didn't
        # accidentally fuse it, but we skip the 'is-fused' check.
        # TODO: add support for other shape combinations and make this set empty:
        skip_is_fused_check_sizes = ["[[128], [128]]",
                                     "[[128], [128, 128]]",
                                     "[[3], [3]]",
                                     "[[3, 4], [4]]",
                                     "[[10, 3, 4], [4]]",
                                     "[[10, 3, 4], [10, 4, 5]]",
                                     "[[10, 3, 4], [4, 5]]",
                                     ]
        for dtype, size, device in product(self.dtypes, sizes, devices):
            if dtype in [torch.float16, torch.bfloat16] and device == "cpu":
                continue
            try:
                size_x, size_y = size
                x = self.data_for(dtype, device, size=size_x)
                y = self.data_for(dtype, device, size=size_y)
                ref = fn(x, y)
            except Exception as e:
                # If eager mode doesn't support a dtype/op/device combo,
                # neither does the fuser.  Catch everything to avoid needing to
                # guess what errors might be thrown by eager.
                continue
            try:
                t = torch.jit.trace(fn, (x, y))
                t(x, y)
                self.assertEqual(ref, t(x, y))
                if not str(size) in skip_is_fused_check_sizes:
                    self.assertAllFused(t.graph_for(x, y))
            except Exception as e:
                raise RuntimeError(
                    " ".join(["Failed:", str(dtype), device])
                )

    def test_binary_tensor_scalar_ops(self):
        def apply_with_scalar(fn, scalar):
            return lambda x: fn(x, scalar)

        # FIXME: Fails in IR Eval: torch.int64 and_ cpu
        binary_ops = [
            operator.__and__,
            operator.__or__,
            operator.__xor__,
            torch.add,
            torch.sub,
            torch.mul,
            torch.eq,
            torch.ne,
            torch.ge,
            torch.lt,
            torch.gt,
        ]
        devices = self.devices
        # Maybe we should split this into separate tests to speed it up by
        # only using  scalar values relevant to particular ops
        scalars = [1.5, 3, 0, -2.0, -1]
        for dtype, op, device, scalar in product(self.dtypes, binary_ops, devices, scalars):
            if dtype in [torch.float16, torch.bfloat16] and device == "cpu":
                continue
            try:
                x = self.data_for(dtype, device)
                fn = apply_with_scalar(op, scalar)
                ref = fn(x)
            except Exception:
                # If eager mode doesn't support a dtype/op/device combo,
                # neither does the fuser.  Catch everything to avoid needing to
                # guess what errors might be thrown by eager.
                continue
            try:
                t = torch.jit.trace(fn, (x))
                self.assertEqual(ref, t(x))
                self.assertAllFused(t.graph_for(x))
            except Exception as e:
                raise RuntimeError(
                    " ".join(["Failed:", str(dtype), op.__name__, device])
                )

    def test_binary_div_ops(self):
        def apply_with_scalar(fn, scalar):
            return lambda x: fn(x, scalar)

        binary_ops = [
            torch.div,
            torch.remainder,
            torch.fmod,
        ]
        devices = self.devices
        # Maybe we should split this into separate tests to speed it up by
        # only using  scalar values relevant to particular ops
        scalars = [1.5, 3, -2.0, -1]  # skip 0
        for dtype, op, device, scalar in product(self.dtypes, binary_ops, devices, scalars):
            if dtype in [torch.float16, torch.bfloat16] and device == "cpu":
                continue
            try:
                x = self.data_for(dtype, device)
                fn = apply_with_scalar(op, scalar)
                ref = fn(x)
            except Exception:
                # If eager mode doesn't support a dtype/op/device combo,
                # neither does the fuser.  Catch everything to avoid needing to
                # guess what errors might be thrown by eager.
                continue
            try:
                t = torch.jit.trace(fn, (x))
                self.assertEqual(ref, t(x))
            except Exception as e:
                raise RuntimeError(
                    "Failed: {} {} {} {}".format(dtype, op.__name__, device, scalar)
                )

    def test_binary_pow(self):
        def apply_with_scalar(fn, scalar):
            return lambda x: fn(x, scalar)

        dtypes = [
            # FIXME: 'pow' fails with dtype=torch.float16/device=cuda/scalar=0
            # torch.float16,
            torch.float32,
            torch.float64,
            # torch.bool intentionally not included
        ]
        binary_ops = [
            torch.pow,
        ]
        # Maybe we should split this into separate tests to speed it up by
        # only using  scalar values relevant to particular ops
        scalars = [1.5, 3, 0, -2.0, -1]
        for dtype, op, device, scalar in product(dtypes, binary_ops, self.devices, scalars):
            if dtype in [torch.float16, torch.bfloat16] and device == "cpu":
                continue
            try:
                x = self.data_for(dtype, device)
                fn = apply_with_scalar(op, scalar)
                ref = fn(x)
            except Exception:
                # If eager mode doesn't support a dtype/op/device combo,
                # neither does the fuser.  Catch everything to avoid needing to
                # guess what errors might be thrown by eager.
                continue
            try:
                t = torch.jit.trace(fn, (x))
                self.assertEqual(ref, t(x))
                self.assertAllFused(t.graph_for(x))
            except Exception as e:
                raise RuntimeError(
                    " ".join(["Failed:", str(dtype), op.__name__, device])
                )

    def test_ternary_ops(self):
        def apply(fn):
            return lambda x, y, z: fn(x, y, z)

        ternary_ops = [
            torch.lerp,
            torch.addcmul,
        ]
        devices = self.devices
        for dtype, op, device in product(self.dtypes, ternary_ops, devices):
            if dtype in [torch.float16, torch.bfloat16] and device == "cpu":
                continue
            try:
                x = self.data_for(dtype, device)
                y = self.data_for(dtype, device)
                z = self.data_for(dtype, device)
                fn = apply(op)
                ref = fn(x, y, z)
            except Exception:
                # If eager mode doesn't support a dtype/op/device combo,
                # neither does the fuser.  Catch everything to avoid needing to
                # guess what errors might be thrown by eager.
                continue
            try:
                t = torch.jit.trace(fn, (x, y, z))
                self.assertEqual(ref, t(x, y, z))
                self.assertAllFused(t.graph_for(x, y, z))
            except Exception as e:
                raise RuntimeError(
                    " ".join(["Failed:", str(dtype), op.__name__, device])
                )

    def test_ternary_norm_ops(self):
        def apply(fn):
            return lambda x, y, z: fn(x, y, z)

        ternary_ops = [
            F.batch_norm,
        ]
        devices = self.devices
        for dtype, op, device in product(self.dtypes, ternary_ops, devices):
            if dtype in [torch.float16, torch.bfloat16] and device == "cpu":
                continue
            try:
                x = self.data_for(dtype, device, size=[5, 3, 128, 128])
                y = self.data_for(dtype, device, size=[3])
                z = self.data_for(dtype, device, size=[3])
                fn = apply(op)
                ref = fn(x, y, z)
            except Exception:
                # If eager mode doesn't support a dtype/op/device combo,
                # neither does the fuser.  Catch everything to avoid needing to
                # guess what errors might be thrown by eager.
                continue
            try:
                t = torch.jit.trace(fn, (x, y, z))
                self.assertEqual(ref, t(x, y, z))
                self.assertAllFused(t.graph_for(x, y, z))
            except Exception as e:
                raise RuntimeError(
                    " ".join(["Failed:", str(dtype), op.__name__, device])
                )


    @unittest.skip("FIXME: fuser doesn't include ListConstruct nodes to the group causing a failure")
    def test_list_ops(self):
        def apply(fn):
            return lambda x, y, z: fn([x * x, y * y, z * z])

        devices = self.devices
        list_ops = [
            torch.cat,
        ]
        for dtype, op, device in product(self.dtypes, list_ops, devices):
            if dtype in [torch.float16, torch.bfloat16] and device == "cpu":
                continue
            try:
                x = self.data_for(dtype, device, size=[5, 4, 1, 7])
                y = self.data_for(dtype, device, size=[5, 4, 1, 7])
                z = self.data_for(dtype, device, size=[5, 4, 1, 7])
                fn = apply(op)
                ref = fn(x, y, z)
            except Exception:
                # If eager mode doesn't support a dtype/op/device combo,
                # neither does the fuser.  Catch everything to avoid needing to
                # guess what errors might be thrown by eager.
                continue
            try:
                t = torch.jit.trace(fn, (x, y, z))
                self.assertEqual(ref, t(x, y, z))
                self.assertAllFused(t.graph_for(x, y, z))
            except Exception as e:
                raise RuntimeError(
                    " ".join(["Failed:", str(dtype), op.__name__, device])
                )

    def test_where_ops(self):
        def apply(fn):
            return lambda cond, x, y: fn(cond, x, y)

        ops = [
            torch.where,
            lambda cond, x, y: torch.where(cond, x, 3.1415),
            lambda cond, x, y: torch.where(cond, 42, y),
        ]
        devices = self.devices
        for dtype, op, device in product(self.dtypes, ops, devices):
            if dtype in [torch.float16, torch.bfloat16] and device == "cpu":
                continue
            try:
                cond = self.data_for(torch.bool, device)
                x = self.data_for(dtype, device)
                y = self.data_for(dtype, device)
                fn = apply(op)
                ref = fn(cond, x, y)
            except Exception:
                # If eager mode doesn't support a dtype/op/device combo,
                # neither does the fuser.  Catch everything to avoid needing to
                # guess what errors might be thrown by eager.
                continue
            try:
                t = torch.jit.trace(fn, (cond, x, y))
                self.assertEqual(ref, t(cond, x, y))
                self.assertAllFused(t.graph_for(cond, x, y))
            except Exception as e:
                raise RuntimeError(
                    " ".join(["Failed:", str(dtype), op.__name__, device])
                )

    def test_unsupported_dtypes(self):
        for device in self.devices:
            def fn(x):
                return x * x + x

            unsupported_dtypes = [
                torch.uint8,
                torch.complex32,
                torch.complex64,
                torch.complex128,
                torch.qint8,
                torch.quint8,
                torch.qint32,
            ]
            for dtype in unsupported_dtypes:
                try:
                    x = self.data_for(dtype, device)
                    ref = fn(x)
                except Exception:
                    # If eager mode doesn't support a dtype/op/device combo,
                    # neither does the fuser.  Catch everything to avoid needing to
                    # guess what errors might be thrown by eager.
                    continue
                t = torch.jit.trace(fn, (x,))
                self.assertEqual(ref, t(x))
                self.assertEqual(len(self.findFusionGroups(t.graph_for(x))), 0)

    def test_superslomo(self):
        devices = self.devices.copy()
        if not LLVM_ENABLED:
            devices.remove("cpu")
        for device in devices:
            # Test extracted from Super-SloMo: https://github.com/avinashpaliwal/Super-SloMo
            # A few interesting things happen here: strided inputs of mixed size,
            # plus outputs of mixed shapes.  The latter characteristic happened to
            # expose a memory corruption bug due to not properly guarding the
            # outputs.
            def eager(t0, t1, t2, t3, t4):
                t5 = torch.mul(t0, t4)
                t6 = torch.mul(t2, t3)
                t7 = torch.mul(t6, t1)
                t9 = torch.add(t5, t7)
                t11 = torch.add(t0, t6)
                ft_p = torch.div(t9, t11)
                return (ft_p, t11, t9, t6)

            t0 = torch.rand(1, 6, 352, 352, device=device).transpose(0, 1)
            t1 = torch.rand(6, 3, 352, 352, device=device)
            t2 = torch.rand(6, device=device)[None, None, None, :].permute(3, 0, 1, 2)
            t3 = torch.rand(6, 1, 352, 352, device=device)
            t4 = torch.rand(6, 3, 352, 352, device=device)
            inputs = [t0, t1, t2, t3, t4]

            script = torch.jit.script(eager)
            for _ in range(4):
                for pair in zip(script(*inputs), eager(*inputs)):
                    test, ref = pair
                    torch.testing.assert_close(test, ref)
                    self.assertAllFused(script.graph_for(*inputs))

    def test_sub_gt_and(self):
        for device in self.devices:
            def eager(t1, t2, t3, t4, t: float):
                w = t1 - t2
                h = t3 - t4
                k = (w > t) & (h > t)
                assert k.dtype == torch.bool
                if t > 0.5:
                    # Putting a use of k in a never-executed conditional prevents
                    # profiling its type, which leaves it as "Tensor".  If we
                    # propagate Tensor back to the definition of k, we have to be
                    # careful not to create a fusion group containing it.
                    return k + 1
                return w
            t = torch.rand(8, dtype=torch.float, device=device)
            scripted = self.checkScript(eager, (t, t, t, t, 0.1))

    def test_chunk_mul_one(self):
        for device in self.devices:
            def eager(x):
                z, y, w = torch.chunk(x, 3, -1)
                return z * 3, y, w
            x = torch.rand(64, 1, 3072, dtype=torch.float, device=device)
            z, y, w = eager(x)
            script = self.checkScript(eager, (x,))

    def test_eq_unsqueeze_type_as(self):
        for device in self.devices:
            def eager(a, b):
                mask = b == 1
                mask = torch.unsqueeze(mask, -1)
                x = mask.type_as(a)
                return x, mask
            a = torch.rand(1, 64, 1024, device=device, dtype=torch.float)
            b = torch.randint(-2, 2, (1, 64), device=device, dtype=torch.long)
            script = self.checkScript(eager, (a, b))

    def test_neg_pow(self):
        def eager_tt(a: torch.Tensor, b: torch.Tensor):
            return torch.neg(torch.pow(a, b))

        def eager_ts(a: torch.Tensor, b: float):
            return torch.neg(torch.pow(a, b))

        def eager_st(a: float, b: torch.Tensor):
            return torch.neg(torch.pow(a, b))

        a = torch.rand(1, dtype=torch.float)
        b = torch.rand(1, dtype=torch.float)
        s = b.item()
        script = self.checkScript(eager_tt, (a, b))
        self.assertAllFused(script.graph_for(a, b))
        script = self.checkScript(eager_ts, (a, s))
        self.assertAllFused(script.graph_for(a, s))
        script = self.checkScript(eager_st, (s, b))
        self.assertAllFused(script.graph_for(s, b))

    @unittest.skipIf(not LLVM_ENABLED, "Too slow to run with the TE interpreter")
    def test_conv2d_depthwise(self):
        def eager(input, weight, bias):
            return torch.conv2d(input, weight, bias, stride=1, padding=1, groups=72)

        input = torch.rand((1, 72, 56, 56), dtype=torch.float)
        weight = torch.rand((72, 1, 3, 3), dtype=torch.float)
        bias = torch.rand((72), dtype=torch.float)

        script = self.checkScript(eager, (input, weight, bias))
        self.assertAllFused(script.graph_for(input, weight, bias))

    def test_conv2d(self):
        def eager(input, weight, bias):
            return torch.conv2d(input, weight, bias, stride=1, padding=1, groups=1)

        input = torch.rand((1, 64, 56, 56), dtype=torch.float)
        weight = torch.rand((64, 64, 3, 3), dtype=torch.float)
        bias = torch.rand((64), dtype=torch.float)

        script = self.checkScript(eager, (input, weight, bias))
        FileCheck().check_not("TensorExpr").run(torch.jit.last_executed_optimized_graph())

    def test_type_as_cat(self):
        with inline_fusion_groups():
            def eager(x, y):
                return torch.cat((x, y.type_as(x)), dim=1)
            dtypes = self.dtypes.copy()
            # CPU fuser doesn't support float16.
            dtypes.remove(torch.float16)
            dtypes.remove(torch.bfloat16)
            for dtype1, dtype2 in product(dtypes, dtypes):
                x = torch.randint(2, (1, 13,)).to(dtype1)
                zero = torch.tensor([[0]]).to(dtype2)
                one = torch.tensor([[1]]).to(dtype2)
                script = torch.jit.trace(eager, (x, zero))
                for _ in range(3):
                    torch.testing.assert_close(
                        script(x, zero),
                        eager(x, zero))
                    torch.testing.assert_close(
                        script(x, one),
                        eager(x, one))
                self.assertAllFused(script.graph_for(x, one))

    def test_to_device(self):
        def eager(x):
            return x.to(device="cpu").relu()
        x = torch.rand(8)
        script = self.checkScript(eager, (x,))
        self.assertAllFused(script.graph_for(x))

    def test_dims(self):
        def eager(x, y):
            return x / (y + 0.0001)
        x = torch.linspace(-1, 1, 768, dtype=torch.float32).as_strided((1, 1, 768), (768, 1, 1))
        y = torch.tensor([[[2.0]]], dtype=torch.float32)
        script = self.checkScript(eager, (x, y))
        self.assertAllFused(script.graph_for(x, y))

    def test_unsqueeze_var_dim(self):
        def eager(x, y, z: int):
            return x * torch.unsqueeze(y, dim=z)
        x = torch.rand(4, 4, 64).permute(1, 0, 2)
        y = torch.rand(4, 4)
        z = 2
        script = self.checkScript(eager, (x, y, z))

    def _test_fwd_bwd(self, fn):
        x = torch.arange(-10, 10, dtype=torch.float32, requires_grad=True)
        xs = torch.arange(-10, 10, dtype=torch.float32, requires_grad=True)
        script = torch.jit.script(fn)
        for i in range(11):
            y = fn(x)
            g0 = torch.rand_like(y)
            y.backward(g0)

            ys = script(xs)
            ys.backward(g0)

            with torch.no_grad():
                x -= 0.1 * x.grad
                xs -= 0.1 * xs.grad
                x.grad = None
                xs.grad = None
        torch.testing.assert_close(y, ys)

    def test_relu_fwd_bwd(self):
        def eager(x):
            return torch.relu(x * 1.01)
        self._test_fwd_bwd(eager)

    def test_hardswish_fwd_bwd(self):
        def eager(x):
            return F.hardswish(x) * 1.01
        self._test_fwd_bwd(eager)

    def test_hardsigmoid_fwd_bwd(self):
        def eager(x):
            return F.hardsigmoid(x) * 1.01
        self._test_fwd_bwd(eager)

    def test_dynamic_cat(self):
        with inline_fusion_groups():
            @torch.jit.script
            def repro(xs: List[torch.Tensor], ys: List[torch.Tensor], zs: List[torch.Tensor]):
                return [
                    torch.cat([x, torch.cat([y, z], dim=-1)], dim=-1)
                    for x, y, z in zip(xs, ys, zs)
                ]
            for _ in range(3):
                N = 3
                xs = [torch.ones(21) for _ in range(N)]
                # Note: concat of ys and zs will have the same size for each
                # pair, even though the individual ys and zs do not.
                ys = [torch.ones(N - i) for i in range(N)]
                zs = [torch.ones(i) for i in range(N)]
                repro(xs, ys, zs)

    def test_scalar_only_inputs(self):
        def eager(b: float):
            a = torch.ones(1)
            return a * b

        script = self.checkScript(eager, (1.0,))

    def test_cat_2k_args(self):
        with inline_fusion_groups():
            def eager(x):
                return torch.relu(torch.cat([x for _ in range(2000)]))
            x = torch.randn(1)
            trace = self.checkTrace(eager, (x,))
            fusion_groups = self.findFusionGroups(trace.graph_for(x))
            self.assertEqual(len(fusion_groups), 0)

    def test_adaptive_avg_pool2d(self):
        # TODO: once the adaptive_avg_pool2d is available in OpInfo DB, this
        # test should be moved there
        with inline_fusion_groups():
            def foo1(x):
                return torch.nn.functional.adaptive_avg_pool2d(x, (2, 2))

            def foo2(x):
                return torch.nn.functional.adaptive_avg_pool2d(x, (2))

            x = torch.randn(4, 4, 4)
            for foo in [foo1, foo2]:
                f = torch.jit.trace(foo, (x,))
                kernel = torch._C._te.TensorExprKernel(f.graph)
                correct_val = f(x)
                self.assertEqual(kernel.run((x,)), correct_val)

    def test_unrolled_cat(self):
        with inline_fusion_groups():
            def eager(x):
                ret = torch.empty(0)
                for i in range(x.shape[0]):
                    ret = torch.cat([ret, x[i].relu()])
                return ret
            script = torch.jit.script(eager)

            # Warm up with size=1 tensor; since the loop iterates once the
            # profile data will be "burned in" assuming size=1, and then
            # unrolled.
            x = torch.ones(1, 1)
            for _ in range(3):
                script(x)

            torch.testing.assert_close(eager(x), script(x))

            # Now when an input hits the unrolled path, it will produce an
            # incorrectly-sized tensor, since size=1 has been burned in.
            x = torch.ones((8, 1))
            torch.testing.assert_close(eager(x), script(x))

    def test_batch_norm(self):
        def test(fn, args):
            trace = torch.jit.trace(fn, args)
            self.assertAllFused(trace.graph_for(*args))
            torch.testing.assert_allclose(fn(*args), trace(*args))

        def bn(i, x):
            return torch.batch_norm(i, x, x, x, x, False, 0.1, 1e-4, False).relu()

        def bn_no_weight(i, x):
            return torch.batch_norm(i, None, x, x, x, False, 0.1, 1e-4, False).relu()

        def bn_no_bias(i, x):
            return torch.batch_norm(i, x, None, x, x, False, 0.1, 1e-4, False).relu()

        def bn_neither(i, x):
            return torch.batch_norm(i, None, None, x, x, False, 0.1, 1e-4, False).relu()

        for device in self.devices:
            i = torch.randn(4, 16, 32, 40, device=device)
            x = torch.randn(16, device=device)
            for fn in [bn, bn_no_weight, bn_no_bias, bn_neither]:
                test(fn, (i, x))

    def test_profiler(self):
        @torch.jit.script
        def test(x, y, z):
            return x * y + z

        args = [torch.randn(4) for _ in range(3)]
        with torch.autograd.profiler.profile() as prof:
            for _ in range(3):
                test(*args)
        self.assertIn("fused_mul_add", prof.table())


works_list = [
    '__radd__',
    '__rdiv__',
    '__rmul__',
    '__rmod__',
    'abs',
    'acos',
    'add',
    'addcmul',
    'addmm.decomposed',
    'asin',
    'atan',
    'atan2',
    'ceil',
    'clamp',
    'clamp.scalar',
    'contiguous',
    'cos',
    'cosh',
    'div.no_rounding_mode',
    'div.true_rounding',
    'div.floor_rounding',
    'div.trunc_rounding',
    'eq',
    'erf',
    'erfc',
    'exp',
    'expand',
    'expand_as',
    'expm1',
    'floor',
    'fmod',
    'fmod.autodiffed',
    'ge',
    'gt',
    'isnan',
    'le',
    'lerp',
    'lgamma',
    'log',
    'log10',
    'log1p',
    'log2',
    'lt',
    'masked_fill',
    'max.binary',
    'mean',
    'min.binary',
    'mm',
    'mul',
    'ne',
    'neg',
    'nn.functional.gelu',
    'nn.functional.hardshrink',
    'nn.functional.hardsigmoid',
    'nn.functional.hardswish',
    'nn.functional.softplus',
    'nn.functional.hardtanh',
    'nn.functional.leaky_relu',
    'nn.functional.relu',
    'nn.functional.relu6',
    'nn.functional.softsign',
    'nn.functional.tanhshrink',
    'nn.functional.threshold',
    'permute',
    'pow',
    'reciprocal',
    'remainder',
    'remainder.autodiffed',
    'reshape',
    'reshape_as',
    'round',
    'rsub',
    'rsub.rsub_tensor',
    'rsqrt',
    'sigmoid',
    'sign',
    'sin',
    'sinh',
    'sqrt',
    'sub',
    'sum',
    't',
    'tan',
    'tanh',
    'transpose',
    'true_divide',
    'trunc',
    'unsqueeze',
    'view',
    'view_as',
    'where',
    'bool',
    'byte',
    'char',
    'double',
    'float',
    'half',
    'int',
    'long',
    'short',
    'bool.channels_last',
    'byte.channels_last',
    'char.channels_last',
    'double.channels_last',
    'float.channels_last',
    'half.channels_last',
    'int.channels_last',
    'long.channels_last',
    'short.channels_last',
]

known_failures = [
    '__rmatmul__'
    'frac',
    'matmul',
]

# If your OpInfo test causes this test to fail, add it here
skip_ops = [
    'conj'
]

def get_name(op):
    l = [op.name]
    if op.variant_test_name != '':
        l.append(op.variant_test_name)
    return '.'.join(l)

class TestNNCOpInfo(TestCase):
    def te_compile(self, device, dtype, op):
        if op.name in skip_ops:
            return
        sample_inputs_itr = op.sample_inputs(device, dtype, requires_grad=False)
        for sample_input in sample_inputs_itr:
            arg_values = [sample_input.input] + list(sample_input.args)
            kwarg_values = sample_input.kwargs
            param_names = []
            param_values = []
            fx_args = []
            for idx, v in enumerate(arg_values):
                if isinstance(v, torch.Tensor):
                    param_names.append(f"arg_{idx}")
                    param_values.append(v)
                    fx_args.append(param_names[-1])
                else:
                    fx_args.append(f'{repr(v)}')

            for k, v in kwarg_values.items():
                if isinstance(v, torch.Tensor):
                    param_names.append(k)
                    param_values.append(v)
                    fx_args.append(f'{k} = {k}')
                else:
                    fx_args.append(f'{k} = {repr(v)}')

            code = f"""
def f({', '.join(param_names)}):
    return op.op({', '.join(fx_args)})"""
            g = {'torch': torch, 'inf' : math.inf, 'op': op}
            exec(code, g)
            f = g['f']
            f.__module__ = 'test'
            out = f(*param_values)

            ts_g = torch.jit.trace(f, param_values)
            kernel = torch._C._te.TensorExprKernel(ts_g.graph)
            correct_val = f(*param_values)
            self.assertEqual(kernel.run(tuple(param_values)), correct_val)
            self.assertEqual(kernel.fallback(tuple(param_values)), correct_val)

    @onlyCPU
    @unittest.skipIf(not LLVM_ENABLED, "Compiles with TensorExprKernel")
    @ops([op for op in op_db if get_name(op) in works_list], allowed_dtypes=(torch.float,))
    def test_working(self, device, dtype, op):
        self.te_compile(device, dtype, op)

    @onlyCPU
    @unittest.skipIf(not LLVM_ENABLED, "Compiles with TensorExprKernel")
    @ops([op for op in op_db if get_name(op) in known_failures], allowed_dtypes=(torch.float,))
    def test_failures(self, device, dtype, op):
        try:
            self.te_compile(device, dtype, op)
        except Exception as e:
            pass
        else:
            raise RuntimeError("Expected test to fail. If it now works, move op into works_list")

    @onlyCPU
    @unittest.skipIf(not LLVM_ENABLED, "Compiles with TensorExprKernel")
    @ops([op for op in op_db if get_name(op) not in works_list + known_failures], allowed_dtypes=(torch.float,))
    def test_unsupported(self, device, dtype, op):
        if get_name(op) in skip_ops:
            return
        try:
            with warnings.catch_warnings():
                warnings.simplefilter('ignore', TracerWarning)
                self.te_compile(device, dtype, op)
        except Exception as e:
            pass
        else:
            raise RuntimeError("Expected test to fail. If it now works, move op into works_list")


only_for = ("cpu", "cuda")
instantiate_device_type_tests(TestNNCOpInfo, globals(), only_for=only_for)

if __name__ == '__main__':
    run_tests()
