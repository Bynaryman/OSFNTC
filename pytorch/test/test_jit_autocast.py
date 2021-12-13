# Owner(s): ["oncall: jit"]

import torch
from torch.cuda.amp import autocast
from typing import Optional

import unittest
from test_jit import JitTestCase
from torch.testing._internal.common_cuda import TEST_CUDA
from torch.testing._internal.common_utils import run_tests
from torch.testing import FileCheck

TEST_BFLOAT16 = TEST_CUDA and torch.cuda.is_bf16_supported()

class TestAutocast(JitTestCase):
    def setUp(self):
        # common input tensors
        self.a_fp16 = torch.rand((2, 2), dtype=torch.float16, device='cuda')
        self.b_fp16 = torch.rand((2, 2), dtype=torch.float16, device='cuda')
        self.c_fp16 = torch.rand((2, 2), dtype=torch.float16, device='cuda')
        self.d_fp16 = torch.rand((2, 2), dtype=torch.float16, device='cuda')
        self.a_fp32 = torch.rand((2, 2), dtype=torch.float32, device='cuda')
        self.b_fp32 = torch.rand((2, 2), dtype=torch.float32, device='cuda')
        self.c_fp32 = torch.rand((2, 2), dtype=torch.float32, device='cuda')
        self.d_fp32 = torch.rand((2, 2), dtype=torch.float32, device='cuda')
        self.old_value = torch._C._jit_set_autocast_mode(True)
        super().setUp()

    def tearDown(self):
        torch._C._jit_set_autocast_mode(self.old_value)
        super().tearDown()

    @unittest.skipIf(not TEST_CUDA, "No cuda")
    def test_minimal(self):
        @torch.jit.script
        def fn(a, b):
            with autocast():
                x = torch.mm(a, b)
                y = torch.sum(x)
                return x, y
        x, y = fn(self.a_fp32, self.b_fp32)
        self.assertEqual(x.dtype, torch.float16)
        self.assertEqual(y.dtype, torch.float32)

    @unittest.skipIf(not TEST_CUDA or not TEST_BFLOAT16, "No cuda bfloat16 support")
    def test_linear_bf16(self):
        @torch.jit.script
        def fn(a, b):
            with autocast(dtype=torch.bfloat16):
                x = torch.mm(a, b)
                y = torch.sum(x)
                return x, y
        x, y = fn(self.a_fp32, self.b_fp32)
        self.assertEqual(x.dtype, torch.bfloat16)
        self.assertEqual(y.dtype, torch.float32)

    @unittest.skipIf(not TEST_CUDA, "No cuda")
    def test_minimal_cpu(self):
        @torch.jit.script
        def fn(a, b):
            with autocast():
                return torch.mm(a, b)
        result = fn(self.a_fp32.to('cpu'), self.b_fp32.to('cpu'))
        self.assertEqual(result.dtype, torch.float32)

    @unittest.skipIf(not TEST_CUDA, "No cuda")
    def test_minimal_off(self):
        @torch.jit.script
        def fn(a, b):
            with autocast(enabled=False):
                return torch.mm(a, b)
        result = fn(self.a_fp32, self.b_fp32)
        self.assertEqual(result.dtype, torch.float32)

    @unittest.skipIf(not TEST_CUDA, "No cuda")
    def test_runtime_autocast_state(self):
        @torch.jit.script
        def fn(a, b, use_amp: bool):
            with autocast(enabled=use_amp):
                return torch.mm(a, b)
        # runtime values for autocast enable argument are not supported
        with self.assertRaises(RuntimeError):
            fn(self.a_fp32, self.b_fp32, True)

    @unittest.skipIf(not TEST_CUDA, "No cuda")
    def test_runtime_autocast_state_expr(self):
        @torch.jit.script
        def fn(a, b):
            with autocast(enabled=True if a[0][0] > 0.5 else False):
                return torch.mm(a, b)
        # runtime values for autocast enable argument are not supported
        with self.assertRaises(RuntimeError):
            fn(self.a_fp32, self.b_fp32)

    @unittest.skipIf(not TEST_CUDA, "No cuda")
    def test_explicit_casts(self):
        @torch.jit.script
        def fn(a, b, c, d):
            with autocast():
                e = torch.mm(a.double(), b.double()).float()
                f = torch.mm(c, d).double()
            g = torch.mm(c.double(), f)
            return e, f, g
        e, f, g = fn(self.a_fp32, self.b_fp32, self.c_fp32, self.d_fp32)
        self.assertEqual(e.dtype, torch.float32)
        self.assertEqual(f.dtype, torch.float64)
        self.assertEqual(g.dtype, torch.float64)

    # multiple uses of the same input value
    @unittest.skipIf(not TEST_CUDA, "No cuda")
    def test_duplicate_inputs(self):
        @torch.jit.script
        def fn(a, b):
            with autocast():
                e = torch.mm(a, a)
                f = torch.mm(e, e)
            return e, f
        e, f = fn(self.a_fp32, self.b_fp32)
        self.assertEqual(e.dtype, torch.float16)
        self.assertEqual(f.dtype, torch.float16)

    @unittest.skipIf(not TEST_CUDA, "No cuda")
    def test_fp32_policy(self):
        @torch.jit.script
        def fn(a):
            with autocast(enabled=True):
                return torch.log(a)
        result = fn(self.a_fp16)
        self.assertEqual(result.dtype, torch.float32)

    @unittest.skipIf(not TEST_CUDA, "No cuda")
    def test_fp32_policy_with_fp64(self):
        @torch.jit.script
        def fn(a):
            with autocast(enabled=True):
                return torch.log(a)
        # fp32 policy should not narrow fp64 to fp32!
        result = fn(self.a_fp32.double())
        self.assertEqual(result.dtype, torch.float64)

    @unittest.skipIf(not TEST_CUDA, "No cuda")
    def test_promote_policy(self):
        @torch.jit.script
        def fn(a, b, c, d):
            with autocast():
                e = torch.mm(a, b)
                f = torch.addcmul(e, c, d, value=0.1)
            return e, f
        e, f = fn(self.a_fp32, self.b_fp32, self.c_fp32, self.d_fp32)
        self.assertEqual(e.dtype, torch.float16)
        self.assertEqual(f.dtype, torch.float32)

    @unittest.skipIf(not TEST_CUDA, "No cuda")
    def test_promote_policy_fp64(self):
        @torch.jit.script
        def fn(a, b):
            with autocast(enabled=True):
                return torch.addcmul(a, a, b, value=0.1)
        result = fn(self.a_fp32.double(), self.b_fp32.double())
        self.assertEqual(result.dtype, torch.float64)

    @unittest.skipIf(not TEST_CUDA, "No cuda")
    def test_fp32_set_opt_dtype_policy(self):
        @torch.jit.script
        def fn(a, b, c, d, dtype: Optional[int]):
            with autocast(enabled=True):
                x = torch.softmax(a, 0)
                y = torch.softmax(b, 0, None)
                z = torch.softmax(c, 0, torch.float64)
                w = torch.softmax(d, 0, dtype)
            return x, y, z, w
        x, y, z, w = fn(self.a_fp16, self.b_fp16, self.c_fp16, self.d_fp16, None)
        self.assertEqual(x.dtype, torch.float32)
        self.assertEqual(y.dtype, torch.float32)
        self.assertEqual(z.dtype, torch.float64)
        self.assertEqual(w.dtype, torch.float16)

    @unittest.skipIf(not TEST_CUDA, "No cuda")
    def test_fp32_set_opt_dtype_policy_fp64(self):
        @torch.jit.script
        def fn(a, b, c, d, dtype: Optional[int]):
            with autocast(enabled=True):
                x = torch.softmax(a, 0)
                y = torch.softmax(b, 0, None)
                z = torch.softmax(c, 0, torch.float64)
                w = torch.softmax(d, 0, dtype)
            return x, y, z, w
        x, y, z, w = fn(self.a_fp32.double(), self.b_fp32.double(), self.c_fp32.double(), self.d_fp32.double(), None)
        self.assertEqual(x.dtype, torch.float64)
        self.assertEqual(y.dtype, torch.float64)
        self.assertEqual(z.dtype, torch.float64)
        self.assertEqual(w.dtype, torch.float64)

    @unittest.skipIf(True, "broken due to lack of type propagation")
    @unittest.skipIf(not TEST_CUDA, "No cuda")
    def test_control_flow(self):
        @torch.jit.script
        def fn(a, b, c, d):
            with autocast():
                if a[0][0] > 0.5:
                    e = torch.mm(a, b)
                    x = 1
                else:
                    e = torch.mm(c, d)
                    x = 2
                f = torch.mm(d, e) * x
            return e, f
        e, f = fn(self.a_fp32, self.b_fp32, self.c_fp32, self.d_fp32)
        self.assertEqual(e.dtype, torch.float16)
        self.assertEqual(f.dtype, torch.float16)

    # this works find in regular Python, but it creates a delicate
    # situation in TorchScript where the types are not consistent across
    # the then/else branches
    @unittest.skipIf(not TEST_CUDA, "No cuda")
    def test_divergent_types(self):
        @torch.jit.script
        def fn(a, b, c, d):
            with autocast():
                if a[0][0] > 0.5:
                    e = torch.mm(a, b)
                    f = torch.mm(a, b).float()
                else:
                    e = torch.mm(c, d).float()
                    f = torch.mm(a, b)
            return torch.mm(e.float(), f.float())
        result = fn(self.a_fp32, self.b_fp32, self.c_fp32, self.d_fp32)
        self.assertEqual(result.dtype, torch.float32)

    # another, more complex case of divergent types
    @unittest.skipIf(not TEST_CUDA, "No cuda")
    def test_divergent_autocast(self):
        @torch.jit.script
        def fn(a, b, c, d):
            autocast_on = autocast(enabled=True)
            autocast_off = autocast(enabled=False)
            if a[0][0] > 0.5:
                with autocast_on:
                    e = torch.mm(a, b)
            else:
                with autocast_off:
                    e = torch.mm(c, d)
            return torch.mm(e, e)
        fn(self.a_fp32, self.b_fp32, self.c_fp32, self.d_fp32)

    @unittest.skipIf(not TEST_CUDA, "No cuda")
    def test_conditional_autocast(self):
        @torch.jit.script
        def fn(a, b):
            autocast_on = autocast(enabled=True)
            autocast_off = autocast(enabled=False)
            with autocast_on if a[0][0] > 0.5 else autocast_off:
                return torch.mm(a, b)
        # conditional autocast expressions are not supported
        with self.assertRaises(RuntimeError):
            fn(self.a_fp32, self.b_fp32)

    @unittest.skipIf(not TEST_CUDA, "No cuda")
    def test_nested_autocast(self):
        @torch.jit.script
        def fn(a, b, c, d):
            with autocast(enabled=False):
                e = torch.mm(a, b)
                with autocast(enabled=True):
                    f = torch.mm(e, c)
                    with autocast(enabled=False):
                        g = torch.mm(e, d)
            return e, f, g
        e, f, g = fn(self.a_fp32, self.b_fp32, self.c_fp32, self.d_fp32)
        self.assertEqual(e.dtype, torch.float32)
        self.assertEqual(f.dtype, torch.float16)
        self.assertEqual(g.dtype, torch.float32)

    @unittest.skipIf(not TEST_CUDA, "No cuda")
    def test_implicitly_nested_autocast(self):
        @torch.jit.script
        def fn(a, b):
            with autocast(enabled=False), autocast(enabled=True):
                return torch.mm(a, b)
        result = fn(self.a_fp32, self.b_fp32)
        self.assertEqual(result.dtype, torch.float16)

    @unittest.skipIf(not TEST_CUDA, "No cuda")
    def test_reused_autocast(self):
        @torch.jit.script
        def fn(a, b, c, d):
            autocast_instance = autocast(enabled=True)
            with autocast_instance:
                e = torch.mm(a, b)
                with autocast_instance:
                    e = torch.mm(c, d)
                    f = torch.mm(d, e)
            g = torch.mm(e, f)
            return e, f, g
        e, f, g = fn(self.a_fp32, self.b_fp32, self.c_fp32, self.d_fp32)
        self.assertEqual(e.dtype, torch.float16)
        self.assertEqual(f.dtype, torch.float16)
        self.assertEqual(g.dtype, torch.float16)

    # TODO: fix and enable this test?
    #   (we could technically fix this, but is it really worth it?)
    @unittest.skipIf(True, "unsuported autocast syntax")
    def test_reused_autocast_expr(self):
        @torch.jit.script
        def fn(a, b, c, d):
            with autocast(enabled=True) as autocast_instance:
                e = torch.mm(a, b)
                with autocast_instance:
                    e = torch.mm(c, d)
                    f = torch.mm(d, e)
            g = torch.mm(e, f)
            return e, f, g
        e, f, g = fn(self.a_fp32, self.b_fp32, self.c_fp32, self.d_fp32)
        self.assertEqual(e.dtype, torch.float16)
        self.assertEqual(f.dtype, torch.float16)
        self.assertEqual(g.dtype, torch.float16)

    @unittest.skipIf(not TEST_CUDA, "No cuda")
    def test_callees(self):
        def helper(a, b):
            return torch.mm(a, b)

        @torch.jit.script
        def fn(a, b):
            with autocast(enabled=True):
                tmp = helper(a, b)
                tmp = helper(tmp, tmp)
                tmp = helper(tmp, tmp)
                tmp = helper(tmp, tmp)
                return helper(tmp, b)

        result = fn(self.a_fp32, self.b_fp32)
        self.assertEqual(result.dtype, torch.float16)

    @unittest.skipIf(not TEST_CUDA, "No cuda")
    def test_callees_with_autocast_on(self):
        def helper(a, b):
            with autocast(enabled=True):
                return torch.mm(a, b)

        @torch.jit.script
        def fn(a, b):
            with autocast(enabled=False):
                return helper(a, b)

        result = fn(self.a_fp32, self.b_fp32)
        self.assertEqual(result.dtype, torch.float16)

    @unittest.skipIf(not TEST_CUDA, "No cuda")
    def test_callees_with_autocast_off(self):
        def helper(a, b):
            with autocast(enabled=False):
                return torch.mm(a, b)

        @torch.jit.script
        def fn(a, b):
            with autocast(enabled=True):
                return helper(a, b)

        result = fn(self.a_fp32, self.b_fp32)
        self.assertEqual(result.dtype, torch.float32)

    # scripting inside eager autocast
    @unittest.skipIf(not TEST_CUDA, "No cuda")
    def test_eager_and_script(self):
        @torch.jit.script
        def fn(a, b):
            return torch.mm(a, b)
        for i in range(8):
            use_autocast = (i % 2 == 0)
            expected_dtype = torch.float16 if use_autocast else torch.float32
            with autocast(enabled=use_autocast):
                result = fn(self.a_fp32, self.b_fp32)
            self.assertEqual(result.dtype, expected_dtype)

    # traced inside scripting
    @unittest.skipIf(not TEST_CUDA, "No cuda")
    def test_script_and_tracing(self):
        def helper(a, b):
            return torch.mm(a, b)

        traced = torch.jit.trace(helper, (self.a_fp32, self.a_fp32))

        @torch.jit.script
        def fn(a, b):
            with autocast(enabled=True):
                return traced(a, b)

        result = fn(self.a_fp32, self.b_fp32)
        self.assertEqual(result.dtype, torch.float16)

    # traced with autocast inside scripting
    @unittest.skipIf(True, "autocast(False) is ignored inside traced functions")
    @unittest.skipIf(not TEST_CUDA, "No cuda")
    def test_script_and_tracing_with_autocast(self):
        def helper(a, b):
            with autocast(enabled=False):
                return torch.mm(a, b) * 2.0

        traced = torch.jit.trace(helper, (self.a_fp32, self.a_fp32))

        @torch.jit.script
        def fn(a, b):
            with autocast(enabled=True):
                return traced(a, b)

        result = fn(self.a_fp32, self.b_fp32)
        self.assertEqual(result.dtype, torch.float32)

    # scripted called from traced
    @unittest.skipIf(not TEST_CUDA, "No cuda")
    def test_tracing_and_script(self):
        @torch.jit.script
        def fn(a, b):
            with autocast():
                return torch.mm(a, b)

        def traced(a, b):
            return fn(a, b)

        traced = torch.jit.trace(traced, (self.a_fp32, self.b_fp32))
        result = traced(self.a_fp32, self.b_fp32)
        self.assertEqual(result.dtype, torch.float16)

    # scripted called from traced with autocast
    @unittest.skipIf(True, "scripted called from traced TorchScript is not yet working")
    @unittest.skipIf(not TEST_CUDA, "No cuda")
    def test_tracing_with_autocast_and_script(self):
        @torch.jit.script
        def fn(a, b):
            return torch.mm(a, b)

        def traced(a, b):
            with autocast(enabled=True):
                return fn(a, b)

        traced = torch.jit.trace(traced, (self.a_fp32, self.b_fp32))
        result = traced(self.a_fp32, self.b_fp32)
        self.assertEqual(result.dtype, torch.float16)

    @unittest.skipIf(not TEST_CUDA, "No cuda")
    def test_script_module(self):
        class TestModule(torch.nn.Module):
            def __init__(self, N, M):
                super().__init__()
                self.weight = torch.nn.Parameter(torch.rand((N, M), dtype=torch.float32))
                self.linear = torch.nn.Linear(N, M).float()

            def forward(self, input):
                with autocast(enabled=True):
                    output = self.weight.mv(input)
                    output = self.linear(output)
                    return output

        scripted_module = torch.jit.script(TestModule(2, 3)).cuda()
        input = torch.rand(3, dtype=torch.float32, device='cuda')
        result = scripted_module(input)
        self.assertEqual(result.dtype, torch.float16)

    @unittest.skipIf(True, "autocast decorators not supported")
    @unittest.skipIf(not TEST_CUDA, "No cuda")
    def test_autocast_decorator(self):
        @torch.jit.script
        @autocast(enabled=True)
        def fn(a, b):
            return torch.mm(a, b)
        result = fn(self.a_fp32, self.b_fp32)
        self.assertEqual(result.dtype, torch.float16)

    # this is equivalent to running scripted functions inside autocast)
    # (see also test_eager_and_script)
    @unittest.skipIf(not TEST_CUDA, "No cuda")
    def test_autocast_decorator_outside_jit(self):
        @autocast(enabled=True)
        @torch.jit.script
        def fn(a, b):
            return torch.mm(a, b)
        result = fn(self.a_fp32, self.b_fp32)
        self.assertEqual(result.dtype, torch.float16)

    @unittest.skipIf(not TEST_CUDA, "No cuda")
    def test_inplace(self):
        @torch.jit.script
        def fn(a, b, c):
            with autocast(enabled=True):
                x = torch.addmm(a, b, c)
                y = torch.addmm(a, b, c, out=a)
                z = a.addmm_(b, c)
                return x, y, z
        x, y, z = fn(self.a_fp32, self.b_fp32, self.c_fp32)
        self.assertEqual(x.dtype, torch.float16)
        self.assertEqual(y.dtype, torch.float32)
        self.assertEqual(z.dtype, torch.float32)

    def _test_autocast(self, func, cast_op, *args):
        jit_func = torch.jit.script(func)
        o = func(*args)
        jit_o = jit_func(*args)
        if cast_op is not None:
            FileCheck().check(cast_op).run(jit_func.graph_for(*args))
        for o0, o1 in zip(o, jit_o):
            self.assertEqual(o0.dtype, o1.dtype)

    @unittest.skipIf(not TEST_CUDA, "No cuda")
    def test_autocast_api(self):

        def t_autocast_cpu(x, y):
            with torch.autocast("cpu", dtype=torch.bfloat16):
                return torch.mm(x, y)

        def t_autocast_cuda(x, y):
            with torch.autocast("cuda", dtype=torch.half):
                return torch.mm(x, y)

        def t_cuda_amp_autocast(x, y):
            with torch.cuda.amp.autocast():
                return torch.mm(x, y)

        def t_cpu_amp_autocast(x, y):
            with torch.cpu.amp.autocast():
                return torch.mm(x, y)

        x = torch.randn(5, 5, device="cuda", dtype=torch.float32)
        y = torch.randn(5, 5, device="cuda", dtype=torch.float32)
        self._test_autocast(t_autocast_cpu, "aten::_autocast_to_reduced_precision", x, y)
        self._test_autocast(t_autocast_cuda, "aten::_autocast_to_reduced_precision", x, y)
        self._test_autocast(t_cuda_amp_autocast, "aten::_autocast_to_reduced_precision", x, y)
        self._test_autocast(t_cpu_amp_autocast, "aten::_autocast_to_reduced_precision", x, y)

    @unittest.skipIf(True, "we need to provide dtype argument at this moment")
    @unittest.skipIf(not TEST_CUDA, "No cuda")
    def test_autocast_api_not_supported(self):

        def t_autocast_cpu(x, y):
            # no dtype provided is not currently supported
            with torch.autocast("cpu"):
                return torch.mm(x, y)

        def t_autocast_cuda(x, y):
            # no dtype provided is not currently supported
            with torch.autocast("cuda"):
                return torch.mm(x, y)

        x = torch.randn(5, 5, device="cuda", dtype=torch.float32)
        y = torch.randn(5, 5, device="cuda", dtype=torch.float32)
        self._test_autocast(t_autocast_cpu, "aten::_autocast_to_reduced_precision", x, y)
        self._test_autocast(t_autocast_cuda, "aten::_autocast_to_reduced_precision", x, y)

    @unittest.skipIf(not TEST_CUDA, "No cuda")
    def test_autocast_mixed_dtypes(self):

        def t(cpu0, cpu1, cuda0, cuda1):
            with torch.autocast("cpu", torch.bfloat16):
                with torch.autocast("cuda", torch.float16):
                    cpu_o = torch.mm(cpu0, cpu1)
                    cuda_o = torch.mm(cuda0, cuda1)
                    return cpu_o, cuda_o

        jit_t = torch.jit.script(t)
        cpu0 = torch.randn(5, 5, device="cpu", dtype=torch.float32)
        cpu1 = torch.randn(5, 5, device="cpu", dtype=torch.float32)
        cuda0 = torch.randn(5, 5, device="cuda", dtype=torch.float32)
        cuda1 = torch.randn(5, 5, device="cuda", dtype=torch.float32)
        self._test_autocast(t, "aten::_autocast_to_reduced_precision", cpu0, cpu1, cuda0, cuda1)

    @unittest.skipIf(not TEST_CUDA, "No cuda")
    def test_jit_executor_under_autocast(self):

        def t(cpu0, cpu1, cuda0, cuda1):
            cpu_o = torch.mm(cpu0, cpu1)
            cuda_o = torch.mm(cuda0, cuda1)
            return cpu_o, cuda_o

        jit_t = torch.jit.script(t)
        cpu0 = torch.randn(5, 5, device="cpu", dtype=torch.float32)
        cpu1 = torch.randn(5, 5, device="cpu", dtype=torch.float32)
        cuda0 = torch.randn(5, 5, device="cuda", dtype=torch.float32)
        cuda1 = torch.randn(5, 5, device="cuda", dtype=torch.float32)

        with torch.autocast("cpu", torch.bfloat16):
            with torch.autocast("cuda", torch.float16):
                self._test_autocast(t, "aten::_autocast_to_reduced_precision", cpu0, cpu1, cuda0, cuda1)

        with torch.autocast("cpu", torch.bfloat16):
            self._test_autocast(t, "aten::_autocast_to_reduced_precision", cpu0, cpu1, cuda0, cuda1)

        with torch.autocast("cuda", torch.float16):
            self._test_autocast(t, "aten::_autocast_to_reduced_precision", cpu0, cpu1, cuda0, cuda1)

        # no cast op should be observed when executing outside autocast context
        self._test_autocast(t, None, cpu0, cpu1, cuda0, cuda1)

    @unittest.skipIf(not TEST_CUDA, "No cuda")
    def test_autocast_autodiff(self):
        def t(t0, t1):
            o = torch.mm(t0, t1)
            return o.relu()

        jit_t = torch.jit.script(t)
        t0 = torch.randn(5, 5, device="cuda", dtype=torch.float32).requires_grad_()
        t1 = torch.randn(5, 5, device="cuda", dtype=torch.float32).requires_grad_()

        # run optimization
        for i in range(5):
            with torch.autocast("cuda", torch.float16):
                jit_o = jit_t(t0, t1)
            jit_o.sum().backward()

        t0.grad = None
        t1.grad = None
        ref_t0 = t0.detach().requires_grad_()
        ref_t1 = t1.detach().requires_grad_()

        with torch.autocast("cuda", torch.float16):
            o = t(ref_t0, ref_t1)
            jit_o = jit_t(t0, t1)
        jit_o.sum().backward()
        o.sum().backward()
        self.assertEqual(o, jit_o)
        self.assertEqual(t0.grad, ref_t0.grad)
        self.assertEqual(t1.grad, ref_t1.grad)
        self.assertEqual(o.dtype, jit_o.dtype)
        self.assertEqual(t0.grad.dtype, ref_t0.grad.dtype)
        self.assertEqual(t1.grad.dtype, ref_t1.grad.dtype)

    @unittest.skipIf(not TEST_CUDA, "No cuda")
    def test_jit_call_method_under_autocast(self):
        @torch.jit.interface
        class Iface(torch.nn.Module):
            def forward(self, x, y) -> torch.Tensor:
                pass

        class Impl(Iface):
            def forward(self, x, y):
                return torch.mm(x, y)

        class Thing1(torch.nn.Module):
            impl: Iface

            def forward(self, x, y):
                with torch.cuda.amp.autocast():
                    a = torch.mm(x, y)
                    b = self.impl.forward(a, x)
                    return b

        scripted_impl = torch.jit.script(Impl())
        thing1 = Thing1()
        thing1.impl = scripted_impl
        scripted_thing1 = torch.jit.script(thing1)
        x = torch.rand([2, 2])
        y = torch.rand([2, 2])

        # make sure this doesn't throw an error
        with torch.cuda.amp.autocast():
            ans = scripted_thing1.forward(x, y)
        self.assertEqual(torch.mm(torch.mm(x, y), x), ans)

        # sanity check: this isn't supported currently when global autocasting
        # isn't enabled
        self.assertRaises(RuntimeError, lambda: scripted_thing1.forward(x, y))


if __name__ == "__main__":
    run_tests()
