# -*- coding: utf-8 -*-
# Owner(s): ["module: tests"]

import torch
import torch.utils.data
import numpy as np

import contextlib
import gc
import io
import inspect
import itertools
import math
import random
import re
import copy
import os
import tempfile
import unittest
import warnings
import types
import pickle
import textwrap
import subprocess
import weakref
import sys
from torch.utils.dlpack import from_dlpack, to_dlpack
from torch._six import inf, nan, string_classes
from itertools import product, combinations, permutations
from functools import partial
from torch import multiprocessing as mp
from torch.testing import make_tensor
from torch.testing._internal.common_utils import (
    TestCase, TEST_WITH_ROCM, run_tests,
    IS_WINDOWS, IS_FILESYSTEM_UTF8_ENCODING, NO_MULTIPROCESSING_SPAWN,
    do_test_dtypes, IS_SANDCASTLE, IS_FBCODE, IS_REMOTE_GPU, load_tests, slowTest,
    skipCUDAMemoryLeakCheckIf, BytesIOContext, noarchTest,
    skipIfRocm, skipIfNoSciPy, TemporaryFileName, TemporaryDirectoryName,
    wrapDeterministicFlagAPITest, DeterministicGuard, CudaSyncGuard,
    skipIfNotRegistered, bytes_to_scalar)
from multiprocessing.reduction import ForkingPickler
from torch.testing._internal.common_device_type import (
    expectedFailureMeta,
    expectedFailureXLA,
    instantiate_device_type_tests,
    skipCUDAVersionIn,
    onlyCUDA, onlyCPU,
    dtypes, dtypesIfCUDA, dtypesIfCPU, deviceCountAtLeast,
    skipMeta,
    PYTORCH_CUDA_MEMCHECK, largeTensorTest, onlyNativeDeviceTypes,
    expectedAlertNondeterministic, get_all_device_types, skipXLA)
from typing import Dict, List, Tuple
import torch.backends.quantized
import torch.testing._internal.data
from torch.testing._internal.common_cuda import tf32_on_and_off, tf32_is_not_fp32
from torch.testing._internal.common_dtype import (
    get_all_fp_dtypes, get_all_int_dtypes, get_all_math_dtypes, get_all_dtypes, get_all_complex_dtypes
)

# Protects against includes accidentally setting the default dtype
assert torch.get_default_dtype() is torch.float32

# load_tests from torch.testing._internal.common_utils is used to automatically filter tests for
# sharding on sandcastle. This line silences flake warnings
load_tests = load_tests

AMPERE_OR_ROCM = TEST_WITH_ROCM or tf32_is_not_fp32()

# Wrap base test class into a class to hide it from testing
# See https://stackoverflow.com/a/25695512
class AbstractTestCases:
    # This is intentionally prefixed by an underscore. Otherwise pytest will try to
    # run its methods as test cases.
    class _TestTorchMixin(TestCase):
        def _make_tensors(self, shape, val_range=(-100, 100), use_floating=True, use_integral=True,
                          use_complex=False) -> Dict[str, List[torch.Tensor]]:
            float_types = [torch.double,
                           torch.float]
            int_types = [torch.int64,
                         torch.int32,
                         torch.int16]

            complex_types = [torch.complex64,
                             torch.complex128]

            def make_contiguous(shape, dtype) -> torch.Tensor:
                if dtype in float_types:
                    val = torch.randn(shape, dtype=dtype)
                    val = val * ((val_range[1] - val_range[0]) / (math.pi * 2.0))
                    val = val + ((val_range[1] - val_range[0]) / 2.0)
                    val = torch.clamp(val, min=val_range[0], max=val_range[1])
                    return val
                result = torch.zeros(shape, dtype=dtype)
                result.apply_(lambda x: random.randint(val_range[0], val_range[1]))
                return result

            def make_non_contiguous(shape, dtype) -> torch.Tensor:
                contig = make_contiguous(shape, dtype)
                non_contig = torch.empty(shape + (2, 2), dtype=dtype)[..., 0]
                non_contig = non_contig.select(-1, -1)
                non_contig.copy_(contig)
                self.assertFalse(non_contig.is_contiguous())
                return non_contig

            def make_contiguous_slice(size, dtype) -> torch.Tensor:
                contig = make_contiguous((1, size), dtype)
                non_contig = contig[:1, 1:size - 1]
                self.assertTrue(non_contig.is_contiguous())
                return contig

            types = []
            if use_floating:
                types += float_types
            if use_integral:
                types += int_types
            if use_complex:
                types += complex_types
            tensors: Dict[str, List[torch.Tensor]] = {"cont": [], "noncont": [], "slice": []}
            for dtype in types:
                tensors["cont"].append(make_contiguous(shape, dtype))
                tensors["noncont"].append(make_non_contiguous(shape, dtype))
                tensors["slice"].append(make_contiguous_slice(sum(list(shape)), dtype))

            return tensors

        def test_dir(self):
            dir(torch)

        def test_wildcard_import(self):
            exec('from torch import *')

        @wrapDeterministicFlagAPITest
        def test_deterministic_flag(self):
            for deterministic, warn_only in product([True, False], [True, False]):
                torch.use_deterministic_algorithms(deterministic, warn_only=warn_only)
                self.assertEqual(deterministic, torch.are_deterministic_algorithms_enabled())
                self.assertEqual(warn_only, torch.is_deterministic_algorithms_warn_only_enabled())

                if deterministic:
                    if warn_only:
                        debug_mode = 1
                    else:
                        debug_mode = 2
                else:
                    debug_mode = 0

                self.assertEqual(debug_mode, torch.get_deterministic_debug_mode())

            for debug_mode in [0, 1, 2]:
                torch.set_deterministic_debug_mode(debug_mode)
                self.assertEqual(debug_mode, torch.get_deterministic_debug_mode())
                deterministic = debug_mode in [1, 2]
                warn_only = debug_mode == 1

                self.assertEqual(deterministic, torch.are_deterministic_algorithms_enabled())
                self.assertEqual(warn_only, torch.is_deterministic_algorithms_warn_only_enabled())

            for debug_mode, debug_mode_str in [(0, 'default'), (1, 'warn'), (2, 'error')]:
                torch.set_deterministic_debug_mode(debug_mode_str)
                self.assertEqual(debug_mode, torch.get_deterministic_debug_mode())

            with self.assertRaisesRegex(
                    TypeError,
                    r"_set_deterministic_algorithms\(\): argument 'mode' \(position 1\) must be bool, not int"):
                torch.use_deterministic_algorithms(1)

            with self.assertRaisesRegex(
                    TypeError,
                    r"_set_deterministic_algorithms\(\): argument 'warn_only' must be bool, not int"):
                torch.use_deterministic_algorithms(False, warn_only=1)

        def test_type_conversion_via_dtype_name(self):
            x = torch.tensor([1])
            self.assertEqual(x.byte().dtype, torch.uint8)
            self.assertEqual(x.bool().dtype, torch.bool)
            self.assertEqual(x.char().dtype, torch.int8)
            self.assertEqual(x.double().dtype, torch.float64)
            self.assertEqual(x.float().dtype, torch.float32)
            self.assertEqual(x.half().dtype, torch.float16)
            self.assertEqual(x.int().dtype, torch.int32)
            self.assertEqual(x.bfloat16().dtype, torch.bfloat16)
            cfloat = x.cfloat()
            self.assertEqual(cfloat.dtype, torch.complex64)
            self.assertEqual(cfloat.real, x.float())
            self.assertEqual(cfloat.imag, torch.zeros_like(cfloat.imag))
            cdouble = x.cdouble()
            self.assertEqual(cdouble.dtype, torch.complex128)
            self.assertEqual(cdouble.real, x.double())
            self.assertEqual(cdouble.imag, torch.zeros_like(cdouble.imag))

        def test_doc_template(self) -> None:
            from torch._torch_docs import __file__ as doc_file
            from torch._torch_docs import multi_dim_common, single_dim_common, factory_common_args, factory_like_common_args

            with open(doc_file, "r", encoding="utf-8") as f:
                doc_strs = f.read()

            for doc_str in re.findall(r'add_docstr\((.*?),.*?("""|\'\'\')(.*?)("""|\'\'\')\)', doc_strs, re.MULTILINE | re.DOTALL):
                for common_args in [multi_dim_common, single_dim_common, factory_common_args, factory_like_common_args]:
                    for k, v in common_args.items():
                        self.assertNotIn(v, doc_str[2], 'The argument description "{}" in {} can be '
                                                        'replaced by {{{}}}'.format(v, doc_str[0], k))

        def test_doc(self):
            checked_types = (types.MethodType, types.FunctionType,
                             types.BuiltinFunctionType, types.BuiltinMethodType)

            def test_namespace(ns, *skips):
                if isinstance(ns, object):
                    ns_name = ns.__class__.__name__
                else:
                    ns_name = ns.__name__
                skip_regexes = []
                for r in skips:
                    if isinstance(r, string_classes):
                        skip_regexes.append(re.compile('^{}$'.format(re.escape(r))))
                    else:
                        skip_regexes.append(r)

                for name in dir(ns):
                    if name.startswith('_'):
                        continue
                    if name in ['real', 'imag']:
                        y = torch.randn(1, dtype=torch.cfloat)
                        var = getattr(y, name)
                    elif name in ["H", "mT", "mH"]:
                        y = torch.randn(1, 1)
                        var = getattr(y, name)
                    else:
                        var = getattr(ns, name)
                    if not isinstance(var, checked_types):
                        continue
                    doc = var.__doc__
                    has_doc = doc is not None and len(doc.strip()) > 0
                    full_name = ns_name + '.' + name
                    if any(r.match(name) for r in skip_regexes):
                        self.assertFalse(has_doc,
                                         'New docs have been added for {}, please remove '
                                         'it from the skipped list in TestTorch.test_doc'.format(full_name))
                    else:
                        self.assertTrue(has_doc, '{} is missing documentation'.format(full_name))

            # FIXME: All of the following should be marked as expected failures
            # so that it is easier to tell when missing has been added.
            # FIXME: fix all the skipped ones below!
            test_namespace(torch.randn(1),
                           'as_strided_',
                           re.compile('^clamp_(min|max)_?$'),
                           'is_distributed',
                           'is_nonzero',
                           'is_same_size',
                           'log_softmax',
                           'map2_',
                           'new',
                           'reinforce',
                           'relu',
                           'relu_',
                           'prelu',
                           'resize',
                           'resize_as',
                           'softmax',
                           'split_with_sizes',
                           'unsafe_split_with_sizes',
                           '_autocast_to_fp16',
                           '_autocast_to_fp32',
                           )
            test_namespace(torch.nn)
            test_namespace(torch.nn.functional, 'assert_int_or_pair')
            # TODO: add torch.* tests when we have proper namespacing on ATen functions
            # test_namespace(torch)

        def test_ort_error(self):
            with self.assertRaisesRegex(RuntimeError,
                                        "Could not run 'aten::empty.memory_format' with arguments from the 'ORT' backend"):
                torch.zeros(1, device=torch.device('ort'))

        def test_has_storage(self):
            self.assertIsNotNone(torch.tensor([]).storage())
            self.assertIsNotNone(torch.empty(0).storage())
            self.assertIsNotNone(torch.tensor([]).clone().storage())
            self.assertIsNotNone(torch.tensor([0, 0, 0]).nonzero().storage())
            self.assertIsNotNone(torch.tensor([]).new().storage())

        def test_where_invalid_device(self):
            if torch.cuda.is_available():
                for devices in [('cpu', 'cuda', 'cuda'), ('cuda', 'cpu', 'cpu'),
                                ('cuda', 'cpu', 'cuda'), ('cpu', 'cuda', 'cpu')]:
                    condition = torch.rand(16, device=devices[0])
                    x = torch.rand(16, device=devices[1])
                    y = torch.rand(16, device=devices[2])
                    with self.assertRaisesRegex(RuntimeError,
                                                "Expected condition, x and y to be on the same device"):
                        torch.where(condition, x, y)

        def test_where_bool_tensor(self):
            for d in get_all_device_types():
                a = torch.tensor([True, False], device=d)
                res = torch.where(a > 0)
                self.assertEqual(1, len(res))

        def test_where_tensor(self):
            def rand_tensor(size, dtype, device):
                if dtype.is_floating_point or dtype.is_complex:
                    return torch.rand(size=size, dtype=dtype, device=device)
                elif dtype == torch.uint8:
                    return torch.randint(1, 5, size=size, dtype=dtype, device=device)
                elif dtype == torch.bool:
                    return torch.randint(0, 1, size=size, dtype=dtype, device=device).bool()
                else:
                    return torch.randint(-5, 5, size=size, dtype=dtype, device=device)

            def get_tensor(size, dtype, device, contiguous):
                if not contiguous and len(size) < 2:
                    raise RuntimeError("Unable to generate non contiguous tensor with size < 2")
                t = rand_tensor(size, dtype, device)
                if contiguous:
                    return t
                else:
                    return t.transpose(0, 1)

            height = 5
            width = 5
            for device in get_all_device_types():
                for dt1 in get_all_dtypes():
                    for dt2 in get_all_dtypes():
                        for contiguous in [True, False]:
                            x1 = get_tensor((height, width), dt1, device, contiguous)
                            x2 = get_tensor((height, width), dt2, device, contiguous)
                            if dt1 != dt2:
                                self.assertRaisesRegex(RuntimeError, "expected scalar type", lambda: torch.where(x1 == 1, x1, x2))
                            else:
                                if x1.is_floating_point():
                                    condition = (x1 < 0.5)
                                elif x1.is_complex():
                                    condition = (x1.abs() < 0.5)
                                else:
                                    condition = (x1 == 1)
                                expected = condition.to(x1.dtype) * x1 + (~condition).to(x2.dtype) * x2
                                result = torch.where(condition, x1, x2)
                                self.assertEqual(expected, result)

        def test_dtypes(self):
            all_dtypes = get_all_dtypes()
            do_test_dtypes(self, all_dtypes, torch.strided, torch.device('cpu'))
            if torch.cuda.is_available():
                all_dtypes.remove(torch.bfloat16)  # Remove once _th_zero_ is enabled on cuda for bfloat16
                do_test_dtypes(self, all_dtypes, torch.strided, torch.device('cuda:0'))

        def test_copy_dtypes(self):
            all_dtypes = get_all_dtypes()
            for dtype in all_dtypes:
                copied_dtype = copy.deepcopy(dtype)
                self.assertIs(dtype, copied_dtype)

        def test_copy_transpose(self):
            x = torch.arange(100 * 100, dtype=torch.float).reshape(100, 100).t()
            y = torch.empty(100, 100, dtype=torch.float)
            y.copy_(x)
            self.assertEqual(y[:, 0], range(100))
            self.assertEqual(y[:, 40], range(4000, 4100))

            y = torch.empty(100, 100, dtype=torch.double)
            y.copy_(x)
            self.assertEqual(y[:, 0], range(100))
            self.assertEqual(y[:, 40], range(4000, 4100))

            # Validates regression reported in https://github.com/pytorch/pytorch/issues/45269
            x = torch.arange(100 * 100).reshape(100, 100).to(dtype=torch.cfloat).t()
            y = torch.empty(100, 100, dtype=torch.cfloat)
            y.copy_(x)
            self.assertEqual(y[:, 0], range(100))
            self.assertEqual(y[:, 40], range(4000, 4100))

        # Verifies the bugfix for https://github.com/pytorch/pytorch/issues/64358
        def test_copy_transpose_2d_broadcast(self):
            # The shape (60, 60) is chosen because of
            # `MIN_SZ = 60 * 60` in `copy_transpose_valid` from aten/src/ATen/native/Copy.cpp
            A = torch.randn(60, 60)
            A.copy_(torch.tensor([[1.]]))
            self.assertEqual(A, torch.ones(60, 60))

        def test_device(self):
            cpu = torch.device('cpu')
            self.assertEqual('cpu', str(cpu))
            self.assertEqual('cpu', cpu.type)
            self.assertEqual(None, cpu.index)

            cpu0 = torch.device('cpu:0')
            self.assertEqual('cpu:0', str(cpu0))
            self.assertEqual('cpu', cpu0.type)
            self.assertEqual(0, cpu0.index)

            cpu0 = torch.device('cpu', 0)
            self.assertEqual('cpu:0', str(cpu0))
            self.assertEqual('cpu', cpu0.type)
            self.assertEqual(0, cpu0.index)

            cuda = torch.device('cuda')
            self.assertEqual('cuda', str(cuda))
            self.assertEqual('cuda', cuda.type)
            self.assertEqual(None, cuda.index)

            cuda1 = torch.device('cuda:1')
            self.assertEqual('cuda:1', str(cuda1))
            self.assertEqual('cuda', cuda1.type)
            self.assertEqual(1, cuda1.index)

            cuda1 = torch.device('cuda', 1)
            self.assertEqual('cuda:1', str(cuda1))
            self.assertEqual('cuda', cuda1.type)
            self.assertEqual(1, cuda1.index)

            cuda90 = torch.device('cuda', 90)
            self.assertEqual('cuda:90', str(cuda90))
            self.assertEqual('cuda', cuda90.type)
            self.assertEqual(90, cuda90.index)

            self.assertRaises(RuntimeError, lambda: torch.device('cpu:-1'))
            self.assertRaises(RuntimeError, lambda: torch.device('cuda:-1'))
            self.assertRaises(RuntimeError, lambda: torch.device('cuda:2 '))
            self.assertRaises(RuntimeError, lambda: torch.device('cuda: 2'))
            self.assertRaises(RuntimeError, lambda: torch.device('cuda:2 2'))
            self.assertRaises(RuntimeError, lambda: torch.device('cuda:2.'))
            self.assertRaises(RuntimeError, lambda: torch.device('cuda:2?'))
            self.assertRaises(RuntimeError, lambda: torch.device('cuda:?2'))
            self.assertRaises(RuntimeError, lambda: torch.device('cuda:'))
            self.assertRaises(RuntimeError, lambda: torch.device('cuda:2.232'))
            self.assertRaises(RuntimeError, lambda: torch.device('cuda:2 cuda:3'))
            self.assertRaises(RuntimeError, lambda: torch.device('cuda:2+cuda:3'))
            self.assertRaises(RuntimeError, lambda: torch.device('cuda:2cuda:3'))
            self.assertRaises(RuntimeError, lambda: torch.device(-1))

            self.assertRaises(RuntimeError, lambda: torch.device('other'))
            self.assertRaises(RuntimeError, lambda: torch.device('other:0'))

            device_set = {'cpu', 'cpu:0', 'cuda', 'cuda:0', 'cuda:1', 'cuda:10', 'cuda:100'}
            device_hash_set = set()
            for device in list(device_set):
                device_hash_set.add(hash(torch.device(device)))
            self.assertEqual(len(device_set), len(device_hash_set))

            def get_expected_device_repr(device):
                if device.index is not None:
                    return "device(type='{type}', index={index})".format(
                        type=device.type, index=device.index)

                return "device(type='{type}')".format(type=device.type)

            for device in device_set:
                dev = torch.device(device)
                self.assertEqual(repr(dev), get_expected_device_repr(dev))

        def test_to(self):
            def test_copy_behavior(t, non_blocking=False):
                self.assertIs(t, t.to(t, non_blocking=non_blocking))
                self.assertIs(t, t.to(t.dtype, non_blocking=non_blocking))
                self.assertIs(t, t.to(torch.empty_like(t), non_blocking=non_blocking))
                self.assertIsNot(t, t.to(t, non_blocking=non_blocking, copy=True))
                self.assertIsNot(t, t.to(t.dtype, non_blocking=non_blocking, copy=True))
                self.assertIsNot(t, t.to(torch.empty_like(t), non_blocking=non_blocking, copy=True))

                devices = [t.device]
                if t.device.type == 'cuda':
                    if t.device.index == -1:
                        devices.append('cuda:{}'.format(torch.cuda.current_device()))
                    elif t.device.index == torch.cuda.current_device():
                        devices.append('cuda')
                for device in devices:
                    self.assertIs(t, t.to(device, non_blocking=non_blocking))
                    self.assertIs(t, t.to(device, t.dtype, non_blocking=non_blocking))
                    self.assertIsNot(t, t.to(device, non_blocking=non_blocking, copy=True))
                    self.assertIsNot(t, t.to(device, t.dtype, non_blocking=non_blocking, copy=True))

            a = torch.tensor(5)
            test_copy_behavior(a)
            self.assertEqual(a.device, a.to('cpu').device)
            self.assertEqual(a.device, a.to('cpu', dtype=torch.float32).device)
            self.assertIs(torch.float32, a.to('cpu', dtype=torch.float32).dtype)
            self.assertEqual(a.device, a.to(torch.float32).device)
            self.assertIs(torch.float32, a.to(dtype=torch.float32).dtype)
            self.assertEqual(a.data_ptr(), a.to('cpu').data_ptr())
            self.assertEqual(a.data_ptr(), a.to(dtype=a.dtype, device=a.device, copy=False).data_ptr())
            self.assertEqual(a.data_ptr(), a.to('cpu', copy=False).data_ptr())
            self.assertNotEqual(a.data_ptr(), a.to('cpu', copy=True).data_ptr())

            if torch.cuda.is_available():
                for non_blocking in [True, False]:
                    for cuda in ['cuda', 'cuda:0' if torch.cuda.device_count() == 1 else 'cuda:1']:
                        b = torch.tensor(5., device=cuda)
                        test_copy_behavior(b, non_blocking)
                        self.assertEqual(b.device, b.to(cuda, non_blocking=non_blocking).device)
                        self.assertEqual(a.device, b.to('cpu', non_blocking=non_blocking).device)
                        self.assertEqual(b.device, a.to(cuda, non_blocking=non_blocking).device)
                        self.assertIs(torch.int32, b.to('cpu', dtype=torch.int32, non_blocking=non_blocking).dtype)
                        self.assertEqual(a.device, b.to('cpu', dtype=torch.int32, non_blocking=non_blocking).device)
                        self.assertIs(torch.int32, b.to(dtype=torch.int32).dtype)
                        self.assertEqual(b.device, b.to(dtype=torch.int32).device)

        def test_to_with_tensor(self):
            a = torch.tensor(5)
            self.assertEqual(a.device, a.to(a).device)

            if torch.cuda.is_available():
                for non_blocking in [True, False]:
                    for cuda in ['cuda', 'cuda:0' if torch.cuda.device_count() == 1 else 'cuda:1']:
                        b = torch.tensor(5., device=cuda)
                        self.assertEqual(b.device, b.to(b, non_blocking=non_blocking).device)
                        self.assertEqual(a.device, b.to(a, non_blocking=non_blocking).device)
                        self.assertEqual(b.device, a.to(b, non_blocking=non_blocking).device)

        def test_as_subclass(self):
            class SubTensor(torch.Tensor):
                member_var = object()

            t0 = torch.tensor(0)
            t1 = torch.tensor([1, 2])
            t2 = torch.tensor([[3, 4], [5, 6]])

            s0 = t0.as_subclass(SubTensor)
            s1 = t1.as_subclass(SubTensor)
            s2 = t2.as_subclass(SubTensor)

            # Check that the correct type is returned.
            self.assertTrue(type(s0) is SubTensor)
            self.assertTrue(type(s1) is SubTensor)
            self.assertTrue(type(s2) is SubTensor)

            # Check that the data is equal.
            self.assertEqual(t0, s0)
            self.assertEqual(t1, s1)
            self.assertEqual(t2, s2)

            t0[()] = 1
            t1[1] = 3
            t2[1, 1] = 7

            # Check that the data is equal even after modification.
            self.assertEqual(t0, s0)
            self.assertEqual(t1, s1)
            self.assertEqual(t2, s2)

            # Check that member variables are passed through.
            self.assertTrue(s0.member_var is SubTensor.member_var)
            self.assertTrue(s1.member_var is SubTensor.member_var)
            self.assertTrue(s2.member_var is SubTensor.member_var)

            # Test that autograd is propagated.
            t = torch.tensor(5, dtype=torch.float32, requires_grad=True)

            # Run a calculation on the tensor.
            exp_t = torch.exp(t)

            # Cast exp_t to a subclass.
            exp_s = exp_t.as_subclass(SubTensor)

            # Make sure that t.grad was initially None
            self.assertTrue(t.grad is None)

            # Run the autograd calculation.
            exp_s.backward()

            # Make sure autograd was propagated to the original tensor
            # declared with requires_grad.
            self.assertTrue(t.grad is not None)

            # Make sure invalid subclasses raise nice errors
            class BadSubTensor():
                member_var = object()

            err_msg = "Creating a Tensor subclass from a class that does not inherit from Tensor"
            with self.assertRaisesRegex(RuntimeError, err_msg):
                s0 = t0.as_subclass(BadSubTensor)


        def test_type(self):
            x = torch.randn(3, 3).double()
            self.assertEqual(x.type('torch.FloatTensor').dtype, torch.float32)
            self.assertEqual(x.type(torch.FloatTensor).dtype, torch.float32)
            self.assertEqual(x.int().type(torch.Tensor).dtype, torch.get_default_dtype())
            self.assertEqual(x.type(torch.int32).dtype, torch.int32)

        def test_qengine(self):
            qengines = torch.backends.quantized.supported_engines
            original_qe = torch.backends.quantized.engine
            for qe in qengines:
                torch.backends.quantized.engine = qe
                assert torch.backends.quantized.engine == qe, 'qengine not set successfully'
            torch.backends.quantized.engine = original_qe

        def _spawn_method(self, method, arg):
            try:
                mp.set_start_method('spawn')
            except RuntimeError:
                pass
            with mp.Pool(1) as pool:
                out: list = pool.map(method, [arg])
                self.assertTrue(out[0])

        @staticmethod
        def _test_multinomial_invalid_probs(probs):
            try:
                # n_sample = 1 is a special case, test n_sample=2 which is more general
                torch.multinomial(probs.to('cpu'), 2)
                return False  # Should not be reached
            except RuntimeError as e:
                return 'probability tensor contains either `inf`, `nan` or element < 0' in str(e)

        @slowTest
        @unittest.skipIf(NO_MULTIPROCESSING_SPAWN, "Disabled for environments that \
                         don't support multiprocessing with spawn start method")
        @unittest.skipIf(IS_WINDOWS, 'FIXME: CUDA OOM error on Windows')
        def test_multinomial_invalid_probs(self):
            test_method = AbstractTestCases._TestTorchMixin._test_multinomial_invalid_probs
            self._spawn_method(test_method, torch.tensor([1., -1., 1.]))
            self._spawn_method(test_method, torch.tensor([1., inf, 1.]))
            self._spawn_method(test_method, torch.tensor([1., -inf, 1.]))
            self._spawn_method(test_method, torch.tensor([1., 1., nan]))

        def test_copy_broadcast(self):
            torch.zeros(5, 6).copy_(torch.zeros(6))
            self.assertRaises(RuntimeError, lambda: torch.zeros(5, 6).copy_(torch.zeros(30)))

        def test_copy_many_to_one(self):
            # Testing in-place copy where it attempt to write from many memory
            # storage to a single storage would cause RuntimeError to be thrown
            self.assertRaises(RuntimeError, lambda: torch.zeros(1, 6).expand(5, 6).copy_(torch.zeros(5, 6)))

        def test_slice(self):
            empty = torch.empty(0, 4)
            x = torch.arange(0., 16).view(4, 4)
            self.assertEqual(x[:], x)
            self.assertEqual(x[:4], x)
            # start and stop are clamped to the size of dim
            self.assertEqual(x[:5], x)
            # if start >= stop then the result is empty
            self.assertEqual(x[2:1], empty)
            self.assertEqual(x[2:2], empty)
            # out of bounds is also empty
            self.assertEqual(x[10:12], empty)
            # additional correctness checks
            self.assertEqual(x[:1].tolist(), [[0, 1, 2, 3]])
            self.assertEqual(x[:-3].tolist(), [[0, 1, 2, 3]])
            self.assertEqual(x[:, -2:3].tolist(), [[2], [6], [10], [14]])
            self.assertEqual(x[0:-1:2].tolist(), [[0, 1, 2, 3], [8, 9, 10, 11]])

        @unittest.skip("Not implemented yet")
        def test_conv2(self):
            x = torch.rand(math.floor(torch.uniform(50, 100)), math.floor(torch.uniform(50, 100)))
            k = torch.rand(math.floor(torch.uniform(10, 20)), math.floor(torch.uniform(10, 20)))
            imvc = torch.conv2(x, k)
            imvc2 = torch.conv2(x, k, 'V')
            imfc = torch.conv2(x, k, 'F')

            ki = k.clone()
            ks = k.storage()
            kis = ki.storage()
            for i in range(ks.size() - 1, 0, -1):
                kis[ks.size() - i + 1] = ks[i]
            # for i=ks.size(), 1, -1 do kis[ks.size()-i+1]=ks[i] end
            imvx = torch.xcorr2(x, ki)
            imvx2 = torch.xcorr2(x, ki, 'V')
            imfx = torch.xcorr2(x, ki, 'F')

            self.assertEqual(imvc, imvc2, atol=0, rtol=0, msg='torch.conv2')
            self.assertEqual(imvc, imvx, atol=0, rtol=0, msg='torch.conv2')
            self.assertEqual(imvc, imvx2, atol=0, rtol=0, msg='torch.conv2')
            self.assertEqual(imfc, imfx, atol=0, rtol=0, msg='torch.conv2')
            self.assertLessEqual(math.abs(x.dot(x) - torch.xcorr2(x, x)[0][0]), 1e-10, 'torch.conv2')

            xx = torch.empty(2, x.size(1), x.size(2))
            xx[1].copy_(x)
            xx[2].copy_(x)
            kk = torch.empty(2, k.size(1), k.size(2))
            kk[1].copy_(k)
            kk[2].copy_(k)

            immvc = torch.conv2(xx, kk)
            immvc2 = torch.conv2(xx, kk, 'V')
            immfc = torch.conv2(xx, kk, 'F')

            self.assertEqual(immvc[0], immvc[1], atol=0, rtol=0, msg='torch.conv2')
            self.assertEqual(immvc[0], imvc, atol=0, rtol=0, msg='torch.conv2')
            self.assertEqual(immvc2[0], imvc2, atol=0, rtol=0, msg='torch.conv2')
            self.assertEqual(immfc[0], immfc[1], atol=0, rtol=0, msg='torch.conv2')
            self.assertEqual(immfc[0], imfc, atol=0, rtol=0, msg='torch.conv2')

        @unittest.skip("Not implemented yet")
        def test_conv3(self):
            x = torch.rand(math.floor(torch.uniform(20, 40)),
                           math.floor(torch.uniform(20, 40)),
                           math.floor(torch.uniform(20, 40)))
            k = torch.rand(math.floor(torch.uniform(5, 10)),
                           math.floor(torch.uniform(5, 10)),
                           math.floor(torch.uniform(5, 10)))
            imvc = torch.conv3(x, k)
            imvc2 = torch.conv3(x, k, 'V')
            imfc = torch.conv3(x, k, 'F')

            ki = k.clone()
            ks = k.storage()
            kis = ki.storage()
            for i in range(ks.size() - 1, 0, -1):
                kis[ks.size() - i + 1] = ks[i]
            imvx = torch.xcorr3(x, ki)
            imvx2 = torch.xcorr3(x, ki, 'V')
            imfx = torch.xcorr3(x, ki, 'F')

            self.assertEqual(imvc, imvc2, atol=0, rtol=0, msg='torch.conv3')
            self.assertEqual(imvc, imvx, atol=0, rtol=0, msg='torch.conv3')
            self.assertEqual(imvc, imvx2, atol=0, rtol=0, msg='torch.conv3')
            self.assertEqual(imfc, imfx, atol=0, rtol=0, msg='torch.conv3')
            self.assertLessEqual(math.abs(x.dot(x) - torch.xcorr3(x, x)[0][0][0]), 4e-10, 'torch.conv3')

            xx = torch.empty(2, x.size(1), x.size(2), x.size(3))
            xx[1].copy_(x)
            xx[2].copy_(x)
            kk = torch.empty(2, k.size(1), k.size(2), k.size(3))
            kk[1].copy_(k)
            kk[2].copy_(k)

            immvc = torch.conv3(xx, kk)
            immvc2 = torch.conv3(xx, kk, 'V')
            immfc = torch.conv3(xx, kk, 'F')

            self.assertEqual(immvc[0], immvc[1], atol=0, rtol=0, msg='torch.conv3')
            self.assertEqual(immvc[0], imvc, atol=0, rtol=0, msg='torch.conv3')
            self.assertEqual(immvc2[0], imvc2, atol=0, rtol=0, msg='torch.conv3')
            self.assertEqual(immfc[0], immfc[1], atol=0, rtol=0, msg='torch.conv3')
            self.assertEqual(immfc[0], imfc, atol=0, rtol=0, msg='torch.conv3')

        @unittest.skip("Not implemented yet")
        def _test_conv_corr_eq(self, fn, fn_2_to_3):
            ix = math.floor(random.randint(20, 40))
            iy = math.floor(random.randint(20, 40))
            iz = math.floor(random.randint(20, 40))
            kx = math.floor(random.randint(5, 10))
            ky = math.floor(random.randint(5, 10))
            kz = math.floor(random.randint(5, 10))

            x = torch.rand(ix, iy, iz)
            k = torch.rand(kx, ky, kz)

            o3 = fn(x, k)
            o32 = torch.zeros(o3.size())
            fn_2_to_3(x, k, o3, o32)
            self.assertEqual(o3, o32)

        @unittest.skip("Not implemented yet")
        def test_xcorr3_xcorr2_eq(self):
            def reference(x, k, o3, o32):
                for i in range(o3.size(1)):
                    for j in range(k.size(1)):
                        o32[i].add(torch.xcorr2(x[i + j - 1], k[j]))
            self._test_conv_corr_eq(torch.xcorr3, reference)

        @unittest.skip("Not implemented yet")
        def test_xcorr3_xcorr2_eq_full(self):
            def reference(x, k, o3, o32):
                for i in range(x.size(1)):
                    for j in range(k.size(1)):
                        o32[i].add(torch.xcorr2(x[i], k[k.size(1) - j + 1], 'F'))
            self._test_conv_corr_eq(lambda x, k: torch.xcorr3(x, k, 'F'), reference)

        @unittest.skip("Not implemented yet")
        def test_conv3_conv2_eq_valid(self):
            def reference(x, k, o3, o32):
                for i in range(o3.size(1)):
                    for j in range(k.size(1)):
                        o32[i].add(torch.conv2(x[i + j - 1], k[k.size(1) - j + 1]))
            self._test_conv_corr_eq(torch.conv3, reference)

        @unittest.skip("Not implemented yet")
        def test_fconv3_fconv2_eq(self):
            def reference(x, k, o3, o32):
                for i in range(o3.size(1)):
                    for j in range(k.size(1)):
                        o32[i + j - 1].add(torch.conv2(x[i], k[j], 'F'))
            self._test_conv_corr_eq(lambda x, k: torch.conv3(x, k, 'F'), reference)

        def test_dtype_is_signed(self):
            for dtype in get_all_dtypes():
                self.assertEqual(dtype.is_signed, torch.is_signed(torch.tensor(0, dtype=dtype)))

            self.assertRaisesRegex(RuntimeError, 'not supported for quantized', lambda: torch.quint8.is_signed)
            self.assertRaisesRegex(RuntimeError, 'not supported for quantized', lambda: torch.qint8.is_signed)
            self.assertRaisesRegex(RuntimeError, 'not supported for quantized', lambda: torch.qint32.is_signed)

        def test_RNGState(self):
            state = torch.get_rng_state()
            stateCloned = state.clone()
            before = torch.rand(1000)

            self.assertEqual(state.ne(stateCloned).long().sum(), 0, atol=0, rtol=0)

            torch.set_rng_state(state)
            after = torch.rand(1000)
            self.assertEqual(before, after, atol=0, rtol=0)

        def test_RNGStateAliasing(self):
            # Fork the random number stream at this point
            gen = torch.Generator()
            gen.set_state(torch.get_rng_state())
            self.assertEqual(gen.get_state(), torch.get_rng_state())

            target_value = torch.rand(1000)
            # Dramatically alter the internal state of the main generator
            _ = torch.rand(100000)
            forked_value = torch.rand(1000, generator=gen)
            self.assertEqual(target_value, forked_value, atol=0, rtol=0, msg="RNG has not forked correctly.")

        def test_RNG_after_pickle(self):
            torch.random.manual_seed(100)
            before = torch.rand(10)

            torch.random.manual_seed(100)
            buf = io.BytesIO()
            tensor = torch.tensor([1, 2, 3])
            ForkingPickler(buf, pickle.HIGHEST_PROTOCOL).dump(tensor)
            after = torch.rand(10)

            self.assertEqual(before, after, atol=0, rtol=0)

        def test_boxMullerState(self):
            torch.manual_seed(123)
            odd_number = 101
            seeded = torch.randn(odd_number)
            state = torch.get_rng_state()
            midstream = torch.randn(odd_number)
            torch.set_rng_state(state)
            repeat_midstream = torch.randn(odd_number)
            torch.manual_seed(123)
            reseeded = torch.randn(odd_number)
            self.assertEqual(midstream, repeat_midstream, atol=0, rtol=0,
                             msg='get_rng_state/set_rng_state not generating same sequence of normally distributed numbers')
            self.assertEqual(seeded, reseeded, atol=0, rtol=0,
                             msg='repeated calls to manual_seed not generating same sequence of normally distributed numbers')

        def test_manual_seed(self):
            rng_state = torch.get_rng_state()
            torch.manual_seed(2)
            x = torch.randn(100)
            self.assertEqual(torch.initial_seed(), 2)
            torch.manual_seed(2)
            y = torch.randn(100)
            self.assertEqual(x, y)

            max_int64 = 0x7fff_ffff_ffff_ffff
            min_int64 = -max_int64 - 1
            max_uint64 = 0xffff_ffff_ffff_ffff
            # Check all boundary cases of valid seed value inputs
            test_cases = [
                # (seed, expected_initial_seed)
                # Positive seeds should be unchanged
                (max_int64, max_int64),
                (max_int64 + 1, max_int64 + 1),
                (max_uint64, max_uint64),
                (0, 0),
                # Negative seeds wrap around starting from the largest seed value
                (-1, max_uint64),
                (min_int64, max_int64 + 1)
            ]
            for seed, expected_initial_seed in test_cases:
                torch.manual_seed(seed)
                actual_initial_seed = torch.initial_seed()
                msg = "expected initial_seed() = %x after calling manual_seed(%x), but got %x instead" % (
                    expected_initial_seed, seed, actual_initial_seed)
                self.assertEqual(expected_initial_seed, actual_initial_seed, msg=msg)
            for invalid_seed in [min_int64 - 1, max_uint64 + 1]:
                with self.assertRaisesRegex(RuntimeError, r'Overflow when unpacking long'):
                    torch.manual_seed(invalid_seed)

            torch.set_rng_state(rng_state)

        def test_numel(self):
            b = torch.ByteTensor(3, 100, 100)
            self.assertEqual(b.nelement(), 3 * 100 * 100)
            self.assertEqual(b.numel(), 3 * 100 * 100)

        def test_empty_storage_view(self):
            # we should be able to "modify" slices of a 0-element
            # array without an error being raised due to
            # trying to resize its storage
            t = torch.from_numpy(np.empty((0, 4)))
            t[:, 1::2] *= 1

        def test_newaxis_numpy_comparison(self):
            def run_test(tensor, *idx):
                npt = tensor.numpy()
                self.assertEqual(tensor[idx], npt[idx])

            # 1D Tensor Tests
            x = torch.arange(0, 10)
            cases = [
                [None],
                [None, None],
                [Ellipsis, None],
                [None, Ellipsis],
                [2, None],
                [None, 2],
                [Ellipsis, None, 2],
                [Ellipsis, 2, None],
                [2, Ellipsis, None],
                [2, None, Ellipsis],
                [None, 2, Ellipsis],
                [None, Ellipsis, 2],
            ]

            for case in cases:
                run_test(x, *case)

            # 2D Tensor Tests
            x = torch.arange(0, 12).view(3, 4)
            cases = [
                [None],
                [None, None],
                [None, None, None],
                [Ellipsis, None],
                [Ellipsis, None, None],
                [None, Ellipsis],
                [None, Ellipsis, None],
                [None, None, Ellipsis],
                [2, None],
                [2, None, Ellipsis],
                [2, Ellipsis, None],
                [None, 2, Ellipsis],
                [Ellipsis, 2, None],
                [Ellipsis, None, 2],
                [None, Ellipsis, 2],
                [1, 2, None],
                [1, 2, Ellipsis, None],
                [1, Ellipsis, 2, None],
                [Ellipsis, 1, None, 2],
                [Ellipsis, 1, 2, None],
                [1, None, 2, Ellipsis],
                [None, 1, Ellipsis, 2],
                [None, 1, 2, Ellipsis],
            ]

            for case in cases:
                run_test(x, *case)

        def _consecutive(self, size, start=1):
            sequence = torch.ones(torch.tensor(size).prod(0)).cumsum(0)
            sequence.add_(start - 1)
            return sequence.resize_(*size)

        def test_newindex(self):
            reference = self._consecutive((3, 3, 3))
            # This relies on __index__() being correct - but we have separate tests for that

            def checkPartialAssign(index):
                reference = torch.zeros(3, 3, 3)
                reference[index] = self._consecutive((3, 3, 3))[index]
                self.assertEqual(reference[index], self._consecutive((3, 3, 3))[index], atol=0, rtol=0)
                reference[index] = 0
                self.assertEqual(reference, torch.zeros(3, 3, 3), atol=0, rtol=0)

            checkPartialAssign(0)
            checkPartialAssign(1)
            checkPartialAssign(2)
            checkPartialAssign((0, 1))
            checkPartialAssign((1, 2))
            checkPartialAssign((0, 2))
            checkPartialAssign(torch.LongTensor((0, 2)))

            with self.assertRaises(IndexError):
                reference[1, 1, 1, 1] = 1
            with self.assertRaises(IndexError):
                reference[1, 1, 1, (1, 1)] = 1
            with self.assertRaises(IndexError):
                reference[3, 3, 3, 3, 3, 3, 3, 3] = 1
            with self.assertRaises(IndexError):
                reference[0.0] = 1
            with self.assertRaises(TypeError):
                reference[0.0:2.0] = 1
            with self.assertRaises(IndexError):
                reference[0.0, 0.0:2.0] = 1
            with self.assertRaises(IndexError):
                reference[0.0, :, 0.0:2.0] = 1
            with self.assertRaises(IndexError):
                reference[0.0, ..., 0.0:2.0] = 1
            with self.assertRaises(IndexError):
                reference[0.0, :, 0.0] = 1

        def test_index_add(self):
            for device in get_all_device_types():
                for dest_contig, src_contig, index_contig in product([True, False], repeat=3):
                    for other_sizes in ((), (4, 5)):
                        for dtype in [torch.int, torch.long]:
                            num_copy, num_dest = 3, 3
                            dest = torch.randn(num_dest, *other_sizes, device=device)
                            if not dest_contig:
                                dest = torch.testing.make_non_contiguous(dest)
                            src = torch.randn(num_copy, *other_sizes, device=device)
                            if not src_contig:
                                src = torch.testing.make_non_contiguous(src)
                            idx = torch.randperm(num_dest, dtype=dtype, device=device).narrow(0, 0, num_copy)
                            if not index_contig:
                                idx = torch.testing.make_non_contiguous(idx)
                            # index_add_ without alpha argument
                            dest2 = dest.clone()
                            dest.index_add_(0, idx, src)
                            for i in range(idx.size(0)):
                                dest2[idx[i]] += src[i]
                            self.assertEqual(dest, dest2)
                            # index_add_ with alpha argument
                            dest2 = dest.clone()
                            dest.index_add_(0, idx, src, alpha=2)
                            for i in range(idx.size(0)):
                                dest2[idx[i]] += src[i] * 2
                            self.assertEqual(dest, dest2)

        # add coverage for issue with atomic add that appeared only for
        # specific dtypes on cuda:
        # https://github.com/pytorch/pytorch/issues/29153
        def test_index_add_all_dtypes(self):
            for device in get_all_device_types():
                for dtype in get_all_math_dtypes(device):
                    for idx_dtype in [torch.int, torch.long]:
                        size = [5, 5]
                        if dtype.is_floating_point or dtype.is_complex:
                            tensor = torch.rand(size, dtype=dtype, device=device)
                        elif dtype.is_signed:
                            tensor = torch.randint(-5, 15, size, dtype=dtype, device=device)
                        else:
                            tensor = torch.randint(0, 10, size, dtype=dtype, device=device)

                        # index_add calls atomicAdd on cuda.
                        zeros = torch.zeros(size, dtype=dtype, device=device)

                        added = zeros.index_add(0, torch.arange(0, size[0], dtype=idx_dtype, device=device), tensor)
                        self.assertEqual(added, tensor)

                        added = zeros.index_add(0, torch.arange(0, size[0], dtype=idx_dtype, device=device), tensor, alpha=-1)
                        self.assertEqual(added, -tensor)

        # Fill idx with valid indices.
        @staticmethod
        def _fill_indices(self, idx, dim, dim_size, elems_per_row, m, n, o):
            for i in range(1 if dim == 0 else m):
                for j in range(1 if dim == 1 else n):
                    for k in range(1 if dim == 2 else o):
                        ii = [i, j, k]
                        ii[dim] = slice(0, idx.size(dim) + 1)
                        idx[tuple(ii)] = torch.randperm(dim_size)[0:elems_per_row]

        def test_unflatten(self):
            # test args: tensor, int, sizes
            self.assertEqual(torch.tensor([]).unflatten(0, (0, 1)), torch.empty(0, 1))
            self.assertEqual(torch.tensor([1]).unflatten(0, (1, 1)), torch.tensor([[1]]))
            self.assertEqual(torch.tensor([1, 2, 3, 4]).unflatten(0, (2, 2)), torch.tensor([[1, 2], [3, 4]]))
            self.assertEqual(torch.tensor([1, 2, 3, 4]).unflatten(0, [2, 2]), torch.tensor([[1, 2], [3, 4]]))
            self.assertEqual(torch.tensor([1, 2, 3, 4]).unflatten(0, torch.Size([2, 2])), torch.tensor([[1, 2], [3, 4]]))
            self.assertEqual(torch.ones(2, 10).unflatten(1, (5, 2)), torch.ones(2, 5, 2))
            self.assertEqual(torch.tensor([1, 2, 3, 4]).unflatten(0, (-1, 2)),
                             torch.tensor([[1, 2], [3, 4]]))
            self.assertEqual(torch.ones(2, 10).unflatten(1, (5, -1)),
                             torch.ones(2, 5, 2))
            self.assertEqual(torch.ones(2, 10).unflatten(1, (-1,)),
                             torch.ones(2, 10))
            self.assertEqual(torch.ones(2, 3 * 4 * 5 * 6).unflatten(1, (3, 4, -1, 6)),
                             torch.ones(2, 3, 4, 5, 6))
            self.assertEqual(torch.ones(2, 0, 2).unflatten(1, (3, -1, 4, 5)),
                             torch.ones(2, 3, 0, 4, 5, 2))

            # test invalid args: tensor, str, sizes
            with self.assertRaisesRegex(TypeError, r"received an invalid combination of arguments"):
                torch.tensor([1]).unflatten('A', (1, 1))

            # test invalid args: tensor, str, namedshape
            with self.assertRaisesRegex(RuntimeError, r"Name 'A' not found in Tensor\[None\]."):
                torch.ones(4).unflatten('A', (('A', 2), ('B', 2)))

            # test other invalid arguments
            with self.assertRaisesRegex(RuntimeError, r"sizes must be non-empty"):
                torch.tensor([1]).unflatten(0, [])
            with self.assertRaisesRegex(RuntimeError, r"Provided sizes \[2, 2\] don't multiply up to the size of dim 0 \(1\)"):
                torch.tensor([1]).unflatten(0, [2, 2])
            with self.assertRaisesRegex(IndexError, r"dimension specified as 0 but tensor has no dimensions"):
                torch.tensor(1).unflatten(0, [0])
            with self.assertRaisesRegex(RuntimeError, r"only one dimension can be inferred"):
                torch.randn(5, 10).unflatten(1, (-1, -1))
            with self.assertRaisesRegex(RuntimeError,
                                        r"Provided sizes \[-1, 4\] don't multiply up to the size of dim 1 \(10\)"):
                torch.randn(5, 10).unflatten(1, (-1, 4))
            with self.assertRaisesRegex(RuntimeError,
                                        r"the unspecified dimension size -1 can be any value and is ambiguous"):
                torch.randn(2, 0).unflatten(1, (2, -1, 0))

        @staticmethod
        def _test_gather(self, cast, test_bounds=True):
            m, n, o = random.randint(10, 20), random.randint(10, 20), random.randint(10, 20)
            elems_per_row = random.randint(1, 10)
            dim = random.randrange(3)

            for dtype in {torch.float32, torch.complex64, torch.complex128}:
                src = torch.randn(m, n, o, dtype=dtype)
                idx_size = [m, n, o]
                idx_size[dim] = elems_per_row
                idx = torch.LongTensor().resize_(*idx_size)
                AbstractTestCases._TestTorchMixin._fill_indices(self, idx, dim, src.size(dim), elems_per_row, m, n, o)

                src = cast(src)
                idx = cast(idx)

                actual = torch.gather(src, dim, idx)
                expected = cast(torch.zeros(idx_size, dtype=dtype))
                for i in range(idx_size[0]):
                    for j in range(idx_size[1]):
                        for k in range(idx_size[2]):
                            ii = [i, j, k]
                            ii[dim] = idx[i, j, k]
                            expected[i, j, k] = src[tuple(ii)]
                self.assertEqual(actual, expected, atol=0, rtol=0)

            bad_src = torch.randn(*[i - 1 for i in idx_size])
            self.assertRaises(RuntimeError, lambda: torch.gather(bad_src, dim, idx))

            # should throw an error when index dtype is not long
            with self.assertRaisesRegex(RuntimeError, 'Expected dtype int64 for index'):
                torch.gather(src, dim, idx.to(torch.int))

            # should throw an error when out.dtype != src.dtype.
            # Note that on Windows, the out tensor's dtype is returned as: struct c10::complex<double> in the error
            # message, hence the use of .* in regex here
            with self.assertRaisesRegex(RuntimeError,
                                        'Expected out tensor to have dtype .*c10::complex<double>, but got int instead'):
                torch.gather(src.to(torch.complex128), dim, idx, out=expected.to(torch.int))

            # checks for the same dimensionality
            with self.assertRaisesRegex(RuntimeError, 'Index tensor must have the same number of dimensions as input tensor'):
                torch.gather(src, dim, idx.unsqueeze(-1))

            with self.assertRaisesRegex(RuntimeError, 'Index tensor must have the same number of dimensions as input tensor'):
                torch.gather(src.unsqueeze(-1), dim, idx)

            if test_bounds:
                idx[0][0][0] = 23
                self.assertRaises(RuntimeError, lambda: torch.gather(src, dim, idx))

            src = cast(torch.randn(3, 4, 5))
            expected, idx = src.max(2, True)
            expected = cast(expected)
            idx = cast(idx)
            actual = torch.gather(src, 2, idx)
            self.assertEqual(actual, expected, atol=0, rtol=0)

            # Bool test case
            t = torch.tensor([[False, True], [True, True]])
            self.assertEqual(torch.gather(t, 1, torch.tensor([[0, 0], [1, 0]])), torch.tensor([[False, False], [True, True]]))

        def test_gather(self):
            self._test_gather(self, lambda t: t)

        @staticmethod
        def _test_scatter_add_mult_index_base(self, cast):
            m, n = 30, 40
            idx = torch.zeros(m, n).long()
            src = torch.ones(m, n)
            res0 = torch.zeros(m, n).scatter_add_(0, idx, src)
            res1 = torch.zeros(m, n).scatter_add_(1, idx, src)

            self.assertEqual(res0[0, :], m * torch.ones(n), atol=0, rtol=0)
            self.assertEqual(res1[:, 0], n * torch.ones(m), atol=0, rtol=0)

        def test_scatter_add_mult_index(self):
            self._test_scatter_add_mult_index_base(self, lambda t: t)

        @staticmethod
        def _test_scatter_base(self, cast, method, is_scalar=False, test_bounds=True, reduction=None, *, test_complex=False):
            if test_complex:
                dtypes = [torch.complex64, torch.complex128]
            else:
                dtypes = [torch.float16, torch.float32, torch.float64]

            for dtype in dtypes:
                m, n, o = random.randint(10, 20), random.randint(10, 20), random.randint(10, 20)
                elems_per_row = random.randint(1, 10)
                dim = random.randrange(3)

                idx_size = [m, n, o]
                idx_size[dim] = elems_per_row
                idx = cast(torch.LongTensor().resize_(*idx_size))
                AbstractTestCases._TestTorchMixin._fill_indices(self, idx, dim, ([m, n, o])[dim], elems_per_row, m, n, o)

                src_size = [random.randint(1, 5) + s for s in idx_size]
                if is_scalar:
                    src = random.random()
                else:
                    src = cast(torch.randn(src_size, dtype=dtype))

                base = cast(torch.randn(m, n, o, dtype=dtype))
                if reduction:
                    actual = getattr(base.clone(), method)(dim, idx, src, reduce=reduction)
                else:
                    actual = getattr(base.clone(), method)(dim, idx, src)
                expected = base.clone()
                for i in range(idx_size[0]):
                    for j in range(idx_size[1]):
                        for k in range(idx_size[2]):
                            ii = [i, j, k]
                            ii[dim] = idx[i, j, k]
                            if method == 'scatter_add_':
                                expected[tuple(ii)] += src[i, j, k]
                            else:
                                # method may be 'scatter_' or 'scatter'
                                # both might have a reduction argument
                                value = src if is_scalar else src[i, j, k]

                                if reduction == "add":
                                    expected[tuple(ii)] += value
                                elif reduction == "multiply":
                                    expected[tuple(ii)] *= value
                                else:
                                    expected[tuple(ii)] = value

                self.assertEqual(actual, expected, atol=0, rtol=0)

                # should throw an error when self.dtype != src.dtype.
                # we ignore the case when src is Scalar, as it gets
                # cast via src.to<scalar_t>.
                if not is_scalar:
                    with self.assertRaisesRegex(RuntimeError, 'Expected self.dtype to be equal to src.dtype'):
                        getattr(base.clone().type(torch.int), method)(dim, idx, src)

                    with self.assertRaisesRegex(RuntimeError, 'Expected self.dtype to be equal to src.dtype'):
                        getattr(base.clone(), method)(dim, idx, src.type(torch.int))

                # should throw an error when index dtype is not long
                with self.assertRaisesRegex(RuntimeError, 'Expected dtype int64 for index'):
                    getattr(base.clone(), method)(dim, idx.type(torch.int), src)

                # check for the same dimensionality
                with self.assertRaisesRegex(RuntimeError, 'Index tensor must have the same number of dimensions as self tensor'):
                    getattr(base.clone().unsqueeze(-1), method)(dim, idx, src)

                with self.assertRaisesRegex(RuntimeError, 'Index tensor must have the same number of dimensions as self tensor'):
                    getattr(base.clone(), method)(dim, idx.unsqueeze(-1), src)

                if not is_scalar:
                    with self.assertRaisesRegex(RuntimeError, 'Index tensor must have the same number of dimensions as src tensor'):
                        getattr(base.clone(), method)(dim, idx, src.unsqueeze(-1))

                if test_bounds:
                    idx[0][0][0] = 34
                    with self.assertRaises(RuntimeError):
                        if reduction:
                            getattr(base.clone(), method)(dim, idx, src, reduce=reduction)
                        else:
                            getattr(base.clone(), method)(dim, idx, src)

                # test for empty index, should be a no-op
                idx = cast(torch.LongTensor())
                if reduction:
                    actual = getattr(base.clone(), method)(dim, idx, src, reduce=reduction)
                else:
                    actual = getattr(base.clone(), method)(dim, idx, src)
                self.assertEqual(actual, base, atol=0, rtol=0)

        def test_scatter(self):
            self._test_scatter_base(self, lambda t: t, 'scatter_')

        def test_scatterAdd(self):
            self._test_scatter_base(self, lambda t: t, 'scatter_add_')

        def test_scatterFill(self):
            self._test_scatter_base(self, lambda t: t, 'scatter_', True)

        def test_scatterReduce(self):
            for method in ["add", "multiply"]:
                self._test_scatter_base(self, lambda t: t, 'scatter_', reduction=method)
                self._test_scatter_base(self, lambda t: t, 'scatter_', True, reduction=method)

        def test_scatter_reduce(self):
            dtype = device = None

            output_size = 10
            shape = [5, 10, 20]

            index = torch.randint(0, output_size, shape, dtype=torch.long, device=device)
            input = torch.randn(shape, dtype=dtype, device=device)

            for dim in range(len(shape)):
                output = input._scatter_reduce(dim, index, "sum", output_size=output_size)

                output_shape = copy.copy(shape)
                output_shape[dim] = output_size
                self.assertEqual(output.shape, output_shape)

                expected = torch.zeros(output_shape, dtype=dtype, device=device)
                for i, j, k in itertools.product(range(shape[0]), range(shape[1]), range(shape[2])):
                    v = input[i, j, k]
                    m = index[i, j, k]

                    if dim == 0:
                        i = m
                    elif dim == 1:
                        j = m
                    else:
                        k = m

                    expected[i, j, k] += v

                self.assertTrue(torch.allclose(output, expected))

                torch._scatter_reduce(input, dim, index, "sum", out=output)
                self.assertTrue(torch.allclose(output, expected))

            with self.assertRaisesRegex(RuntimeError, "Expected `dim` to be in range -3 to 2"):
                torch._scatter_reduce(input, 4, index, "sum")

            with self.assertRaisesRegex(RuntimeError, "Shape mismatch"):
                index2 = torch.randint(0, output_size, (10, ), dtype=torch.long, device=device)
                torch._scatter_reduce(input, 0, index2, "sum")

            with self.assertRaisesRegex(RuntimeError, "`reduce` argument must be 'sum'"):
                torch._scatter_reduce(input, 2, index, "mean")

            with self.assertRaisesRegex(RuntimeError, "Expected `index` values to be in range 0 to 2"):
                input2 = torch.randn(10, dtype=dtype, device=device)
                index2 = torch.tensor([0, 1, 0, 1, 2, 3, 3, 4, 4, 3])
                torch._scatter_reduce(input2, 0, index2, "sum", output_size=2)

        def test_structseq_repr(self):
            a = torch.arange(250).reshape(5, 5, 10)
            expected = """
            torch.return_types.max(
            values=tensor([[ 40,  41,  42,  43,  44,  45,  46,  47,  48,  49],
                    [ 90,  91,  92,  93,  94,  95,  96,  97,  98,  99],
                    [140, 141, 142, 143, 144, 145, 146, 147, 148, 149],
                    [190, 191, 192, 193, 194, 195, 196, 197, 198, 199],
                    [240, 241, 242, 243, 244, 245, 246, 247, 248, 249]]),
            indices=tensor([[4, 4, 4, 4, 4, 4, 4, 4, 4, 4],
                    [4, 4, 4, 4, 4, 4, 4, 4, 4, 4],
                    [4, 4, 4, 4, 4, 4, 4, 4, 4, 4],
                    [4, 4, 4, 4, 4, 4, 4, 4, 4, 4],
                    [4, 4, 4, 4, 4, 4, 4, 4, 4, 4]]))"""
            self.assertEqual(repr(a.max(1)), textwrap.dedent(expected).strip())

        def test_is_same_size(self):
            t1 = torch.empty(3, 4, 9, 10)
            t2 = torch.empty(3, 4)
            t3 = torch.empty(1, 9, 3, 3)
            t4 = torch.empty(3, 4, 9, 10)

            self.assertFalse(t1.is_same_size(t2))
            self.assertFalse(t1.is_same_size(t3))
            self.assertTrue(t1.is_same_size(t4))

        def test_tensor_set(self):
            t1 = torch.tensor([])
            t2 = torch.empty(3, 4, 9, 10).uniform_()
            t1.set_(t2)
            self.assertEqual(t1.storage()._cdata, t2.storage()._cdata)
            size = torch.Size([9, 3, 4, 10])
            t1.set_(t2.storage(), 0, size)
            self.assertEqual(t1.size(), size)
            t1.set_(t2.storage(), 0, tuple(size))
            self.assertEqual(t1.size(), size)
            self.assertEqual(t1.stride(), (120, 40, 10, 1))
            stride = (10, 360, 90, 1)
            t1.set_(t2.storage(), 0, size, stride)
            self.assertEqual(t1.stride(), stride)
            t1.set_(t2.storage(), 0, size=size, stride=stride)
            self.assertEqual(t1.size(), size)
            self.assertEqual(t1.stride(), stride)

            # test argument names
            t1 = torch.tensor([])
            # 1. case when source is tensor
            t1.set_(source=t2)
            self.assertEqual(t1.storage()._cdata, t2.storage()._cdata)
            # 2. case when source is storage
            t1.set_(source=t2.storage())
            self.assertEqual(t1.storage()._cdata, t2.storage()._cdata)
            # 3. case when source is storage, and other args also specified
            t1.set_(source=t2.storage(), storage_offset=0, size=size, stride=stride)
            self.assertEqual(t1.size(), size)
            self.assertEqual(t1.stride(), stride)

            t1 = torch.tensor([True, True], dtype=torch.bool)
            t2 = torch.tensor([False, False], dtype=torch.bool)
            t1.set_(t2)
            self.assertEqual(t1.storage()._cdata, t2.storage()._cdata)

        def test_tensor_set_errors(self):
            f_cpu = torch.randn((2, 3), dtype=torch.float32)
            d_cpu = torch.randn((2, 3), dtype=torch.float64)

            # change dtype
            self.assertRaises(RuntimeError, lambda: f_cpu.set_(d_cpu.storage()))
            self.assertRaises(RuntimeError,
                              lambda: f_cpu.set_(d_cpu.storage(), 0, d_cpu.size(), d_cpu.stride()))
            self.assertRaises(RuntimeError, lambda: f_cpu.set_(d_cpu))

            # change device
            if torch.cuda.is_available():
                f_cuda = torch.randn((2, 3), dtype=torch.float32, device='cuda')

                # cpu -> cuda
                self.assertRaises(RuntimeError, lambda: f_cpu.set_(f_cuda.storage()))
                self.assertRaises(RuntimeError,
                                  lambda: f_cpu.set_(f_cuda.storage(), 0, f_cuda.size(), f_cuda.stride()))
                self.assertRaises(RuntimeError, lambda: f_cpu.set_(f_cuda))

                # cuda -> cpu
                self.assertRaises(RuntimeError, lambda: f_cuda.set_(f_cpu.storage()))
                self.assertRaises(RuntimeError,
                                  lambda: f_cuda.set_(f_cpu.storage(), 0, f_cpu.size(), f_cpu.stride()))
                self.assertRaises(RuntimeError, lambda: f_cuda.set_(f_cpu))

        def test_equal(self):
            # Contiguous, 1D
            t1 = torch.tensor((3., 4., 9., 10.))
            t2 = t1.contiguous()
            t3 = torch.tensor((1., 9., 3., 10.))
            t4 = torch.tensor((3., 4., 9.))
            t5 = torch.tensor([])
            self.assertTrue(t1.equal(t2))
            self.assertFalse(t1.equal(t3))
            self.assertFalse(t1.equal(t4))
            self.assertFalse(t1.equal(t5))
            self.assertTrue(torch.equal(t1, t2))
            self.assertFalse(torch.equal(t1, t3))
            self.assertFalse(torch.equal(t1, t4))
            self.assertFalse(torch.equal(t1, t5))

            # Non contiguous, 2D
            s = torch.tensor(((1, 2, 3, 4), (5, 6, 7, 8)))
            s1 = s[:, 1:3]
            s2 = s1.clone()
            s3 = torch.tensor(((2, 3), (6, 7)))
            s4 = torch.tensor(((0, 0), (0, 0)))

            self.assertFalse(s1.is_contiguous())
            self.assertTrue(s1.equal(s2))
            self.assertTrue(s1.equal(s3))
            self.assertFalse(s1.equal(s4))
            self.assertTrue(torch.equal(s1, s2))
            self.assertTrue(torch.equal(s1, s3))
            self.assertFalse(torch.equal(s1, s4))

        def test_element_size(self):
            byte = torch.ByteStorage().element_size()
            char = torch.CharStorage().element_size()
            short = torch.ShortStorage().element_size()
            int = torch.IntStorage().element_size()
            long = torch.LongStorage().element_size()
            float = torch.FloatStorage().element_size()
            double = torch.DoubleStorage().element_size()
            bool = torch.BoolStorage().element_size()
            bfloat16 = torch.BFloat16Storage().element_size()
            complexfloat = torch.ComplexFloatStorage().element_size()
            complexdouble = torch.ComplexDoubleStorage().element_size()

            self.assertEqual(byte, torch.ByteTensor().element_size())
            self.assertEqual(char, torch.CharTensor().element_size())
            self.assertEqual(short, torch.ShortTensor().element_size())
            self.assertEqual(int, torch.IntTensor().element_size())
            self.assertEqual(long, torch.LongTensor().element_size())
            self.assertEqual(float, torch.FloatTensor().element_size())
            self.assertEqual(double, torch.DoubleTensor().element_size())
            self.assertEqual(bool, torch.BoolTensor().element_size())
            self.assertEqual(bfloat16, torch.tensor([], dtype=torch.bfloat16).element_size())
            self.assertEqual(complexfloat, torch.tensor([], dtype=torch.complex64).element_size())
            self.assertEqual(complexdouble, torch.tensor([], dtype=torch.complex128).element_size())

            self.assertGreater(byte, 0)
            self.assertGreater(char, 0)
            self.assertGreater(short, 0)
            self.assertGreater(int, 0)
            self.assertGreater(long, 0)
            self.assertGreater(float, 0)
            self.assertGreater(double, 0)
            self.assertGreater(bool, 0)
            self.assertGreater(bfloat16, 0)
            self.assertGreater(complexfloat, 0)
            self.assertGreater(complexdouble, 0)

            # These tests are portable, not necessarily strict for your system.
            self.assertEqual(byte, 1)
            self.assertEqual(char, 1)
            self.assertEqual(bool, 1)
            self.assertGreaterEqual(short, 2)
            self.assertGreaterEqual(int, 2)
            self.assertGreaterEqual(int, short)
            self.assertGreaterEqual(long, 4)
            self.assertGreaterEqual(long, int)
            self.assertGreaterEqual(double, float)

        def test_permute(self):
            orig = [1, 2, 3, 4, 5, 6, 7]
            perm = torch.randperm(7).tolist()
            x = torch.empty(*orig).fill_(0)
            new = [i - 1 for i in x.permute(*perm).size()]
            self.assertEqual(perm, new)
            self.assertEqual(x.size(), orig)

        def test_reversed(self):
            val = torch.arange(0, 10)
            self.assertEqual(reversed(val), torch.arange(9, -1, -1))

            val = torch.arange(1, 10).view(3, 3)
            self.assertEqual(reversed(val), torch.tensor([[7, 8, 9], [4, 5, 6], [1, 2, 3]]))

            val = torch.tensor(42)
            self.assertEqual(reversed(val), torch.tensor(42))

        def test_contains(self):
            x = torch.arange(0, 10)
            self.assertEqual(4 in x, True)
            self.assertEqual(12 in x, False)

            x = torch.arange(1, 10).view(3, 3)
            val = torch.arange(1, 4)
            self.assertEqual(val in x, True)
            val += 10
            self.assertEqual(val in x, False)

            self.assertRaisesRegex(
                RuntimeError,
                "Tensor.__contains__ only supports Tensor or scalar, but you passed in a {}.".format(type("foo")),
                lambda: "foo" in x)
            self.assertRaisesRegex(
                RuntimeError,
                "Tensor.__contains__ only supports Tensor or scalar, but you passed in a {}.".format(type([1, 2])),
                lambda: [1, 2] in x)

        def test_deepcopy_parameter(self):
            from copy import deepcopy
            l = torch.nn.Linear(10, 1)
            s = l.state_dict(keep_vars=True)
            self.assertEqual(torch.nn.Parameter, type(s['weight']))
            self.assertEqual(torch.nn.Parameter, type(s['bias']))

            s2 = deepcopy(s)
            self.assertEqual(torch.nn.Parameter, type(s2['weight']))
            self.assertEqual(torch.nn.Parameter, type(s2['bias']))

        def test_pickle(self):
            import pickle
            a = torch.randn(5, 5)
            serialized = pickle.dumps(a)
            b = pickle.loads(serialized)
            self.assertEqual(a, b)

        def test_pickle_parameter(self):
            import pickle
            a = torch.nn.Parameter(torch.randn(5, 5))
            serialized = pickle.dumps(a)
            b = pickle.loads(serialized)
            self.assertTrue(isinstance(b, torch.nn.Parameter))
            self.assertEqual(a.requires_grad, b.requires_grad)
            self.assertEqual(a, b)

        def test_pickle_parameter_no_requires_grad(self):
            import pickle
            a = torch.nn.Parameter(torch.randn(5, 5), requires_grad=False)
            serialized = pickle.dumps(a)
            b = pickle.loads(serialized)
            self.assertTrue(isinstance(b, torch.nn.Parameter))
            self.assertEqual(a.requires_grad, b.requires_grad)
            self.assertEqual(a, b)

        def test_pickle_dtype(self):
            t = torch.float32
            serialized = pickle.dumps(t)
            b = pickle.loads(serialized)
            self.assertTrue(isinstance(b, torch.dtype))
            self.assertEqual(id(b), id(t))

        def test_pickle_size(self):
            a = torch.rand(10).size()
            serialized = pickle.dumps(a)
            b = pickle.loads(serialized)
            self.assertTrue(isinstance(b, torch.Size))
            self.assertEqual(a, b)

        def test_pickle_function(self):
            # https://github.com/pytorch/pytorch/issues/37703
            a = torch.tanh
            serialized = pickle.dumps(a)
            b = pickle.loads(serialized)
            self.assertEqual(a, b)

        def test_generator_cpu(self):
            # test default generators are equal
            self.assertEqual(torch.default_generator, torch.default_generator)

            # tests Generator API
            # manual_seed, seed, initial_seed, get_state, set_state
            g1 = torch.Generator()
            g2 = torch.Generator()
            g1.manual_seed(12345)
            g2.manual_seed(12345)
            self.assertEqual(g1.initial_seed(), g2.initial_seed())

            g1.seed()
            g2.seed()
            self.assertNotEqual(g1.initial_seed(), g2.initial_seed())

            g1 = torch.Generator()
            g2_state = g2.get_state()
            g2_randn = torch.randn(1, generator=g2)
            g1.set_state(g2_state)
            g1_randn = torch.randn(1, generator=g1)
            self.assertEqual(g1_randn, g2_randn)

            default_state = torch.default_generator.get_state()
            q = torch.empty(100)
            g1_normal = q.normal_()
            g2 = torch.Generator()
            g2.set_state(default_state)
            g2_normal = q.normal_(generator=g2)
            self.assertEqual(g1_normal, g2_normal)

        def test_invalid_generator_raises(self):
            self.assertRaises(RuntimeError, lambda: torch.Generator('opengl'))

        def _sobol_reference_samples(self, scramble: bool) -> torch.Tensor:
            if not scramble:
                # theoretical values from Joe Kuo 2010
                return torch.tensor(
                    [
                        [0., 0.],
                        [0.5, 0.5],
                        [0.75, 0.25],
                        [0.25, 0.75],
                        [0.375, 0.375],
                        [0.875, 0.875],
                        [0.625, 0.125],
                        [0.125, 0.625],
                    ],
                )
            else:
                # theoretical values unknown: convergence properties checked
                return torch.tensor(
                    [
                        [0.50860737, 0.29320504],
                        [0.07116939, 0.89594537],
                        [0.49354145, 0.11524881],
                        [0.93097717, 0.70244044],
                        [0.87266153, 0.23887917],
                        [0.31021884, 0.57600391],
                        [0.13687253, 0.42054182],
                        [0.69931293, 0.77336788],
                    ],
                )

        def test_sobolengine_bounds(self, scramble: bool = False):
            engine = torch.quasirandom.SobolEngine(100, scramble=scramble, seed=123456)
            sample = engine.draw(512)
            self.assertTrue(torch.all(sample >= 0))
            self.assertTrue(torch.all(sample <= 1))

        def test_sobolengine_bounds_scrambled(self):
            self.test_sobolengine_bounds(scramble=True)

        def test_sobolengine_draw(self, scramble: bool = False):
            ref_sample = self._sobol_reference_samples(scramble=scramble)
            engine = torch.quasirandom.SobolEngine(2, scramble=scramble, seed=123456)
            sample = engine.draw(n=len(ref_sample))
            self.assertEqual(sample, ref_sample)
            self.assertEqual(engine.num_generated, len(ref_sample))

        def test_sobolengine_draw_scrambled(self):
            self.test_sobolengine_draw(scramble=True)

        def test_sobolengine_first_point(self):
            for dtype in (torch.float, torch.double):
                engine = torch.quasirandom.SobolEngine(2, scramble=False)
                sample = engine.draw(1, dtype=dtype)
                self.assertTrue(torch.all(sample == 0))
                self.assertEqual(sample.dtype, dtype)
            for dtype in (torch.float, torch.double):
                engine = torch.quasirandom.SobolEngine(2, scramble=True, seed=123456)
                sample = engine.draw(1, dtype=dtype)
                self.assertTrue(torch.all(sample != 0))
                self.assertEqual(sample.dtype, dtype)

        def test_sobolengine_continuing(self, scramble: bool = False):
            ref_sample = self._sobol_reference_samples(scramble=scramble)
            engine = torch.quasirandom.SobolEngine(2, scramble=scramble, seed=123456)
            n_half = len(ref_sample) // 2
            _ = engine.draw(n=n_half)
            sample = engine.draw(n=n_half)
            torch.testing.assert_close(sample, ref_sample[n_half:])

        def test_sobolengine_continuing_scrambled(self):
            self.test_sobolengine_continuing(scramble=True)

        def test_sobolengine_reset(self, scramble: bool = False):
            ref_sample = self._sobol_reference_samples(scramble=scramble)
            engine = torch.quasirandom.SobolEngine(2, scramble=scramble, seed=123456)
            _ = engine.draw(n=len(ref_sample) // 2)
            engine.reset()
            self.assertEqual(engine.num_generated, 0)
            sample = engine.draw(n=len(ref_sample))
            torch.testing.assert_close(sample, ref_sample)

        def test_sobolengine_reset_scrambled(self):
            self.test_sobolengine_reset(scramble=True)

        def test_sobolengine_fast_forward(self, scramble: bool = False):
            ref_sample = self._sobol_reference_samples(scramble=scramble)
            engine = torch.quasirandom.SobolEngine(2, scramble=scramble, seed=123456)
            engine.fast_forward(4)
            sample = engine.draw(n=4)
            torch.testing.assert_close(sample, ref_sample[4:])
            # alternate fast forwarding with sampling
            engine.reset()
            even_draws = []
            for i in range(8):
                if i % 2 == 0:
                    even_draws.append(engine.draw())
                else:
                    engine.fast_forward(1)
            torch.testing.assert_close(
                ref_sample[[i for i in range(8) if i % 2 == 0]],
                torch.from_numpy(np.concatenate(even_draws)),
            )

        def test_sobolengine_fast_forward_scrambled(self):
            self.test_sobolengine_fast_forward(scramble=True)

        def test_sobolengine_distribution(self, scramble=False):
            d = 50
            engine = torch.quasirandom.SobolEngine(d, scramble=scramble, seed=123456)
            sample = engine.draw(1024)
            torch.testing.assert_close(
                torch.mean(sample, dim=0), torch.full((d,), 0.5), atol=2, rtol=2
            )
            torch.testing.assert_close(
                np.percentile(sample, 25, axis=0), np.repeat(0.25, d), atol=2, rtol=2
            )
            torch.testing.assert_close(
                np.percentile(sample, 75, axis=0), np.repeat(0.75, d), atol=2, rtol=2
            )

        def test_sobolengine_distribution_scrambled(self):
            self.test_sobolengine_distribution(scramble=True)

        def test_sobolengine_draw_base2(self, scramble=False):
            ref_sample = self._sobol_reference_samples(scramble=scramble)
            engine = torch.quasirandom.SobolEngine(2, scramble=scramble, seed=123456)
            sample = engine.draw_base2(2)
            self.assertEqual(ref_sample[:4], sample)
            # resampling still having N=2**n
            sample = engine.draw_base2(2)
            self.assertEqual(ref_sample[4:8], sample)

        def test_sobolengine_draw_base2_scrambled(self):
            self.test_sobolengine_draw_base2(scramble=True)

        def test_sobolengine_raise(self):
            maxdim = torch.quasirandom.SobolEngine.MAXDIM
            with self.assertRaises(ValueError):
                torch.quasirandom.SobolEngine(maxdim + 1)

        def test_sobolengine_high_dim(self):
            engine = torch.quasirandom.SobolEngine(1111, scramble=False, seed=123456)
            samples1 = engine.draw()
            vals1, counts1 = torch.unique(samples1, return_counts=True)
            samples2 = engine.draw()
            vals2, counts2 = torch.unique(samples2, return_counts=True)
            self.assertEqual(vals1.item(), 0.0)
            self.assertEqual(counts1.item(), 1111)
            self.assertEqual(vals2.item(), 0.5)
            self.assertEqual(counts1.item(), 1111)

        def test_parsing_int64(self):
            # accepts integer arguments
            x = torch.cumsum(torch.ones(5, 5), 0)
            self.assertEqual(x, torch.cumsum(torch.ones(5, 5), torch.tensor(0)))
            # doesn't accept floating point variables
            self.assertRaises(TypeError, lambda: torch.cumsum(torch.ones(5, 5), torch.tensor(0.)))

        def test_parsing_double(self):
            # accepts floating point and integer arguments
            x = torch.randn(2, 3)
            torch.isclose(x, x, 1, 1)
            self.assertTrue(torch.isclose(x, x, 1, 1).all())
            self.assertTrue(torch.isclose(x, x, 1.5, 1.).all())
            # accepts floating point and integer tensors
            self.assertTrue(torch.isclose(x, x, torch.tensor(1), torch.tensor(1)).all())
            self.assertTrue(torch.isclose(x, x, torch.tensor(1.5), torch.tensor(1.)).all())
            # doesn't accept variables with requires_grad
            self.assertRaises(TypeError,
                              lambda: torch.isclose(x, x, torch.tensor(1.5), torch.tensor(1., requires_grad=True)).all())

        def test_parsing_intlist(self):
            #  parse with integer variables
            self.assertEqual(torch.Size([3, 4]), torch.ones((torch.tensor(3), torch.tensor(4))).shape)
            self.assertEqual(torch.Size([3, 4]), torch.ones(torch.tensor(3), torch.tensor(4)).shape)
            # parse with numpy integers
            self.assertEqual(torch.Size([3, 4]), torch.ones((np.array(3), np.int64(4))).shape)
            self.assertEqual(torch.Size([3, 4]), torch.ones(np.array(3), np.int64(4)).shape)
            self.assertEqual(torch.Size([3, 4]), torch.ones((np.int64(3), np.array(4))).shape)
            self.assertEqual(torch.Size([3, 4]), torch.ones(np.int64(3), np.array(4)).shape)

            # fail parse with float variables
            self.assertRaises(TypeError, lambda: torch.ones((torch.tensor(3.), torch.tensor(4))))
            # fail parse with numpy floats
            self.assertRaises(TypeError, lambda: torch.ones((np.float(3.), torch.tensor(4))))
            self.assertRaises(TypeError, lambda: torch.ones((np.array(3.), torch.tensor(4))))

            # fail parse with > 1 element variables
            self.assertRaises(TypeError, lambda: torch.ones(torch.tensor(3, 3)))
            self.assertRaises(TypeError, lambda: torch.ones((torch.tensor(3, 3))))
            self.assertRaises(TypeError, lambda: torch.ones(np.array(3, 3)))
            self.assertRaises(TypeError, lambda: torch.ones((np.array(3, 3))))

            # fail parse with additional positional args after intlist arg
            self.assertRaisesRegex(TypeError,
                                   "received an invalid combination of arguments",
                                   lambda: torch.LongTensor((6, 0), 1, 1, 0))
            self.assertRaisesRegex(TypeError,
                                   "missing 1 required positional arguments",
                                   lambda: torch.tensor().new_zeros((5, 5), 0))

        def test_half_tensor(self):
            devices = ["cpu"]
            if torch.cuda.is_available():
                devices.append("cuda")

            # contiguous tensor
            # non-contiguous tensor
            # dense non-overlapping tensor
            # non-dense non-overlapping sliced tensor
            # non-dense overlapping equal strides
            for device in devices:
                tset = (
                    torch.randn(4, 3, 2, device=device, dtype=torch.float).contiguous(),
                    torch.randn(4, 3, 2, device=device, dtype=torch.float).transpose(0, 1),
                    torch.randn(4, 3, 2, device=device, dtype=torch.float),
                    torch.randn(4, 3, 2, device=device, dtype=torch.float)[:, :, ::2],
                    torch.empty_strided(
                        (4, 2, 3), (10, 3, 3), device=device, dtype=torch.float
                    ).copy_(torch.rand((4, 2, 3), dtype=torch.float, device=device)),
                )

                for x in tset:
                    self.assertEqual(x.half().float(), x, atol=1e-3, rtol=0)
                    xh = x.half()
                    with tempfile.NamedTemporaryFile() as f:
                        torch.save(xh, f)
                        f.seek(0)
                        xh2 = torch.load(f)
                        self.assertEqual(xh.float(), xh2.float())

        def test_from_buffer(self):
            a = bytearray([1, 2, 3, 4])
            self.assertEqual(torch.ByteStorage.from_buffer(a).tolist(), [1, 2, 3, 4])
            shorts = torch.ShortStorage.from_buffer(a, 'big')
            self.assertEqual(shorts.size(), 2)
            self.assertEqual(shorts.tolist(), [258, 772])
            ints = torch.IntStorage.from_buffer(a, 'little')
            self.assertEqual(ints.size(), 1)
            self.assertEqual(ints[0], 67305985)
            f = bytearray([0x40, 0x10, 0x00, 0x00])
            floats = torch.FloatStorage.from_buffer(f, 'big')
            self.assertEqual(floats.size(), 1)
            self.assertEqual(floats[0], 2.25)

            f = bytearray([0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x10, 0x40])
            bools = torch.BoolStorage.from_buffer(f, 'big')
            self.assertEqual(bools.size(), 8)
            self.assertEqual(bools.tolist(), [False, True, True, True, True, True, True, True])
            self.assertEqual(bools.type(), 'torch.BoolStorage')

            f = bytearray(b'\x80\x02\x8a\nl\xfc\x9cF\xf9 j\xa8P\x19.\x80\x02M\xe9')
            bools = torch.BoolStorage.from_buffer(f, 'big')
            self.assertEqual(bools.size(), 19)

            f = bytearray(b'\0x4A')
            bools = torch.BoolStorage.from_buffer(f, 'big')
            self.assertEqual(bools.size(), 4)
            self.assertEqual(bools.tolist(), [False, True, True, True])
            bytes = torch.ByteStorage.from_buffer(a)
            self.assertEqual(bytes.nbytes(), 4)
            self.assertEqual(bytes.tolist(), [1, 2, 3, 4])

        def test_storage_casts(self):
            storage = torch.IntStorage([-1, 0, 1, 2, 3, 4])
            self.assertEqual(storage.size(), 6)
            self.assertEqual(storage.tolist(), [-1, 0, 1, 2, 3, 4])
            self.assertEqual(storage.type(), 'torch.IntStorage')
            self.assertIs(storage.dtype, torch.int32)

            floatStorage = storage.float()
            self.assertEqual(floatStorage.size(), 6)
            self.assertEqual(floatStorage.tolist(), [-1, 0, 1, 2, 3, 4])
            self.assertEqual(floatStorage.type(), 'torch.FloatStorage')
            self.assertEqual(floatStorage.int().tolist(), [-1, 0, 1, 2, 3, 4])
            self.assertIs(floatStorage.dtype, torch.float32)

            halfStorage = storage.half()
            self.assertEqual(halfStorage.size(), 6)
            self.assertEqual(halfStorage.tolist(), [-1, 0, 1, 2, 3, 4])
            self.assertEqual(halfStorage.type(), 'torch.HalfStorage')
            self.assertEqual(halfStorage.int().tolist(), [-1, 0, 1, 2, 3, 4])
            self.assertIs(halfStorage.dtype, torch.float16)

            bfloat16Storage = storage.bfloat16()
            self.assertEqual(bfloat16Storage.size(), 6)
            self.assertEqual(bfloat16Storage.tolist(), [-1, 0, 1, 2, 3, 4])
            self.assertEqual(bfloat16Storage.type(), 'torch.BFloat16Storage')
            self.assertEqual(bfloat16Storage.int().tolist(), [-1, 0, 1, 2, 3, 4])
            self.assertIs(bfloat16Storage.dtype, torch.bfloat16)

            longStorage = storage.long()
            self.assertEqual(longStorage.size(), 6)
            self.assertEqual(longStorage.tolist(), [-1, 0, 1, 2, 3, 4])
            self.assertEqual(longStorage.type(), 'torch.LongStorage')
            self.assertEqual(longStorage.int().tolist(), [-1, 0, 1, 2, 3, 4])
            self.assertIs(longStorage.dtype, torch.int64)

            shortStorage = storage.short()
            self.assertEqual(shortStorage.size(), 6)
            self.assertEqual(shortStorage.tolist(), [-1, 0, 1, 2, 3, 4])
            self.assertEqual(shortStorage.type(), 'torch.ShortStorage')
            self.assertEqual(shortStorage.int().tolist(), [-1, 0, 1, 2, 3, 4])
            self.assertIs(shortStorage.dtype, torch.int16)

            doubleStorage = storage.double()
            self.assertEqual(doubleStorage.size(), 6)
            self.assertEqual(doubleStorage.tolist(), [-1.0, 0.0, 1.0, 2.0, 3.0, 4.0])
            self.assertEqual(doubleStorage.type(), 'torch.DoubleStorage')
            self.assertEqual(doubleStorage.int().tolist(), [-1, 0, 1, 2, 3, 4])
            self.assertIs(doubleStorage.dtype, torch.float64)

            charStorage = storage.char()
            self.assertEqual(charStorage.size(), 6)
            self.assertEqual(charStorage.tolist(), [-1.0, 0.0, 1.0, 2.0, 3.0, 4.0])
            self.assertEqual(charStorage.type(), 'torch.CharStorage')
            self.assertEqual(charStorage.int().tolist(), [-1, 0, 1, 2, 3, 4])
            self.assertIs(charStorage.dtype, torch.int8)

            byteStorage = storage.byte()
            self.assertEqual(byteStorage.size(), 6)
            self.assertEqual(byteStorage.tolist(), [255, 0, 1, 2, 3, 4])
            self.assertEqual(byteStorage.type(), 'torch.ByteStorage')
            self.assertEqual(byteStorage.int().tolist(), [255, 0, 1, 2, 3, 4])
            self.assertIs(byteStorage.dtype, torch.uint8)

            boolStorage = storage.bool()
            self.assertEqual(boolStorage.size(), 6)
            self.assertEqual(boolStorage.tolist(), [True, False, True, True, True, True])
            self.assertEqual(boolStorage.type(), 'torch.BoolStorage')
            self.assertEqual(boolStorage.int().tolist(), [1, 0, 1, 1, 1, 1])
            self.assertIs(boolStorage.dtype, torch.bool)

            complexfloat_storage = torch.ComplexFloatStorage([-1, 0, 1 + 2j, 2.5j, 3.5, 4 - 2j])
            self.assertEqual(complexfloat_storage.size(), 6)
            self.assertEqual(complexfloat_storage.tolist(), [-1, 0, 1 + 2j, 2.5j, 3.5, 4 - 2j])
            self.assertEqual(complexfloat_storage.type(), 'torch.ComplexFloatStorage')
            self.assertIs(complexfloat_storage.dtype, torch.complex64)

            complexdouble_storage = complexfloat_storage.complex_double()
            self.assertEqual(complexdouble_storage.size(), 6)
            self.assertEqual(complexdouble_storage.tolist(), [-1, 0, 1 + 2j, 2.5j, 3.5, 4 - 2j])
            self.assertEqual(complexdouble_storage.type(), 'torch.ComplexDoubleStorage')
            self.assertIs(complexdouble_storage.dtype, torch.complex128)

        def test_from_file(self):
            def assert_with_filename(filename):
                size = 10000
                s1 = torch.FloatStorage.from_file(filename, True, size)
                t1 = torch.FloatTensor(s1).copy_(torch.randn(size))
                self.assertEqual(s1.data_ptr(), torch.FloatTensor(s1).data_ptr())

                # check mapping
                s2 = torch.FloatStorage.from_file(filename, True, size)
                t2 = torch.FloatTensor(s2)
                self.assertEqual(t1, t2, atol=0, rtol=0)

                # check changes to t1 from t2
                rnum = random.uniform(-1, 1)
                t1.fill_(rnum)
                self.assertEqual(t1, t2, atol=0, rtol=0)

                # check changes to t2 from t1
                rnum = random.uniform(-1, 1)
                t2.fill_(rnum)
                self.assertEqual(t1, t2, atol=0, rtol=0)

                # release the tensors
                del s1, t1, s2, t2

            with TemporaryFileName() as fname:
                assert_with_filename(fname)

            if IS_FILESYSTEM_UTF8_ENCODING:
                with TemporaryDirectoryName(suffix='中文') as dname, TemporaryFileName(dir=dname) as fname:
                    assert_with_filename(fname)

        def test_torch_from_file(self):
            def assert_with_filename(filename):
                size = 10000
                s1 = torch.from_file(filename, True, size, dtype=torch.float)
                t1 = torch.FloatTensor(s1).copy_(torch.randn(size))

                # check mapping
                s2 = torch.from_file(filename, True, size, dtype=torch.float)
                t2 = torch.FloatTensor(s2)
                self.assertEqual(t1, t2, atol=0, rtol=0)

                # check changes to t1 from t2
                rnum = random.uniform(-1, 1)
                t1.fill_(rnum)
                self.assertEqual(t1, t2, atol=0, rtol=0)

                # check changes to t2 from t1
                rnum = random.uniform(-1, 1)
                t2.fill_(rnum)
                self.assertEqual(t1, t2, atol=0, rtol=0)

                # release the tensors
                del s1, t1, s2, t2

            with TemporaryFileName() as fname:
                assert_with_filename(fname)

            if IS_FILESYSTEM_UTF8_ENCODING:
                with TemporaryDirectoryName(suffix='中文') as dname, TemporaryFileName(dir=dname) as fname:
                    assert_with_filename(fname)

        def test_print(self):
            default_type = torch.tensor([]).type()
            for t in torch._tensor_classes:
                if t == torch.HalfTensor:
                    continue  # HalfTensor does not support fill
                if t.is_sparse:
                    continue
                if t.is_cuda and not torch.cuda.is_available():
                    continue
                obj = t(100, 100).fill_(1)
                obj.__repr__()
                str(obj)
            # test half tensor
            obj = torch.rand(100, 100, device='cpu').half()
            obj.__repr__()
            str(obj)
            for t in torch._storage_classes:
                if t == torch.BFloat16Storage:
                    continue  # Fix once fill is enabled for bfloat16
                if t.is_cuda and not torch.cuda.is_available():
                    continue
                if t == torch.BoolStorage or t == torch.cuda.BoolStorage:
                    obj = t(100).fill_(True)
                else:
                    obj = t(100).fill_(1)
                obj.__repr__()
                str(obj)

            # test complex tensor
            # complex tensor print uses two formatters, one for real values
            # and the other for imag values. this is consistent with numpy
            x = torch.tensor([2.3 + 4j, 7 + 6j])
            self.assertEqual(x.__repr__(), str(x))
            self.assertExpectedInline(str(x), '''tensor([2.3000+4.j, 7.0000+6.j])''')

            # test scientific notation for complex tensors
            x = torch.tensor([1e28 + 2j , -1e-28j])
            self.assertEqual(x.__repr__(), str(x))
            self.assertExpectedInline(str(x), '''tensor([1.0000e+28+2.0000e+00j, -0.0000e+00-1.0000e-28j])''')

            # test big integer
            x = torch.tensor(2341234123412341)
            self.assertEqual(x.__repr__(), str(x))
            self.assertExpectedInline(str(x), '''tensor(2341234123412341)''')

            # test scientific notation
            x = torch.tensor([1e28, 1e-28])
            self.assertEqual(x.__repr__(), str(x))
            self.assertExpectedInline(str(x), '''tensor([1.0000e+28, 1.0000e-28])''')

            # test scientific notation using set_printoptions
            x = torch.tensor([1e2, 1e-2])
            torch.set_printoptions(sci_mode=True)
            self.assertEqual(x.__repr__(), str(x))
            self.assertExpectedInline(str(x), '''tensor([1.0000e+02, 1.0000e-02])''')
            torch.set_printoptions(sci_mode=False)
            self.assertEqual(x.__repr__(), str(x))
            self.assertExpectedInline(str(x), '''tensor([  100.0000,     0.0100])''')
            torch.set_printoptions(sci_mode=None)  # reset to the default value

            # test no leading space if all elements positive
            x = torch.tensor([1, 2])
            self.assertEqual(x.__repr__(), str(x))
            self.assertExpectedInline(str(x), '''tensor([1, 2])''')

            # test for leading space if there are negative elements
            x = torch.tensor([1, -2])
            self.assertEqual(x.__repr__(), str(x))
            self.assertExpectedInline(str(x), '''tensor([ 1, -2])''')

            # test inf and nan
            x = torch.tensor([4, inf, 1.5, -inf, 0, nan, 1])
            self.assertEqual(x.__repr__(), str(x))
            self.assertExpectedInline(str(x), '''tensor([4.0000,    inf, 1.5000,   -inf, 0.0000,    nan, 1.0000])''')

            y = torch.tensor([4, inf, complex(1.5, inf), complex(-inf, 4), 0, complex(nan, inf), complex(3, nan)])
            self.assertEqual(y.__repr__(), str(y))
            expected_str = '''\
tensor([4.0000+0.j,    inf+0.j, 1.5000+infj,   -inf+4.j, 0.0000+0.j,    nan+infj,
        3.0000+nanj])'''
            self.assertExpectedInline(str(y), expected_str)

            # test dtype
            torch.set_default_dtype(torch.float)
            x = torch.tensor([1e-324, 1e-323, 1e-322, 1e307, 1e308, 1e309], dtype=torch.float64)
            self.assertEqual(x.__repr__(), str(x))
            expected_str = '''\
tensor([ 0.0000e+00, 9.8813e-324, 9.8813e-323, 1.0000e+307, 1.0000e+308,
                inf], dtype=torch.float64)'''
            self.assertExpectedInline(str(x), expected_str)

            # test changing default dtype
            torch.set_default_dtype(torch.float64)
            self.assertEqual(x.__repr__(), str(x))
            expected_str = '''\
tensor([ 0.0000e+00, 9.8813e-324, 9.8813e-323, 1.0000e+307, 1.0000e+308,
                inf])'''
            self.assertExpectedInline(str(x), expected_str)

            # test summary
            x = torch.zeros(10000)
            self.assertEqual(x.__repr__(), str(x))
            self.assertExpectedInline(str(x), '''tensor([0., 0., 0.,  ..., 0., 0., 0.])''')

            # test internal summary function
            x = torch.rand(1, 20, 5, 30)
            summary = torch._tensor_str.get_summarized_data(x)
            self.assertEqual(summary.shape, (1, 6, 5, 6))
            first_and_last = [0, 1, 2, -3, -2, -1]
            self.assertEqual(summary, x[:, first_and_last][..., first_and_last])

            # test device
            if torch.cuda.is_available():
                x = torch.tensor([123], device='cuda:0')
                self.assertEqual(x.__repr__(), str(x))
                self.assertExpectedInline(str(x), '''tensor([123], device='cuda:0')''')

                # test changing default to cuda
                torch.set_default_tensor_type(torch.cuda.FloatTensor)
                self.assertEqual(x.__repr__(), str(x))
                self.assertExpectedInline(str(x), '''tensor([123])''')

                # test printing a tensor on a different gpu than current one.
                if torch.cuda.device_count() >= 2:
                    with torch.cuda.device(1):
                        self.assertEqual(x.__repr__(), str(x))
                        self.assertExpectedInline(str(x), '''tensor([123], device='cuda:0')''')

                # test printing cpu tensor when default device is cuda
                y = torch.tensor([123], device='cpu')
                self.assertEqual(y.__repr__(), str(y))
                self.assertExpectedInline(str(y), '''tensor([123], device='cpu')''')
            torch.set_default_tensor_type(default_type)


            # test integral floats and requires_grad
            x = torch.tensor([123.], requires_grad=True)
            self.assertEqual(x.__repr__(), str(x))
            self.assertExpectedInline(str(x), '''tensor([123.], requires_grad=True)''')

            # test non-contiguous print
            # sliced tensor should have > PRINT_OPTS.threshold elements
            x = torch.ones(100, 2, 2, 10)
            y = x.as_strided(size=(100, 2, 10), stride=(2 * 2 * 10, 2 * 10, 1))
            self.assertEqual(str(y), y.__repr__())
            expected_str = '''\
tensor([[[1., 1., 1.,  ..., 1., 1., 1.],
         [1., 1., 1.,  ..., 1., 1., 1.]],

        [[1., 1., 1.,  ..., 1., 1., 1.],
         [1., 1., 1.,  ..., 1., 1., 1.]],

        [[1., 1., 1.,  ..., 1., 1., 1.],
         [1., 1., 1.,  ..., 1., 1., 1.]],

        ...,

        [[1., 1., 1.,  ..., 1., 1., 1.],
         [1., 1., 1.,  ..., 1., 1., 1.]],

        [[1., 1., 1.,  ..., 1., 1., 1.],
         [1., 1., 1.,  ..., 1., 1., 1.]],

        [[1., 1., 1.,  ..., 1., 1., 1.],
         [1., 1., 1.,  ..., 1., 1., 1.]]])\
'''

            self.assertExpectedInline(str(y), expected_str)

            x = torch.ones(100, 2, 2, 10) * (1 + 1j)
            y = x.as_strided(size=(100, 2, 10), stride=(2 * 2 * 10, 2 * 10, 1))
            self.assertEqual(str(y), y.__repr__())
            expected_str = '''\
tensor([[[1.+1.j, 1.+1.j, 1.+1.j,  ..., 1.+1.j, 1.+1.j, 1.+1.j],
         [1.+1.j, 1.+1.j, 1.+1.j,  ..., 1.+1.j, 1.+1.j, 1.+1.j]],

        [[1.+1.j, 1.+1.j, 1.+1.j,  ..., 1.+1.j, 1.+1.j, 1.+1.j],
         [1.+1.j, 1.+1.j, 1.+1.j,  ..., 1.+1.j, 1.+1.j, 1.+1.j]],

        [[1.+1.j, 1.+1.j, 1.+1.j,  ..., 1.+1.j, 1.+1.j, 1.+1.j],
         [1.+1.j, 1.+1.j, 1.+1.j,  ..., 1.+1.j, 1.+1.j, 1.+1.j]],

        ...,

        [[1.+1.j, 1.+1.j, 1.+1.j,  ..., 1.+1.j, 1.+1.j, 1.+1.j],
         [1.+1.j, 1.+1.j, 1.+1.j,  ..., 1.+1.j, 1.+1.j, 1.+1.j]],

        [[1.+1.j, 1.+1.j, 1.+1.j,  ..., 1.+1.j, 1.+1.j, 1.+1.j],
         [1.+1.j, 1.+1.j, 1.+1.j,  ..., 1.+1.j, 1.+1.j, 1.+1.j]],

        [[1.+1.j, 1.+1.j, 1.+1.j,  ..., 1.+1.j, 1.+1.j, 1.+1.j],
         [1.+1.j, 1.+1.j, 1.+1.j,  ..., 1.+1.j, 1.+1.j, 1.+1.j]]])\
'''
            self.assertExpectedInline(str(y), expected_str)

            # test print 0-dim tensor: there's no 0-dim in Numpy, we match arrayprint style
            x = torch.tensor(0.00002)
            self.assertEqual(x.__repr__(), str(x))
            self.assertExpectedInline(str(x), '''tensor(2.0000e-05)''')

            # test print boolean tensor
            x = torch.tensor([True])
            self.assertEqual(x.__repr__(), str(x))
            self.assertExpectedInline(str(x), '''tensor([True])''')

            x = torch.tensor(True)
            self.assertEqual(x.__repr__(), str(x))
            self.assertExpectedInline(str(x), '''tensor(True)''')

            # [Numpy] test print float in sci_mode when min < 0.0001.
            x = torch.tensor([0.00002])
            self.assertEqual(x.__repr__(), str(x))
            self.assertExpectedInline(str(x), '''tensor([2.0000e-05])''')

            # [Numpy] test print complex in sci_mode when real_min < 0.0001 and (or) imag_min < 0.0001.
            x = torch.tensor([0.00002]) * (1 + 1j)
            self.assertEqual(x.__repr__(), str(x))
            self.assertExpectedInline(str(x), '''tensor([2.0000e-05+2.0000e-05j])''')

            # [Numpy] test print float in sci_mode when max > 1e8.
            # TODO: Pytorch uses fixed precision to print, while Numpy uses dragon4_scientific
            # to do automatic trimming and padding.
            x = torch.tensor([123456789.])
            self.assertEqual(x.__repr__(), str(x))
            self.assertExpectedInline(str(x), '''tensor([1.2346e+08])''')

            # [Numpy] test print float in sci_mode when max / min > 1000.
            x = torch.tensor([0.01, 11])
            self.assertEqual(x.__repr__(), str(x))
            self.assertExpectedInline(str(x), '''tensor([1.0000e-02, 1.1000e+01])''')

            # [Numpy] test print int max / min > 1000, no sci_mode
            x = torch.tensor([1, 1010])
            self.assertEqual(x.__repr__(), str(x))
            self.assertExpectedInline(str(x), '''tensor([   1, 1010])''')

            # [Numpy] test print int > 1e8, no sci_mode
            x = torch.tensor([1000000000])  # 1e9
            self.assertEqual(x.__repr__(), str(x))
            self.assertExpectedInline(str(x), '''tensor([1000000000])''')

            # [Numpy] test printing float in int_mode
            x = torch.tensor([1., 1000.])
            self.assertEqual(x.__repr__(), str(x))
            self.assertExpectedInline(str(x), '''tensor([   1., 1000.])''')

            # [Numpy] test printing float in int_mode in sci format when max / min > 1000.
            x = torch.tensor([1., 1010.])
            self.assertEqual(x.__repr__(), str(x))
            self.assertExpectedInline(str(x), '''tensor([1.0000e+00, 1.0100e+03])''')

        def test_sizeof(self) -> None:
            sizeof_empty = torch.randn(0).storage().__sizeof__()
            sizeof_10 = torch.randn(10).storage().__sizeof__()
            sizeof_100 = torch.randn(100).storage().__sizeof__()
            self.assertEqual((sizeof_100 - sizeof_empty) // (sizeof_10 - sizeof_empty), 10)
            self.assertEqual((sizeof_100 - sizeof_empty) % (sizeof_10 - sizeof_empty), 0)

            sizeof_empty = torch.randn(0).to(torch.uint8).storage().__sizeof__()
            sizeof_10 = torch.randn(10).to(torch.uint8).storage().__sizeof__()
            sizeof_100 = torch.randn(100).to(torch.uint8).storage().__sizeof__()
            self.assertEqual((sizeof_100 - sizeof_empty) // (sizeof_10 - sizeof_empty), 10)
            self.assertEqual((sizeof_100 - sizeof_empty) % (sizeof_10 - sizeof_empty), 0)

        def test_iter(self) -> None:
            x = torch.randn(5, 5)
            for i, sub in enumerate(x):
                self.assertEqual(sub, x[i])

            x = torch.tensor([])
            self.assertEqual(list(x), [])

        def test_assertEqual(self) -> None:
            x = torch.FloatTensor([0])
            self.assertEqual(x, 0)
            xv = torch.autograd.Variable(x)
            self.assertEqual(xv, 0)
            self.assertEqual(x, xv)
            self.assertEqual(xv, x)

            # Tests that setting atol or rtol without the other throws
            self.assertRaises(AssertionError,
                              lambda: self.assertEqual(x, xv, atol=4))
            self.assertRaises(AssertionError,
                              lambda: self.assertEqual(x, xv, rtol=4))

            self.assertRaisesRegex(TypeError, "takes from 3 to 4 positional arguments",
                                   lambda: self.assertEqual(x, xv, "", 1.0))  # type: ignore[misc]

        def test_new(self) -> None:
            x = torch.autograd.Variable(torch.tensor([]))
            y = torch.autograd.Variable(torch.randn(4, 4))
            z = torch.autograd.Variable(torch.IntTensor([1, 2, 3]))
            self.assertEqual(x.new().shape, [0])
            self.assertEqual(x.new(), x)
            self.assertEqual(x.new(1, 2).shape, [1, 2])
            self.assertEqual(x.new(torch.Size([3, 4])).shape, [3, 4])
            self.assertEqual(x.new([3, 4]).shape, [2])
            self.assertEqual(x.new([3, 4]).tolist(), [3, 4])
            self.assertEqual(x.new((3, 4)).tolist(), [3, 4])
            self.assertEqual(x.new([np.int32(3), np.float64(4)]).tolist(), [3, 4])
            self.assertEqual(x.new(np.array((3, 4))).tolist(), [3, 4])
            self.assertEqual(x.new([z[2], z[0] + 3]).tolist(), [3, 4])
            self.assertEqual(x.new(size=(3, 4)).shape, [3, 4])
            self.assertEqual(x.new(()).shape, [0])
            self.assertEqual(x.new(y.storage()).data_ptr(), y.data_ptr())
            self.assertEqual(x.new(y).data_ptr(), y.data_ptr())
            self.assertIsNot(x.new(y), y)

            self.assertRaises(TypeError, lambda: x.new(z))
            # TypeError would be better
            self.assertRaises(RuntimeError, lambda: x.new(z.storage()))

        @unittest.skipIf(PYTORCH_CUDA_MEMCHECK, "is_pinned uses failure to detect pointer property")
        def test_pin_memory(self):
            x = torch.randn(3, 5)
            self.assertFalse(x.is_pinned())
            if not torch.cuda.is_available():
                self.assertRaises(RuntimeError, lambda: x.pin_memory())
            else:
                pinned = x.pin_memory()
                self.assertTrue(pinned.is_pinned())
                self.assertEqual(pinned, x)
                self.assertNotEqual(pinned.data_ptr(), x.data_ptr())
                # test that pin_memory on already pinned tensor has no effect
                self.assertIs(pinned, pinned.pin_memory())
                self.assertEqual(pinned.data_ptr(), pinned.pin_memory().data_ptr())

        def test_error_msg_type_translation(self):
            with self.assertRaisesRegex(
                    RuntimeError,
                    # message includes both Double and Long
                    '(?=.*Double)(?=.*Long)'):

                # Calls model with a LongTensor input but DoubleTensor weights
                input = torch.zeros(1, 1, 1, 6, dtype=torch.long)
                weight = torch.nn.Parameter(torch.zeros(1, 1, 1, 3, dtype=torch.double))
                model = torch.nn.Conv2d(1, 1, (1, 3), stride=1, padding=0, bias=False)
                model.weight = weight
                out = model(input)

        def test_apply(self):
            x = torch.arange(1, 6)
            res = x.clone().apply_(lambda k: k + k)
            self.assertEqual(res, x * 2)
            self.assertRaises(TypeError, lambda: x.apply_(lambda k: "str"))

        def test_map(self):
            x = torch.autograd.Variable(torch.randn(3, 3))
            y = torch.autograd.Variable(torch.randn(3))
            res = x.clone()
            res.map_(y, lambda a, b: a + b)
            self.assertEqual(res, x + y)
            self.assertRaisesRegex(TypeError, "not callable", lambda: res.map_(y, "str"))

        def test_map2(self):
            x = torch.autograd.Variable(torch.randn(3, 3))
            y = torch.autograd.Variable(torch.randn(3))
            z = torch.autograd.Variable(torch.randn(1, 3))
            res = x.clone()
            res.map2_(y, z, lambda a, b, c: a + b * c)
            self.assertEqual(res, x + y * z)
            z.requires_grad = True
            self.assertRaisesRegex(
                RuntimeError, "requires grad",
                lambda: res.map2_(y, z, lambda a, b, c: a + b * c))

        def test_Size(self):
            x = torch.Size([1, 2, 3])
            self.assertIsInstance(x, tuple)
            self.assertEqual(x[0], 1)
            self.assertEqual(x[1], 2)
            self.assertEqual(x[2], 3)
            self.assertEqual(len(x), 3)
            self.assertRaises(TypeError, lambda: torch.Size(torch.ones(3)))

            self.assertIsInstance(x * 2, torch.Size)
            self.assertIsInstance(x[:-1], torch.Size)
            self.assertIsInstance(x + x, torch.Size)

        def test_Size_scalar(self):
            three = torch.tensor(3)
            two = torch.tensor(2)
            x = torch.Size([0, 1, two, three, 4])
            for i in range(1, 5):
                self.assertEqual(x[i], i)

        def test_Size_iter(self):
            for sizes in [iter([1, 2, 3, 4, 5]), range(1, 6)]:
                x = torch.Size(sizes)
                for i in range(0, 5):
                    self.assertEqual(x[i], i + 1)

        def test_t_not_2d_error(self):
            self.assertRaises(RuntimeError, lambda: torch.randn(2, 3, 4).t())
            self.assertRaises(RuntimeError, lambda: torch.randn(2, 3, 4).t_())

        # skip this test for now as it affects all tests
        @unittest.skipIf(True, "flush_denormal not supported")
        def test_set_flush_denormal(self):
            tiny_float = 1e-42
            tiny_double = 1e-320
            float_tensor = torch.FloatTensor([1.0, tiny_float])
            double_tensor = torch.DoubleTensor([1.0, tiny_float, tiny_double])

            self.assertEqual(float_tensor[0], 1.0, atol=0.0, rtol=0)
            self.assertEqual(float_tensor[1], tiny_float, atol=tiny_float / 16, rtol=0)
            self.assertEqual(double_tensor[0], 1.0, atol=0.0, rtol=0)
            self.assertEqual(double_tensor[1], tiny_float, atol=0.0, rtol=0)
            self.assertEqual(double_tensor[2], tiny_double, atol=0.0, rtol=0)

            torch.set_flush_denormal(True)
            self.assertEqual(float_tensor[0], 1.0, atol=0.0, rtol=0)
            self.assertEqual(float_tensor[1], 0.0, atol=0.0, rtol=0)  # tiny_float to zero
            self.assertEqual(double_tensor[0], 1.0, atol=0.0, rtol=0)
            # tiny_float is not converted to zero in double type
            self.assertEqual(double_tensor[1], tiny_float, atol=0.0, rtol=0)
            self.assertEqual(double_tensor[2], 0.0, atol=0.0, rtol=0)  # tiny_double to zero
            torch.set_flush_denormal(False)

        def test_show_config(self):
            # We can't usefully test the output; just make sure this doesn't crash
            torch.__config__.show()

        @unittest.skipIf(IS_FBCODE, "CXX_FLAGS is only for OSS build.")
        def test_cxx_flags(self):
            torch.__config__._cxx_flags()

        def test_parallel_info(self):
            torch.__config__.parallel_info()

        @slowTest
        def test_slow_test(self):
            # Just a smoketest to make sure our slowTest decorator works.
            pass

        def test_is_nonzero(self):
            with self.assertRaisesRegex(RuntimeError, "Boolean value of Tensor with no values is ambiguous"):
                torch.tensor([]).is_nonzero()
            with self.assertRaisesRegex(RuntimeError, "Boolean value of Tensor with more than one value is ambiguous"):
                torch.tensor([0, 0]).is_nonzero()
            self.assertFalse(torch.tensor(0).is_nonzero())
            self.assertTrue(torch.tensor(1).is_nonzero())
            self.assertFalse(torch.tensor([0]).is_nonzero())
            self.assertTrue(torch.tensor([1]).is_nonzero())
            self.assertFalse(torch.tensor([[0]]).is_nonzero())
            self.assertTrue(torch.tensor([[1]]).is_nonzero())
            self.assertTrue(torch.tensor(0.1).is_nonzero())
            self.assertTrue(torch.tensor(-0.1).is_nonzero())
            self.assertFalse(torch.tensor(0.0).is_nonzero())
            self.assertTrue(torch.tensor(True).is_nonzero())
            self.assertFalse(torch.tensor(False).is_nonzero())
            self.assertFalse(torch.tensor(0 + 0j).is_nonzero())
            self.assertTrue(torch.tensor(0 + 0.1j).is_nonzero())

        def test_assert_async(self):
            with self.assertRaisesRegex(RuntimeError, "Boolean value of Tensor with no values is ambiguous"):
                torch._assert_async(torch.tensor([]))
            with self.assertRaisesRegex(RuntimeError, "Boolean value of Tensor with more than one value is ambiguous"):
                torch._assert_async(torch.tensor([0, 0]))
            with self.assertRaisesRegex(RuntimeError, "Expected Tensor with single nonzero value, but got zero"):
                torch._assert_async(torch.tensor(0))
            torch._assert_async(torch.tensor(1))
            torch._assert_async(torch.tensor(0.1))
            torch._assert_async(torch.tensor(-0.1))
            with self.assertRaisesRegex(RuntimeError, "Expected Tensor with single nonzero value, but got zero"):
                torch._assert_async(torch.tensor(0.0))
            torch._assert_async(torch.tensor(True))
            with self.assertRaisesRegex(RuntimeError, "Expected Tensor with single nonzero value, but got zero"):
                torch._assert_async(torch.tensor(False))
            torch._assert_async(torch.tensor(0 + 0.1j))
            with self.assertRaisesRegex(RuntimeError, "Expected Tensor with single nonzero value, but got zero"):
                torch._assert_async(torch.tensor(0 + 0j))

        # NB: we must not be built with CUDA; if we are built with CUDA but no CUDA
        # is available, we get a different error.
        @unittest.skipIf(torch.backends.cuda.is_built() or IS_SANDCASTLE, "CUDA is built, can't test CUDA not built error")
        def test_cuda_not_built(self):
            msg = "Torch not compiled with CUDA enabled"
            self.assertRaisesRegex(AssertionError, msg, lambda: torch.cuda.current_device())
            self.assertRaisesRegex(AssertionError, msg, lambda: torch.tensor([1], device="cuda"))
            self.assertRaisesRegex(AssertionError, msg, lambda: torch.tensor([1]).cuda())
            self.assertRaisesRegex(TypeError, msg, lambda: torch.cuda.FloatTensor())
            self.assertRaisesRegex(TypeError, msg, lambda: torch.set_default_tensor_type(torch.cuda.FloatTensor))
            self.assertRaisesRegex(AssertionError, msg, lambda: torch.tensor([1]).to(device="cuda"))

        def test_has_internal_overlap(self):
            OVERLAP_NO = 0
            OVERLAP_YES = 1
            OVERLAP_TOO_HARD = 2

            # Check for contiguous tensors
            a = torch.randn(3, 3)
            self.assertEqual(torch._debug_has_internal_overlap(a), OVERLAP_NO)

            # Checks for zero strides
            b = torch.randn(1, 3)
            b_expanded = b.expand(4, 3)
            self.assertEqual(torch._debug_has_internal_overlap(b_expanded), OVERLAP_YES)

            # Check for zero strided, size 1 axis, in non-contiguous storage (gh-33812)
            c = torch.randn(10).as_strided([2, 1, 5], [1, 0, 2])
            self.assertEqual(torch._debug_has_internal_overlap(c), OVERLAP_NO)
            c = torch.randn(2, 1, 10)[::2].as_strided((2, 1, 5), (10, 0, 2))
            self.assertEqual(torch._debug_has_internal_overlap(c), OVERLAP_TOO_HARD)


        def test_allow_tensor_metadata_change(self):
            def do_test(t):
                with self.assertRaisesRegex(
                        RuntimeError,
                        "set_sizes_contiguous is not allowed on a Tensor created from .data or .detach()"):
                    t.resize_((2, 1))
                with self.assertRaisesRegex(
                        RuntimeError,
                        "set_storage is not allowed on a Tensor created from .data or .detach()"):
                    t.set_()
                with self.assertRaisesRegex(
                        RuntimeError,
                        "set_storage_offset is not allowed on a Tensor created from .data or .detach()"):
                    t.set_(t.storage(), 0, t.size(), list(t.stride()))

            do_test(torch.tensor([[1, 2]]).data)
            do_test(torch.tensor([[1, 2]]).detach())

        @skipIfNotRegistered("LayerNorm", "Skipping as LayerNorm is not registered")
        def test_c10_layer_norm(self):
            # test that we can call c10 ops and they return a reasonable result
            X = torch.rand(5, 5, dtype=torch.float)
            weight = torch.rand(*X.size()[1:], dtype=torch.float)
            bias = torch.rand(*X.size()[1:], dtype=torch.float)
            epsilon = 1e-4

            expected_norm = torch.nn.functional.layer_norm(
                X, X.size()[1:], weight=weight, bias=bias, eps=epsilon)
            actual_norm, actual_mean, actual_stdev = \
                torch.ops._caffe2.LayerNorm(torch.tensor(X), torch.tensor(
                    weight), torch.tensor(bias), 1, epsilon, True)
            torch.testing.assert_close(expected_norm, actual_norm)

        def test_memory_format(self):
            def test_helper(x, memory_format):
                y = x.contiguous(memory_format=memory_format)
                self.assertFalse(y.is_contiguous())
                self.assertTrue(y.is_contiguous(memory_format=memory_format))
                self.assertEqual(y, x)

            test_helper(torch.randn(4, 3, 8, 8), torch.channels_last)
            test_helper(torch.randn(4, 3, 8, 8, 8), torch.channels_last_3d)

        def test_memory_format_contiguous_returns_same_tensor_if_already_satisfies(self):
            def test_helper(x, memory_format):
                alias = x.contiguous(memory_format=memory_format)
                alias.fill_(7)
                self.assertEqual(x, alias)

            test_helper(torch.randn(4, 8, 8, 3).permute(0, 3, 1, 2), torch.channels_last)
            test_helper(torch.randn(4, 8, 8, 8, 3).permute(0, 4, 1, 2, 3), torch.channels_last_3d)

        def test_memory_format_empty(self):
            def test_helper(dim1, dim2, memory_format):
                with self.assertRaises(RuntimeError):
                    x = torch.empty(dim1, memory_format=memory_format)
                x = torch.empty(dim2, memory_format=memory_format)
                self.assertTrue(x.is_contiguous(memory_format=memory_format))

            test_helper((3, 3), (3, 3, 3, 3), torch.channels_last)
            test_helper((3, 3, 3), (3, 3, 3, 3, 3), torch.channels_last_3d)

        def test_subclass_tensors(self):
            # raise an error when trying to subclass FloatTensor
            with self.assertRaisesRegex(TypeError, "type 'torch.FloatTensor' is not an acceptable base type"):
                class Foo1(torch.FloatTensor):
                    pass

            # but allow subclassing Tensor:
            class Foo2(torch.Tensor):
                def foo(self):
                    return 5
            f = Foo2()
            self.assertEqual(f.foo(), 5)

        def test_ndim(self):
            a = torch.randn(1, 2, 3)
            self.assertEqual(3, a.ndim)
            b = torch.randn(())
            self.assertEqual(0, b.ndim)
            c = torch.randn(1, 0)
            self.assertEqual(2, c.ndim)

        def test_fill_diagonal(self):
            a1 = torch.randn(7, 3)
            a2 = a1.clone()
            v = 1
            for i in range(3):
                a2[i][i] = v
            a1.fill_diagonal_(v)
            self.assertEqual(a1, a2)

            b1 = torch.randn(7, 3)
            b2 = b1.clone()
            for i in range(3):
                b2[i][i] = v
                b2[i + 4][i] = v
            b1.fill_diagonal_(v, wrap=True)
            self.assertEqual(b1, b2)

            c1 = torch.rand(3, 3, 3)
            c2 = c1.clone()
            for i in range(3):
                c2[i][i][i] = v
            c1.fill_diagonal_(v)
            self.assertEqual(c1, c2)

            # non-contiguous tensor
            d1 = torch.rand(3, 3, 3)[:, 1, ...]
            d2 = d1.clone()
            for i in range(3):
                d2[i][i] = v
            d1.fill_diagonal_(v)
            self.assertEqual(d1, d2)

            e1 = torch.rand(7, 3, 3)[:, 1, ...]
            e2 = e1.clone()
            for i in range(3):
                e2[i][i] = v
                e2[i + 4][i] = v
            e1.fill_diagonal_(v, wrap=True)
            self.assertEqual(e1, e2)

        def test_batch_norm_cpu_inference(self):
            # input nchw in (2,1,1,1), (2,2,2,2)
            inputs = [
                torch.tensor([[[[-0.5000]]], [[[0.5000]]]]),
                torch.tensor([
                    [
                        [[-0.5000, 0.5000], [-1.0000, 1.0000]],
                        [[-0.2500, -0.5000], [0.2500, 0.5000]]
                    ],
                    [
                        [[0.1000, 1.0000], [1.0000, 0.1000]],
                        [[1.0000, 0.5000], [1.5000, -1.5000]]
                    ]])]
            # output nchw in (2,1,1,1), (2,2,2,2)
            outputs = [
                torch.tensor([
                    [[[-0.499997496604919433593750000]]],
                    [[[0.499997496604919433593750000]]]]),
                torch.tensor([
                    [[[-0.499997496604919433593750000, 0.499997496604919433593750000],
                      [-0.999994993209838867187500000, 0.999994993209838867187500000]],
                     [[-0.249998748302459716796875000, -0.499997496604919433593750000],
                      [0.249998748302459716796875000, 0.499997496604919433593750000]]],
                    [[[0.099999502301216125488281250, 0.999994993209838867187500000],
                      [0.999994993209838867187500000, 0.099999502301216125488281250]],
                     [[0.999994993209838867187500000, 0.499997496604919433593750000],
                      [1.499992489814758300781250000, -1.499992489814758300781250000]]]])]


            for i in range(len(inputs)):
                for affine in [False, True]:
                    m = torch.nn.BatchNorm2d(inputs[i].size()[1], 1e-05, 0.1, affine=affine)
                    m.eval()
                    # contiguous case
                    input1 = inputs[i].contiguous()
                    output1 = m(input1)
                    # non-contiguous case
                    input2 = input1.permute(0, 1, 3, 2)
                    output2 = m(input2).permute(0, 1, 3, 2)
                    # channels last case
                    input3 = input1.contiguous(memory_format=torch.channels_last)
                    output3 = m(input3)
                    self.assertEqual(output3, outputs[i])
                    self.assertEqual(output3, output1)
                    self.assertEqual(output3, output2)

        @noarchTest
        def test_empty_meta(self):
            x = torch.empty(2 ** 20, 2 ** 20, device='meta')
            y = torch.empty(2 ** 20, device='meta')
            z = x + y
            self.assertEqual(z.size(), (2 ** 20, 2 ** 20))
            self.assertRaises(RuntimeError, lambda: z[0][0].item())

        @noarchTest
        def test_upsample_nearest1d_meta(self):
            # TODO: this test should be triggered by test_nn.py but right
            # now meta is not enabled (and even if it was, we are probably
            # missing too many meta functions to get through the test unmolested)

            # NB: Can't make the exponent too big, or it will overflow
            # signed 64-bit integer
            x = torch.empty(2 * 10 ** 8, 3, 2 * 10 ** 8, device='meta')
            z = torch.nn.functional.interpolate(x, scale_factor=2)
            self.assertEqual(z.size(), (2 * 10 ** 8, 3, 4 * 10 ** 8))
            self.assertRaises(RuntimeError, lambda: z[0][0][0].item())

            # TODO: the out tests cannot be triggered by test_nn.py because
            # we don't actually do out= arguments for nn functions, so there
            # is no public API by which to get the out version

            # interpolate doesn't seem to support out=
            # (not sure why passing None here doesn't work? How strange...)
            z = torch.empty(0, device='meta')
            torch._C._nn.upsample_nearest1d(x, (4 * 10 ** 8,), 2, out=z)
            self.assertEqual(z.size(), (2 * 10 ** 8, 3, 4 * 10 ** 8))
            self.assertRaises(RuntimeError, lambda: z[0][0][0].item())

        @noarchTest
        def test_upsample_nearest2d_meta(self):
            # TODO: the out tests cannot be triggered by test_nn.py because
            # we don't actually do out= arguments for nn functions, so there
            # is no public API by which to get the out version

            # Make sure we don't clobber strides of out tensor.  NB: this
            # test must be done on 2d/3d, because 1d doesn't have any meaningful
            # layout support
            x = torch.empty(4, 3, 8, 8, device='meta')
            out = torch.empty(4, 3, 16, 16, device='meta', memory_format=torch.channels_last)
            torch._C._nn.upsample_nearest2d(x, (16, 16), out=out)
            self.assertTrue(out.is_contiguous(memory_format=torch.channels_last))

            x = torch.empty(4, 3, 8, 8, device='meta', memory_format=torch.channels_last)
            out = torch.empty(4, 3, 16, 16, device='meta')
            torch._C._nn.upsample_nearest2d(x, (16, 16), out=out)
            self.assertTrue(out.is_contiguous())

            # But if resize occurs, do clobber
            x = torch.empty(4, 3, 8, 8, device='meta', memory_format=torch.channels_last)
            out = torch.empty(0, device='meta')
            torch._C._nn.upsample_nearest2d(x, (16, 16), out=out)
            self.assertTrue(out.is_contiguous(memory_format=torch.channels_last))

            # Complain if out dtype mismatch
            x = torch.empty(4, 3, 8, 8, device='meta', dtype=torch.float)
            out = torch.empty(4, 3, 16, 16, device='meta', dtype=torch.double)
            self.assertExpectedRaisesInline(
                RuntimeError, lambda: torch._C._nn.upsample_nearest2d(x, (16, 16), out=out),
                """Expected out tensor to have dtype float, but got double instead"""
            )

            # Complain if out device mismatch
            x = torch.empty(0, 3, 8, 8, device='meta')
            out = torch.empty(0, 3, 16, 16, device='cpu')
            self.assertExpectedRaisesInline(
                RuntimeError, lambda: torch._C._nn.upsample_nearest2d(x, (16, 16), out=out),
                """Expected out tensor to have device meta, but got cpu instead"""
            )

        @noarchTest
        def test_detach_meta(self):
            x = torch.empty(2, device='meta')
            # This used to segfault
            self.assertRaises(RuntimeError, lambda: x.detach().storage())

        @noarchTest
        def test_add_meta_scalar(self):
            # From https://github.com/pytorch/pytorch/issues/53815
            x = torch.empty(2, device='meta')
            y = x + 2
            self.assertEqual(y.size(), x.size())

        def test_normal_shape(self):
            warned = False
            for device in get_all_device_types():
                tensor1 = torch.rand(1, device=device)
                tensor4 = torch.rand(4, device=device)
                tensor120 = torch.rand(120, device=device)
                tensor2145 = torch.rand(2, 1, 4, 5, device=device)
                tensor2345 = torch.rand(2, 3, 4, 5, device=device)
                tensor2345_non_contiguous = torch.rand(2, 4, 3, 5, device=device).permute(0, 2, 1, 3)
                tensor2345_channels_last = tensor2345.contiguous(memory_format=torch.channels_last)
                output2345 = torch.zeros(2, 3, 4, 5, device=device)
                output345 = torch.zeros(3, 4, 5, device=device)

                # inputs have same size
                self.assertEqual(torch.normal(tensor2345, tensor2345).size(), (2, 3, 4, 5))
                self.assertEqual(torch.normal(tensor2345_non_contiguous, tensor2345).size(), (2, 3, 4, 5))
                self.assertEqual(torch.normal(tensor2345, tensor2345_channels_last).size(), (2, 3, 4, 5))
                self.assertEqual(torch.normal(tensor2345_non_contiguous, tensor2345_channels_last).size(), (2, 3, 4, 5))

                # scalar case
                self.assertEqual(torch.normal(tensor2345, 2).size(), (2, 3, 4, 5))
                self.assertEqual(torch.normal(2, tensor2345).size(), (2, 3, 4, 5))

                # inputs are expandable tensors
                self.assertEqual(torch.normal(tensor2345, tensor1).size(), (2, 3, 4, 5))
                self.assertEqual(torch.normal(tensor2145, tensor2345).size(), (2, 3, 4, 5))

                # inputs are non-expandable tensors, but they have same number of elements
                # TORCH_WARN_ONCE is used in torch.normal, only 1st assertEqual will show warn msg
                if not warned:
                    self.assertWarnsRegex(UserWarning, "deprecated and the support will be removed",
                                          lambda: self.assertEqual(torch.normal(tensor120, tensor2345).size(), (120,)))
                    warned = True
                else:
                    self.assertEqual(torch.normal(tensor120, tensor2345).size(), (120,))
                self.assertEqual(torch.normal(tensor2345, tensor120).size(), (2, 3, 4, 5))

                # inputs are non-expandable tensors and they don't have same number of elements
                with self.assertRaisesRegex(RuntimeError, "inconsistent tensor"):
                    torch.normal(tensor2345, tensor4)

                # output and inputs are size compatible
                self.assertEqual(torch.normal(tensor2345, tensor2345, out=output2345).size(), (2, 3, 4, 5))

                # output and inputs are not size compatible
                with self.assertRaisesRegex(RuntimeError, "inconsistent tensor"):
                    # inputs are expandable but have different broadcasted size than output
                    torch.normal(tensor2345, tensor2145, out=output345)
                with self.assertRaisesRegex(RuntimeError, "inconsistent tensor"):
                    # inputs are not expandable but reshapeable, output size is not the same as mean
                    torch.normal(tensor2345, tensor120, out=output345)

        def test_tensoriterator_output_setup(self):
            # Test whether the output's memory layout is correct
            def test_memory_layout(x, y, scale, zero_point, out):
                self.assertEqual(x.dim(), 4)
                self.assertEqual(x.size(), y.size())
                self.assertEqual(y.size(), out.size())

                shape = x.size()
                for n in range(shape[0]):
                    for c in range(shape[1]):
                        for h in range(shape[2]):
                            for w in range(shape[3]):
                                if scale is not None and zero_point is not None:
                                    self.assertEqual(
                                        out[n][c][h][w],
                                        torch.ops.quantized.add(x[n][c][h][w], y[n][c][h][w], scale, zero_point))
                                else:
                                    self.assertEqual(out[n][c][h][w], x[n][c][h][w] + y[n][c][h][w])

            xraw = torch.rand(2, 3, 4, 4)
            yraw = torch.rand(2, 3, 4, 4)
            qxraw = torch.quantize_per_tensor(xraw, 0.1, 5, torch.quint8)
            qyraw = torch.quantize_per_tensor(yraw, 0.1, 5, torch.quint8)

            # contiguous case fast setup
            test_memory_layout(xraw, yraw, None, None, xraw + yraw)
            test_memory_layout(qxraw, qyraw, 0.1, 5, torch.ops.quantized.add(qxraw, qyraw, 0.1, 5))

            # channels last case fast setup
            x = xraw.contiguous(memory_format=torch.channels_last)
            y = yraw.contiguous(memory_format=torch.channels_last)
            test_memory_layout(x, y, None, None, x + y)
            qx = qxraw.contiguous(memory_format=torch.channels_last)
            qy = qyraw.contiguous(memory_format=torch.channels_last)
            test_memory_layout(qx, qy, 0.1, 5, torch.ops.quantized.add(qx, qy, 0.1, 5))

            # non contiguous case fast setup (dense, non-overlapping, same shape and strides)
            x = xraw.permute(0, 2, 3, 1)
            y = yraw.permute(0, 2, 3, 1)
            test_memory_layout(x, y, None, None, x + y)
            qx = qxraw.permute(0, 2, 3, 1)
            qy = qyraw.permute(0, 2, 3, 1)
            test_memory_layout(qx, qy, 0.1, 5, torch.ops.quantized.add(qx, qy, 0.1, 5))

            # non contiguous case fast setup (dense, non-overlapping)
            # input tensors have same shape and strides
            # output tensor have same shape as input tensors but different stride
            # output tensor should preserve its strides in this case
            x = xraw.permute(0, 2, 3, 1)
            y = yraw.permute(0, 2, 3, 1)
            out = torch.empty_like(xraw)
            out = out.permute(0, 3, 2, 1)
            expected_stride = out.stride()
            test_memory_layout(x, y, None, None, torch.add(x, y, out=out))
            self.assertEqual(expected_stride, out.stride())

            # non contiguous case non fast setup
            x = xraw.permute(0, 2, 3, 1)
            y = yraw.permute(0, 3, 2, 1)
            test_memory_layout(x, y, None, None, x + y)
            qx = qxraw.permute(0, 2, 3, 1)
            qy = qyraw.permute(0, 3, 2, 1)
            test_memory_layout(qx, qy, 0.1, 5, torch.ops.quantized.add(qx, qy, 0.1, 5))

        # Tests to make sure we still handle .data properly until it is removed
        def test_dot_data_use(self):
            # .data allows to change the Tensors types inplace, check that we still
            # raise a nice error.
            with self.assertRaisesRegex(
                    RuntimeError,
                    # message includes both Double and Long
                    '(?=.*Double)(?=.*Long)'):

                # Calls model with a LongTensor input but DoubleTensor weights
                input = torch.randn(1, 1, 1, 6, dtype=torch.double)
                weight = torch.zeros(1, 1, 1, 3, dtype=torch.long)
                model = torch.nn.Conv2d(1, 1, (1, 3), stride=1, padding=0, bias=False)
                model.weight.data = weight
                out = model(input)


# Functions to test negative dimension wrapping
METHOD = 1
INPLACE_METHOD = 2
FUNCTIONAL = 4
DIM_ARG = None

def make_neg_dim_test(name, tensor_arg, arg_constr, types, extra_dim=0):
    def neg_dim_test(self):
        if isinstance(tensor_arg, list):
            assert METHOD not in types and INPLACE_METHOD not in types
            x = [torch.randn(arg) for arg in tensor_arg]
            ndim = len(tensor_arg[-1])
        else:
            x = torch.randn(*tensor_arg)
            ndim = len(tensor_arg)
        ndim += extra_dim

        n_dim_to_test = sum(e is DIM_ARG for e in arg_constr())

        for dims_val in combinations(range(ndim), n_dim_to_test):
            arg = arg_constr()
            arg_neg = copy.deepcopy(arg)
            idx = 0
            for i, v in enumerate(arg):
                if v is DIM_ARG:
                    arg[i] = dims_val[idx]
                    arg_neg[i] = dims_val[idx] - ndim
                    idx += 1

            if METHOD in types:
                a = getattr(x, name)(*arg)
                b = getattr(x, name)(*arg_neg)
                self.assertEqual(a, b)

            if INPLACE_METHOD in types:
                a = x.clone()
                getattr(a, name + '_')(*arg)
                b = x.clone()
                getattr(b, name + '_')(*arg_neg)
                self.assertEqual(a, b)

            if FUNCTIONAL in types:
                a = getattr(torch, name)(x, *arg)
                b = getattr(torch, name)(x, *arg_neg)
                self.assertEqual(a, b)

    return neg_dim_test


def idx_tensor(size, max_val):
    return torch.LongTensor(*size).random_(0, max_val - 1)


def add_neg_dim_tests():
    neg_dim_tests = [
        ('narrow', (10, 20, 30), lambda: [DIM_ARG, 0, 5], [METHOD]),
        ('transpose', (10, 20, 30), lambda: [DIM_ARG, DIM_ARG], [METHOD, INPLACE_METHOD, FUNCTIONAL]),
        ('size', (10, 20, 30), lambda: [DIM_ARG], [METHOD]),
        ('cat', [(2, 3, 4), (2, 3, 4)], lambda: [DIM_ARG], [FUNCTIONAL]),
        ('chunk', (10, 20, 30), lambda: [5, DIM_ARG], [METHOD, FUNCTIONAL]),
        ('gather', (10, 20), lambda: [DIM_ARG, idx_tensor((10, 20), 10)], [METHOD, FUNCTIONAL]),
        ('index_select', (10, 10), lambda: [DIM_ARG, idx_tensor((10,), 10)], [METHOD, FUNCTIONAL]),
        ('split', (10, 20), lambda: [5, DIM_ARG], [METHOD, FUNCTIONAL]),
        ('squeeze', (10, 1, 20, 1), lambda: [DIM_ARG], [METHOD, INPLACE_METHOD, FUNCTIONAL]),
        ('unbind', (2, 3, 4), lambda: [DIM_ARG], [FUNCTIONAL]),
        ('unsqueeze', (10, 20), lambda: [DIM_ARG], [METHOD, INPLACE_METHOD, FUNCTIONAL], 1),
        ('logcumsumexp', (10, 20), lambda: [DIM_ARG], [METHOD, FUNCTIONAL]),
        ('cumprod', (10, 20), lambda: [DIM_ARG], [METHOD, FUNCTIONAL]),
        ('cumsum', (10, 20), lambda: [DIM_ARG], [METHOD, FUNCTIONAL]),
        ('cummax', (10, 20), lambda: [DIM_ARG], [METHOD, FUNCTIONAL]),
        ('cummin', (10, 20), lambda: [DIM_ARG], [METHOD, FUNCTIONAL]),
        ('mean', (10, 20), lambda: [DIM_ARG], [METHOD, FUNCTIONAL]),
        ('median', (10, 20), lambda: [DIM_ARG], [METHOD, FUNCTIONAL]),
        ('nanmedian', (10, 20), lambda: [DIM_ARG], [METHOD, FUNCTIONAL]),
        ('mode', (10, 20), lambda: [DIM_ARG], [METHOD, FUNCTIONAL]),
        ('norm', (10, 20), lambda: [2, DIM_ARG], [METHOD, FUNCTIONAL]),
        ('prod', (10, 20), lambda: [DIM_ARG], [METHOD, FUNCTIONAL]),
        ('std', (10, 20), lambda: [DIM_ARG], [METHOD, FUNCTIONAL]),
        ('sum', (10, 20), lambda: [DIM_ARG], [METHOD, FUNCTIONAL]),
        ('var', (10, 20), lambda: [DIM_ARG], [METHOD, FUNCTIONAL]),
        ('kthvalue', (10, 20), lambda: [3, DIM_ARG], [METHOD, FUNCTIONAL]),
        ('max', (10, 20), lambda: [DIM_ARG], [METHOD, FUNCTIONAL]),
        ('min', (10, 20), lambda: [DIM_ARG], [METHOD, FUNCTIONAL]),
        ('sort', (10, 20), lambda: [DIM_ARG], [METHOD, FUNCTIONAL]),
        ('topk', (10, 20), lambda: [5, DIM_ARG], [METHOD, FUNCTIONAL]),
        ('renorm', (10, 20), lambda: [2, DIM_ARG, 1], [METHOD, INPLACE_METHOD, FUNCTIONAL]),
        ('index_add', (10, 10), lambda: [DIM_ARG, idx_tensor((10,), 10), torch.randn(10, 10)], [INPLACE_METHOD]),
        ('index_copy', (10, 10), lambda: [DIM_ARG, idx_tensor((10,), 10), torch.randn(10, 10)], [INPLACE_METHOD]),
        ('index_fill', (10, 10), lambda: [DIM_ARG, idx_tensor((10,), 10), 12], [INPLACE_METHOD]),
        ('scatter', (10, 10), lambda: [DIM_ARG, idx_tensor((10, 10), 10), torch.randn(10, 10)], [INPLACE_METHOD]),
        ('select', (10, 20), lambda: [DIM_ARG, 3], [METHOD]),
        ('unfold', (10, 20), lambda: [DIM_ARG, 5, 2], [METHOD]),
    ]

    for decl in neg_dim_tests:
        if len(decl) == 4:
            name, tensor_arg, arg_constr, types = decl
            extra_dim = 0
        elif len(decl) == 5:
            name, tensor_arg, arg_constr, types, extra_dim = decl

        test_name = 'test_' + name + '_neg_dim'

        assert not hasattr(AbstractTestCases._TestTorchMixin, test_name), "Duplicated test name: " + test_name
        setattr(AbstractTestCases._TestTorchMixin, test_name, make_neg_dim_test(name, tensor_arg, arg_constr, types, extra_dim))


@contextlib.contextmanager
def torch_vital_set(value):
    stash = None
    if 'TORCH_VITAL' in os.environ:
        stash = os.environ['TORCH_VITAL']
    os.environ['TORCH_VITAL'] = value
    try:
        yield
    finally:
        if stash:
            os.environ['TORCH_VITAL'] = stash
        else:
            del os.environ['TORCH_VITAL']


# Tests Vital Signs for Torch
class TestBasicVitalSigns(TestCase):
    def test_basic_vitals(self):
        with torch_vital_set(''):
            self.assertFalse(torch.vitals_enabled())
        with torch_vital_set('ON'):
            self.assertTrue(torch.vitals_enabled())

    def test_basic_vitals_read_write(self):
        with torch_vital_set('ON'):
            self.assertTrue(torch.vitals_enabled())
            # This tests the code path of setting a vital
            self.assertTrue(torch.set_vital('Dataloader', 'basic_unit_test', 'TEST_VALUE_STRING'))
            self.assertIn('TEST_VALUE_STRING', torch.read_vitals())
            self.assertIn('CUDA.used', torch.read_vitals())

    def test_dataloader_vitals(self):
        with torch_vital_set('ON'):
            inps = torch.arange(10 * 5, dtype=torch.float32).view(10, 5)
            tgts = torch.arange(10 * 5, dtype=torch.float32).view(10, 5)
            dataset = torch.utils.data.TensorDataset(inps, tgts)
            loader = torch.utils.data.DataLoader(dataset, batch_size=2)
            self.assertIn('Dataloader.enabled\t\t True', torch.read_vitals())


class TestVitalSignsCuda(TestCase):
    @onlyCUDA
    def test_cuda_vitals_gpu_only(self, device):
        with torch_vital_set('ON'):
            self.assertIn('CUDA.used\t\t true', torch.read_vitals())


# Device-generic tests. Instantiated below and not run directly.
class TestTorchDeviceType(TestCase):
    exact_dtype = True

    # TODO: move all tensor creation to common ops
    def _rand_shape(self, dim, min_size, max_size):
        shape = []
        for i in range(dim):
            shape.append(random.randint(min_size, max_size))
        return tuple(shape)

    # Validates that mathematical constants are defined properly, as required by
    # the Python Array API (https://data-apis.org/array-api/latest/API_specification/constants.html)
    @onlyCPU
    def test_constants(self, device):
        self.assertIsInstance(torch.e, float)
        self.assertEqual(torch.e, math.e, atol=0, rtol=0)

        self.assertIsInstance(torch.pi, float)
        self.assertEqual(torch.pi, math.pi, atol=0, rtol=0)

        self.assertIsInstance(torch.nan, float)
        self.assertEqual(torch.nan, math.nan, equal_nan=True)

        self.assertIsInstance(torch.inf, float)
        self.assertEqual(torch.inf, math.inf)

    @onlyNativeDeviceTypes
    @dtypes(torch.int8, torch.uint8, torch.int16, torch.int32, torch.int64,
            torch.bool, torch.float32, torch.complex64, torch.float64,
            torch.complex128)
    def test_bytes_to_scalar(self, device, dtype):
        def rand_byte():
            if dtype == torch.bool:
                return torch.randint(0, 2, ()).item()
            else:
                return torch.randint(0, 256, ()).item()

        element_size = torch._utils._element_size(dtype)

        for i in range(10):
            bytes_list = [rand_byte() for _ in range(element_size)]
            scalar = bytes_to_scalar(bytes_list, dtype, device)
            self.assertEqual(scalar.storage()._untyped().tolist(), bytes_list)

    @dtypes(torch.int8, torch.uint8, torch.int16, torch.int32, torch.int64,
            torch.bool, torch.float32, torch.complex64, torch.float64,
            torch.complex128)
    def test_storage(self, device, dtype):
        v = make_tensor((3, 5), device, dtype, low=-9, high=9)
        self.assertEqual(v.storage()[0], v[0][0])
        self.assertEqual(v.storage()[14], v[2][4])
        v_s = v.storage()

        for el_num in range(v.numel()):
            dim0 = el_num // v.size(1)
            dim1 = el_num % v.size(1)
            self.assertEqual(
                v_s[el_num],
                v[dim0][dim1])

        v_s_byte = v.storage()._untyped()
        el_size = v.element_size()

        for el_num in range(v.numel()):
            start = el_num * el_size
            end = start + el_size
            dim0 = el_num // v.size(1)
            dim1 = el_num % v.size(1)
            self.assertEqual(
                bytes_to_scalar(v_s_byte[start:end], dtype, device),
                v[dim0][dim1])

    @onlyNativeDeviceTypes
    @dtypes(torch.int8, torch.uint8, torch.int16, torch.int32, torch.int64,
            torch.bool, torch.float32, torch.complex64, torch.float64,
            torch.complex128, torch.quint8, torch.qint8, torch.qint32,
            torch.quint4x2)
    def test_storage_setitem(self, device, dtype):
        # Skip quantized dtypes for CUDA, since they're not supported
        if torch.device(device).type == 'cuda':
            if dtype in [torch.quint8, torch.qint8, torch.qint32, torch.quint4x2]:
                return

        storage_type_name = torch.storage._dtype_to_storage_type_map()[dtype]
        if torch.device(device).type == 'cuda':
            storage_type = eval('torch.cuda.' + storage_type_name)
        else:
            storage_type = eval('torch.' + storage_type_name)

        N = 10

        s = storage_type(N)
        s[:] = 0
        l = [0] * N
        self.assertEqual(s, storage_type(l))

        for i in range(N):
            s[i] = i
            l[i] = i

        self.assertEqual(s, storage_type(l))

        l[2:7] = [1] * 5
        s[2:7] = 1
        self.assertEqual(s, storage_type(l))


    @onlyNativeDeviceTypes
    @dtypes(*get_all_dtypes())
    def test_tensor_from_storage(self, device, dtype):
        a = make_tensor((4, 5, 3), device, dtype, low=-9, high=9)
        a_s = a.storage()
        b = torch.tensor(a_s, device=device, dtype=dtype).reshape(a.size())
        self.assertEqual(a, b)
        c = torch.tensor(a_s._untyped(), device=device, dtype=dtype).reshape(a.size())
        self.assertEqual(a, c)

        for error_dtype in get_all_dtypes():
            if error_dtype == dtype:
                continue
            with self.assertRaisesRegex(RuntimeError, r'Expected a Storage of type'):
                error_storage = a.to(error_dtype).storage()
                torch.tensor(error_storage, device=device, dtype=dtype)

    @onlyNativeDeviceTypes
    @dtypes(*get_all_dtypes())
    def test_set_storage(self, device, dtype):
        a = make_tensor((4, 5, 3), device, dtype, low=-9, high=9)
        a_s = a.storage()
        b = torch.tensor([], device=device, dtype=dtype).set_(a_s).reshape(a.size())
        self.assertEqual(a, b)
        c = torch.tensor([], device=device, dtype=dtype).set_(a_s._untyped()).reshape(a.size())
        self.assertEqual(a, c)

        for error_dtype in get_all_dtypes():
            if error_dtype == dtype:
                continue
            with self.assertRaisesRegex(RuntimeError, r'Expected a Storage of type'):
                error_storage = a.to(error_dtype).storage()
                b = torch.tensor([], device=device, dtype=dtype).set_(error_storage)

    @dtypes(torch.float32, torch.complex64)
    def test_deepcopy(self, device, dtype):
        from copy import deepcopy
        a = torch.randn(5, 5, dtype=dtype, device=device)
        b = torch.randn(5, 5, dtype=dtype, device=device)
        c = a.view(25)
        q = [a, [a.storage(), b.storage()], b, c]
        w = deepcopy(q)
        self.assertEqual(w[0], q[0], atol=0, rtol=0)
        self.assertEqual(w[1][0], q[1][0], atol=0, rtol=0)
        self.assertEqual(w[1][1], q[1][1], atol=0, rtol=0)
        self.assertEqual(w[1], q[1], atol=0, rtol=0)
        self.assertEqual(w[2], q[2], atol=0, rtol=0)

        # Check that deepcopy preserves sharing
        w[0].add_(1)
        for i in range(a.numel()):
            self.assertEqual(w[1][0][i], q[1][0][i] + 1)
        self.assertEqual(w[3], c + 1)
        w[2].sub_(1)
        for i in range(a.numel()):
            self.assertEqual(w[1][1][i], q[1][1][i] - 1)

        # Check that deepcopy preserves attributes
        a.foo = 3
        self.assertEqual(deepcopy(a).foo, 3)

    @dtypes(torch.float32, torch.complex64)
    def test_deepcopy_scalar(self, device, dtype):
        from copy import deepcopy
        a = torch.tensor(5, dtype=dtype, device=device)
        self.assertEqual(a.size(), deepcopy(a).size())
        self.assertEqual(a, deepcopy(a))

    def check_internal_mem_overlap(self, inplace_op, num_inputs,
                                   dtype, device,
                                   expected_failure=False):
        if isinstance(inplace_op, str):
            inplace_op = getattr(torch.Tensor, inplace_op)
        input = torch.randn(1, dtype=dtype, device=device).expand(3, 3)
        inputs = [input] + [torch.randn_like(input)
                            for i in range(num_inputs - 1)]
        if not expected_failure:
            with self.assertRaisesRegex(RuntimeError, 'single memory location'):
                inplace_op(*inputs)
        else:
            with self.assertRaises(AssertionError):
                with self.assertRaisesRegex(RuntimeError, 'single memory location'):
                    inplace_op(*inputs)

    def unary_check_input_output_mem_overlap(self, data, sz, op,
                                             expected_failure=False):

        def _test(op, output, input):
            output_exp = torch.empty_like(output)
            op(input, out=output_exp)
            self.assertEqual(op(input, out=output), output_exp, msg=op.__name__)

        # output is identical to input:
        _test(op, output=data[0:sz], input=data[0:sz])
        # output and input are independent:
        _test(op, output=data[0:sz], input=data[sz:2 * sz])
        # output partially overlaps with input:
        if not expected_failure:
            with self.assertRaisesRegex(RuntimeError, 'unsupported operation'):
                _test(op, data[0:sz], data[1:sz + 1])
        else:
            with self.assertRaises(AssertionError):
                with self.assertRaisesRegex(RuntimeError, 'unsupported operation'):
                    _test(op, data[0:sz], data[1:sz + 1])
        # output is transpose of input:
        length = int(math.sqrt(sz))
        input = data[:length**2].view([length, length])
        out = input.t()
        if not expected_failure:
            with self.assertRaisesRegex(RuntimeError, 'unsupported operation'):
                _test(op, out, input)
        else:
            with self.assertRaises(AssertionError):
                with self.assertRaisesRegex(RuntimeError, 'unsupported operation'):
                    _test(op, out, input)

    def ternary_check_input_output_mem_overlap(self, op, device,
                                               expected_failure=False):
        sz = 9
        data = torch.randn(2 * sz, device=device)
        other1 = torch.randn(sz, device=device)
        other2 = torch.randn(sz, device=device)

        self.unary_check_input_output_mem_overlap(
            data, sz, lambda input, out:
                op(input, other1.view(input.shape), other2.view(input.shape), out=out),
            expected_failure=expected_failure)

        self.unary_check_input_output_mem_overlap(
            data, sz, lambda input, out:
                op(other1.view(input.shape), input, other2.view(input.shape), out=out),
            expected_failure=expected_failure)

        self.unary_check_input_output_mem_overlap(
            data, sz, lambda input, out:
                op(other1.view(input.shape), other2.view(input.shape), input, out=out),
            expected_failure=expected_failure)



    def _select_broadcastable_dims(self, dims_full=None):
        # select full dimensionality
        if dims_full is None:
            dims_full = []
            ndims = random.randint(1, 4)
            dims_full = [random.randint(1, 8) for _ in range(ndims)]
        else:
            ndims = len(dims_full)

        # select actual dimensions for ops:
        # larger: full ndims, individual sizes may be reduced
        # smaller: possibly reduced ndims, sizes may be reduced
        smaller_ndims = random.randint(1, ndims)
        dims_small = []
        dims_large = []
        for i in range(ndims - 1, -1, -1):
            j = random.randint(1, 3)
            if j == 1:  # no reduced singleton dimension
                ds = dims_full[i]
                dl = dims_full[i]
            elif j == 2:  # larger may have reduced singleton dimension
                ds = dims_full[i]
                dl = 1 if len(dims_small) < smaller_ndims else dims_full[i]
            elif j == 3:  # smaller may have reduced singleton dimension
                ds = 1
                dl = dims_full[i]
            dims_large = [dl] + dims_large
            if len(dims_small) < smaller_ndims:
                dims_small = [ds] + dims_small
        return (dims_small, dims_large, dims_full)

    # collected tests of ops that used scalar_check in Declarations.cwrap for
    # correctness
    def test_scalar_check(self, device):
        zero_d = torch.randn((), device=device)
        one_d = torch.randn((1,), device=device)

        # remainder
        self.assertEqual((), torch.remainder(zero_d, zero_d).shape)
        self.assertEqual((), torch.remainder(zero_d, 2).shape)
        self.assertEqual((1,), torch.remainder(zero_d, one_d).shape)
        self.assertEqual((1,), torch.remainder(one_d, zero_d).shape)

        # fmod
        self.assertEqual((), torch.fmod(zero_d, zero_d).shape)
        self.assertEqual((), torch.fmod(zero_d, 2).shape)
        self.assertEqual((1,), torch.fmod(zero_d, one_d).shape)
        self.assertEqual((1,), torch.fmod(one_d, zero_d).shape)

        # exp, cos, cosh, tan, atan, tanh, erf, erfc, reciprocal
        self.assertEqual((), torch.exp(zero_d).shape)
        self.assertEqual((), torch.cos(zero_d).shape)
        self.assertEqual((), torch.cosh(zero_d).shape)
        self.assertEqual((), torch.tan(zero_d).shape)
        self.assertEqual((), torch.atan(zero_d).shape)
        self.assertEqual((), torch.acosh(zero_d).shape)
        self.assertEqual((), torch.asinh(zero_d).shape)
        self.assertEqual((), torch.atanh(zero_d).shape)
        self.assertEqual((), torch.tanh(zero_d).shape)
        self.assertEqual((), torch.erf(zero_d).shape)
        self.assertEqual((), torch.erfc(zero_d).shape)
        self.assertEqual((), torch.reciprocal(zero_d).shape)
        self.assertEqual((1,), torch.exp(one_d).shape)
        self.assertEqual((1,), torch.cos(one_d).shape)
        self.assertEqual((1,), torch.cosh(one_d).shape)
        self.assertEqual((1,), torch.tan(one_d).shape)
        self.assertEqual((1,), torch.atan(one_d).shape)
        self.assertEqual((1,), torch.acosh(one_d).shape)
        self.assertEqual((1,), torch.asinh(one_d).shape)
        self.assertEqual((1,), torch.atanh(one_d).shape)
        self.assertEqual((1,), torch.tanh(one_d).shape)
        self.assertEqual((1,), torch.erf(one_d).shape)
        self.assertEqual((1,), torch.erfc(one_d).shape)
        self.assertEqual((1,), torch.reciprocal(one_d).shape)

        # clamp
        self.assertEqual((), torch.clamp(zero_d, min=0, max=1).shape)
        self.assertEqual((), torch.clamp(zero_d, min=0).shape)
        self.assertEqual((), torch.clamp(zero_d, max=1).shape)
        self.assertEqual((1,), torch.clamp(one_d, min=0, max=1).shape)
        self.assertEqual((1,), torch.clamp(one_d, min=0).shape)
        self.assertEqual((1,), torch.clamp(one_d, max=1).shape)

        # cumsum, cumprod, cummax, cummin
        self.assertEqual((), torch.logcumsumexp(zero_d, 0).shape)
        self.assertEqual((), torch.cumsum(zero_d, 0).shape)
        self.assertEqual((), torch.cumprod(zero_d, 0).shape)
        self.assertEqual((), torch.cummax(zero_d, 0)[0].shape)
        self.assertEqual((), torch.cummin(zero_d, 0)[0].shape)

        # renorm
        self.assertRaises(RuntimeError, lambda: torch.renorm(zero_d, 0.5, 0, 1.0))

        # sort, topk
        self.assertEqual([(), ()], [x.shape for x in torch.sort(zero_d, 0, False)])
        self.assertEqual([(), ()], [x.shape for x in torch.sort(zero_d, 0, True)])
        self.assertEqual([(), ()], [x.shape for x in torch.topk(zero_d, 1, 0, False)])
        self.assertEqual([(), ()], [x.shape for x in torch.topk(zero_d, 1, 0, True)])

        # lstsq (gels)
        self.assertRaises(RuntimeError, lambda: torch.lstsq(zero_d, zero_d))

        # eig
        self.assertRaises(RuntimeError, lambda: torch.eig(zero_d, False))
        self.assertRaises(RuntimeError, lambda: torch.eig(zero_d, True))

        # this is only implemented on cpu
        if (torch.device(device).type == 'cpu'):
            self.assertRaises(RuntimeError, lambda: torch.ormqr(zero_d, zero_d, zero_d))

        # max, min
        self.assertEqual((), torch.max(zero_d, zero_d).shape)
        self.assertEqual((1,), torch.max(one_d, zero_d).shape)
        self.assertEqual((1,), torch.max(zero_d, one_d).shape)
        self.assertEqual((), torch.min(zero_d, zero_d).shape)
        self.assertEqual((1,), torch.min(one_d, zero_d).shape)
        self.assertEqual((1,), torch.min(zero_d, one_d).shape)

        # diag
        self.assertRaises(RuntimeError, lambda: torch.diag(zero_d))

        zero_d_int = torch.tensor(1, device=device)
        one_d_int = torch.tensor([1], device=device)

        # lshift, rshift
        self.assertEqual((), (zero_d_int >> zero_d_int).shape)
        self.assertEqual((), (zero_d_int >> 1).shape)
        self.assertEqual((1,), (one_d_int >> zero_d_int).shape)
        self.assertEqual((1,), (zero_d_int >> one_d_int).shape)
        self.assertEqual((1,), (one_d_int >> 1).shape)

        self.assertEqual((), (zero_d_int << zero_d_int).shape)
        self.assertEqual((), (zero_d_int << 1).shape)
        self.assertEqual((1,), (one_d_int << zero_d_int).shape)
        self.assertEqual((1,), (zero_d_int << one_d_int).shape)
        self.assertEqual((1,), (one_d_int << 1).shape)

        # or
        self.assertEqual((), (zero_d_int | zero_d_int).shape)
        self.assertEqual((), (zero_d_int | 1).shape)
        self.assertEqual((1,), (one_d_int | zero_d_int).shape)
        self.assertEqual((1,), (zero_d_int | one_d_int).shape)
        self.assertEqual((1,), (one_d_int | 1).shape)

        # and
        self.assertEqual((), (zero_d_int & zero_d_int).shape)
        self.assertEqual((), (zero_d_int & 1).shape)
        self.assertEqual((1,), (one_d_int & zero_d_int).shape)
        self.assertEqual((1,), (zero_d_int & one_d_int).shape)
        self.assertEqual((1,), (one_d_int & 1).shape)

        # clone
        self.assertEqual((), zero_d.clone().shape)

        zero_d_bool = torch.tensor(True, device=device)
        one_d_bool = torch.tensor([True], device=device)

        # masked_select
        self.assertEqual((1,), torch.masked_select(zero_d_bool, zero_d_bool).shape)
        self.assertEqual((1,), torch.masked_select(zero_d_bool, one_d_bool).shape)
        self.assertEqual((1,), torch.masked_select(one_d_bool, zero_d_bool).shape)

        zero_d_uint8 = torch.tensor(1, dtype=torch.uint8, device=device)
        one_d_uint8 = torch.tensor([1], dtype=torch.uint8, device=device)

        with warnings.catch_warnings():
            warnings.simplefilter("ignore")
            self.assertEqual((1,), torch.masked_select(zero_d_uint8, zero_d_uint8).shape)
            self.assertEqual((1,), torch.masked_select(zero_d_uint8, one_d_uint8).shape)
            self.assertEqual((1,), torch.masked_select(one_d_uint8, zero_d_uint8).shape)

        # mode
        self.assertEqual([(), ()], [x.shape for x in torch.mode(zero_d, dim=0, keepdim=True)])
        self.assertEqual([(), ()], [x.shape for x in torch.mode(zero_d, dim=0, keepdim=False)])
        self.assertEqual([(1,), (1,)], [x.shape for x in torch.mode(one_d, dim=0, keepdim=True)])
        self.assertEqual([(), ()], [x.shape for x in torch.mode(one_d, dim=0, keepdim=False)])

        # max
        self.assertEqual([(), ()], [x.shape for x in torch.max(zero_d, dim=0, keepdim=True)])
        self.assertEqual([(), ()], [x.shape for x in torch.max(zero_d, dim=0, keepdim=False)])
        self.assertEqual([(1,), (1,)], [x.shape for x in torch.max(one_d, dim=0, keepdim=True)])
        self.assertEqual([(), ()], [x.shape for x in torch.max(one_d, dim=0, keepdim=False)])

        # amax
        self.assertEqual((), torch.amax(zero_d, dim=0, keepdim=True).shape)
        self.assertEqual((), torch.amax(zero_d, dim=0, keepdim=False).shape)
        self.assertEqual((1,), torch.amax(one_d, dim=0, keepdim=True).shape)
        self.assertEqual((), torch.amax(one_d, dim=0, keepdim=False).shape)

        # min
        self.assertEqual([(), ()], [x.shape for x in torch.min(zero_d, dim=0, keepdim=True)])
        self.assertEqual([(), ()], [x.shape for x in torch.min(zero_d, dim=0, keepdim=False)])
        self.assertEqual([(1,), (1,)], [x.shape for x in torch.min(one_d, dim=0, keepdim=True)])
        self.assertEqual([(), ()], [x.shape for x in torch.min(one_d, dim=0, keepdim=False)])

        # amin
        self.assertEqual((), torch.amin(zero_d, dim=0, keepdim=True).shape)
        self.assertEqual((), torch.amin(zero_d, dim=0, keepdim=False).shape)
        self.assertEqual((1,), torch.amin(one_d, dim=0, keepdim=True).shape)
        self.assertEqual((), torch.amin(one_d, dim=0, keepdim=False).shape)

        # set_
        zero_d_clone = zero_d.clone()
        one_d_clone = one_d.clone()
        self.assertEqual((), zero_d_clone.set_(one_d.storage(), 0, (), ()).shape)
        self.assertEqual((1,), zero_d_clone.set_(one_d.storage(), 0, (1,), (1,)).shape)
        self.assertEqual((), one_d_clone.set_(one_d.storage(), 0, (), ()).shape)
        self.assertEqual((1,), one_d_clone.set_(one_d.storage(), 0, (1,), (1,)).shape)

        self.assertEqual((), zero_d.clone().set_(zero_d).shape)
        self.assertEqual((), one_d.clone().set_(zero_d).shape)
        self.assertEqual((1,), zero_d.clone().set_(one_d).shape)
        self.assertEqual((1,), one_d.clone().set_(one_d).shape)

        # take
        self.assertEqual((), torch.randn((2, 3), device=device).take(zero_d_int).shape)
        self.assertEqual((1,), torch.randn((2, 3), device=device).take(one_d_int).shape)

        # gather
        self.assertEqual((), torch.gather(zero_d, 0, torch.zeros((), dtype=torch.int64, device=device)).shape)
        self.assertEqual((1,), torch.gather(zero_d, 0, torch.zeros((1,), dtype=torch.int64, device=device)).shape)
        self.assertEqual((), torch.gather(one_d, 0, torch.zeros((), dtype=torch.int64, device=device)).shape)
        self.assertEqual((1,), torch.gather(one_d, 0, torch.zeros((1,), dtype=torch.int64, device=device)).shape)

        # normal
        # std must be >= 0
        zero_d_ge_0 = torch.rand((), device=device)
        # documentation says out shape matches shape of mean
        self.assertEqual((), torch.normal(zero_d, zero_d_ge_0).shape)
        self.assertEqual((1,), torch.normal(one_d, zero_d_ge_0).shape)
        self.assertEqual((), torch.normal(1, zero_d_ge_0).shape)
        self.assertEqual((), torch.normal(zero_d, 1).shape)
        self.assertEqual((1,), torch.normal(one_d, 1).shape)
        # TODO: this behavior differs on CPU and GPU, see https://github.com/pytorch/pytorch/issues/30480.
        # self.assertEqual((), torch.normal(zero_d, one_d).shape)
        # self.assertEqual((), torch.normal(1, one_d).shape)

        # convolutions.  Yes, we are testing nn.functional here; seems justified
        # given its similar to the other tests
        w = torch.randn(2, 1, 3, 3, device=device).div_(2).requires_grad_()
        self.assertRaises(RuntimeError, lambda: torch.nn.functional.conv2d(zero_d, w, groups=1))
        self.assertRaises(RuntimeError, lambda: torch.nn.functional.conv2d(zero_d, w, groups=2))

        # nll_loss -- verify input can't be 0-dimensional.
        self.assertRaises(ValueError, lambda: torch.nn.functional.nll_loss(zero_d, zero_d, reduction='none'))
        self.assertRaises(ValueError, lambda: torch.nn.functional.nll_loss(zero_d, one_d, reduction='none'))
        # verify output is 0-dimensional when reduction != 'none'
        for (input, target) in ((torch.randn(1, 1, device=device), torch.tensor([0], device=device)),
                                (torch.randn(1, 1, 1, 1, device=device), torch.tensor([[[0]]], device=device))):
            self.assertEqual((), torch.nn.functional.nll_loss(input, target, reduction='mean').shape)
            self.assertEqual((), torch.nn.functional.nll_loss(input, target, reduction='sum').shape)

        # multilabel_margin_loss
        for input in (zero_d, one_d, torch.randn(1, 1, device=device)):
            for target in (torch.tensor(0, device=device), torch.tensor([0], device=device), torch.tensor([[0]], device=device)):
                if (input.dim() <= 1 and target.dim() <= 1) or (input.dim() == 2 and target.dim() == 2):
                    output_shape = (target.shape[0],) if target.dim() == 2 else ()
                    self.assertEqual(output_shape,
                                     torch.nn.functional.multilabel_margin_loss(input, target, reduction='none').shape)
                    self.assertEqual((), torch.nn.functional.multilabel_margin_loss(input, target, reduction='mean').shape)
                    self.assertEqual((), torch.nn.functional.multilabel_margin_loss(input, target, reduction='sum').shape)
                else:
                    self.assertRaises(RuntimeError,
                                      lambda: torch.nn.functional.multilabel_margin_loss(input, target, reduction='none'))
                    self.assertRaises(RuntimeError,
                                      lambda: torch.nn.functional.multilabel_margin_loss(input, target, reduction='mean'))
                    self.assertRaises(RuntimeError,
                                      lambda: torch.nn.functional.multilabel_margin_loss(input, target, reduction='sum'))

        # multi_margin_loss
        for input in (zero_d, one_d, torch.randn(1, 1, device=device)):
            for target in (torch.tensor(0, device=device), torch.tensor([0], device=device)):
                self.assertEqual(target.shape, torch.nn.functional.multi_margin_loss(input, target, reduction='none').shape)
                self.assertEqual((), torch.nn.functional.multi_margin_loss(input, target, reduction='mean').shape)
                self.assertEqual((), torch.nn.functional.multi_margin_loss(input, target, reduction='sum').shape)

    # Uses mismatched arange out size to trigger a warning
    def test_cpp_warnings_have_python_context(self, device):
        # Creates long string in advance to avoid a too-long Python line
        s = ".+Triggered internally at.+RangeFactories.+"

        def cpp_warn_fn():
            out = torch.empty((5,))
            torch.arange(0, 3, out=out)
            return out

        # Checks eager-mode cpp warning
        with warnings.catch_warnings(record=True) as w:
            cpp_warn_fn()
            frameinfo = inspect.getframeinfo(inspect.currentframe())
            warning = w[0]

            # Checks for cpp context in the warning message
            self.assertTrue(re.search(s, str(warning.message)) is not None)

            # Checks the Python features of the warning
            # Note: the eager mode warning refers to the line in the function
            # that throws the warning.
            self.assertEqual(frameinfo.lineno - 6, warning.lineno)
            self.assertEqual(len(w), 1)

        # Checks jitted cpp warning
        with warnings.catch_warnings(record=True) as w:
            scripted_cpp_warn_fn = torch.jit.script(cpp_warn_fn)
            scripted_cpp_warn_fn()
            warning = w[0]

            # Checks for cpp context in the warning message
            self.assertTrue(re.search(s, str(warning.message)) is not None)

            # Checks the Python features of the warning
            # Note: the jitted warning's lineno refers to the call to the jitted
            # function, which in our test suite has a layer of indirection
            # that makes checking the Python lineno fragile
            self.assertEqual(len(w), 1)

        # Checks jitted Python warning
        def warn_fn():
            warnings.warn("Warning!")

        # The jit mimics an eager-mode Python warning in this case
        with warnings.catch_warnings(record=True) as w:
            scripted_warn_fn = torch.jit.script(warn_fn)
            scripted_warn_fn()
            frameinfo = inspect.getframeinfo(inspect.currentframe())
            warning = w[0]

            self.assertTrue(re.search('Warning!', str(warning.message)) is not None)

            # Checks the Python features of the warning
            self.assertEqual(frameinfo.lineno - 6, warning.lineno)
            self.assertEqual(len(w), 1)

    @onlyCPU
    def test_warn_always_caught(self, device):
        # Check that we can catch a TORCH_WARN_ONCE warning twice
        # since assertWarnsOnceRegex uses set_warn_always(True) which changes
        # TORCH_WARN_ONCE to TORCH_WARN
        a = np.arange(10)
        a.flags.writeable = False
        with self.assertWarnsOnceRegex(UserWarning, '.*non-writable.*'):
            torch.from_numpy(a)

        # OK, got it once, now try again
        with self.assertWarnsOnceRegex(UserWarning, '.*non-writable.*'):
            torch.from_numpy(a)

        # Make sure emitting two warnings will pass the assertWarnsOnceRegex
        # context manager
        with self.assertWarnsOnceRegex(UserWarning, '.*non-writable.*'):
            torch.from_numpy(a)
            torch.from_numpy(a)

    # TODO: this test should be in test_nn.py
    def test_conv_transposed_backward_agnostic_to_memory_format(self, device):
        in_channels = 64
        out_channels = 128
        scale_factor = 8
        batch_size = 8
        length = 16

        conv = torch.nn.ConvTranspose1d(
            in_channels, out_channels, kernel_size=scale_factor * 2, stride=scale_factor).to(device)
        layer_norm = torch.nn.LayerNorm(out_channels).to(device)

        input_ = torch.randn(batch_size, in_channels, length).to(device).contiguous()
        input_ = conv(input_).contiguous()
        input_ = layer_norm(input_.transpose(1, 2).contiguous()).contiguous()
        input_.sum().backward()

        # 3d
        conv = torch.nn.ConvTranspose3d(3, 3, kernel_size=3).to(device)
        input = torch.randn(batch_size, 3, length, length, length, device=device)
        out = conv(input)
        out.backward(torch.ones_like(out).transpose(-2, -1))

    # TODO: this test should be in test_nn.py
    @onlyCUDA
    @largeTensorTest('12GB')
    def test_conv_transposed_large(self, device):
        # ConvTranspose3d works for large input tensors (gh-32866)
        in_channels = 64
        out_channels = 128
        kernel_size = 5

        conv = torch.nn.ConvTranspose3d(
            in_channels, out_channels, kernel_size=kernel_size,
            stride=2, padding=2, output_padding=1).to(device)

        x = torch.rand([1, 64, 8, 128, 172]).to(device)
        y = conv(x)

    def test_is_set_to(self, device):
        t1 = torch.empty(3, 4, 9, 10, device=device)
        t2 = torch.empty(3, 4, 9, 10, device=device)
        t3 = torch.tensor([], device=device).set_(t1)
        t4 = t3.clone().resize_(12, 90)
        self.assertFalse(t1.is_set_to(t2))
        self.assertTrue(t1.is_set_to(t3))
        self.assertTrue(t3.is_set_to(t1), "is_set_to should be symmetric")
        self.assertFalse(t1.is_set_to(t4))
        self.assertFalse(torch.tensor([]).is_set_to(torch.tensor([])),
                         "Tensors with no storages should not appear to be set "
                         "to each other")

        t1 = torch.tensor([True, True], dtype=torch.bool, device=device)
        t2 = torch.tensor([0], dtype=torch.bool, device=device).set_(t1)
        self.assertTrue(t1.is_set_to(t2))

        # test that sizes must match
        t1 = torch.empty([2, 3, 4], device=device)
        t2 = t1.view(4, 3, 2)
        self.assertFalse(t1.is_set_to(t2))
        self.assertFalse(t2.is_set_to(t1))

        # test that legacy empty size behavior used to be respected (i.e. all
        # empty tensors were logically collapsed to size [0]).
        t1 = torch.empty([2, 5, 0], device=device)
        t2 = t1.view([0])
        self.assertFalse(t1.is_set_to(t2))
        self.assertFalse(t2.is_set_to(t1))

    def test_broadcast(self, device):

        # all functions
        fns = {
            "dist", "atan2", "pow", "lerp", "add",
            "sub", "mul", "div", "fmod", "remainder",
            "eq", "ge", "gt", "le", "lt", "max", "min", "ne",
            "addcdiv", "addcmul", "masked_scatter", "masked_select", "masked_fill",
            "map", "map2", "copy"
        }
        # functions with three tensor arguments
        fns_3_args = {"map2"}
        fns_value_kwarg = {"addcdiv", "addcmul"}

        for fn in fns:
            (dims_small, dims_large, dims_full) = self._select_broadcastable_dims()
            full1d = torch.randn(*dims_full, device=device).flatten().float()
            small = torch.randn(*dims_small, device=device).float()
            large = torch.randn(*dims_large, device=device).float()
            small_expanded = small.expand(*dims_full)
            large_expanded = large.expand(*dims_full)
            small2 = None
            small2_expanded = None
            if fn in fns_3_args or fn in fns_value_kwarg:
                # create another smaller tensor
                (dims_small2, _, _) = self._select_broadcastable_dims(dims_full)
                small2 = torch.randn(*dims_small2, device=device).float()
                small2_expanded = small2.expand(*dims_full)

            if small.is_cuda and fn in ['map', 'map2']:
                # map and map2 are not implementd on CUDA tensors
                continue

            if hasattr(large_expanded, fn):
                # run through tensor versions of functions
                # and verify fully expanded inputs give same results
                expanded = {large: large_expanded, small: small_expanded, small2: small2_expanded}

                def tensorfn(myfn, t1, t2):
                    if fn == "lerp":
                        return myfn(t1, 0.5)
                    elif fn == "masked_select":
                        return myfn(t1 < 0)
                    elif fn == "masked_scatter":
                        return myfn(t1 < 0.5, full1d)
                    elif fn == "masked_fill":
                        return myfn(t1 < 0.5, 1.0)
                    elif fn in fns_3_args:
                        return myfn(1, t1, t2)
                    elif fn in fns_value_kwarg:
                        return myfn(t1, t2, value=1)
                    else:
                        return myfn(t1)

                # test various orders
                for first, second, third in [(large, small, small2), (small, large, small2),
                                             (small2, small, large), (small2, large, small)]:
                    if first is None:
                        break  # ignore last iter when small2 is None
                    method_expanded = getattr(expanded[first], fn)
                    method = getattr(first, fn)
                    r1 = tensorfn(method_expanded, expanded[second], expanded[third])
                    r2 = tensorfn(method, second, third)
                    self.assertEqual(r1, r2)

            # now for torch. versions of functions
            if hasattr(torch, fn):
                fntorch = getattr(torch, fn)
                expanded = {large: large_expanded, small: small_expanded, small2: small2_expanded}

                def torchfn(t1, t2, t3):
                    if fn == "lerp":
                        return fntorch(t1, t2, 0.5)
                    elif fn == "masked_select":
                        return fntorch(t1, t2 < 0)
                    elif fn == "masked_scatter":
                        return fntorch(t1, t2 < 0.5, full1d)
                    elif fn == "masked_fill":
                        return fntorch(t1, t2 < 0.5, 1.0)
                    elif fn in fns_3_args:
                        return fntorch(t1, 1.0, t2, t3)
                    elif fn in fns_value_kwarg:
                        return fntorch(t1, t2, t3, value=1.0)
                    else:
                        return fntorch(t1, t2)

                # test various orders
                for first, second, third in [(large, small, small2), (small, large, small2),
                                             (small2, small, large), (small2, large, small)]:
                    if first is None:
                        break  # ignore last iter when small2 is None
                    r1 = torchfn(expanded[first], expanded[second], expanded[third])
                    r2 = torchfn(first, second, third)
                    self.assertEqual(r1, r2)

            # now for in place functions
            # in-place tensor is not broadcastable; test only guaranteed
            # to work by broadcasting other argument(s)
            if not hasattr(large_expanded, fn + "_"):
                continue

            # need to clone largeExpanded so we can reuse, since functions are in-place
            large_expanded_clone = large_expanded.clone()

            def tensorfn_inplace(t0, t1, t2=None):
                t0_fn = getattr(t0, fn + "_")
                if fn == "lerp":
                    return t0_fn(t1, 0.5)
                elif fn == "masked_scatter":
                    return t0_fn(t1 < 0.5, full1d)
                elif fn == "masked_fill":
                    return t0_fn(t1 < 0.5, 1.0)
                elif fn == "map":
                    return t0_fn(t1, lambda x, y: x + y)
                elif fn == "map2":
                    return t0_fn(t1, t2, lambda x, y, z: x + y + z)
                elif fn in fns_3_args:
                    return t0_fn(1.0, t1, t2)
                elif fn in fns_value_kwarg:
                    return t0_fn(t1, t2, value=1.0)
                else:
                    return t0_fn(t1)
            # in-place pointwise operations don't actually work if the in-place
            # tensor is 0-strided (numpy has the same issue)
            if (0 not in large_expanded.stride() and 0 not in large_expanded_clone.stride()):
                r1 = tensorfn_inplace(large_expanded, small_expanded, small2_expanded)
                r2 = tensorfn_inplace(large_expanded_clone, small, small2)
                self.assertEqual(r1, r2)

            def broadcastable(t0, t1, t2=None):
                try:
                    t1.expand_as(t0)
                    if t2 is not None:
                        t2.expand_as(t0)
                except RuntimeError:
                    return False
                return True

            def _test_in_place_broadcastable(t0, t1, t2=None):
                if not broadcastable(t0, t1, t2):
                    same_size = t0.numel() == t1.numel() and (t0.numel() == t2.numel() if t2 is not None else True)
                    if not same_size:
                        self.assertRaises(RuntimeError, lambda: tensorfn_inplace(t0, t1, t2))
                else:
                    tensorfn_inplace(t0, t1, t2)

            if fn not in fns_3_args and fn not in fns_value_kwarg:
                _test_in_place_broadcastable(small, large_expanded)
                _test_in_place_broadcastable(small, large)
            else:
                _test_in_place_broadcastable(small2, small_expanded, large_expanded)
                _test_in_place_broadcastable(small2, small, large)

    @unittest.skipIf(IS_FBCODE and IS_REMOTE_GPU, "cublas runtime error")
    @onlyCUDA
    @wrapDeterministicFlagAPITest
    def test_cublas_config_nondeterministic_alert(self, device):
        test_cases = [
            # (function, (tensor sizes))
            ('mm', ((2, 2), (2, 2),)),
            ('mv', ((2, 2), (2,),)),
            ('bmm', ((1, 2, 2), (1, 2, 2),))]

        test_configs = [
            # (CuBLAS workspace config, is deterministic)
            ('garbage', False),
            (None, False),
            (':4096:8', True),
            (':16:8', True)]

        cublas_var_name = 'CUBLAS_WORKSPACE_CONFIG'
        is_cuda10_2_or_higher = (
            (torch.version.cuda is not None)
            and ([int(x) for x in torch.version.cuda.split(".")] >= [10, 2]))

        def test_case_info(fn_name, config):
            return f'function "{fn_name}" with config "{"" if config is None else config}"'

        # Create processes to test each combination of test cases and config settings
        processes = []
        for fn_name, arg_sizes in test_cases:
            for config, is_config_deterministic in test_configs:
                env = os.environ.copy()
                if config is None:
                    if env.get(cublas_var_name) is not None:
                        del env[cublas_var_name]
                else:
                    env[cublas_var_name] = config
                should_throw_error = is_cuda10_2_or_higher and not is_config_deterministic
                script = f"""
import torch
torch.use_deterministic_algorithms(True)
fn = torch.{fn_name}
arg_sizes = {arg_sizes}
device = '{device}'
should_throw_error = {should_throw_error}
args = []
for arg_size in arg_sizes:
    args.append(torch.randn(*arg_size, device=device))
try:
    fn(*args)
except RuntimeError as e:
    if not should_throw_error:
        raise RuntimeError('Did not expect any error to be raised')
    elif 'Deterministic behavior was enabled with either' not in str(e):
        raise RuntimeError('Expected a CuBLAS nondeterministic error, but got a different error')
else:
    if should_throw_error:
        raise RuntimeError('Expected a CuBLAS nondeterministic error, but it was not raised')

"""
                try:
                    subprocess.check_output(
                        [sys.executable, '-c', script],
                        stderr=subprocess.STDOUT,
                        # On Windows, opening the subprocess with the default CWD makes `import torch`
                        # fail, so just set CWD to this script's directory
                        cwd=os.path.dirname(os.path.realpath(__file__)),
                        env=env)
                except subprocess.CalledProcessError as e:
                    self.fail(msg=(
                        f'Subprocess exception while attempting to run {test_case_info(fn_name, config)}:\n'
                        + e.output.decode("utf-8")))

    def test_nondeterministic_alert_AvgPool3d(self, device):
        module = torch.nn.AvgPool3d(3)
        input = torch.randn(2, 3, 3, 3, requires_grad=True, device=device)
        res = module(input)
        grad = torch.ones_like(res)

        @expectedAlertNondeterministic('avg_pool3d_backward_cuda', ['cuda'])
        def backward_func(slf, device):
            res.backward(grad)

        backward_func(self, device)

    def test_nondeterministic_alert_AdaptiveAvgPool2d(self, device):
        module = torch.nn.AdaptiveAvgPool2d(3)
        input = torch.randn(2, 3, 3, requires_grad=True, device=device)
        res = module(input)
        grad = torch.ones_like(res)

        @expectedAlertNondeterministic('adaptive_avg_pool2d_backward_cuda', ['cuda'])
        def backward_func(slf, device):
            res.backward(grad)

        backward_func(self, device)

    def test_nondeterministic_alert_AdaptiveAvgPool3d(self, device):
        module = torch.nn.AdaptiveAvgPool3d(3)
        input = torch.randn(2, 3, 3, 3, requires_grad=True, device=device)
        res = module(input)
        grad = torch.ones_like(res)

        @expectedAlertNondeterministic('adaptive_avg_pool3d_backward_cuda', ['cuda'])
        def backward_func(slf, device):
            res.backward(grad)

        backward_func(self, device)

    def test_nondeterministic_alert_MaxPool3d(self, device):
        module = torch.nn.MaxPool3d(3)
        input = torch.randn(2, 3, 3, 3, requires_grad=True, device=device)
        res = module(input)
        grad = torch.ones_like(res)

        @expectedAlertNondeterministic('max_pool3d_with_indices_backward_cuda', ['cuda'])
        def backward_func(slf, device):
            res.backward(grad)

        backward_func(self, device)

    def test_nondeterministic_alert_AdaptiveMaxPool2d(self, device):
        module = torch.nn.AdaptiveMaxPool2d(3)
        input = torch.randn(2, 3, 3, requires_grad=True, device=device)
        res = module(input)
        grad = torch.ones_like(res)

        @expectedAlertNondeterministic('adaptive_max_pool2d_backward_cuda', ['cuda'])
        def backward_func(slf, device):
            res.backward(grad)

        backward_func(self, device)

    def test_nondeterministic_alert_FractionalMaxPool2d(self, device):
        module = torch.nn.FractionalMaxPool2d(2, output_ratio=0.5)
        input = torch.randn(2, 3, 3, 3, requires_grad=True, device=device)
        res = module(input)
        grad = torch.ones_like(res)

        @expectedAlertNondeterministic('fractional_max_pool2d_backward_cuda', ['cuda'])
        def backward_func(slf, device):
            res.backward(grad)

        backward_func(self, device)

    def test_nondeterministic_alert_FractionalMaxPool3d(self, device):
        module = torch.nn.FractionalMaxPool3d(2, output_ratio=0.5)
        input = torch.randn(2, 3, 3, 3, 3, requires_grad=True, device=device)
        res = module(input)
        grad = torch.ones_like(res)

        @expectedAlertNondeterministic('fractional_max_pool3d_backward_cuda', ['cuda'])
        def backward_func(slf, device):
            res.backward(grad)

        backward_func(self, device)

    def test_nondeterministic_alert_interpolate_linear(self, device):
        input = torch.randn(1, 2, 4, device=device, requires_grad=True)
        res = torch.nn.functional.interpolate(
            input,
            size=12,
            mode='linear',
            align_corners=False)
        grad = torch.ones_like(res)

        @expectedAlertNondeterministic('upsample_linear1d_backward_out_cuda', ['cuda'])
        def backward_func(slf, device):
            res.backward(grad)

        backward_func(self, device)

    def test_nondeterministic_alert_interpolate_bilinear(self, device):
        input = torch.randn(1, 2, 4, 4, device=device, requires_grad=True)
        res = torch.nn.functional.interpolate(
            input,
            size=12,
            mode='bilinear',
            align_corners=False)
        grad = torch.ones_like(res)

        @expectedAlertNondeterministic('upsample_bilinear2d_backward_out_cuda', ['cuda'])
        def backward_func(slf, device):
            res.backward(grad)

        backward_func(self, device)

    def test_nondeterministic_alert_interpolate_bicubic(self, device):
        input = torch.randn(1, 2, 4, 4, device=device, requires_grad=True)
        res = torch.nn.functional.interpolate(
            input,
            size=12,
            mode='bicubic',
            align_corners=False)
        grad = torch.ones_like(res)

        @expectedAlertNondeterministic('upsample_bicubic2d_backward_out_cuda', ['cuda'])
        def backward_func(slf, device):
            res.backward(grad)

        backward_func(self, device)

    def test_nondeterministic_alert_interpolate_trilinear(self, device):
        input = torch.randn(1, 2, 4, 4, 4, device=device, requires_grad=True)
        res = torch.nn.functional.interpolate(
            input,
            size=12,
            mode='trilinear',
            align_corners=False)
        grad = torch.ones_like(res)

        @expectedAlertNondeterministic('upsample_trilinear3d_backward_out_cuda', ['cuda'])
        def backward_func(slf, device):
            res.backward(grad)

        backward_func(self, device)

    def test_nondeterministic_alert_ReflectionPad1d(self, device):
        module = torch.nn.ReflectionPad1d((1, 2))
        input = torch.randn(2, 3, 8, device=device, requires_grad=True)
        res = module(input)
        grad = torch.ones_like(res)

        @expectedAlertNondeterministic('reflection_pad1d_backward_out_cuda', ['cuda'])
        def backward_func(slf, device):
            res.backward(grad)

        backward_func(self, device)

    def test_nondeterministic_alert_ReflectionPad2d(self, device):
        module = torch.nn.ReflectionPad2d((1, 2, 3, 4))
        input = torch.randn(2, 3, 8, 8, device=device, requires_grad=True)
        res = module(input)
        grad = torch.ones_like(res)

        @expectedAlertNondeterministic('reflection_pad2d_backward_cuda', ['cuda'])
        def backward_func(slf, device):
            res.backward(grad)

        backward_func(self, device)

    def test_nondeterministic_alert_ReflectionPad3d(self, device):
        module = torch.nn.ReflectionPad3d((1, 2, 3, 4, 5, 6))
        input = torch.randn(2, 3, 8, 8, 8, device=device, requires_grad=True)
        res = module(input)
        grad = torch.ones_like(res)

        @expectedAlertNondeterministic('reflection_pad3d_backward_out_cuda', ['cuda'])
        def backward_func(slf, device):
            res.backward(grad)

        backward_func(self, device)

    def test_nondeterministic_alert_ReplicationPad1d(self, device):
        module = torch.nn.ReplicationPad1d((1, 2))
        input = torch.randn(2, 3, 4, device=device, requires_grad=True)
        res = module(input)
        grad = torch.ones_like(res)

        @expectedAlertNondeterministic('replication_pad1d_backward_cuda', ['cuda'])
        def backward_func(slf, device):
            res.backward(grad)

        backward_func(self, device)

    def test_nondeterministic_alert_ReplicationPad2d(self, device):
        module = torch.nn.ReplicationPad2d((1, 2, 3, 4))
        input = torch.randn(2, 3, 4, 4, device=device, requires_grad=True)
        res = module(input)
        grad = torch.ones_like(res)

        @expectedAlertNondeterministic('replication_pad2d_backward_cuda', ['cuda'])
        def backward_func(slf, device):
            res.backward(grad)

        backward_func(self, device)

    def test_nondeterministic_alert_ReplicationPad3d(self, device):
        module = torch.nn.ReplicationPad3d((1, 2, 3, 4, 5, 6))
        input = torch.randn(2, 3, 4, 4, 4, device=device, requires_grad=True)
        res = module(input)
        grad = torch.ones_like(res)

        @expectedAlertNondeterministic('replication_pad3d_backward_cuda', ['cuda'])
        def backward_func(slf, device):
            res.backward(grad)

        backward_func(self, device)

    def test_nondeterministic_alert_NLLLoss(self, device):
        module = torch.nn.NLLLoss()
        input = torch.randn(2, 3, 5, 5, device=device)
        target = torch.rand(2, 5, 5, device=device).mul(3).floor().long()

        @expectedAlertNondeterministic('nll_loss2d_forward_out_cuda_template', ['cuda'])
        def forward_func(slf, device):
            module(input, target)

        forward_func(self, device)

    def test_nondeterministic_alert_CTCLoss(self, device):
        module = torch.nn.CTCLoss()
        input = torch.randn(50, 3, 15, device=device, requires_grad=True)
        target = torch.randint(0, 14, (3, 30), device=device)
        input_lengths = [50, 50, 50]
        target_lengths = [30, 25, 20]
        res = module(input, target, input_lengths, target_lengths)
        grad = torch.ones_like(res)

        @expectedAlertNondeterministic('ctc_loss_backward_gpu', ['cuda'])
        def backward_func(slf, device):
            res.backward(grad, retain_graph=True)

        backward_func(self, device)

    def test_nondeterministic_alert_EmbeddingBag_max(self, device):
        module = torch.nn.EmbeddingBag(
            4, 3, None, 2., False, 'max',
            _weight=torch.randn(4, 3, device=device, requires_grad=True))
        input = torch.randint(0, 3, (4, 3), device=device)
        res = module(input)
        grad = torch.ones_like(res)

        @expectedAlertNondeterministic('embedding_bag_backward_cuda_max', ['cuda'])
        def backward_func(slf, device):
            res.backward(grad)

        backward_func(self, device)

    def test_nondeterministic_alert_scatter_add(self, device):
        def test_func(op_call):
            input = torch.randn(5, 4, device=device)
            dim = 0
            index = torch.tensor([[3]], device=device)
            src = torch.tensor([[1.0]], device=device)

            @expectedAlertNondeterministic('scatter_add_cuda_kernel', ['cuda'])
            def forward_func(slf, device):
                op_call(input, dim, index, src)

            forward_func(self, device)

        test_func(torch.Tensor.scatter_add_)
        test_func(torch.Tensor.scatter_add)
        test_func(torch.scatter_add)

    @expectedFailureMeta  # expected a non-determinitic error, but it was not raised
    @onlyNativeDeviceTypes
    def test_nondeterministic_alert_put(self, device):
        def test_func(op_call):
            a = torch.randn(10, device=device)
            indices = torch.tensor([0, 0], device=device)
            values = torch.tensor([0., 1.], device=device)

            @expectedAlertNondeterministic('put_')
            def forward_func(slf, device):
                op_call(a, indices, values, accumulate=False)

            forward_func(self, device)

        test_func(torch.Tensor.put)
        test_func(torch.Tensor.put_)

    def test_nondeterministic_alert_put_accumulate(self, device):
        def test_func(op_call):
            a = torch.randn(10, device=device)
            indices = torch.tensor([0, 0], device=device)
            values = torch.tensor([0., 1.], device=device)

            @expectedAlertNondeterministic('put_', ['cuda'])
            def forward_func(slf, device):
                op_call(a, indices, values, accumulate=True)

            forward_func(self, device)

        test_func(torch.Tensor.put)
        test_func(torch.Tensor.put_)

    def test_nondeterministic_alert_histc(self, device):
        def test_func(op_call):
            a = torch.tensor([], device=device)

            @expectedAlertNondeterministic('_histc_cuda', ['cuda'])
            def forward_func(slf, device):
                res = op_call(a, min=0, max=3)

            forward_func(self, device)

        test_func(torch.histc)
        test_func(torch.Tensor.histc)

    def test_nondeterministic_alert_bincount(self, device):
        def test_func(op_call):
            a = torch.tensor([], device=device, dtype=torch.long)

            @expectedAlertNondeterministic('_bincount_cuda', ['cuda'])
            def forward_func(slf, device):
                res = op_call(a)

            forward_func(self, device)

        test_func(torch.bincount)
        test_func(torch.Tensor.bincount)

    # Ensures that kthvalue throws nondeterministic alerts in the correct cases
    @dtypes(torch.double)
    def test_nondeterministic_alert_kthvalue(self, device, dtype):
        @expectedAlertNondeterministic('kthvalue CUDA', ['cuda'])
        def test_func(slf, device, call_type):
            S = 10
            k = 5
            a = torch.randn(S, device=device)
            if call_type == 'function':
                torch.kthvalue(a, k)
            elif call_type == 'method':
                a.kthvalue(k)
            elif call_type == 'out':
                values = torch.empty_like(a)
                indices = torch.empty((), device=device, dtype=torch.long)
                torch.kthvalue(a, k, out=(values, indices))
            else:
                self.fail(f"'{call_type}' is not a valid call type")

        test_func(self, device, 'function')
        test_func(self, device, 'method')
        test_func(self, device, 'out')

    @onlyNativeDeviceTypes
    def test_nondeterministic_alert_gather(self, device):
        def test_func(op_call):
            a = torch.randn(3, 3, device=device, requires_grad=True)
            dim = 0
            index = torch.tensor([[0]], device=device)
            res = op_call(a, dim, index)
            grad = torch.ones_like(res)

            @expectedAlertNondeterministic('scatter_add_cuda_kernel', ['cuda'])
            def backward_func(slf, device):
                res.backward(grad)

            backward_func(self, device)

        test_func(torch.gather)
        test_func(torch.Tensor.gather)

    def test_nondeterministic_alert_grid_sample_2d(self, device):
        input = torch.empty(1, 1, 2, 2, device=device, requires_grad=True)
        grid = torch.empty(1, 1, 1, 2, device=device)
        res = torch.nn.functional.grid_sample(input, grid, align_corners=False)
        grad = torch.ones_like(res)

        @expectedAlertNondeterministic('grid_sampler_2d_backward_cuda', ['cuda'])
        def backward_func(slf, device):
            res.backward(grad)

        backward_func(self, device)

    def test_nondeterministic_alert_grid_sample_3d(self, device):
        input = torch.empty(1, 1, 2, 2, 2, device=device, requires_grad=True)
        grid = torch.empty(1, 1, 1, 2, 3, device=device)
        res = torch.nn.functional.grid_sample(input, grid, align_corners=False)
        grad = torch.ones_like(res)

        @expectedAlertNondeterministic('grid_sampler_3d_backward_cuda', ['cuda'])
        def backward_func(slf, device):
            res.backward(grad)

        backward_func(self, device)

    def test_embedding_scalar_weight_error(self, device):
        indices = torch.rand(2, 2, device=device).long()
        weights = [
            torch.tensor(1.0, device=device),
            torch.tensor(1.0, device=device).reshape(1, 1, 1),
        ]
        for weight in weights:
            with self.assertRaisesRegex(RuntimeError, "'weight' must be 2-D"):
                torch.embedding(weight, indices)

    def test_dist(self, device):
        def run_test(x, y):
            for p in [0, 1, 2, 3, 4, inf, -inf]:
                dist_xy = torch.dist(x, y, p)
                dist_xy_norm = torch.norm(x - y, p)
                self.assertEqual(dist_xy, dist_xy_norm)

        run_test(torch.randn(5, device=device), torch.randn(5, device=device))

        x = torch.zeros(3, device=device)
        y = torch.zeros(3, device=device)
        y[1] = 1.
        run_test(x, y)

    # Ensures that median throws nondeterministic alerts in the correct cases
    @dtypes(torch.double)
    def test_nondeterministic_alert_median(self, device, dtype):
        def test_func(slf, device, call_type):
            S = 10
            a = torch.randn(S, device=device)
            if call_type == 'function':
                torch.median(a)
            elif call_type == 'function with indices':
                torch.median(a, 0)
            elif call_type == 'method':
                a.median()
            elif call_type == 'method with indices':
                a.median(0)
            elif call_type == 'out with indices':
                result = torch.empty_like(a)
                indices = torch.empty((), dtype=torch.long, device=device)
                torch.median(a, 0, out=(result, indices))
            else:
                self.fail(f"'{call_type}' is not a valid call type")

        @expectedAlertNondeterministic('median CUDA with indices output', ['cuda'])
        def test_func_expect_error(slf, device, call_type):
            test_func(slf, device, call_type)

        test_func(self, device, 'function')
        test_func_expect_error(self, device, 'function with indices')
        test_func(self, device, 'method')
        test_func_expect_error(self, device, 'method with indices')
        test_func_expect_error(self, device, 'out with indices')

    def _test_gather_backward_one_dim(self, device, deterministic: bool = False) -> None:
        with DeterministicGuard(deterministic):
            m = random.randint(2000, 3000)
            elems = random.randint(10 * m, 20 * m)
            dim = 0
            src = torch.randn(m, device=device, requires_grad=True)
            idx = torch.randint(m, (elems,), device=device)
            res = torch.gather(src, dim, idx)
            weight = torch.rand_like(res, device=device) * 10 ** 6
            res.backward(weight)
            grad = src.grad.detach().clone()

            if torch.device(device).type == 'cuda':
                for _ in range(2):
                    src.grad.data.zero_()
                    res = torch.gather(src, dim, idx)
                    res.backward(weight)
                    self.assertEqual(src.grad, grad, atol=0, rtol=0)
            else:
                expected = torch.zeros_like(src, device=device)
                for i in range(elems):
                    expected[idx[i]] += weight[i]
                self.assertEqual(grad, expected, atol=0, rtol=0)

    @onlyNativeDeviceTypes
    def test_gather_backward_deterministic_path(self, device) -> None:
        self._test_gather_backward_one_dim(device, True)

    @onlyCPU
    def test_gather_backward_one_dim(self, device) -> None:
        self._test_gather_backward_one_dim(device, False)

    @onlyNativeDeviceTypes
    def test_scatter_add_one_dim_deterministic(self, device) -> None:
        with DeterministicGuard(True):
            m = random.randint(20, 30)
            elems = random.randint(2000 * m, 3000 * m)
            dim = 0
            src = torch.randn(elems, device=device)
            idx = torch.randint(m, (elems,), device=device)

            x = torch.zeros(m, device=device)
            res = x.scatter_add(dim, idx, src)

            expected = torch.zeros(m, device=device)
            for i in range(elems):
                expected[idx[i]] += src[i]

            self.assertEqual(res, expected, atol=0, rtol=0)

    @onlyCUDA
    def test_sync_warning(self, device):

        def _sync_raises_helper(f, level):
            with CudaSyncGuard(level):
                if level == 1:
                    with self.assertWarnsRegex(UserWarning, "called a synchronizing "):
                        f()
                elif level == 2:
                    with self.assertRaisesRegex(RuntimeError, "called a synchronizing "):
                        f()

        def _no_sync_helper(f, level):
            with CudaSyncGuard(level):
                f()

        def _ind_put_fn(x, ind, val):
            x[ind] = val
            return x

        def _ind_get_fn(x, ind):
            return x[ind]

        def _cond_fn(x):
            if x:  # taking boolean value of a tensor synchronizes
                return x
            else:
                return 2 * x

        # prepare inputs for subsequent ops
        size = 4
        x = torch.rand(size, device=device)
        y = torch.rand((), device=device)
        ind = torch.randint(size, (3,), device=device)
        ind_cpu = ind.cpu()
        repeats = torch.full((1,), 2, device=device)
        mask = torch.randint(2, (size,), device=device, dtype=bool)
        expect_no_sync = (lambda: _ind_put_fn(x, mask, 1.),
                          lambda: _ind_put_fn(x, ind, y),
                          lambda: _ind_get_fn(x, ind),
                          lambda: torch.nn.functional.one_hot(ind, num_classes=size),
                          lambda: torch.randperm(20000, device=device),
                          lambda: torch.repeat_interleave(x, 2, output_size=2 * size),
                          lambda: torch.repeat_interleave(x, repeats, output_size=2 * size))
        expect_sync = (lambda: _ind_put_fn(x, mask, y),
                       lambda: _ind_put_fn(x, ind_cpu, y),
                       lambda: _ind_get_fn(x, mask),
                       lambda: _ind_get_fn(x, ind_cpu),
                       lambda: x.nonzero(),
                       lambda: _cond_fn(y),
                       lambda: torch.nn.functional.one_hot(ind),
                       lambda: torch.repeat_interleave(x, 2),
                       lambda: torch.repeat_interleave(x, repeats))
        for f, level in product(expect_no_sync, (1, 2)):
            _no_sync_helper(f, level)
        for f, level in product(expect_sync, (1, 2)):
            _sync_raises_helper(f, level)


    @dtypes(*get_all_fp_dtypes())
    def test_log_normal(self, device, dtype):
        a = torch.tensor([10], dtype=dtype, device=device).log_normal_()
        self.assertEqual(a.dtype, dtype)
        self.assertEqual(a.size(), torch.Size([1]))

    @dtypes(*(get_all_int_dtypes() + get_all_fp_dtypes()))
    def test_geometric(self, device, dtype):
        a = torch.tensor([10], dtype=dtype, device=device).geometric_(0.5)
        self.assertEqual(a.dtype, dtype)
        self.assertEqual(a.size(), torch.Size([1]))

    def test_repeat_interleave(self, device):
        y = torch.tensor([[1, 2], [3, 4]], device=device)
        # exercise single argument function signature
        temp = y.repeat_interleave(2)
        self.assertEqual(torch.Size([8]), temp.size())

        for dtype in [torch.int, torch.long]:
            lengths = torch.tensor([1, 2], dtype=dtype, device=device)
            output_size = torch.sum(lengths)
            a = torch.repeat_interleave(
                y,
                lengths,
                dim=0,
            )
            self.assertEqual(a.dtype, y.dtype)
            self.assertEqual(a.size(), torch.Size([3, 2]))

            a_with_output = torch.repeat_interleave(
                y,
                lengths,
                dim=0,
                output_size=output_size,
            )
            self.assertEqual(a_with_output.dtype, y.dtype)
            self.assertEqual(a_with_output.size(), torch.Size([3, 2]))

    @dtypes(*get_all_fp_dtypes(include_half=False, include_bfloat16=False))
    @dtypesIfCPU(*(get_all_fp_dtypes(include_half=False, include_bfloat16=True)))
    @dtypesIfCUDA(*(get_all_fp_dtypes(include_bfloat16=False)))
    def test_bernoulli_p(self, device, dtype):
        for trivial_p in ([0, 1], [1, 0, 1, 1, 0, 1]):
            x = torch.tensor(trivial_p, dtype=dtype, device=device)
            self.assertEqual(x.bernoulli().tolist(), trivial_p)

        def isBinary(t):
            return torch.ne(t, 0).mul_(torch.ne(t, 1)).sum().item() == 0

        p = torch.rand(5, 5, dtype=dtype, device=device)
        self.assertTrue(isBinary(p.bernoulli()))

        p = torch.rand(5, dtype=dtype, device=device).expand(5, 5)
        self.assertTrue(isBinary(p.bernoulli()))

        p = torch.rand(5, 5, dtype=dtype, device=device)
        torch.bernoulli(torch.rand_like(p), out=p)
        self.assertTrue(isBinary(p))

    # RngUniform not implemented for Integral type in XLA test
    @dtypes(*(get_all_fp_dtypes(include_half=False, include_bfloat16=False)))
    @dtypesIfCPU(*(get_all_dtypes(include_half=False, include_bfloat16=False, include_complex=False)))
    @dtypesIfCUDA(*(get_all_dtypes(include_bfloat16=False, include_complex=False)))
    def test_bernoulli_self(self, device, dtype):

        def isBinary(t):
            return torch.ne(t, 0).mul_(torch.ne(t, 1)).sum().item() == 0

        t = torch.empty(10, 10, dtype=dtype, device=device)

        t.fill_(2)
        t.bernoulli_(0.5)
        self.assertTrue(isBinary(t))

        for p_dtype in get_all_fp_dtypes(include_half=device.startswith('cuda'), include_bfloat16=False):
            p = torch.rand(10, dtype=p_dtype, device=device).expand(10, 10)
            t.fill_(2)
            t.bernoulli_(p)
            self.assertTrue(isBinary(t))

            t.fill_(2)
            torch.bernoulli(torch.rand_like(t, dtype=p_dtype), out=t)
            self.assertTrue(isBinary(t))

            t.fill_(2)
            t.bernoulli_(torch.rand_like(t, dtype=p_dtype))
            self.assertTrue(isBinary(t))

    @slowTest
    @dtypes(*(get_all_fp_dtypes(include_half=False, include_bfloat16=False)))
    @dtypesIfCUDA(*(get_all_fp_dtypes(include_bfloat16=False)))
    def test_bernoulli_edge_cases(self, device, dtype):
        # Need to draw a lot of samples to cover every random floating point number.
        a = torch.zeros(10000, 10000, dtype=dtype, device=device)  # probability of drawing "1" is 0
        num_ones = (torch.bernoulli(a) == 1).sum()
        self.assertEqual(num_ones, 0)

        b = torch.ones(10000, 10000, dtype=dtype, device=device)  # probability of drawing "1" is 1
        num_zeros = (torch.bernoulli(b) == 0).sum()
        self.assertEqual(num_zeros, 0)

    @dtypes(*get_all_fp_dtypes())
    def test_exponential(self, device, dtype):
        a = torch.tensor([10], dtype=dtype, device=device).exponential_(0.5)
        self.assertEqual(a.dtype, dtype)
        self.assertEqual(a.size(), torch.Size([1]))

        # Tests extremal behavior
        tests = ((-0, float('inf')), (0, float('inf')), (float('inf'), 0))
        for test in tests:
            t = torch.empty((1,), device=device, dtype=dtype).exponential_(test[0])
            self.assertTrue(t.item() == test[1])

        # Tests that negative lambda fails
        with self.assertRaises(RuntimeError):
            torch.empty((1,), device=device, dtype=dtype).exponential_(-0.5)

    @onlyCUDA
    @dtypes(torch.half, torch.float)
    def test_exponential_no_zero(self, device, dtype):
        # naively, 0 in exponential can be generated with probability 2^-24
        # so we need more samples to check if it's not generated
        # instead of doing one
        # don't test CPU, that would be a long test
        x = torch.empty(50000000, device=device, dtype=dtype).exponential_()
        self.assertTrue(x.min() > 0)

    def _generate_correlation_tensors(self, device, dtype):
        yield make_tensor((0, 0), device, dtype)
        yield make_tensor((1, 0), device, dtype)
        yield make_tensor((0, 1), device, dtype)
        yield make_tensor((2,), device, dtype)
        yield make_tensor((2, 1), device, dtype)
        yield make_tensor((2, 2), device, dtype)
        yield make_tensor((2, 3), device, dtype)
        yield make_tensor((5, 10), device, dtype)
        yield make_tensor((5, 10), device, dtype, noncontiguous=True)
        if dtype != torch.int:
            yield torch.tensor([0, -2, nan, 10.2, inf], dtype=dtype, device=device)

    @onlyNativeDeviceTypes
    @dtypes(torch.int, torch.float, torch.cfloat)
    def test_corrcoef(self, device, dtype):
        for x in self._generate_correlation_tensors(device, dtype):
            res = torch.corrcoef(x)
            ref = np.corrcoef(x.cpu().numpy())
            self.assertEqual(res, ref, exact_dtype=False)

    @dtypes(torch.int, torch.float, torch.cfloat)
    def test_cov(self, device, dtype):
        def check(t, correction=1, fweights=None, aweights=None):
            res = torch.cov(t, correction=correction, fweights=fweights, aweights=aweights)
            t = t.cpu().numpy()
            fweights = fweights.cpu().numpy() if fweights is not None else None
            aweights = aweights.cpu().numpy() if aweights is not None else None
            ref = np.cov(t, ddof=correction, fweights=fweights, aweights=aweights)
            self.assertEqual(res, ref, atol=1e-05, rtol=1e-05, exact_dtype=False)

        for x in self._generate_correlation_tensors(device, dtype):
            check(x)
            num_observations = x.numel() if x.ndim < 2 else x.size(1)
            if num_observations > 0:
                fweights = torch.randint(1, 10, (num_observations,), device=device)
                aweights = make_tensor((num_observations,), device, torch.float, low=1)
                for correction, fw, aw in product([0, 1, 2], [None, fweights], [None, aweights]):
                    check(x, correction, fweights, aweights)

    def test_cov_error(self, device):
        def check(msg, *args, **kwargs):
            with self.assertRaisesRegex(RuntimeError, r'cov\(\):.*' + msg + r'.*'):
                torch.cov(*args, **kwargs)

        a = torch.rand(2)
        check(r'expected input to have two or fewer dimensions', torch.rand(2, 2, 2))
        check(r'expected fweights to have one or fewer dimensions', a, fweights=torch.rand(2, 2))
        check(r'expected aweights to have one or fewer dimensions', a, aweights=torch.rand(2, 2))
        check(r'expected fweights to have integral dtype', a, fweights=torch.rand(2))
        check(r'expected aweights to have floating point dtype', a, aweights=torch.tensor([1, 1]))
        check(r'expected fweights to have the same numel', a, fweights=torch.tensor([1]))
        check(r'expected aweights to have the same numel', a, aweights=torch.rand(1))
        check(r'fweights cannot be negative', a, fweights=torch.tensor([-1, -2]))
        check(r'aweights cannot be negative', a, aweights=torch.tensor([-1., -2.]))

    @skipIfNoSciPy
    @dtypes(*get_all_fp_dtypes())
    def test_uniform_kstest(self, device, dtype):
        from scipy import stats
        size = 1000
        for from_ in [-42, 0, 4.2]:
            for to_ in [-4.2, 0, 42]:
                if to_ > from_:
                    t = torch.empty(size, dtype=dtype, device=device).uniform_(from_, to_)
                    res = stats.kstest(t.cpu().to(torch.double), 'uniform', args=(from_, (to_ - from_)))
                    self.assertTrue(res.statistic < 0.1)

    @skipIfNoSciPy
    @dtypes(*get_all_fp_dtypes(include_bfloat16=False))
    @dtypesIfCUDA(*get_all_fp_dtypes())
    def test_normal_kstest(self, device, dtype):
        from scipy import stats
        size = 1000
        for mean in [-10, 0, 50]:
            for std in [1, 5, 10]:
                t = torch.empty(size, dtype=dtype, device=device).normal_(mean=mean, std=std)
                res = stats.kstest(t.cpu().to(torch.double), 'norm', args=(mean, std))
                self.assertTrue(res.statistic < 0.1)

    @skipIfNoSciPy
    @dtypes(*get_all_fp_dtypes())
    def test_lognormal_kstest(self, device, dtype):
        from scipy import stats
        size = 1000
        for mean in [-3, 0, 7]:
            for std in [1, 5, 7]:
                t = torch.empty(size, dtype=dtype, device=device).log_normal_(mean=mean, std=std)
                res = stats.kstest(t.cpu().to(torch.double), 'lognorm', args=(std, 0, math.exp(mean)))
                if dtype == torch.half:
                    self.assertTrue(res.statistic < 0.3)
                else:
                    self.assertTrue(res.statistic < 0.1)

    @skipIfNoSciPy
    @dtypes(*get_all_fp_dtypes())
    def test_exponential_kstest(self, device, dtype):
        from scipy import stats
        size = 1000
        for lambd in [0.5, 1.0, 5.0]:
            t = torch.empty(size, dtype=dtype, device=device).exponential_(lambd=lambd)
            res = stats.kstest(t.cpu().to(torch.double), 'expon', args=(0, 1 / lambd,))
            self.assertTrue(res.statistic < 0.1)

    @skipIfNoSciPy
    @dtypes(*get_all_fp_dtypes())
    def test_cauchy_kstest(self, device, dtype):
        from scipy import stats
        size = 1000
        for median in [-10, 0, 50]:
            for sigma in [0.5, 1.0, 10.0]:
                t = torch.empty(size, dtype=dtype, device=device).cauchy_(median=median, sigma=sigma)
                res = stats.kstest(t.cpu().to(torch.double), 'cauchy', args=(median, sigma))
                self.assertTrue(res.statistic < 0.1)

    @slowTest
    @onlyCUDA
    @dtypes(torch.bfloat16, torch.float32)
    def test_cauchy_no_inf(self, device, dtype):
        # torch.float16 will have `inf` because of its smaller range.
        for _ in range((2**16) * 2):
            x = torch.empty((2**16), dtype=dtype, device=device)
            x.cauchy_()
            self.assertFalse(x.isinf().sum())

    @skipIfNoSciPy
    @dtypes(*(get_all_int_dtypes() + get_all_fp_dtypes()))
    def test_geometric_kstest(self, device, dtype):
        from scipy import stats
        size = 1000
        for p in [0.2, 0.5, 0.8]:
            t = torch.empty(size, dtype=dtype, device=device).geometric_(p=p)
            actual = np.histogram(t.cpu().to(torch.double), np.arange(1, 100))[0]
            expected = stats.geom(p).pmf(np.arange(1, 99)) * size
            res = stats.chisquare(actual, expected)
            self.assertEqual(res.pvalue, 1.0, atol=0.1, rtol=0)

    def test_pairwise_distance_empty(self, device):
        shape = (2, 0)
        x = torch.randn(shape, device=device)
        y = torch.randn(shape, device=device)

        self.assertEqual(torch.zeros(2, device=device), torch.pairwise_distance(x, y))
        self.assertEqual(torch.zeros((2, 1), device=device), torch.pairwise_distance(x, y, keepdim=True))

        shape = (0, 2)
        x = torch.randn(shape, device=device)
        y = torch.randn(shape, device=device)
        self.assertEqual(torch.zeros(0, device=device), torch.pairwise_distance(x, y))
        self.assertEqual(torch.zeros((0, 1), device=device), torch.pairwise_distance(x, y, keepdim=True))

    def test_pdist_empty(self, device):
        shape = (0, 2)
        x = torch.randn(shape, device=device)
        self.assertEqual(torch.empty(0, device=device), torch.pdist(x))

        shape = (1, 2)
        x = torch.randn(shape, device=device)
        self.assertEqual(torch.empty(0, device=device), torch.pdist(x))

        shape = (3, 0)
        x = torch.randn(shape, device=device)
        self.assertEqual(torch.zeros(3, device=device), torch.pdist(x))

    def test_cdist_empty(self, device):
        x = torch.randn((0, 5), device=device)
        y = torch.randn((4, 5), device=device)
        self.assertEqual(torch.empty(0, 4, device=device), torch.cdist(x, y))

        x = torch.randn((2, 5), device=device)
        y = torch.randn((0, 5), device=device)
        self.assertEqual(torch.empty(2, 0, device=device), torch.cdist(x, y))

        x = torch.randn((2, 0), device=device)
        y = torch.randn((3, 0), device=device)
        self.assertEqual(torch.zeros(2, 3, device=device), torch.cdist(x, y))

        x = torch.randn((2, 0), device=device)
        y = torch.randn((0, 0), device=device)
        self.assertEqual(torch.empty(2, 0, device=device), torch.cdist(x, y))

    def _brute_cdist(self, x, y, p=2):
        r1 = x.shape[-2]
        r2 = y.shape[-2]
        if r1 == 0 or r2 == 0:
            return torch.empty(r1, r2, device=x.device)
        return torch.norm(x[..., None, :] - y[..., None, :, :], p=p, dim=-1)

    def test_cdist_norm(self, device):
        for r1 in [3, 4, 5, 6]:
            for m in [2, 3, 4, 10]:
                for r2 in [4, 6, 7, 8]:
                    for p in [0, 1, 2, 3, 1.5, 2.5, float('inf')]:
                        x = torch.randn(r1, m, device=device)
                        y = torch.randn(r2, m, device=device)
                        if p == 2:
                            for cm in ['use_mm_for_euclid_dist', 'donot_use_mm_for_euclid_dist']:
                                actual = torch.cdist(x, y, p=2, compute_mode=cm)
                                expected = self._brute_cdist(x, y, p=2)
                                self.assertEqual(expected, actual, rtol=0, atol=0.02)
                        else:
                            actual = torch.cdist(x, y, p=p)
                            expected = self._brute_cdist(x, y, p=p)
                            self.assertEqual(expected, actual)

    def test_cdist_norm_batch(self, device):
        for r1 in [3, 4, 5, 6]:
            for m in [2, 3, 4, 10]:
                for r2 in [4, 6, 7, 8]:
                    for p in [0, 1, 2, 3, 1.5, 2.5, float('inf')]:
                        x = torch.randn(2, 3, 6, r1, m, device=device)
                        y = torch.randn(2, 3, 6, r2, m, device=device)
                        if p == 2:
                            for cm in ['use_mm_for_euclid_dist', 'donot_use_mm_for_euclid_dist']:
                                actual = torch.cdist(x, y, p=2, compute_mode=cm)
                                expected = self._brute_cdist(x, y, p=2)
                                self.assertEqual(expected, actual, rtol=0, atol=0.02)
                        else:
                            actual = torch.cdist(x, y, p=p)
                            expected = self._brute_cdist(x, y, p=p)
                            self.assertEqual(expected, actual)

    @onlyCUDA
    def test_cdist_cuda_backward(self, device):
        for l1 in [1, 511, 513]:
            for l2 in [1, 511, 513]:
                for p in [0, 1, 2, 3, 1.5, 2.5, float('inf')]:
                    x1 = torch.randn(4, l1, 32, device=device, requires_grad=True)
                    x2 = x1.clone().detach_().requires_grad_()
                    y1 = torch.randn(4, l2, 32, device=device, requires_grad=True)
                    y2 = y1.clone().detach_().requires_grad_()
                    if p == 2:
                        for cm in ['use_mm_for_euclid_dist', 'donot_use_mm_for_euclid_dist']:
                            z1 = torch.cdist(x1, y1, p=2, compute_mode=cm).mean()
                            z2 = self._brute_cdist(x2, y2, p=2).mean()
                            z1.backward()
                            z2.backward()
                            self.assertEqual(x1.grad, x2.grad, rtol=0, atol=0.001)
                            self.assertEqual(y1.grad, y2.grad, rtol=0, atol=0.001)
                    else:
                        z1 = torch.cdist(x1, y1, p=p).mean()
                        z2 = self._brute_cdist(x2, y2, p=p).mean()
                        self.assertEqual(x1.grad, x2.grad, rtol=0, atol=0.001)
                        self.assertEqual(y1.grad, y2.grad, rtol=0, atol=0.001)

    @tf32_on_and_off(0.005)
    def test_cdist_large(self, device):
        for cm in ['use_mm_for_euclid_dist_if_necessary', 'use_mm_for_euclid_dist', 'donot_use_mm_for_euclid_dist']:
            x = torch.randn(1000, 10, device=device)
            y = torch.randn(1000, 10, device=device)
            actual = torch.cdist(x, y, p=2, compute_mode=cm)
            expected = self._brute_cdist(x, y, p=2)
            self.assertEqual(expected, actual)

    @slowTest
    @tf32_on_and_off(0.01)
    def test_cdist_large_batch(self, device):
        for cm in ['use_mm_for_euclid_dist_if_necessary', 'use_mm_for_euclid_dist', 'donot_use_mm_for_euclid_dist']:
            x = torch.randn(4, 3, 1000, 10, device=device)
            y = torch.randn(4, 3, 1000, 10, device=device)
            actual = torch.cdist(x, y, p=2, compute_mode=cm)
            expected = self._brute_cdist(x, y, p=2)
            self.assertEqual(expected, actual)

    @tf32_on_and_off(0.005)
    def test_cdist_non_contiguous(self, device):
        for cm in ['use_mm_for_euclid_dist', 'donot_use_mm_for_euclid_dist']:
            x = torch.randn(5, 7, device=device).mT
            y = torch.randn(5, 3, device=device).mT
            actual = torch.cdist(x, y, p=2, compute_mode=cm)
            expected = self._brute_cdist(x, y, p=2)
            self.assertFalse(x.is_contiguous())
            self.assertFalse(y.is_contiguous())
            self.assertEqual(expected, actual)

            x = torch.randn(7, 5, device=device)
            y = torch.randn(5, 3, device=device).t()
            actual = torch.cdist(x, y, p=2, compute_mode=cm)
            expected = self._brute_cdist(x, y, p=2)
            self.assertTrue(x.is_contiguous())
            self.assertFalse(y.is_contiguous())
            self.assertEqual(expected, actual)

            x = torch.randn(5, 7, device=device).t()
            y = torch.randn(3, 5, device=device)
            actual = torch.cdist(x, y, p=2, compute_mode=cm)
            expected = self._brute_cdist(x, y, p=2)
            self.assertFalse(x.is_contiguous())
            self.assertTrue(y.is_contiguous())
            self.assertEqual(expected, actual)

    @tf32_on_and_off()
    def test_cdist_non_contiguous_batch(self, device):
        for cm in ['use_mm_for_euclid_dist', 'donot_use_mm_for_euclid_dist']:
            x = torch.randn(4, 3, 2, 5, 7, device=device).mT
            y = torch.randn(4, 3, 2, 5, 3, device=device).mT
            actual = torch.cdist(x, y, p=2, compute_mode=cm)
            expected = self._brute_cdist(x, y, p=2)
            self.assertFalse(x.is_contiguous())
            self.assertFalse(y.is_contiguous())
            self.assertEqual(expected, actual)

            x = torch.randn(7, 2, 7, 5, device=device)
            y = torch.randn(7, 2, 5, 3, device=device).mT
            actual = torch.cdist(x, y, p=2, compute_mode=cm)
            expected = self._brute_cdist(x, y, p=2)
            self.assertTrue(x.is_contiguous())
            self.assertFalse(y.is_contiguous())
            self.assertEqual(expected, actual)

            x = torch.randn(4, 5, 7, device=device).mT
            y = torch.randn(4, 3, 5, device=device)
            actual = torch.cdist(x, y, p=2, compute_mode=cm)
            expected = self._brute_cdist(x, y, p=2)
            self.assertFalse(x.is_contiguous())
            self.assertTrue(y.is_contiguous())
            self.assertEqual(expected, actual)

    # Maybe merge into OpInfo?
    def test_cdist_euclidean_large(self, device):
        def _test_euclidean_large_cdist(sizex, sizey=None):
            if sizey is None:
                sizey = sizex
            x = torch.randn(sizex, device=device, dtype=torch.float)
            y = torch.randn(sizey, device=device, dtype=torch.float)
            eps = 1e-6
            # to avoid extremum
            x = x - (((x - y) < eps).float() * 2 * eps)
            x.requires_grad = True
            y.requires_grad = True
            dist = torch.cdist(x, y, p=2)
            # Do a backward pass to check that it is valid for large
            # matrices
            loss = dist.sum()
            loss.backward()

        _test_euclidean_large_cdist((2000, 5))

    # Ensure that cdist backward with p<1 does not produce NaNs
    def test_cdist_grad_p_lt_1_no_nan(self, device):
        for p in [0.99, 0.7, 0.5, 0.1, 0.01]:
            x = torch.randn(1, 2, device=device)
            y = x.clone().detach() + torch.tensor([[1., 0.]], device=device)
            x.requires_grad = True
            y.requires_grad = True
            result = torch.cdist(x, y, p=p)
            result.backward(torch.ones_like(result))
            self.assertFalse(torch.isnan(x.grad).any())
            self.assertFalse(torch.isnan(y.grad).any())

    def test_cdist_same_inputs(self, device):
        # Test to detect issues in cdist gradient calculation
        # When the distances are 0
        sizex = (1, 27, 32)
        for p in [0, 1, 2, 3, 1.5, 2.5, float('inf')]:
            x = torch.randn(sizex, device=device, dtype=torch.float)
            dist_grad = torch.randn((1, 27, 27), device=device, dtype=torch.float)
            y = x.clone()
            eps = 1e-6
            x.requires_grad = True
            d = torch.cdist(x, y)
            d.backward(dist_grad)
            # Check that the backward passs does not contain invalid
            # values such as nan or inf
            assert torch.isfinite(x.grad).all()

    def test_multinomial_constraints(self, device):
        x = torch.empty(1, 2, 3, dtype=torch.double, device=device)
        self.assertRaisesRegex(
            RuntimeError, "prob_dist must be 1 or 2 dim",
            lambda: torch.multinomial(x, 2))
        x = torch.empty(1, 2, dtype=torch.long, device=device)
        self.assertRaisesRegex(
            RuntimeError, "multinomial only supports floating-point dtypes for input",
            lambda: torch.multinomial(x, 2))
        x = torch.empty(1, 2, dtype=torch.double, device=device)
        y = torch.empty(1, 2, dtype=torch.double, device=device)
        self.assertRaisesRegex(
            RuntimeError, "multinomial expects Long tensor out",
            lambda: torch.multinomial(x, 2, out=y))
        x = torch.empty(2, dtype=torch.double, device=device)
        self.assertRaisesRegex(
            RuntimeError, "cannot sample n_sample <= 0 samples",
            lambda: torch.multinomial(x, 0))
        x = torch.empty(2, dtype=torch.double, device=device)
        self.assertRaisesRegex(
            RuntimeError, "cannot sample n_sample <= 0 samples",
            lambda: torch.multinomial(x, -1))
        x = torch.empty(2, dtype=torch.double, device=device)
        self.assertRaisesRegex(
            RuntimeError, "cannot sample n_sample > prob_dist",
            lambda: torch.multinomial(x, 3, False))
        x = torch.empty(16777217, dtype=torch.double, device=device)
        self.assertRaisesRegex(
            RuntimeError, "number of categories cannot exceed",
            lambda: torch.multinomial(x, 3))

    def test_cumsum(self, device):
        x = torch.rand(100, 100, device=device)
        res1 = torch.cumsum(x, 1)
        res2 = torch.tensor([]).to(device)
        torch.cumsum(x, 1, out=res2)
        self.assertEqual(res1, res2)
        x.cumsum_(1)
        self.assertEqual(res1, x)

        a = torch.tensor([[True, False, True],
                          [False, False, False],
                          [True, True, True]], device=device)
        b = a.byte()
        aRes = torch.cumsum(a, 0)
        bRes = torch.cumsum(b, 0)
        self.assertEqual(aRes, bRes)
        self.assertEqual(aRes, torch.tensor([[1, 0, 1],
                                             [1, 0, 1],
                                             [2, 1, 2]]))

        aRes = torch.cumsum(a, 1)
        bRes = torch.cumsum(b, 1)
        self.assertEqual(aRes, bRes)
        self.assertEqual(aRes, torch.tensor([[1, 1, 2],
                                             [0, 0, 0],
                                             [1, 2, 3]]))

        # Check that cummulative sum over a zero length dimension doesn't crash on backprop.
        # Also check that cumsum over other dimensions in a tensor with a zero-length
        # dimensiuon also works
        # Also include a basic suite of similar tests for other bases cases.
        shapes = [[2, 0], [2, 1, 4], [0, 2, 3], [1], [5]]
        for shape in shapes:
            for dim in range(len(shape)):
                raw_tensor = torch.zeros(*shape, requires_grad=True)
                integrated = raw_tensor.cumsum(dim=dim)
                # Check that backward does not crash
                integrated.sum().backward()
                # Check that output maintained correct shape
                self.assertEqual(raw_tensor.shape, raw_tensor.grad.shape)

        # Check a scalar example
        raw_tensor = torch.tensor(3., requires_grad=True)
        integrated = raw_tensor.cumsum(dim=-1)
        self.assertEqual(raw_tensor, integrated)
        # Check that backward does not crash
        integrated.sum().backward()
        # Check that output maintained correct shape
        self.assertEqual(raw_tensor.shape, raw_tensor.grad.shape)

    def test_cumprod(self, device):
        x = torch.rand(100, 100, device=device)
        res1 = torch.cumprod(x, 1)
        res2 = torch.tensor([]).to(device)
        torch.cumprod(x, 1, out=res2)
        self.assertEqual(res1, res2)
        x.cumprod_(1)
        self.assertEqual(res1, x)

        a = torch.tensor([[True, False, True],
                          [False, False, False],
                          [True, True, True]], dtype=torch.bool, device=device)
        b = a.byte()
        aRes = torch.cumprod(a, 0)
        bRes = torch.cumprod(b, 0)
        self.assertEqual(aRes, bRes)
        self.assertEqual(aRes, torch.tensor([[1, 0, 1],
                                             [0, 0, 0],
                                             [0, 0, 0]]))

        aRes = torch.cumprod(a, 1)
        bRes = torch.cumprod(b, 1)
        self.assertEqual(aRes, bRes)
        self.assertEqual(aRes, torch.tensor([[1, 0, 0],
                                             [0, 0, 0],
                                             [1, 1, 1]]))

        # Check that cummulative prod over a zero length dimension doesn't crash on backprop.
        # Also check that cumprod over other dimensions in a tensor with a zero-length
        # dimensiuon also works
        # Also include a basic suite of similar tests for other bases cases.
        shapes = [[2, 0], [2, 1, 4], [0, 2, 3], [1], [5]]
        for shape in shapes:
            for dim in range(len(shape)):
                raw_tensor = torch.zeros(*shape, requires_grad=True)
                integrated = raw_tensor.cumprod(dim=dim)
                # Check that backward does not crash
                integrated.sum().backward()
                # Check that output maintained correct shape
                self.assertEqual(raw_tensor.shape, raw_tensor.grad.shape)

        # Check a scalar example
        raw_tensor = torch.tensor(3., requires_grad=True)
        integrated = raw_tensor.cumprod(dim=-1)
        self.assertEqual(raw_tensor, integrated)
        # Check that backward does not crash
        integrated.sum().backward()
        # Check that output maintained correct shape
        self.assertEqual(raw_tensor.shape, raw_tensor.grad.shape)

    def test_cummax_cummin(self, device):
        def test_ops(op, string_of_function_name, expected_output1, expected_output2):
            x = torch.rand(100, 100, device=device)
            out1 = op(x, 1)
            res2 = torch.empty(0, device=device)
            indices2 = torch.empty(0, dtype=torch.int64, device=device)
            op(x, 1, out=(res2, indices2))
            self.assertEqual(out1[0], res2)
            self.assertEqual(out1[1], indices2)

            a = torch.tensor([[True, False, True],
                              [False, False, False],
                              [True, True, True]], dtype=torch.bool, device=device)
            b = a.byte()
            aRes = op(a, 0)
            bRes = op(b, 0)
            self.assertEqual(aRes[0], bRes[0].bool())
            self.assertEqual(aRes[0], expected_output1.bool())

            # test inf and nan input
            x = torch.tensor([4, inf, 1.5, -inf, 0, nan, 1])
            xRes = op(x, 0)[0]
            self.assertEqual(xRes, expected_output2)

            # op shouldn't support values, indices with a dtype, device type or layout
            # different from that of input tensor
            t = torch.randn(10)
            values = torch.empty(0, dtype=torch.int16)
            indices = torch.empty(0, dtype=torch.int64)
            with self.assertRaisesRegex(
                    RuntimeError,
                    'expected scalar_type Float but found Short'):
                op(t, 0, out=(values, indices))

            # Check that op over a zero length dimension doesn't crash on backprop.
            # Also check that op over other dimensions in a tensor with a zero-length
            # dimension also works
            # Also include a basic suite of similar tests for other bases cases.
            shapes = [[2, 0], [2, 1, 4], [0, 2, 3], [1], [5]]
            for shape in shapes:
                for dim in range(len(shape)):
                    raw_tensor = torch.zeros(*shape, requires_grad=True)
                    integrated = getattr(raw_tensor, string_of_function_name)(dim=dim)
                    # Check that backward does not crash
                    integrated[0].sum().backward()
                    # Check that output maintained correct shape
                    self.assertEqual(raw_tensor.shape, raw_tensor.grad.shape)

            # Check a scalar example
            raw_tensor = torch.tensor(3., requires_grad=True)
            integrated = getattr(raw_tensor, string_of_function_name)(dim=-1)
            # Check that backward does not crash
            integrated[0].sum().backward()
            # Check that output maintained correct shape
            self.assertEqual(raw_tensor.shape, raw_tensor.grad.shape)

        expected_out = torch.tensor([4, inf, inf, inf, inf, nan, nan])
        test_ops(torch.cummax, "cummax", torch.tensor([[1, 0, 1],
                                                       [1, 0, 1],
                                                       [1, 1, 1]]), expected_out)

        expected_out = torch.tensor([4, 4, 1.5, -inf, -inf, nan, nan])
        test_ops(torch.cummin, "cummin", torch.tensor([[1, 0, 1],
                                                       [0, 0, 0],
                                                       [0, 0, 0]]), expected_out)

    def test_logcumsumexp(self, device):
        def logcumsumexp(a, axis):
            return torch.cumsum(a.exp(), axis=axis).log_()

        axis = -1
        a = torch.randn(100, 100, device=device)

        actual = a.logcumsumexp(axis)
        expected = logcumsumexp(a, axis)
        self.assertEqual(a.dtype, actual.dtype)
        self.assertEqual(expected.shape, actual.shape)
        self.assertEqual(expected, actual)

        # check -inf and nan handling
        x = torch.tensor([-float('inf'), -float('inf'), 1.0, 1.0, float('inf'),
                         float('inf'), float('nan'), 1.0, 1.0], device=device)
        x2d = x.unsqueeze(0).expand(2, -1)

        for inp in (x, x2d):
            actual = inp.logcumsumexp(axis)
            expected = logcumsumexp(inp, axis)
            self.assertEqual(expected, actual)

        # Check that out is actually inplace
        b = torch.randn(5, 2, device=device)
        inplace_out = torch.zeros(5, 2, device=device)

        expected = logcumsumexp(b, axis)
        torch.logcumsumexp(b, axis=axis, out=inplace_out)

        self.assertEqual(inplace_out, expected)

        # Check input and inplace_output type mismatch
        b = torch.randn(5, 2, device=device, dtype=torch.float64)
        inplace_out = torch.zeros(5, 2, device=device, dtype=torch.float32)
        with self.assertRaisesRegex(
                RuntimeError,
                'expected scalar_type Double but found Float'):
            torch.logcumsumexp(b, axis, out=inplace_out)

    def _test_diff_numpy(self, t, dims=None):
        # Helper for test_diff to compare with NumPy reference implementation
        def to_np(t):
            if t.dtype == torch.bfloat16:
                return t.to(dtype=torch.float, device="cpu").numpy()
            else:
                return t.cpu().numpy()

        for dim in dims if dims else range(t.dim()):
            prepend = t.narrow(dim, 0, 1)
            append = t.narrow(dim, 0, 1)
            np_t = to_np(t)

            # test when no prepend and append
            for n in range(t.size(dim)):
                actual = torch.diff(t, dim=dim, n=n)
                expected = torch.from_numpy(np.diff(np_t, axis=dim, n=n))
                self.assertEqual(actual, expected.to(t.dtype))

            # test when prepend and append's size along dim is 1
            for n in range(1, t.size(dim) + 4):
                actual = torch.diff(t, dim=dim, n=n, prepend=prepend, append=append)
                expected = torch.from_numpy(np.diff(np_t, axis=dim, n=n, prepend=to_np(prepend), append=to_np(append)))
                self.assertEqual(actual, expected.to(t.dtype))

            # test when prepend and append's size along dim != 1
            for n in range(1, t.size(dim) * 3):
                actual = torch.diff(t, dim=dim, n=n, prepend=t, append=t)
                expected = torch.from_numpy(np.diff(np_t, axis=dim, n=n, prepend=np_t, append=np_t))
                self.assertEqual(actual, expected.to(t.dtype))

    # All tensors appear contiguous on XLA
    @onlyNativeDeviceTypes
    @dtypes(*get_all_dtypes(include_bfloat16=False))
    def test_diff_noncontig(self, device, dtype):
        shapes = (
            (1,),
            (1, 5),
            (3, 5),
            (1, 5, 1),
            (2, 3, 5))

        for shape in shapes:
            contig = make_tensor(shape, device, dtype, low=-9, high=9)

            non_contig = torch.empty(shape + (2, 2), device=device, dtype=dtype)[..., 0]
            non_contig = non_contig.select(-1, -1)
            non_contig.copy_(contig)
            self.assertTrue(not non_contig.is_contiguous() or shape == (1,))

            self._test_diff_numpy(non_contig)

    # RngNormal not implemented for type f16 for XLA
    @dtypes(*get_all_dtypes(include_half=False, include_bfloat16=False))
    @dtypesIfCPU(*get_all_dtypes(include_bfloat16=False))
    @dtypesIfCUDA(*get_all_dtypes(include_bfloat16=False))
    def test_diff(self, device, dtype):
        shapes = (
            (1,),
            (1, 5),
            (3, 5),
            (1, 5, 1),
            (2, 3, 5))

        for shape in shapes:
            contig = make_tensor(shape, device, dtype, low=-9, high=9)
            self._test_diff_numpy(contig)

        t = torch.ones(2, 3)

        with self.assertRaisesRegex(
                RuntimeError, 'diff expects prepend or append to be the same dimension as input'):
            invalid_prepend = torch.tensor([1, 2, 3], device=device, dtype=dtype)
            t.diff(dim=0, prepend=invalid_prepend)

        with self.assertRaisesRegex(
                RuntimeError, 'diff expects the shape of tensor to prepend or append to match that of input'):
            invalid_prepend = torch.tensor([[0, 1]], device=device, dtype=dtype)
            t.diff(dim=0, prepend=invalid_prepend)

        with self.assertRaisesRegex(
                RuntimeError, 'diff expects input to be at least one-dimensional'):
            scalar = torch.tensor(2, device=device, dtype=dtype)
            torch.diff(scalar)

    # if the given input arg is not a list, it returns a list of single element: [arg]
    def _wrap_to_list(self, input_array):
        return input_array if isinstance(input_array, list) else [input_array]

    # To ensure inf, -inf, and nan values do not cause divergence between Numpy and PyTorch.
    # There are two types of possible divergence:
    # 1. When we compute a,b both real numbers and has very small absolute values (i.e. very near to 0.0)
    # then, result of a/b be inf, -inf and nan, and this cause divergence.
    # 2. When we are dividing complex numbers by zero. For example, when a = torch.tensor(3+5j) we have
    # a/0 to be equal to nan + nan*j in PyTorch and inf + inf*j in Numpy.
    def _inf_nan_preprocess(self, actual, expected):
        for i in range(len(expected)):
            expected[i] = np.nan_to_num(expected[i], nan=nan, posinf=nan, neginf=nan)
            # nan_to_num is not defined for complex tensors in PyTorch.
            if actual[i].dtype == torch.complex64 :
                actual[i].real = torch.nan_to_num(actual[i].real, nan=nan, posinf=nan, neginf=nan)
                actual[i].imag = torch.nan_to_num(actual[i].imag, nan=nan, posinf=nan, neginf=nan)
            else:
                actual[i] = torch.nan_to_num(actual[i], nan=nan, posinf=nan, neginf=nan)

        return actual, expected

    @onlyNativeDeviceTypes
    @dtypes(torch.long, torch.float32, torch.complex64)
    def test_gradient_all(self, device, dtype):
        def create_scalar(shape):
            return make_tensor((1,), device='cpu', dtype=dtype, low=1.).item()

        def create_list(shape):
            return make_tensor((len(shape),), device='cpu', dtype=dtype, low=1.).tolist()

        def create_coordinate_tensors(shape):
            tensor_list = []
            for i in range(len(shape)):
                tensor_list.append(make_tensor((shape[i],), device=device, dtype=dtype))
            return tensor_list

        def filter_shape(shape, dim):
            filtered_shape = []
            for i in range(len(dim)):
                filtered_shape.append(shape[dim[i]])
            return filtered_shape

        # shape, dims format
        test_cases = (
            ((5,), (0,)),
            ((4, 4), (0, 1)),
            ((3, 3, 3), (-1, 0)),
            ((4, 4, 4), (2,)),
            ((4, 4, 4), (0, 1)),
            ((4, 4, 4, 3), (0, 2, 3)),
            ((4, 5, 3, 4, 3), (1, 2)),
            ((4, 3, 6, 5, 3), (2, 4)),
            ((4, 3, 3, 5, 3), (0, 1, 2, 3, 4)),
        )

        for case, contig, edge_order, space_fn in product(test_cases, [True, False], [1, 2],
                                                          (create_scalar, create_list, create_coordinate_tensors)):
            shape, dims = case
            # filter shape by dims before passing filtered shape to create_* functions
            filtered_shape = filter_shape(shape, dims)

            spacing = space_fn(filtered_shape)
            t = make_tensor(shape, device=device, dtype=dtype, noncontiguous=not contig)
            t_np = t.cpu().numpy()

            actual = torch.gradient(t, spacing=spacing, dim=dims, edge_order=edge_order)
            if space_fn == create_coordinate_tensors and spacing[0].device != 'cpu':
                spacing = [space.cpu().detach().numpy() for space in spacing]
            expected = np.gradient(t_np, *self._wrap_to_list(spacing), axis=dims, edge_order=edge_order)
            actual, expected = self._inf_nan_preprocess(list(actual), self._wrap_to_list(expected))
            self.assertEqual(actual, expected, equal_nan=True, atol=1e-4, rtol=0, exact_dtype=False)

    @onlyNativeDeviceTypes
    @dtypes(torch.long, torch.float32, torch.complex64)
    def test_gradient_extreme_cases(self, device, dtype):
        # Test behaviour for inf and nan values
        actual = torch.gradient(torch.tensor([2, -2, inf, inf, -inf, -inf, inf, 3, -inf, 2, nan, nan, 3, inf, nan]))
        expected = np.gradient(np.array([2, -2, inf, inf, -inf, -inf, inf, 3, -inf, 2, nan, nan, 3, inf, nan]))
        self.assertEqual(actual, self._wrap_to_list(expected), exact_dtype=False)

        # Test behaviour in very big tensors
        large_size = 100000
        t = make_tensor((large_size,), device, dtype)
        t_np = t.cpu().numpy()
        coordinates_np = list(np.random.randn(large_size))
        coordinates = [torch.tensor(coordinates_np, device=device)]
        actual = torch.gradient(t, spacing=coordinates, dim=0, edge_order=1)
        expected = [np.gradient(t_np, coordinates_np, axis=0, edge_order=1)]
        self.assertEqual(actual, expected, exact_dtype=False)

        actual = torch.gradient(t, spacing=coordinates, dim=0, edge_order=2)
        expected = [np.gradient(t_np, coordinates_np, axis=0, edge_order=2)]
        self.assertEqual(actual, expected, exact_dtype=False)

    @onlyNativeDeviceTypes
    def test_gradient_type_promotion(self, device):
        inputs = (
            make_tensor((4, 4), device=device, dtype=torch.float32),
            make_tensor((4, 4), device=device, dtype=torch.complex64),
            make_tensor((4, 4), device=device, dtype=torch.int64),
        )

        spacing = (
            make_tensor((1,), device='cpu', dtype=torch.float32).item(),
            make_tensor((1,), device='cpu', dtype=torch.int64).item(),
            make_tensor((1,), device='cpu', dtype=torch.complex64).item(),
            make_tensor((2,), device='cpu', dtype=torch.float32, low=0.1).tolist(),
            make_tensor((2,), device='cpu', dtype=torch.int64, low=1).tolist(),
            make_tensor((2,), device='cpu', dtype=torch.complex64).tolist(),
            [make_tensor((4,), device=device, dtype=torch.float32),
             make_tensor((4,), device=device, dtype=torch.float32)],
            [make_tensor((4,), device=device, dtype=torch.int64),
             make_tensor((4,), device=device, dtype=torch.int64)],
            [make_tensor((4,), device=device, dtype=torch.complex64),
             make_tensor((4,), device=device, dtype=torch.complex64)],
        )

        for input, spacing_or_coord, edge_order in product(inputs, spacing, [1, 2]):
            input_np = input.cpu().numpy()
            input_np = input.cpu().numpy()
            actual = torch.gradient(input, spacing=spacing_or_coord, dim=(0, 1), edge_order=edge_order)
            spacing_or_coord_wrapped = self._wrap_to_list(spacing_or_coord)
            spacing_or_coord_np = []
            if torch.is_tensor(spacing_or_coord_wrapped[0]) and torch.device(spacing_or_coord_wrapped[0].device).type != 'cpu':
                for i in range(len(spacing_or_coord_wrapped)):
                    spacing_or_coord_np.append(spacing_or_coord_wrapped[i].detach().clone().cpu().numpy())
            else:
                spacing_or_coord_np = spacing_or_coord_wrapped
            expected = np.gradient(input_np, *spacing_or_coord_np, axis=(0, 1), edge_order=edge_order)
            if actual[0].dtype == torch.complex64 and input.dtype != torch.complex64:
                for i in range(len(actual)):
                    self.assertEqual(actual[i].real, expected[i].real, exact_dtype=False)
                    # Type promotion fails on Numpy when spacing is given as complex number and input is given as real.
                    # Result is given just as real number and all the imaginary parts to be equal to zero.
                    self.assertEqual(expected[i].imag, torch.zeros(actual[i].shape), exact_dtype=False)
            else:
                actual, expected = self._inf_nan_preprocess(list(actual), expected)
                self.assertEqual(actual, expected, equal_nan=True, exact_dtype=False)

    @onlyNativeDeviceTypes
    @dtypes(torch.long, torch.float32, torch.complex64)
    def test_error_gradient(self, device, dtype):
        t = torch.tensor([[1, 2, 3], [4, 5, 6], [7, 8, 9]], device=device, dtype=dtype)
        with self.assertRaisesRegex(RuntimeError, 'torch.gradient expected spacing to be unspecified, a scalar '):
            dim = (1, 0)
            spacing = [0.1]
            torch.gradient(t, spacing=spacing, dim=dim, edge_order=1)

        with self.assertRaisesRegex(RuntimeError, 'torch.gradient only supports edge_order=1 and edge_order=2.'):
            torch.gradient(t, edge_order=3)

        with self.assertRaisesRegex(RuntimeError, 'dim 1 appears multiple times in the list of dims'):
            dim = (1, 1)
            spacing = 0.1
            torch.gradient(t, spacing=spacing, dim=dim, edge_order=1)

        with self.assertRaisesRegex(RuntimeError, 'torch.gradient expected each tensor to be on the same device,'):
            dim = (0, 1)
            coordinates = [torch.tensor([1, 2, 4], device='cpu'), torch.tensor([1, 2, 4], device='meta')]
            torch.gradient(t, spacing=coordinates, dim=dim, edge_order=1)

        with self.assertRaises(IndexError):
            torch.gradient(t, dim=3)

        with self.assertRaisesRegex(RuntimeError, 'torch.gradient expected each dimension size to be at least'):
            torch.gradient(torch.tensor([[1], [2], [3]]), edge_order=1)

        with self.assertRaisesRegex(RuntimeError, 'torch.gradient expected each dimension size to be at least'):
            torch.gradient(torch.tensor([[1, 2], [3, 4]]), edge_order=2)

    def _test_large_cum_fn_helper(self, x, fn):
        x_cpu = x.cpu().float()
        expected = fn(x_cpu)
        actual = fn(x).cpu().float()
        self.assertEqual(expected, actual.cpu().float())

    @unittest.skipIf(IS_FBCODE and IS_REMOTE_GPU, "sandcastle OOM with current tpx gpu/re configuration")
    @onlyCUDA
    @dtypes(torch.half)  # only small dtype not to get oom
    def test_large_cumsum(self, device, dtype):
        # initialization to avoid overflow and half caveats
        x = torch.empty(2**30 + 200, device=device, dtype=dtype)
        x[::3] = -3
        x[1::3] = 2
        x[2::3] = 1
        self._test_large_cum_fn_helper(x, lambda x: torch.cumsum(x, 0))

    @onlyCUDA
    @dtypes(torch.half)  # only small dtype not to get oom
    def test_large_cumprod(self, device, dtype):
        # initialization to avoid overflow and half caveats
        x = torch.empty(2**30 + 200, device=device, dtype=dtype)
        x[::3] = 8
        x[1::3] = .25
        x[2::3] = .5
        self._test_large_cum_fn_helper(x, lambda x: torch.cumprod(x, 0))

    def test_discontiguous_out_cumsum(self, device):
        x = torch.randn(4, 8, device=device)
        y = torch.empty(4, 16, device=device)[:, ::2]
        out = torch.cumsum(x, 0)
        torch.cumsum(x, 0, out=y)
        self.assertFalse(y.is_contiguous())
        self.assertEqual(out, y, atol=0., rtol=0.)

    def _test_cumminmax_helper(self, x, fn, expected_val, expected_ind):
        val, ind = fn(x, -1)
        self.assertEqual(val, expected_val, atol=0, rtol=0)
        self.assertEqual(ind, expected_ind, atol=0, rtol=0)
        out_val = torch.empty_like(val).t().contiguous().t()
        out_ind = torch.empty_like(ind).t().contiguous().t()
        fn(x, -1, out=(out_val, out_ind))
        self.assertFalse(out_val.is_contiguous())
        self.assertFalse(out_ind.is_contiguous())
        self.assertEqual(out_val, expected_val, atol=0, rtol=0)
        self.assertEqual(out_ind, expected_ind, atol=0, rtol=0)

    def test_cummax_discontiguous(self, device):
        x = torch.tensor([[0, 1, 2, 3, 2, 1], [4, 5, 6, 5, 6, 7]], device=device, dtype=torch.float).t().contiguous().t()
        expected_val = torch.tensor([[0, 1, 2, 3, 3, 3], [4, 5, 6, 6, 6, 7]], device=device, dtype=torch.float)
        expected_ind = torch.tensor([[0, 1, 2, 3, 3, 3], [0, 1, 2, 2, 4, 5]], device=device, dtype=torch.long)
        self._test_cumminmax_helper(x, torch.cummax, expected_val, expected_ind)

    def test_cummin_discontiguous(self, device):
        x = torch.tensor([[3, 2, 1, 0, 1, 2], [7, 6, 5, 4, 5, 2]], device=device, dtype=torch.float).t().contiguous().t()
        expected_val = torch.tensor([[3, 2, 1, 0, 0, 0], [7, 6, 5, 4, 4, 2]], device=device, dtype=torch.float)
        expected_ind = torch.tensor([[0, 1, 2, 3, 3, 3], [0, 1, 2, 3, 3, 5]], device=device, dtype=torch.long)
        self._test_cumminmax_helper(x, torch.cummin, expected_val, expected_ind)

    def test_bool_tensor_value_change(self, device):
        x = torch.tensor([True, False], dtype=torch.bool, device=device)
        x[0] = False
        x[1] = True
        self.assertEqual(x, torch.tensor([False, True], dtype=torch.bool, device=device))

    def test_unfold_all_devices_and_dtypes(self, device):
        for dt in get_all_dtypes():

            if dt == torch.bool:
                x = torch.empty((0, 1, 3, 0), dtype=dt, device=device)
                self.assertEqual((0, 1, 1, 0, 3), x.unfold(2, 3, 2).shape)
            else:
                x = torch.empty((0, 1, 3, 0), dtype=dt, device=device)
                self.assertEqual((0, 1, 1, 0, 3), x.unfold(2, 3, 2).shape)

    def test_unfold_scalars(self, device):
        x = torch.tensor(0.5, device=device)
        # unfold on a 0-dimensional tensor should always return a 1-d dimensional
        # tensor of shape [size] (i.e., the second parameter to unfold)

        self.assertEqual(torch.empty(0, device=device), x.unfold(0, 0, 1))
        self.assertEqual(torch.empty(0, device=device), x.unfold(0, 0, 2))
        self.assertEqual(torch.tensor([0.5], device=device), x.unfold(0, 1, 1))

    def test_copy_all_dtypes_and_devices(self, device):
        from copy import copy
        for dt in get_all_dtypes():
            x = torch.tensor([1, 2, 3, 4], dtype=dt, device=device)
            x_clone = x.clone()
            y = copy(x)
            y.fill_(1)
            # copy is a shallow copy, only copies the tensor view,
            # not the data
            self.assertEqual(x, y)

    def test_clone_all_dtypes_and_devices(self, device):
        for dt in get_all_dtypes():
            x = torch.tensor((1, 1), dtype=dt, device=device)
            y = x.clone()
            self.assertEqual(x, y)

    def test_clone_zero_stride_dim(self, device):
        # stride zero, size 1 axis, not contiguous
        x = torch.randn(10)
        y = x.as_strided([2, 1, 5], [1, 0, 2])
        self.assertEqual(y, y.clone())

    def test_clone_not_memory_dense(self):
        # github issue: https://github.com/pytorch/pytorch/issues/64176
        x = torch.randn(10, 8).t()[::2, ::2]
        y = x.clone()
        # should retain permutation after densification
        self.assertTrue(y.stride() == (1, 4))

    @dtypesIfCUDA(*set(get_all_math_dtypes('cuda')))
    @dtypes(*set(get_all_math_dtypes('cpu')))
    def test_addcmul(self, device, dtype):
        # Returns floating or integral scalar corresponding to dtype
        def _number(floating, integer, dtype):
            if dtype in [torch.half, torch.float, torch.double, torch.bfloat16]:
                return floating
            elif dtype in [torch.cfloat, torch.cdouble]:
                return floating * (1 + 1j)
            else:
                return integer

        def rand_tensor(size, dtype, device):
            if dtype.is_floating_point or dtype.is_complex:
                return torch.rand(size=size, dtype=dtype, device=device)
            if dtype == torch.uint8:
                return torch.randint(1, 5, size=size, dtype=dtype, device=device)
            else:
                return torch.randint(-5, 5, size=size, dtype=dtype, device=device)

        a = rand_tensor((2, 2), dtype=dtype, device=device)
        b = rand_tensor((2, 2), dtype=dtype, device=device)
        c = rand_tensor((2, 2), dtype=dtype, device=device)

        alpha = _number(0.5, 3, dtype)

        actual = torch.addcmul(a, b, c, value=alpha)
        expected = a + alpha * b * c

        self.assertEqual(expected, actual)

        with self.assertWarnsOnceRegex(
                UserWarning, "This overload of addcmul is deprecated"):
            self.assertEqual(actual, torch.addcmul(a, alpha, b, c))

        if self.device_type == 'cuda' and dtype == torch.half:
            a = torch.tensor([60000.0], device=device, dtype=dtype)
            b = torch.tensor([60000.0], device=device, dtype=dtype)
            c = torch.tensor([2.0], device=device, dtype=dtype)
            out = torch.addcmul(a, b, c, value=-1)
            self.assertTrue(not (out.isnan() or out.isinf()))

    def test_narrow_empty(self, device):
        x = torch.randn(2, 3, 4, device=device)
        for d in range(x.dim()):
            y = x.narrow(d, x.size(d), 0)
            sz = list(x.size())
            sz[d] = 0
            self.assertEqual(sz, y.size())

    @dtypes(*get_all_dtypes())
    def test_index_copy(self, device, dtype):
        # We just test for num_copy <= num_dest, as otherwise there are repeated indices
        # and the behavior is undefined
        num_copy, num_dest = 3, 5

        def make_arg(batch_sizes, n, dim, contig):
            size_arg = batch_sizes[:dim] + (n,) + batch_sizes[dim:]
            return make_tensor(size_arg, device, dtype, low=None, high=None, noncontiguous=not contig)

        def ref_index_copy(tgt, dim, idx, src):
            for i in range(idx.size(0)):
                idx_dest = dim * (slice(None),) + (idx[i],)
                idx_src = dim * (slice(None),) + (i,)
                tgt[idx_dest] = src[idx_src]

        # More thorough testing as in index_add
        for dest_contig, src_contig, index_contig in product([True, False], repeat=3):
            for other_sizes in ((), (4, 5)):
                for dim in range(len(other_sizes)):
                    dest = make_arg(other_sizes, num_dest, dim, dest_contig)
                    src = make_arg(other_sizes, num_copy, dim, src_contig)
                    idx = torch.randperm(num_dest, dtype=torch.int64, device=device)[:num_copy]
                    if not index_contig:
                        idx = torch.repeat_interleave(idx, 2, dim=-1)
                        idx = idx[..., ::2]
                    dest2 = dest.clone()
                    dest.index_copy_(dim, idx, src)
                    ref_index_copy(dest2, dim, idx, src)
                    self.assertEqual(dest, dest2)

    # onlyNativeDeviceTypes due to an XLA error:
    # https://github.com/pytorch/pytorch/issues/53256
    @onlyNativeDeviceTypes
    @dtypes(*get_all_dtypes())
    def test_index_copy_scalars(self, device, dtype):
        # Create the 8 possible combinations of scalar sizes for target / index / source
        scalars = ((make_tensor(size_t, dtype=dtype, device=device, low=None, high=None),
                    make_tensor(size_i, dtype=torch.int64, device=device, low=0, high=1),
                    make_tensor(size_s, dtype=dtype, device=device, low=None, high=None))
                   for size_t, size_i, size_s in product([(), (1,)], repeat=3))
        for target, idx, source in scalars:
            target.index_copy_(0, idx, source)
            self.assertEqual(target.item(), source.item())

    @onlyCPU
    def test_errors_index_copy(self, device):
        # We do not test the GPU as the CUDA_ASSERT would break the CUDA context
        idx_dim = 8
        tgt_dim = 5
        batch_dim = 3

        # Too large of an index
        a = torch.randn(batch_dim, tgt_dim, device=device)
        idx = torch.full((idx_dim,), tgt_dim, device=device)
        c = torch.zeros(batch_dim, idx_dim, device=device)
        with self.assertRaises(IndexError):
            a.index_copy_(1, idx, c)

        # Too small (negative indices)
        idx = torch.full((idx_dim,), -1, device=device)
        with self.assertRaises(IndexError):
            a.index_copy_(1, idx, c)

        # Too small (very negative indices) - they should be unsupported even
        # when support for negative indices is implemented for index_copy_
        idx = torch.full((idx_dim,), -tgt_dim - 1, device=device)
        with self.assertRaises(IndexError):
            a.index_copy_(1, idx, c)

    def _prepare_data_for_index_copy_and_add_deterministic(
        self, dim: int, device: torch.device
    ) -> Tuple[torch.Tensor, torch.Tensor, torch.Tensor]:
        assert (dim >= 0 and dim < 3)
        a = [5, 4, 3]
        a[dim] = 2000
        x = torch.zeros(a, device=device)
        b = a.copy()
        elems = a[dim] * 20
        b[dim] = elems
        src = torch.rand(b, device=device)
        index = torch.randint(a[dim], (elems,), device=device)
        return (x, index, src)

    @onlyNativeDeviceTypes
    def test_index_copy_deterministic(self, device: torch.device) -> None:
        for dim in range(3):
            x, index, src = self._prepare_data_for_index_copy_and_add_deterministic(dim, device)
            with DeterministicGuard(True):
                y0 = torch.index_copy(x, dim, index, src)

            x0 = x.clone().detach()
            index_list = index.tolist()
            for i in range(len(index_list)):
                if dim == 0:
                    x0[index_list[i], :, :] = src[i, :, :]
                elif dim == 1:
                    x0[:, index_list[i], :] = src[:, i, :]
                elif dim == 2:
                    x0[:, :, index_list[i]] = src[:, :, i]

            self.assertEqual(x0, y0, atol=0, rtol=0)

    @onlyNativeDeviceTypes
    def test_index_add_deterministic(self, device: torch.device) -> None:
        for dim in range(3):
            x, index, src = self._prepare_data_for_index_copy_and_add_deterministic(dim, device)
            alpha = random.random() + 1
            # on CPU it should be deterministic regardless of the deterministic mode
            with DeterministicGuard(True):
                y0 = torch.index_add(x, dim, index, src, alpha=alpha)
                for _ in range(3):
                    y = torch.index_add(x, dim, index, src, alpha=alpha)
                    self.assertEqual(y, y0, atol=0, rtol=0)

            with DeterministicGuard(False):
                for _ in range(3):
                    y_nd = torch.index_add(x, dim, index, src, alpha=alpha)
                    self.assertEqual(y_nd, y0, atol=1e-3, rtol=1e-5)

    @onlyNativeDeviceTypes
    def test_index_put_non_accumulate_deterministic(self, device) -> None:
        with DeterministicGuard(True):
            for i in range(3):
                m = random.randint(10, 20)
                elems = random.randint(20000, 30000)
                values = torch.rand(elems, device=device)
                indices = torch.randint(m, (elems,), device=device)
                input = torch.rand(m, device=device)
                output = input.index_put((indices,), values, accumulate=False)

                input_list = input.tolist()
                indices_list = indices.tolist()
                values_list = values.tolist()
                for i, v in zip(indices_list, values_list):
                    input_list[i] = v

                self.assertEqual(output, input_list)

    @dtypes(*get_all_dtypes())
    def test_index_fill(self, device, dtype):
        x = torch.tensor([[1, 2], [4, 5]], dtype=dtype, device=device)
        index = torch.tensor([0], device=device)
        x.index_fill_(1, index, 0)
        self.assertEqual(x, torch.tensor([[0, 2], [0, 5]], dtype=dtype, device=device))
        if not x.is_complex():
            with self.assertRaisesRegex(RuntimeError, r"Scalar"):
                x.index_fill_(1, index, 1 + 1j)
        # Make sure that the result stays 0-dim while applied to
        # a 0-dim input
        x = torch.tensor(1, dtype=dtype, device=device)
        self.assertEqual(0, x.index_fill(0, index, -1).dim())
        self.assertEqual(0, x.index_fill_(0, index, -1).dim())

    # The test fails for zero-dimensional tensors on XLA
    @onlyNativeDeviceTypes
    @dtypes(*get_all_dtypes())
    def test_index_select(self, device, dtype):
        num_src, num_out = 3, 5

        def make_arg(batch_sizes, n, dim, contig):
            size_arg = batch_sizes[:dim] + (n,) + batch_sizes[dim:]
            return make_tensor(size_arg, device, dtype, low=None, high=None, noncontiguous=not contig)

        def ref_index_select(src, dim, idx):
            # bfloat16 is just used on GPU, so it's not supported on numpy
            if dtype == torch.bfloat16:
                src = src.float()
            out = torch.from_numpy(np.take(src.cpu().numpy(), idx.cpu().numpy(), axis=dim))
            if dtype == torch.bfloat16:
                out = out.to(device=device, dtype=dtype)
            return out

        for src_contig, idx_contig in product([True, False], repeat=2):
            for other_sizes in ((), (4, 5)):
                for dim in range(len(other_sizes)):
                    src = make_arg(other_sizes, num_src, dim, src_contig)
                    idx = make_tensor((num_out,), device, dtype=torch.int64, low=0, high=num_src, noncontiguous=not idx_contig)
                    out = torch.index_select(src, dim, idx)
                    out2 = ref_index_select(src, dim, idx)
                    self.assertEqual(out, out2)

        for idx_type in (torch.int32, torch.int64):
            other_sizes = (3, 2)
            dim = 1
            src = make_arg(other_sizes, num_src, dim, True)
            idx = make_tensor((num_out,), device, dtype=idx_type, low=0, high=num_src, noncontiguous=False)
            out = torch.index_select(src, dim, idx)
            out2 = ref_index_select(src, dim, idx)
            self.assertEqual(out, out2)

        # Create the 4 possible combinations of scalar sizes for index / source
        scalars = ((make_tensor(size_s, device, dtype),
                    torch.zeros(size_i, dtype=torch.int64, device=device))
                   for size_s, size_i in product([(), (1,)], repeat=2))
        for source, idx in scalars:
            out = source.index_select(0, idx)
            self.assertEqual(out.item(), source.item())

    @dtypes(*get_all_dtypes())
    def test_take(self, device, dtype):
        idx_size = (4,)

        make_arg = partial(make_tensor, device=device, dtype=dtype)
        make_idx = partial(make_tensor, low=0, device=device, dtype=torch.int64)

        def ref_take(src, idx):
            if dtype == torch.bfloat16:
                src = src.half()
            src = src.cpu().numpy()
            idx = idx.cpu().numpy()
            out = torch.from_numpy(np.take(src, idx)).to(device=device, dtype=dtype)
            return out

        for src_contig, idx_contig, idx_reshape in product([True, False], repeat=3):
            for src_size in ((5,), (4, 5)):
                src = make_arg(src_size, noncontiguous=not src_contig)
                idx = make_idx(idx_size, high=src.numel(), noncontiguous=not idx_contig)
                if idx_reshape:
                    idx = idx.reshape(2, 2)
                out = torch.take(src, idx)
                out2 = ref_take(src, idx)
                self.assertEqual(out, out2)

        # Create the 4 possible combinations of scalar sizes for source / index
        for size_s, size_i in product([(), (1,)], repeat=2):
            source = make_arg(size_s)
            idx = make_idx(size_i, high=1)
            out = source.take(idx)
            self.assertEqual(out.item(), source.item())

    # The bool instance does not work on GPU. See
    # https://github.com/pytorch/pytorch/issues/54317
    @dtypes(*get_all_dtypes(include_bool=False))
    def test_put(self, device, dtype):
        src_size = (4,)

        make_arg = partial(make_tensor, device=device, dtype=dtype)
        make_idx = partial(make_tensor, low=0, device=device, dtype=torch.int64)

        def ref_put(dst, idx, src, accumulate):
            new_dst = dst.clone(memory_format=torch.contiguous_format).view(-1)
            new_idx = idx.contiguous().view(-1)
            new_src = src.contiguous().view(-1)
            method = new_dst.index_add_ if accumulate else new_dst.index_copy_
            return method(0, new_idx, new_src).view_as(dst)

        for dst_contig, src_contig, idx_contig, idx_reshape, accumulate in product([True, False], repeat=5):
            for dst_size in ((5,), (4, 5)):
                dst = make_arg(dst_size, noncontiguous=not dst_contig)
                src = make_arg(src_size, noncontiguous=not src_contig)

                # If accumulate=True, `put_` should be deterministic regardless of the inputs on CPU
                # On CUDA it may not be, but the test has enough tolerance to account for this
                if accumulate:
                    idx = make_idx(src_size, high=dst.numel())
                else:
                    idx = torch.randperm(dst.numel(), dtype=torch.int64, device=device)[:src_size[0]]
                if not idx_contig:
                    idx = torch.repeat_interleave(idx, 2, dim=-1)[..., ::2]
                if idx_reshape:
                    idx = idx.reshape(2, 2)
                out = torch.put(dst, idx, src, accumulate)
                # out-place
                reference = ref_put(dst, idx, src, accumulate)
                self.assertEqual(out, reference)

                # in-place
                dst.put_(idx, src, accumulate)
                self.assertEqual(dst, reference)


        # Create the 8 possible combinations of scalar sizes for target / index / source
        scalars = ((make_arg(size_t),
                    make_idx(size_i, high=1),
                    make_arg(size_s))
                   for size_t, size_i, size_s in product([(), (1,)], repeat=3))
        for (dest, idx, source), accumulate in product(scalars, [True, False]):
            dest_init = dest.clone()
            # out-place
            out = torch.put(dest, idx, source, accumulate=accumulate)
            # in-place
            dest1 = dest.clone()
            dest1.put_(idx, source, accumulate=accumulate)
            for d in [out, dest1]:
                if accumulate:
                    self.assertEqual(d.item(), (dest_init + source).item())
                else:
                    self.assertEqual(d.item(), source.item())

        # Empty case
        dest = make_arg((3, 2))
        reference = dest.clone()
        idx = make_idx((0,), high=1)
        source = make_arg((0,))
        for accumulate in [True, False]:
            out = torch.put(dest, idx, source, accumulate=accumulate)
            self.assertEqual(out, reference)
            dest.put_(idx, source, accumulate=accumulate)
            self.assertEqual(dest, reference)

    # The bool instance does not work on GPU. See
    # https://github.com/pytorch/pytorch/issues/54317
    @dtypes(*get_all_dtypes(include_bool=False))
    def test_put_accumulate(self, device, dtype):
        # Test for parallel adds with accumulate == True
        low_precision = dtype == torch.half or dtype == torch.bfloat16
        # Less numbers to avoid overflow with low_precision
        # Grainsize is 3000 for the for_loop to be parallized on CPU
        sizes = ((100,)) if low_precision else ((200,), (3002,))
        # Bfloat16 has a particularly bad performance here
        # This operation is nondeterministic on GPU, so we are generous with the rtol
        rtol, atol = (1e-1, 1e-2) if low_precision else (1e-3, 1e-4)

        make_arg = partial(make_tensor, low=-2, high=3, device=device, dtype=dtype)
        # Dump everything into the 0-th position
        make_idx = partial(torch.zeros, device=device, dtype=torch.int64)
        args = ((make_idx(size), make_arg(size)) for size in sizes)

        for idx, source in args:
            orig = make_arg((1,))
            out = orig.put(idx, source, accumulate=True)
            self.assertEqual(out, orig + source.sum(), rtol=rtol, atol=atol)

    def test_take_empty(self, device):
        for input_shape in [(0,), (0, 1, 2, 0), (1, 2, 3)]:
            for indices_shape in [(0,), (0, 1, 2, 0)]:
                input = torch.empty(input_shape, device=device)
                indices = torch.empty(indices_shape, dtype=torch.int64, device=device)
                self.assertEqual(indices, torch.take(input, indices), exact_dtype=False)

    def test_put_empty(self, device):
        for dst_shape in [(0,), (0, 1, 2, 0), (1, 2, 3)]:
            for indices_shape in [(0,), (0, 1, 2, 0)]:
                for accumulate in [False, True]:
                    dst = torch.randn(dst_shape, device=device)
                    indices = torch.empty(indices_shape, dtype=torch.int64, device=device)
                    src = torch.randn(indices_shape, device=device)
                    self.assertEqual(dst, dst.put_(indices, src, accumulate=accumulate))

    def scatter_allow_reduce(self, device, dtype, reduceop):
        device_type = torch.device(device).type
        return device_type != 'cuda' or (reduceop == 'multiply' and dtype.is_floating_point)

    # torch.{zeros, ones} do not support ComplexHalf (torch.complex32)
    # So, we are skipping it here.
    @dtypes(*(get_all_fp_dtypes(include_bfloat16=False, include_half=False) +
              get_all_complex_dtypes()))
    @dtypesIfCPU(*get_all_dtypes())
    @dtypesIfCUDA(*get_all_dtypes())
    def test_scatter_reduce_operations_to_large_input(self, device, dtype):
        index = torch.tensor([[1], [2]], device=device, dtype=torch.long)
        test_data = [
            (torch.zeros(4, 4, device=device, dtype=dtype),
             torch.ones(2, 2, device=device, dtype=dtype),
             torch.tensor([[0, 0, 0, 0],
                           [1, 0, 0, 0],
                           [1, 0, 0, 0],
                           [0, 0, 0, 0]],
                          device=device, dtype=dtype), "add"),
            (torch.tensor([2], device=device, dtype=dtype).repeat(4, 4),
             torch.tensor([6], device=device, dtype=dtype).repeat(2, 2),
             torch.tensor([[2, 2, 2, 2],
                           [12, 2, 2, 2],
                           [12, 2, 2, 2],
                           [2, 2, 2, 2]], device=device, dtype=dtype), "multiply"),
        ]

        for input, src, result, operation in test_data:
            if not self.scatter_allow_reduce(device, dtype, operation):
                continue
            input.scatter_(0, index, src, reduce=operation)
            self.assertEqual(input, result)

    # torch.{zeros, ones} do not support ComplexHalf (torch.complex32)
    # So, we are skipping it here.
    @dtypes(*(get_all_fp_dtypes(include_bfloat16=False, include_half=False) +
              get_all_complex_dtypes()))
    @dtypesIfCPU(*get_all_dtypes())
    @dtypesIfCUDA(*get_all_dtypes())
    def test_scatter_reduce_scalar(self, device, dtype):
        index = torch.tensor([[1], [2]], device=device, dtype=torch.long)
        test_data = [
            (torch.zeros(4, 4, device=device, dtype=dtype), 1,
             torch.tensor([[0, 0, 0, 0],
                           [1, 0, 0, 0],
                           [1, 0, 0, 0],
                           [0, 0, 0, 0]],
                          device=device, dtype=dtype), "add"),
            (torch.tensor([2], device=device, dtype=dtype).repeat(4, 4), 2,
             torch.tensor([[2, 2, 2, 2],
                           [4, 2, 2, 2],
                           [4, 2, 2, 2],
                           [2, 2, 2, 2]], device=device, dtype=dtype), "multiply"),
        ]

        for input, src, result, operation in test_data:
            if not self.scatter_allow_reduce(device, dtype, operation):
                continue
            input.scatter_(0, index, src, reduce=operation)
            self.assertEqual(input, result)

    # TODO: remove this after scatter_add_ is deprecated.
    def test_scatter_add_non_unique_index(self, device):
        height = 2
        width = 65536
        input = torch.ones(height, width, device=device)
        index = torch.zeros(height, width, dtype=torch.long, device=device)
        src = torch.ones(height, width, device=device)
        input.scatter_add_(0, index, src)

        self.assertEqual(input,
                         torch.tensor([[3], [1]], device=device,
                                      dtype=torch.float32).repeat(1, width))

    # torch.{zeros, ones} do not support ComplexHalf (torch.complex32)
    # So, we are skipping it here.
    @dtypes(*(get_all_fp_dtypes(include_bfloat16=False, include_half=False) +
              get_all_complex_dtypes()))
    @dtypesIfCPU(*get_all_dtypes())
    @dtypesIfCUDA(*get_all_dtypes())
    def test_scatter_reduce_non_unique_index(self, device, dtype):
        height = 2
        width = 2
        index = torch.zeros(height, width, dtype=torch.long, device=device)
        test_data = [
            (torch.ones(height, width, device=device, dtype=dtype),
             torch.ones(height, width, device=device, dtype=dtype),
             torch.tensor([[3], [1]], device=device, dtype=dtype).repeat(1, width), "add"),
            (torch.tensor([2], device=device, dtype=dtype).repeat(height, width),
             torch.tensor([2], device=device, dtype=dtype).repeat(height, width),
             torch.tensor([[8], [2]], device=device,
                          dtype=dtype).repeat(1, width), "multiply"),
        ]

        for input, src, result, operation in test_data:
            if not self.scatter_allow_reduce(device, dtype, operation):
                continue
            input.scatter_(0, index, src, reduce=operation)
            self.assertEqual(input, result, msg=f"result: {result} input: {input} method: {str(operation)}")

    # torch.{zeros, ones} do not support ComplexHalf (torch.complex32)
    # So, we are skipping it here.
    @onlyCUDA
    @dtypes(*(get_all_complex_dtypes() +
              get_all_int_dtypes()))
    def test_scatter_reduce_multiply_unsupported_dtypes(self, device, dtype):
        height = 2
        width = 2
        index = torch.zeros(height, width, dtype=torch.long, device=device)
        input = torch.ones(height, width, device=device, dtype=dtype)
        src = torch.ones(height, width, device=device, dtype=dtype)
        with self.assertRaises(RuntimeError):
            input.scatter_(0, index, src, reduce="multiply")

    def test_scatter_to_large_input(self, device):
        input = torch.zeros(4, 4, device=device)
        src = torch.ones(2, 2, device=device)
        index = torch.tensor([[1], [2]], device=device, dtype=torch.long)
        input.scatter_(0, index, src)
        self.assertEqual(input, torch.tensor([[0, 0, 0, 0],
                                              [1, 0, 0, 0],
                                              [1, 0, 0, 0],
                                              [0, 0, 0, 0]], device=device, dtype=torch.float32))

    def test_scatter_add_to_large_input(self, device):
        input = torch.zeros(4, 4, device=device)
        src = torch.ones(2, 2, device=device)
        index = torch.tensor([[1], [2]], device=device, dtype=torch.long)
        input.scatter_add_(0, index, src)
        self.assertEqual(input, torch.tensor([[0, 0, 0, 0],
                                              [1, 0, 0, 0],
                                              [1, 0, 0, 0],
                                              [0, 0, 0, 0]], device=device, dtype=torch.float32))

    def test_scatter_bool(self, device):
        x = torch.tensor([[True, True, True], [True, True, True]], device=device)
        res = torch.zeros(3, 3, dtype=torch.bool, device=device)
        res = res.scatter_(0, torch.tensor([[0, 1, 2], [0, 1, 2]], device=device), x)
        self.assertEqual(res, torch.tensor([[True, False, False],
                                            [False, True, False],
                                            [False, False, True]], device=device))

    def test_scatter_add_bool(self, device):
        x = torch.tensor([[True, True, True, True, True], [True, True, True, True, True]], device=device)
        res = torch.zeros(3, 5, dtype=torch.bool, device=device)
        res = res.scatter_add_(0, torch.tensor([[0, 1, 2, 0, 0], [2, 0, 0, 1, 2]], device=device), x)
        self.assertEqual(res, torch.tensor([[True, True, True, True, True],
                                            [False, True, False, True, False],
                                            [True, False, True, False, True]], device=device))

    @onlyNativeDeviceTypes
    @dtypes(*get_all_dtypes())
    def test_masked_scatter(self, device, dtype):
        dt = dtype
        with warnings.catch_warnings(record=True) as w:
            warnings.simplefilter("always")
            for maskType in [torch.uint8, torch.bool]:
                num_copy, num_dest = 3, 10
                dest = torch.tensor([1, 2, 3, 4, 5, 6, 7, 8, 9, 10], dtype=dt, device=device)
                dest2 = dest.clone()
                dest_ones = dest.clone()
                dest_ones_expected = dest.clone()
                src = torch.tensor([0, 0, 0, 0, 0, 0, 0, 0, 0, 0], dtype=dt, device=device)
                src_ones = torch.tensor([1, 1, 1, 1, 1, 1, 1, 1, 1, 1], dtype=dt, device=device)
                mask = torch.tensor((0, 0, 0, 0, 1, 0, 1, 0, 1, 0), dtype=maskType, device=device)

                if dt == torch.bool:
                    # torch.bool is a special case and is being tested
                    # in a separate test
                    return

                dest.masked_scatter_(mask, src)
                j = 0
                for i in range(num_dest):
                    if mask[i]:
                        dest2[i] = src[j]
                        dest_ones_expected[i] = src_ones[j]
                        j += 1
                self.assertEqual(dest, dest2, atol=0, rtol=0)

                dest_ones.masked_scatter_(mask, src_ones)
                self.assertEqual(dest_ones, dest_ones_expected, atol=0, rtol=0)

                # Bound checking in CUDA is done inside a kernel
                # in order to avoid synchronization, but this means
                # we can not clear the failures. So there is no way
                # to test it then recover.
                if self.device_type != 'cuda':
                    # make src smaller. this should fail
                    src = torch.zeros(num_copy - 1, dtype=dt, device=device)
                    with self.assertRaises(RuntimeError):
                        dest.masked_scatter_(mask, src)

                # empty tensor
                dest = torch.empty((5, 0, 5), dtype=dt, device=device)
                mask = torch.ones_like(dest, dtype=maskType, device=device)
                src = torch.empty((0,), dtype=dt, device=device)
                dest.masked_scatter_(mask, src)

                dest = torch.empty((5, 0, 5), dtype=dt, device=device)
                mask = torch.ones((5, 1, 5), dtype=maskType, device=device)
                src = torch.empty((0,), dtype=dt, device=device)
                dest.masked_scatter_(mask, src)

        if self.device_type != 'cuda':
            self.assertEqual(len(w), 5)
        else:
            self.assertEqual(len(w), 4)

        warn = 'masked_scatter_ received a mask with dtype torch.uint8,'
        for wi in w:
            self.assertEqual(str(wi.message)[0:55], str(warn))

    def test_masked_scatter_bool_tensor(self, device):
        src = torch.tensor([True, True, True], device=device)
        dst = torch.tensor([False, False, False], device=device)
        mask = torch.tensor([False, True, False], device=device)

        dst.masked_scatter_(mask, src)
        self.assertEqual(dst, torch.tensor([False, True, False], device=device))

        mask = torch.tensor([True, False, True], device=device)
        dst = dst.masked_scatter(mask, src)
        self.assertEqual(dst, torch.tensor([True, True, True], device=device))

    # refer https://github.com/pytorch/pytorch/issues/60190
    @skipIfRocm
    @onlyCUDA
    @largeTensorTest('30GB')
    def test_masked_scatter_large_tensor(self, device):
        t_cpu = torch.empty(2**31 + 1, dtype=torch.bool).random_()
        t = t_cpu.to(device)
        result_cpu = t_cpu.masked_scatter(t_cpu, t_cpu)
        result = t.masked_scatter(t, t)
        self.assertEqual(result, result_cpu)

    @dtypes(*get_all_dtypes())
    def test_masked_select(self, device, dtype):
        if device == 'cpu':
            warn = 'masked_select received a mask with dtype torch.uint8,'
        else:
            warn = 'indexing with dtype torch.uint8 is now deprecated, pl'
        for maskType in [torch.uint8, torch.bool]:
            num_src = 10
            src = torch.tensor([0, 0, 0, 0, 0, 0, 0, 0, 0, 0], dtype=dtype, device=device)
            mask = torch.randint(2, (num_src,), device=device, dtype=maskType)

            with warnings.catch_warnings(record=True) as w:
                dst = src.masked_select(mask)
                if maskType is torch.uint8:
                    self.assertEqual(len(w), 1)
                    self.assertEqual(str(w[0].message)[0:53], str(warn))
            dst2 = []
            for i in range(num_src):
                if mask[i]:
                    dst2 += [src[i]]
            self.assertEqual(dst, torch.tensor(dst2), atol=0, rtol=0)

            dst3 = torch.empty(0, device=device, dtype=dtype)
            torch.masked_select(src, mask, out=dst3)
            self.assertEqual(dst3, torch.tensor(dst2, dtype=dst3.dtype), atol=0, rtol=0)

        # Since half on CPU is not supported, need to skip the remaining test cases
        if dtype == torch.half and torch.device(device).type == 'cpu':
            return

        # Ensure that masks are expanded to match tensor properly
        a = torch.rand(100, 100, device=device).mul(100).to(dtype)
        mask_first_el_each_row = torch.zeros(100, device=device, dtype=torch.bool)
        mask_first_el_each_row[0] = True
        a_masked = a.masked_select(mask_first_el_each_row)
        self.assertEqual(a_masked, a[:, 0])

        mask_first_row = torch.zeros(100, 1, device=device, dtype=torch.bool)
        mask_first_row[0][0] = True
        a_masked = a.masked_select(mask_first_row)
        self.assertEqual(a_masked, a[0, :])

        # Ensure that tensor is expanded to match mask properly
        a = torch.rand(100, device=device).mul(100).to(dtype)
        mask_copy_3_times = torch.tensor([[True], [True], [False], [True]], device=device)
        a_masked = a.masked_select(mask_copy_3_times)
        self.assertEqual(a_masked, a.unsqueeze(0).expand(3, 100).flatten())

    def test_masked_select_discontiguous(self, device):
        for size in (10, 200):
            vals = torch.rand(size, size, device=device)
            mask = torch.full((size, size), False, dtype=torch.bool, device=device)
            mask[:, ::2] = True
            vals_list = (vals, vals.t())
            mask_list = (mask, mask.t())
            out_dc = torch.empty(size * size, device=device)[::2]
            for v, m in product(vals_list, mask_list):
                if m.is_contiguous():
                    expected = v[:, ::2].clone().reshape((-1, ))
                else:
                    expected = v[::2].clone().reshape((-1, ))
                out = torch.masked_select(v, m)
                self.assertEqual(out, expected, atol=0, rtol=0)
                torch.masked_select(v, m, out=out_dc)
                self.assertEqual(out_dc, expected, atol=0, rtol=0)

    @dtypes(*product(get_all_dtypes(), (torch.uint8, torch.bool)))
    def test_masked_fill(self, device, dtypes):
        dtype = dtypes[0]
        mask_dtype = dtypes[1]
        with warnings.catch_warnings(record=True) as w:
            warnings.simplefilter("always")

            num_dest = 10
            dst = torch.zeros(num_dest, dtype=dtype)
            mask = torch.randint(2, (num_dest,), dtype=mask_dtype)
            val = random.random()
            dst2 = dst.clone()

            dst.masked_fill_(mask, val)
            for i in range(num_dest):
                if mask[i]:
                    dst2[i] = val
            self.assertEqual(dst, dst2, atol=0, rtol=0)

            # test non-contiguous case
            dst = ((torch.randn(num_dest, num_dest, num_dest) * 10).to(dtype)).permute((2, 0, 1))
            dst2 = dst.contiguous()
            if dtype.is_complex:
                mask = dst.abs() > 0
            else:
                mask = dst > 0
            self.assertTrue(not dst.is_contiguous())
            self.assertTrue(dst2.is_contiguous())
            dst.masked_fill_(mask.to(mask_dtype), val)
            dst2.masked_fill_(mask.to(mask_dtype), val)
            self.assertEqual(dst, dst2, atol=0, rtol=0)

            if mask_dtype == torch.uint8:
                self.assertEqual(len(w), 3)

                warn = 'masked_fill_ received a mask with dtype torch.uint8,'
                for wi in w:
                    self.assertEqual(str(wi.message)[0:52], str(warn))
            else:
                self.assertEqual(len(w), 0)

    def test_masked_fill_bool_tensor(self, device):
        dst = torch.tensor([True, False, True], device=device)
        mask = torch.tensor([False, True, False], device=device)

        dst.masked_fill_(mask, True)
        self.assertEqual(dst, torch.tensor([True, True, True], device=device))

        dst = dst.masked_fill(mask, False)
        self.assertEqual(dst, torch.tensor([True, False, True], device=device))

    def test_tensor_shape_empty(self, device):
        x = torch.randn((0, 1, 3, 0), device=device)
        # flatten
        self.assertEqual((0,), torch.flatten(x, 0, 3).shape)
        self.assertEqual((0, 0), torch.flatten(x, 0, 2).shape)
        self.assertEqual((0, 3, 0), torch.flatten(x, 1, 2).shape)

        # squeeze, unsqueeze
        self.assertEqual((0, 1, 1, 3, 0), torch.unsqueeze(x, 1).shape)
        self.assertEqual((0, 3, 0), torch.squeeze(x, 1).shape)
        self.assertEqual((0, 3, 0), torch.squeeze(x).shape)

        # transpose, t
        self.assertEqual((0, 0, 3, 1), torch.transpose(x, 1, 3).shape)
        y = torch.randn((5, 0), device=device)
        self.assertEqual((0, 5), y.t().shape)

        # select
        self.assertEqual((0, 1, 0), torch.select(x, 2, 2).shape)

        # repeat, permute
        self.assertEqual((9, 0, 5, 6, 0), x.repeat(9, 7, 5, 2, 3).shape)
        self.assertEqual((3, 0, 0, 1), x.permute(2, 3, 0, 1).shape)

        # diagonal, diagflat
        self.assertEqual((0,), torch.diagonal(torch.randn((5, 0), device=device)).shape)
        self.assertEqual((0,), torch.diagonal(torch.randn((0, 5), device=device)).shape)
        # off the end offsets are valid
        self.assertEqual((0,), torch.diagonal(torch.randn((5, 0), device=device), offset=1).shape)
        self.assertEqual((0,), torch.diagonal(torch.randn((0, 5), device=device), offset=1).shape)
        # check non-zero sized offsets off the end
        self.assertEqual((5, 6, 0), torch.diagonal(torch.randn((3, 4, 5, 6), device=device), offset=45252).shape)
        self.assertEqual((5, 6, 0), torch.diagonal(torch.randn((3, 4, 5, 6), device=device), offset=-45252).shape)

        self.assertEqual((0, 0), torch.diagflat(torch.tensor([], device=device)).shape)
        self.assertEqual(torch.zeros(1, 1), torch.diagflat(torch.tensor([], device=device), offset=1))
        self.assertEqual((0, 0), torch.diagflat(torch.tensor([[]], device=device)).shape)
        self.assertEqual(torch.zeros(1, 1), torch.diagflat(torch.tensor([[]], device=device), offset=1))

        # stack, split, chunk
        self.assertEqual((4, 0, 1, 3, 0), torch.stack((x, x, x, x)).shape)
        self.assertEqual([(0, 1, 3, 0)],
                         [z.shape for z in torch.chunk(x, 1, dim=0)])

        self.assertEqual([(0, 1, 3, 0), ] * 3, [z.shape for z in torch.chunk(x, 3, dim=0)])
        self.assertEqual([(0, 1, 1, 0), ] * 3, [z.shape for z in torch.chunk(x, 3, dim=2)])

        # NOTE: split_with_sizes behaves differently than NumPy in that it
        # takes sizes rather than offsets
        self.assertEqual([(0, 1, 0, 0), (0, 1, 1, 0), (0, 1, 2, 0)],
                         [z.shape for z in torch.split(x, (0, 1, 2), dim=2)])

        self.assertRaises(RuntimeError, lambda: torch.split(x, 0, dim=1))
        # This is strange because the split size is larger than the dim size, but consistent with
        # how split handles that case generally (when no 0s are involved).
        self.assertEqual([(0, 1, 3, 0)], [z.shape for z in torch.split(x, 1, dim=0)])
        self.assertEqual([(0, 1, 3, 0)], [z.shape for z in torch.split(x, 0, dim=0)])

    # functions that operate over a dimension but don't reduce.
    def test_dim_function_empty(self, device):
        shape = (0, 1, 2, 0)
        x = torch.randn(shape, device=device)

        # size stride
        self.assertEqual(0, x.size(3))
        self.assertEqual(2, x.size(2))
        self.assertEqual(2, x.stride(0))
        self.assertEqual(1, x.stride(2))

        self.assertEqual(x, torch.nn.functional.glu(x, 0))
        self.assertEqual((0, 1, 1, 0), torch.nn.functional.glu(x, 2).shape)

        # softmax, logsoftmax
        self.assertEqual(x, torch.nn.functional.softmax(x, 0))
        self.assertEqual(x, torch.nn.functional.softmax(x, 2))
        self.assertEqual(x, torch.nn.functional.softmax(x, 3))

        self.assertEqual(x, torch.nn.functional.log_softmax(x, 0))
        self.assertEqual(x, torch.nn.functional.log_softmax(x, 2))
        self.assertEqual(x, torch.nn.functional.log_softmax(x, 3))

        # cumsum, cumprod, cummax, cummin
        self.assertEqual(shape, torch.cumsum(x, 0).shape)
        self.assertEqual(shape, torch.cumsum(x, 2).shape)
        self.assertEqual(shape, torch.cumprod(x, 0).shape)
        self.assertEqual(shape, torch.cumprod(x, 2).shape)
        self.assertEqual(shape, torch.cummax(x, 0)[0].shape)
        self.assertEqual(shape, torch.cummax(x, 2)[0].shape)
        self.assertEqual(shape, torch.cummin(x, 0)[0].shape)
        self.assertEqual(shape, torch.cummin(x, 2)[0].shape)
        self.assertEqual(shape, torch.logcumsumexp(x, 0).shape)
        self.assertEqual(shape, torch.logcumsumexp(x, 2).shape)

        # flip
        self.assertEqual(x, x.flip(0))
        self.assertEqual(x, x.flip(2))

        # roll
        self.assertEqual(x, x.roll(0, 1).roll(0, -1))
        self.assertEqual(x, x.roll(1, x.size(1)))
        self.assertEqual(x, x.roll(1))
        self.assertEqual(x, x.roll((1, 1), (3, 1)))

        # unbind
        self.assertEqual((), x.unbind(0))
        self.assertEqual((torch.empty((0, 1, 0), device=device), torch.empty((0, 1, 0), device=device)),
                         x.unbind(2))

        # cross
        y = torch.randn((0, 1, 3, 0), device=device)
        self.assertEqual(y.shape, torch.cross(y, y).shape)

        # renorm
        self.assertEqual(shape, torch.renorm(x, 1, 0, 5).shape)
        self.assertEqual(shape, torch.renorm(x, 1, 2, 5).shape)

        # sort
        self.assertEqual([shape, shape], [z.shape for z in torch.sort(x, dim=0)])
        self.assertEqual([shape, shape], [z.shape for z in torch.sort(x, dim=2)])

        # topk
        self.assertEqual([shape, shape], [z.shape for z in torch.topk(x, 0, dim=0)])
        self.assertEqual([(0, 1, 1, 0), (0, 1, 1, 0)], [z.shape for z in torch.topk(x, 1, dim=2)])

        y = torch.randn((2, 3, 4), device=device)
        self.assertEqual([(2, 3, 0), (2, 3, 0)], [z.shape for z in torch.topk(y, 0)])

        # gather
        self.assertEqual(shape, torch.gather(x, 0, torch.empty(shape, dtype=torch.int64, device=device)).shape)
        self.assertEqual(shape, torch.gather(x, 2, torch.empty(shape, dtype=torch.int64, device=device)).shape)
        larger_shape = torch.empty((0, 1, 3, 0), dtype=torch.int64, device=device)
        self.assertEqual(larger_shape.shape, torch.gather(x, 2, larger_shape).shape)
        smaller_shape = torch.empty((0, 1, 0, 0), dtype=torch.int64, device=device)
        self.assertEqual(smaller_shape.shape, torch.gather(x, 2, smaller_shape).shape)
        y = torch.randn((2, 3, 4), device=device)
        self.assertEqual((0, 3, 4),
                         torch.gather(y, 0, torch.empty((0, 3, 4), dtype=torch.int64, device=device)).shape)

        # scatter, scatter_add
        for dim in [0, 2]:
            y = torch.randn(shape, device=device)
            y_src = torch.randn(shape, device=device)
            ind = torch.empty(shape, dtype=torch.int64, device=device)
            self.assertEqual(shape, y.scatter_(dim, ind, y_src).shape)
            self.assertEqual(shape, y.scatter_add_(dim, ind, y_src).shape)

        z = torch.randn((2, 3, 4), device=device)
        z_src = torch.randn((2, 3, 4), device=device)
        self.assertEqual(z, z.scatter_(2, torch.empty((2, 3, 0), dtype=torch.int64, device=device), z_src))
        self.assertEqual(z, z.scatter_add_(2, torch.empty((2, 3, 0), dtype=torch.int64, device=device), z_src))

        # index_fill, index_copy, index_add
        c = x.clone()
        c_clone = c.clone()
        ind_empty = torch.tensor([], dtype=torch.int64, device=device)
        ind_01 = torch.tensor([0, 1], dtype=torch.int64, device=device)
        self.assertEqual(c_clone, c.index_fill_(0, ind_empty, -1))
        self.assertEqual(c_clone, c.index_fill_(2, ind_empty, -1))
        self.assertEqual(c_clone, c.index_fill_(2, torch.tensor([0, 1], dtype=torch.int64, device=device), -1))
        self.assertEqual(c_clone, c.index_copy_(0, ind_empty, torch.empty((0, 1, 2, 0), device=device)))
        self.assertEqual(c_clone, c.index_copy_(2, ind_empty, torch.empty((0, 1, 0, 0), device=device)))
        self.assertEqual(c_clone, c.index_copy_(2, ind_01, torch.empty((0, 1, 2, 0), device=device)))
        self.assertEqual(c_clone, c.index_add_(0, ind_empty, torch.empty((0, 1, 2, 0), device=device)))
        self.assertEqual(c_clone, c.index_add_(2, ind_empty, torch.empty((0, 1, 0, 0), device=device)))
        self.assertEqual(c_clone, c.index_add_(2, ind_01, torch.empty((0, 1, 2, 0), device=device)))

        c = torch.randn((0, 1, 2), device=device)
        c_clone = c.clone()
        self.assertEqual(c_clone, c.index_fill_(0, ind_empty, -1))
        self.assertEqual(c_clone, c.index_copy_(0, ind_empty, torch.empty((0, 1, 2), device=device)))
        self.assertEqual(c_clone, c.index_add_(0, ind_empty, torch.empty((0, 1, 2), device=device)))
        self.assertEqual(c_clone, c.index_fill_(0, ind_empty, -1))
        self.assertEqual(c_clone, c.index_copy_(0, ind_empty, torch.empty((0, 1, 2), device=device)))
        self.assertEqual(c_clone, c.index_add_(0, ind_empty, torch.empty((0, 1, 2), device=device)))

        # index fill/copy/add non-empty
        z = torch.randn((2, 3, 4), device=device)
        self.assertEqual(z, z.index_fill_(0, ind_empty, -1))
        z = torch.randn((2, 3, 4), device=device)
        self.assertEqual(z, z.index_copy_(0, ind_empty, torch.empty((0, 3, 4), device=device)))
        z = torch.randn((2, 3, 4), device=device)
        self.assertEqual(z, z.index_add_(0, ind_empty, torch.empty((0, 3, 4), device=device)))

        # index_select
        self.assertEqual(x, x.index_select(0, ind_empty))
        self.assertEqual((0, 1, 0, 0), x.index_select(2, ind_empty).shape)
        self.assertEqual(x, x.index_select(2, ind_01))
        z = torch.randn((2, 3, 4), device=device)  # non-empty
        self.assertEqual((0, 3, 4), z.index_select(0, ind_empty).shape)
        c = torch.randn((0, 1, 2), device=device)
        self.assertEqual(c, c.index_select(0, ind_empty))
        c = torch.randn((0, 1, 2), device=device)
        self.assertEqual(c, c.index_select(0, ind_empty))

    def _brute_pdist(self, inp, p=2):
        """Computes the same as torch.pdist using primitives"""
        n = inp.shape[-2]
        k = n * (n - 1) // 2
        if k == 0:
            # torch complains about empty indices
            return torch.empty(inp.shape[:-2] + (0,), dtype=inp.dtype, device=inp.device)
        square = torch.norm(inp[..., None, :] - inp[..., None, :, :], p=p, dim=-1)
        unroll = square.view(square.shape[:-2] + (n * n,))
        inds = torch.ones(k, dtype=torch.int)
        inds[torch.arange(n - 1, 1, -1, dtype=torch.int).cumsum(0)] += torch.arange(2, n, dtype=torch.int)
        return unroll[..., inds.cumsum(0)]

    def _pdist_single(self, shape, device, p, dtype, trans, grad_check=False):
        x = torch.randn(shape, dtype=dtype, device=device)
        if trans:
            x.transpose_(-2, -1)
        if grad_check:
            x.requires_grad_()
            y = x.detach().clone().requires_grad_()
        else:
            y = x
        actual = torch.pdist(x, p=p)
        expected = self._brute_pdist(y, p=p)
        self.assertEqual(expected.shape, actual.shape)
        self.assertEqual(expected, actual)
        if grad_check and expected.size() != torch.Size([0]):
            g0 = torch.rand_like(actual)
            actual.backward(g0)
            expected.backward(g0)
            self.assertEqual(x.grad, y.grad)

    @slowTest
    def test_pdist_norm_forward(self, device):
        for shape in [(4, 5), (3, 2), (2, 1), (1500, 1)]:
            for p in [0, 1, 2, 3, 1.5, 2.5, float('inf')]:
                for trans in [False, True]:
                    for dtype in [torch.float32, torch.float64]:
                        self._pdist_single(shape, device, p, dtype, trans, grad_check=False)

        # do a simplified comparison with big inputs, see:
        # https://github.com/pytorch/pytorch/issues/15511
        for dtype in [torch.float32, torch.float64]:
            self._pdist_single((1000, 2), device, 2, dtype, trans=False, grad_check=False)

    @slowTest
    def test_pdist_norm_backward(self, device):
        for shape in [(4, 5), (3, 2), (2, 1), (1500, 1)]:
            for p in [0, 1, 2, 3, 1.5, 2.5, float('inf')]:
                for trans in [False, True]:
                    self._pdist_single(shape, device, p, torch.float64, trans, grad_check=True)

    @unittest.skipIf(IS_FBCODE and IS_REMOTE_GPU, "sandcastle OOM with current tpx gpu/re configuration")
    @skipIfRocm
    def test_pdist_norm_large(self, device):
        # use dim0>=46342 for forward, see:
        # https://github.com/pytorch/pytorch/issues/30583
        # Compare output using GPU with the CPU implementation, as brute_pdist uses too much memory
        if 'cuda' in device:
            x = torch.randn(50000, 1, dtype=torch.float32)
            expected_cpu = torch.pdist(x, p=2)
            actual_gpu = torch.pdist(x.to(device), p=2)
            self.assertEqual(expected_cpu, actual_gpu.cpu())

    @onlyNativeDeviceTypes
    @dtypesIfCUDA(*set(get_all_math_dtypes('cuda')))
    @dtypes(*set(get_all_math_dtypes('cpu')))
    def test_addcdiv(self, device, dtype):
        # Returns floating or integral scalar corresponding to dtype
        def _number(floating, integer, dtype):
            if dtype in [torch.half, torch.float, torch.double, torch.bfloat16]:
                return floating
            elif dtype in [torch.cfloat, torch.cdouble]:
                return floating * (1 + 1j)
            else:
                return integer

        def non_zero_rand(size, dtype, device):
            if dtype.is_floating_point or dtype.is_complex:
                a = torch.rand(size=size, dtype=dtype, device=device)
            elif dtype == torch.uint8:
                a = torch.randint(1, 5, size=size, dtype=dtype, device=device)
            else:
                a = torch.randint(-5, 5, size=size, dtype=dtype, device=device)
            return a + (a == 0).to(dtype)

        def _test_addcdiv():
            a = non_zero_rand((2, 2), dtype=dtype, device=device)
            b = non_zero_rand((2, 2), dtype=dtype, device=device)
            c = non_zero_rand((2, 2), dtype=dtype, device=device)
            alpha = _number(0.5, 3, dtype)

            expected = a + (alpha * b) / c
            actual = torch.addcdiv(a, b, c, value=alpha)
            self.assertEqual(expected, actual)

            with self.assertWarnsOnceRegex(
                    UserWarning, "This overload of addcdiv is deprecated"):
                self.assertEqual(actual, torch.addcdiv(a, alpha, b, c))

        if not (dtype.is_floating_point or dtype.is_complex):
            # Integer division with addcdiv is prohibited
            with self.assertRaises(RuntimeError):
                _test_addcdiv()
        else:
            _test_addcdiv()

        if self.device_type == 'cuda' and dtype == torch.half:
            a = torch.tensor([60000.0], device=device, dtype=dtype)
            b = torch.tensor([60000.0], device=device, dtype=dtype)
            c = torch.tensor([1.0], device=device, dtype=dtype)
            out = torch.addcmul(a, b, c, value=-2)
            self.assertTrue(not (out.isnan() or out.isinf()))

    def test_nullary_op_mem_overlap(self, device):
        ops = (
            ("random_", ()),
            ("uniform_", ()),
            ("cauchy_", ()),
            ("log_normal_", ()),
            ("exponential_", ()),
            ("geometric_", (0.5,)),
            ("normal_", ()),
        )

        x = torch.rand((1, 3)).expand((3, 3))
        for op, args in ops:
            with self.assertRaisesRegex(RuntimeError, 'unsupported operation'):
                getattr(x, op)(*args)

    @dtypes(torch.double)
    def test_ternary_op_mem_overlap(self, device, dtype):
        ops = [
            ("addcmul", True, True, 'cpu'),
            ("addcmul", True, True, 'cuda'),
            ("addcdiv", True, True, 'cpu'),
            ("addcdiv", True, True, 'cuda'),
            ("lerp", True, True, 'cpu'),
            ("lerp", True, True, 'cuda')
        ]

        for (fn, has_input_output_mem_overlap_check,
             has_internal_mem_overlap_check, dev) in ops:
            if dev != device:
                continue
            out_op = getattr(torch, fn)
            inplace_op = getattr(torch.Tensor, fn + '_')
            self.check_internal_mem_overlap(
                inplace_op, 3, dtype, device,
                expected_failure=not has_internal_mem_overlap_check)
            self.ternary_check_input_output_mem_overlap(out_op, dev,
                                                        expected_failure=not has_input_output_mem_overlap_check)

    @expectedFailureMeta  # RuntimeError not raised
    @dtypes(torch.double)
    @onlyNativeDeviceTypes
    def test_copy_mem_overlap(self, device, dtype):
        self.check_internal_mem_overlap(
            torch.Tensor.copy_, num_inputs=2, dtype=dtype, device=device)
        sz = 9
        doubles = torch.randn(2 * sz, dtype=dtype, device=device)
        self.unary_check_input_output_mem_overlap(
            doubles, sz, lambda input, out: out.copy_(input))

    @expectedFailureMeta  # RuntimeError not raised
    @onlyNativeDeviceTypes
    def test_index_add_mem_overlap(self, device):
        x = torch.rand((1,), device=device).expand((6,))
        y = torch.rand((6,), device=device)
        ind = torch.tensor([2, 1, 0], device=device)
        value = torch.rand((3,), device=device)
        with self.assertRaisesRegex(RuntimeError, 'unsupported operation'):
            x.index_add_(0, ind, value)
        with self.assertRaisesRegex(RuntimeError, 'unsupported operation'):
            y.index_add_(0, ind, y[:3])
        with self.assertRaisesRegex(RuntimeError, 'unsupported operation'):
            ind.index_add_(0, ind, ind.clone())
        with self.assertRaisesRegex(RuntimeError, 'unsupported operation'):
            ind.index_add_(0, ind.clone(), ind)

    @onlyNativeDeviceTypes
    def test_index_copy_mem_overlap(self, device):
        x = torch.rand((1,), device=device).expand((6,))
        y = torch.rand((6,), device=device)
        ind = torch.tensor([2, 1, 0], device=device)
        value = torch.rand((3,), device=device)
        with self.assertRaisesRegex(RuntimeError, 'unsupported operation'):
            x.index_copy_(0, ind, value)
        with self.assertRaisesRegex(RuntimeError, 'unsupported operation'):
            y.index_copy_(0, ind, y[:3])
        with self.assertRaisesRegex(RuntimeError, 'unsupported operation'):
            ind.index_copy_(0, ind, ind.clone())
        with self.assertRaisesRegex(RuntimeError, 'unsupported operation'):
            ind.index_copy_(0, ind.clone(), ind)

    @expectedFailureMeta  # Warning not triggered
    @onlyNativeDeviceTypes
    def test_index_fill_mem_overlap(self, device):
        x = torch.rand((1,), device=device).expand((6,))
        y = torch.rand((6,), device=device)
        ind = torch.tensor([2, 1, 0], device=device)
        value = torch.rand((3,), device=device)

        with self.assertWarnsRegex(UserWarning, "index_fill_ on expanded tensors"):
            x.index_fill_(0, ind, 1.0)
        with self.assertRaisesRegex(RuntimeError, 'unsupported operation'):
            ind.index_fill_(0, ind, 0)

    @expectedFailureMeta  # RuntimeError not raised
    @onlyNativeDeviceTypes
    def test_shift_mem_overlap(self, device):
        x = torch.rand(3, device=device)
        with self.assertRaisesRegex(RuntimeError, 'unsupported operation'):
            x[:-1] <<= x[1:]
        with self.assertRaisesRegex(RuntimeError, 'unsupported operation'):
            x[:-1] >>= x[1:]

    @expectedFailureMeta  # RuntimeError not raised
    @onlyNativeDeviceTypes
    def test_bernoulli_mem_overlap(self, device):
        x = torch.rand((1,), device=device).expand((6,))

        with self.assertRaisesRegex(RuntimeError, 'unsupported operation'):
            x.bernoulli_()
        with self.assertRaisesRegex(RuntimeError, 'unsupported operation'):
            x.bernoulli_(p=0.1)
        p = torch.rand(6, device=device)
        with self.assertRaisesRegex(RuntimeError, 'unsupported operation'):
            x.bernoulli_(p=p)
        with self.assertRaisesRegex(RuntimeError, 'unsupported operation'):
            torch.bernoulli(torch.rand_like(x), out=x)

    @expectedFailureMeta  # RuntimeError not raised
    @onlyNativeDeviceTypes
    def test_put_mem_overlap(self, device):
        x = torch.rand((1,), device=device).expand((6,))
        y = torch.rand((6,), device=device)
        ind = torch.tensor([2, 1, 0], device=device)
        value = torch.rand((3,), device=device)
        with self.assertRaisesRegex(RuntimeError, 'unsupported operation'):
            x.put_(ind, value)
        with self.assertRaisesRegex(RuntimeError, 'unsupported operation'):
            y.put_(ind[0], y[0])
        with self.assertRaisesRegex(RuntimeError, 'unsupported operation'):
            ind.put_(ind, ind)
        with self.assertRaisesRegex(RuntimeError, 'unsupported operation'):
            y.put_(ind, y[:3])
        with self.assertRaisesRegex(RuntimeError, 'unsupported operation'):
            ind.put_(ind, ind.clone())
        with self.assertRaisesRegex(RuntimeError, 'unsupported operation'):
            ind.put_(ind.clone(), ind)

    @expectedFailureMeta  # UserWarning not triggered
    @onlyNativeDeviceTypes
    def test_index_put_mem_overlap(self, device):
        x = torch.rand((1,), device=device).expand((6,))
        y = torch.rand((6,), device=device)
        ind = torch.tensor([2, 1, 0], device=device)
        value = torch.rand((3,), device=device)
        with self.assertWarnsRegex(UserWarning, 'expanded tensors'):
            x.index_put_((ind,), value)
        with self.assertRaisesRegex(RuntimeError, 'unsupported operation'):
            y.index_put_((ind,), y[0])
        with self.assertRaisesRegex(RuntimeError, 'unsupported operation'):
            ind.index_put_((ind,), ind)
        with self.assertRaisesRegex(RuntimeError, 'unsupported operation'):
            y.index_put_((ind,), y[:3])
        with self.assertRaisesRegex(RuntimeError, 'unsupported operation'):
            ind.index_put_((ind,), ind.clone())
        with self.assertRaisesRegex(RuntimeError, 'unsupported operation'):
            ind.index_put_((ind.clone(),), ind)

    @expectedFailureMeta  # UserWarning not triggered
    @onlyNativeDeviceTypes
    def test_masked_fill_mem_overlap(self, device):
        x = torch.rand((1,), device=device).expand((6,))
        mask = torch.tensor([True, False, True, True, False, False], device=device)
        with self.assertWarnsRegex(UserWarning, 'expanded tensors'):
            x.masked_fill_(mask, 0.)

        fill_val = torch.tensor(0., device=device)
        with self.assertWarnsRegex(UserWarning, 'expanded tensors'):
            x.masked_fill_(mask, fill_val)

        with self.assertRaisesRegex(RuntimeError, 'unsupported operation'):
            mask[1:].masked_fill_(mask[:-1], False)

    @onlyNativeDeviceTypes
    def test_masked_select_mem_overlap(self, device):
        x = torch.rand((1,), device=device).expand((3,))
        y = torch.rand((6,), device=device)
        mask = torch.tensor([True, False, True, True, False, False], device=device)
        with self.assertRaisesRegex(RuntimeError, 'unsupported operation'):
            torch.masked_select(y, mask, out=x)
        with self.assertRaisesRegex(RuntimeError, 'unsupported operation'):
            torch.masked_select(y, mask, out=y)
        with self.assertRaisesRegex(RuntimeError, 'unsupported operation'):
            torch.masked_select(mask.clone(), mask, out=mask)

    @expectedFailureMeta  # RuntimeError not raised
    @onlyNativeDeviceTypes
    def test_masked_scatter_mem_overlap(self, device):
        x = torch.rand((1,), device=device).expand((6,))
        src = torch.rand((3,), device=device)
        mask = torch.tensor([True, False, True, True, False, False], device=device)

        with self.assertRaisesRegex(RuntimeError, 'unsupported operation'):
            x.masked_scatter_(mask, src)

    @onlyNativeDeviceTypes
    def test_index_select_mem_overlap(self, device):
        x = torch.rand((1, 6), device=device).expand((2, 6))
        y = torch.rand((3, 6), device=device)
        ind = torch.tensor([0, 1], dtype=torch.int64, device=device)
        with self.assertRaisesRegex(RuntimeError, 'unsupported operation'):
            torch.index_select(y, 1, ind, out=x)

    @onlyNativeDeviceTypes
    def test_scatter_mem_overlap(self, device):
        x = torch.rand((1,), device=device).expand((6,))
        src = torch.rand((3,), device=device)
        ind = torch.tensor([2, 1, 0], device=device, dtype=torch.int64)

        with self.assertRaisesRegex(RuntimeError, 'unsupported operation'):
            x.scatter_(0, ind, src)
        with self.assertRaisesRegex(RuntimeError, 'unsupported operation'):
            src.scatter_(0, ind, src)
        with self.assertRaisesRegex(RuntimeError, 'unsupported operation'):
            ind.scatter_(0, ind, ind.clone())

    @onlyNativeDeviceTypes
    def test_gather_mem_overlap(self, device):
        x = torch.rand((1,), device=device).expand((3,))
        src = torch.rand((6,), device=device)
        ind = torch.tensor([2, 1, 0], device=device, dtype=torch.int64)
        with self.assertRaisesRegex(RuntimeError, 'unsupported operation'):
            torch.gather(src, 0, ind, out=x)
        with self.assertRaisesRegex(RuntimeError, 'unsupported operation'):
            torch.gather(src, 0, ind, out=src)
        with self.assertRaisesRegex(RuntimeError, 'unsupported operation'):
            torch.gather(ind.clone(), 0, ind[1:], out=ind[:1])

    @onlyNativeDeviceTypes
    def test_take_mem_overlap(self, device):
        x = torch.rand((1,), device=device).expand((3,))
        src = torch.rand((6,), device=device)
        ind = torch.tensor([2, 1, 0], device=device, dtype=torch.int64)
        with self.assertRaisesRegex(RuntimeError, 'unsupported operation'):
            torch.take(src, ind, out=x)
        with self.assertRaisesRegex(RuntimeError, 'unsupported operation'):
            torch.take(src, ind, out=src)
        with self.assertRaisesRegex(RuntimeError, 'unsupported operation'):
            torch.take(ind.clone(), ind[1:], out=ind[:-1])


    @onlyCUDA
    def test_multinomial_device_constrain(self, device):
        x = torch.empty(0, device="cpu")
        y = torch.empty(0, device=device)
        self.assertRaisesRegex(
            RuntimeError, "Expected all tensors to be on the same device",
            lambda: torch.multinomial(x, 2, out=y))

    @deviceCountAtLeast(2)
    @onlyCUDA
    def test_multinomial_gpu_device_constrain(self, devices):
        x = torch.empty(0, device=devices[0])
        y = torch.empty(0, device=devices[1])
        self.assertRaisesRegex(
            RuntimeError, "Expected all tensors to be on the same device",
            lambda: torch.multinomial(x, 2, out=y))

    @deviceCountAtLeast(2)
    @onlyCUDA
    def test_device_guard(self, devices):
        # verify that all operators with `device_guard: False` behave properly with multiple devices.
        # TODO: if we had operator introspection we could figure out this set of operators automatically...
        x = torch.randn((1, 2, 3), device=devices[1])
        y = torch.zeros((1, 3, 2), device=devices[1])
        scalar = torch.tensor(5, device=devices[1])

        # property ops
        torch.cudnn_is_acceptable(x)
        x.is_distributed()
        x.is_floating_point()
        x.is_complex()
        x.is_same_size(y)
        x.is_signed()
        x.size(0)
        x.stride(0)
        x.numel()
        x.is_set_to(y)
        x.data_ptr()
        scalar.is_nonzero()

        # sparse property ops
        y[0][1] = 5
        y_sparse = y.to_sparse()
        y_sparse.sparse_dim()
        y_sparse._dimI()
        y_sparse.dense_dim()
        y_sparse._dimV()
        y_sparse._nnz()
        y_sparse.is_coalesced()
        y_sparse._indices()
        y_sparse._values()
        y_sparse.indices()
        y_sparse.values()

        # in-place ops
        def inplace():
            return torch.randn((1, 2, 3), device=devices[1])
        inplace().as_strided_(y.size(), y.stride())
        inplace().resize_(y.size())
        inplace().squeeze_()
        inplace().squeeze_(0)
        inplace().unsqueeze_(2)
        inplace().transpose_(1, 2)
        inplace().squeeze_().t_()
        inplace().set_(x.storage())
        inplace().set_(x.storage(), x.storage_offset(), x.size(), x.stride())
        inplace().set_(x)
        inplace().set_()
        y_sparse._coalesced_(True)

        # shape modification
        x.as_strided(y.size(), y.stride())
        x.expand((5, 2, 3))
        x.expand_as(x)
        x.sum_to_size((1,))
        torch.broadcast_tensors(x , x)
        x.reshape((1, 3, 2))
        x.reshape_as(y)
        x.squeeze()
        x.squeeze(0)
        x.squeeze().t()
        x.transpose(1, 2)
        x.unsqueeze(2)
        x.view((1, 3, 2))
        x.view_as(y)

        # chunk, split, etc.
        x.chunk(2, dim=1)
        x.split(1, dim=2)
        x.split_with_sizes([1, 2], dim=2)
        x.unfold(dimension=2, size=1, step=1)

        x.narrow(1, 1, 1)
        x.select(1, 1)
        torch.isnan(x)

        torch.empty((1, 3, 2), out=y)
        torch.empty_like(x)
        torch.empty_like(x, dtype=torch.int64)

        # to
        x.to(x)
        x.to(y)
        x.to(x, copy=True)

    def test_is_signed(self, device):
        self.assertEqual(torch.IntTensor(5).to(device).is_signed(), True)
        self.assertEqual(torch.ByteTensor(5).to(device).is_signed(), False)
        self.assertEqual(torch.CharTensor(5).to(device).is_signed(), True)
        self.assertEqual(torch.FloatTensor(5).to(device).is_signed(), True)
        self.assertEqual(torch.HalfTensor(10).to(device).is_signed(), True)

    # Note - reports a leak of 512 bytes on CUDA device 1
    @deviceCountAtLeast(2)
    @skipCUDAMemoryLeakCheckIf(True)
    @onlyCUDA
    def test_tensor_set_errors_multigpu(self, devices):
        f_cuda0 = torch.randn((2, 3), dtype=torch.float32, device=devices[0])
        f_cuda1 = torch.randn((2, 3), dtype=torch.float32, device=devices[1])

        self.assertRaises(RuntimeError, lambda: f_cuda0.set_(f_cuda1.storage()))
        self.assertRaises(RuntimeError,
                          lambda: f_cuda0.set_(f_cuda1.storage(), 0, f_cuda1.size(), f_cuda1.stride()))
        self.assertRaises(RuntimeError, lambda: f_cuda0.set_(f_cuda1))

    @onlyCUDA
    def test_half_tensor(self, device):
        x = torch.randn(5, 5).half()
        self.assertEqual(x.to(device), x)

        xc = x.to(device)
        with tempfile.NamedTemporaryFile() as f:
            torch.save(xc, f)
            f.seek(0)
            xc2 = torch.load(f)
            self.assertIsInstance(xc2, type(xc))
            self.assertEqual(xc.float(), xc2.float())

    @onlyCUDA
    @deviceCountAtLeast(1)  # Note: Tests works with one but prefers more devices
    def test_serialization(self, devices):
        def _test_serialization(filecontext_lambda):
            t0 = torch.cuda.FloatTensor(5).fill_(1)
            with torch.cuda.device(devices[-1]):
                tn = torch.cuda.FloatTensor(3).fill_(2)
            torch.cuda.set_device(devices[0])
            b = (t0, tn)
            with filecontext_lambda() as f:
                torch.save(b, f)
                f.seek(0)
                c = torch.load(f)
                self.assertEqual(b, c, atol=0, rtol=0)
                u0, un = c
                self.assertEqual(str(u0.device), devices[0])
                self.assertEqual(str(un.device), devices[-1])

        _test_serialization(tempfile.NamedTemporaryFile)
        _test_serialization(BytesIOContext)

    def test_memory_format_preserved_after_permute(self, device):
        x = torch.randn(4, 3, 8, 8, device=device)
        nhwc = x.contiguous(memory_format=torch.channels_last)
        y = nhwc.permute(0, 1, 3, 2).permute(0, 1, 3, 2)
        self.assertTrue(y.is_contiguous(memory_format=torch.channels_last))

        x = torch.randn(4, 3, 8, 8, 8, device=device)
        ndhwc = x.contiguous(memory_format=torch.channels_last_3d)
        y = ndhwc.permute(0, 1, 4, 3, 2).permute(0, 1, 4, 3, 2)
        self.assertTrue(y.is_contiguous(memory_format=torch.channels_last_3d))

    def test_memory_format_propagation_rules(self, device):

        contiguous = torch.rand(10, 3, 5, 5, device=device)
        cl = torch.rand(10, 3, 5, 5, device=device).contiguous(memory_format=torch.channels_last)
        ambiguous = torch.rand(10, 3, 1, 1, device=device).contiguous(memory_format=torch.channels_last)
        self.assertTrue(ambiguous.is_contiguous(memory_format=torch.channels_last))
        self.assertTrue(ambiguous.is_contiguous(memory_format=torch.contiguous_format))
        bias = torch.rand(1, 1, 1, 1, device=device).contiguous(memory_format=torch.channels_last)

        def _test_propagation_rules(self, contiguous, cl, ambiguous, bias):
            options = ((ambiguous, contiguous, torch.contiguous_format),
                       (ambiguous, cl, torch.channels_last),
                       (contiguous, ambiguous, torch.contiguous_format),
                       (contiguous, cl, torch.contiguous_format),
                       (cl, ambiguous, torch.channels_last),
                       (cl, contiguous, torch.channels_last),
                       (bias, cl, torch.channels_last),
                       (cl, bias, torch.channels_last),)

            for a, b, mf in options:
                result = a + b
                self.assertTrue(result.is_contiguous(memory_format=mf))

        _test_propagation_rules(self, contiguous, cl, ambiguous, bias)

        cl = cl.to(memory_format=torch.channels_last)
        ambiguous = ambiguous.to(memory_format=torch.channels_last)
        bias = bias.to(memory_format=torch.channels_last)

        _test_propagation_rules(self, contiguous, cl, ambiguous, bias)

        # test cases when strides matter in ambiguous tensors
        for mf in (torch.channels_last, torch.contiguous_format):
            ambiguous = torch.rand(10, 3, 1, 1, device=device).to(memory_format=mf)
            bias = torch.rand(3, 1, 1, device=device)
            result = ambiguous + bias
            self.assertEqual(ambiguous.stride(), result.stride())
            result = bias + ambiguous
            self.assertEqual(ambiguous.stride(), result.stride())
            result = ambiguous * 5
            self.assertEqual(ambiguous.stride(), result.stride())

    def test_memory_format_empty_like(self, device):
        def test_helper(x, memory_format):
            xc = x.contiguous(memory_format=memory_format)

            like = torch.empty_like(xc, memory_format=torch.preserve_format)
            self.assertFalse(like.is_contiguous())
            self.assertTrue(like.is_contiguous(memory_format=memory_format))

            like_x = torch.empty_like(x, memory_format=torch.preserve_format)
            self.assertTrue(like_x.is_contiguous())
            self.assertFalse(like_x.is_contiguous(memory_format=memory_format))

            like = torch.empty_like(x, memory_format=memory_format)
            self.assertFalse(like.is_contiguous())
            self.assertTrue(like.is_contiguous(memory_format=memory_format))

            like = torch.empty_like(xc, memory_format=torch.contiguous_format)
            self.assertTrue(like.is_contiguous())
            self.assertFalse(like.is_contiguous(memory_format=memory_format))

            like = torch.empty_like(xc)
            self.assertFalse(like.is_contiguous())
            self.assertTrue(like.is_contiguous(memory_format=memory_format))

            sparse = x.to_sparse()
            with self.assertRaises(RuntimeError):
                z = torch.empty_like(sparse, memory_format=torch.preserve_format)

        test_helper(torch.randn(4, 3, 8, 8, device=device), torch.channels_last)
        test_helper(torch.randn(4, 3, 8, 8, 8, device=device), torch.channels_last_3d)

    def test_memory_format_consistency(self, device):
        x = torch.randn(10, 3, 1, 1, device=device)
        x_rep = x.as_strided(x.size(), x.stride())
        self.assertEqual(x.size(), x_rep.size())
        self.assertEqual(x.stride(), x_rep.stride())
        self.assertEqual(x.is_contiguous(), x_rep.is_contiguous())
        self.assertEqual(x.is_contiguous(memory_format=torch.channels_last), x_rep.is_contiguous(memory_format=torch.channels_last))
        self.assertEqual(
            x.is_contiguous(memory_format=torch.channels_last_3d), x_rep.is_contiguous(memory_format=torch.channels_last_3d))

    def test_memory_format_operators(self, device):
        def _chunk_op(x, y):
            x1, x2 = x.chunk(2, dim=1)
            return x1 + x2

        def _unsqueeze_op_add(x, y):
            return x[0].unsqueeze(0) + 3

        def _unsqueeze_op_clone(x, y):
            return x[0].unsqueeze(0).clone()

        def _test_helper(x, y, bias, memory_format):
            return_contig_fns = [
                lambda x, y: y + x,
                lambda x, y: y * x,
                lambda x, y: y.addcdiv(x, y, value=2),
                lambda x, y: y.addcmul(x, y, value=2),
            ]
            bias_fns = [
                lambda x, b: x + b,
                lambda x, b: b + x,
            ]
            fns = [
                lambda x, y: x.clone(),
                lambda x, y: x + 3,
                lambda x, y: 3 * x,
                lambda x, y: x + y,
                lambda x, y: x * y,
                lambda x, y: abs(x),
                lambda x, y: x.abs(),
                lambda x, y: x.abs_(),
                lambda x, y: x.acos(),
                lambda x, y: x.acos_(),
                lambda x, y: x.add(y, alpha=3),
                lambda x, y: x.add_(y, alpha=3),
                lambda x, y: x.addcdiv(y, y, value=2),
                lambda x, y: x.addcdiv_(y, y, value=2),
                lambda x, y: x.addcmul(y, y, value=2),
                lambda x, y: x.addcmul_(y, y, value=2),
                lambda x, y: x.acosh(),
                lambda x, y: x.acosh_(),
                lambda x, y: x.asinh(),
                lambda x, y: x.asinh_(),
                lambda x, y: x.atanh(),
                lambda x, y: x.atanh_(),
                lambda x, y: x.asin(),
                lambda x, y: x.asin_(),
                lambda x, y: x.atan(),
                lambda x, y: x.atan2(y),
                lambda x, y: x.atan2_(y),
                lambda x, y: x.ceil(),
                lambda x, y: x.ceil_(),
                lambda x, y: x.clamp(-1, 1),
                lambda x, y: x.cos(),
                lambda x, y: x.cosh(),
                lambda x, y: x.div(0.5),
                lambda x, y: x.div_(0.5),
                lambda x, y: x.div(y),
                lambda x, y: x.div_(y),
                lambda x, y: x.digamma(),
                lambda x, y: x.digamma_(),
                lambda x, y: x.erf(),
                lambda x, y: x.erfc(),
                lambda x, y: x.erfinv(),
                lambda x, y: x.erfinv_(),
                lambda x, y: x.exp(),
                lambda x, y: x.expm1(),
                lambda x, y: x.expm1_(),
                lambda x, y: x.floor(),
                lambda x, y: x.floor_(),
                lambda x, y: x.fmod(2),
                lambda x, y: x.frac(),
                lambda x, y: x.hypot(y),
                lambda x, y: x.hypot_(y),
                lambda x, y: x.i0(),
                lambda x, y: x.i0_(),
                lambda x, y: x.lerp(y, 0.5),
                lambda x, y: x.log(),
                lambda x, y: x.log_(),
                lambda x, y: x.log10(),
                lambda x, y: x.log10_(),
                lambda x, y: x.log1p(),
                lambda x, y: x.log1p_(),
                lambda x, y: x.log2(),
                lambda x, y: x.log2_(),
                lambda x, y: x.mul(3),
                lambda x, y: x.mul_(3),
                lambda x, y: x.neg(),
                lambda x, y: x.neg_(),
                lambda x, y: x.pow(3),
                lambda x, y: x.pow_(3),
                lambda x, y: x.pow(0.0),
                lambda x, y: x.pow(1.0),
                lambda x, y: x.reciprocal(),
                lambda x, y: x.remainder(2),
                lambda x, y: x.round(),
                lambda x, y: x.round_(),
                lambda x, y: x.rsqrt(),
                lambda x, y: x.rsqrt_(),
                lambda x, y: x.sigmoid(),
                lambda x, y: x.sigmoid_(),
                lambda x, y: x.logit(),
                lambda x, y: x.logit_(),
                lambda x, y: x.logit(1e-6),
                lambda x, y: x.logit_(1e-6),
                lambda x, y: x.sign(),
                lambda x, y: x.sign_(),
                lambda x, y: x.sgn(),
                lambda x, y: x.sgn_(),
                lambda x, y: x.sin(),
                lambda x, y: x.sin_(),
                lambda x, y: x.sinh(),
                lambda x, y: x.sinh_(),
                lambda x, y: x.sqrt(),
                lambda x, y: x.sqrt_(),
                lambda x, y: x.tan(),
                lambda x, y: x.tanh(),
                lambda x, y: x.trunc(),
                lambda x, y: x.trunc_(),
                _chunk_op,
                _unsqueeze_op_add,
                _unsqueeze_op_clone,
            ]
            for fn in fns:
                x_c = x.contiguous()
                y_c = y.contiguous()
                result_c = fn(x_c, y_c)
                result = fn(x, y)
                self.assertEqual(result, result_c)
                self.assertTrue(
                    result.is_contiguous(memory_format=memory_format),
                    "result of the '{}' is not in '{}' format".format(inspect.getsource(fn).strip(), memory_format))

            for fn in bias_fns:
                x_c = x.contiguous()
                b_c = bias.contiguous()
                result_c = fn(x_c, b_c)
                result = fn(x, bias)
                self.assertEqual(result, result_c)
                self.assertTrue(
                    result.is_contiguous(memory_format=memory_format),
                    "result of the '{}' is not in '{}' format".format(inspect.getsource(fn).strip(), memory_format))

            for fn in return_contig_fns:
                x_c = x.contiguous()
                y_c = y.contiguous()
                result_c = fn(x_c, y_c)
                result = fn(x, y)
                self.assertEqual(result, result_c)
                self.assertTrue(
                    result.is_contiguous(memory_format=torch.contiguous_format),
                    "result of the '{}' is not in '{}' format".format(inspect.getsource(fn).strip(), torch.contiguous_format))

        _test_helper(
            torch.randn((4, 3, 8, 8), device=device).contiguous(memory_format=torch.channels_last),
            abs(torch.randn((4, 3, 8, 8), device=device)) + 1,
            torch.randn((1, 3, 1, 1), device=device).contiguous(memory_format=torch.channels_last),
            torch.channels_last)
        _test_helper(
            torch.randn((4, 3, 8, 8, 8), device=device).contiguous(memory_format=torch.channels_last_3d),
            abs(torch.randn((4, 3, 8, 8, 8), device=device)) + 1,
            torch.randn((1, 3, 1, 1, 1), device=device).contiguous(memory_format=torch.channels_last_3d),
            torch.channels_last_3d)

    def test_strides_propagation(self, device):

        def _test_helper(x, op, unary=False):
            def compare_strides(s1, s2, div):
                sdiv = [s // div for s in s1]
                self.assertEqual(sdiv, s2)

            dim = x.dim()
            # we produce memory dense outputs, so when input is strided on the last dimension
            # we need to divide by that dimension stride to compare input and result strides
            div = x.stride(-1)
            for p in permutations(range(dim)):
                xp = x.permute(p)
                if not unary:
                    y = torch.randn(xp.size(-1), device=x.device, dtype=x.dtype)
                    for inputs in ((xp, xp), (xp, y), (y, xp)):
                        res = op(*inputs)
                        compare_strides(xp.stride(), res.stride(), div)
                        self.assertEqual(xp.size(), res.size())
                        out = torch.empty(0, device=xp.device, dtype=res.dtype)
                        res = op(*inputs, out=out)
                        compare_strides(xp.stride(), res.stride(), div)
                        self.assertEqual(xp.size(), res.size())
                else:
                    res = op(xp)
                    compare_strides(xp.stride(), res.stride(), div)
                    self.assertEqual(xp.size(), res.size())
                    out = torch.empty(0, device=xp.device, dtype=res.dtype)
                    res = op(xp, out=out)
                    compare_strides(xp.stride(), res.stride(), div)
                    self.assertEqual(xp.size(), res.size())

        # torch.eq by default calls TensorIterator with defined output, torch.add with undefined
        binary_ops = (torch.eq, torch.add)
        unary_ops = (torch.exp,)
        # memory dense, sliced and ambiguous sliced (ambiguous dense loses permutation information)
        xs = (torch.randn(2, 3, 4, device=device), torch.randn(2, 3, 8, device=device)[:, :, ::2],
              torch.randn(1, 1, 4, 12, device=device)[:, :, :, ::2])
        for op in binary_ops:
            for x in xs:
                _test_helper(x, op)
        for op in unary_ops:
            for x in xs:
                _test_helper(x, op, unary=True)

    @skipMeta
    @onlyNativeDeviceTypes
    @dtypes(*get_all_dtypes(include_bool=False))
    def test_dlpack_capsule_conversion(self, device, dtype):
        # DLpack does not explicitly support bool (xref dmlc/dlpack#75)
        x = make_tensor((5,), device, dtype)
        z = from_dlpack(to_dlpack(x))
        self.assertEqual(z, x)

    @skipMeta
    @onlyNativeDeviceTypes
    @dtypes(*get_all_dtypes(include_bool=False))
    def test_dlpack_protocol_conversion(self, device, dtype):
        x = make_tensor((5,), device, dtype)
        z = from_dlpack(x)
        self.assertEqual(z, x)

    @skipMeta
    @onlyNativeDeviceTypes
    def test_dlpack_shared_storage(self, device):
        x = make_tensor((5,), device, torch.float64)
        z = from_dlpack(to_dlpack(x))
        z[0] = z[0] + 20.0
        self.assertEqual(z, x)

    @skipMeta
    @onlyCUDA
    @dtypes(*get_all_dtypes(include_bool=False))
    def test_dlpack_conversion_with_streams(self, device, dtype):
        # Create a stream where the tensor will reside
        stream = torch.cuda.Stream()
        with torch.cuda.stream(stream):
            # Do an operation in the actual stream
            x = make_tensor((5,), device, dtype) + 1
        # DLPack protocol helps establish a correct stream order
        # (hence data dependency) at the exchange boundary.
        # DLPack manages this synchronization for us, so we don't need to
        # explicitly wait until x is populated
        stream = torch.cuda.Stream()
        with torch.cuda.stream(stream):
            z = from_dlpack(x)
        stream.synchronize()
        self.assertEqual(z, x)

    @skipMeta
    @onlyCUDA
    @dtypes(*get_all_dtypes(include_bool=False))
    def test_dlpack_conversion_with_diff_streams(self, device, dtype):
        from torch._C import _from_dlpack
        stream_a = torch.cuda.Stream()
        stream_b = torch.cuda.Stream()
        # DLPack protocol helps establish a correct stream order
        # (hence data dependency) at the exchange boundary.
        # the `tensor.__dlpack__` method will insert a synchronization event
        # in the current stream to make sure that it was correctly populated.
        with torch.cuda.stream(stream_a):
            x = make_tensor((5,), device, dtype) + 1
            z = _from_dlpack(x.__dlpack__(stream_b.cuda_stream))
            stream_a.synchronize()
        stream_b.synchronize()
        self.assertEqual(z, x)

    @skipMeta
    @onlyCUDA
    def test_dlpack_default_stream(self, device):
        class DLPackTensor:
            def __init__(self, tensor):
                self.tensor = tensor

            def __dlpack_device__(self):
                return self.tensor.__dlpack_device__()

            def __dlpack__(self, stream=None):
                if torch.version.hip is None:
                    assert stream == 1
                else:
                    assert stream == 0
                capsule = self.tensor.__dlpack__(stream)
                converted = True
                return capsule

        # CUDA-based tests runs on non-default streams
        with torch.cuda.stream(torch.cuda.default_stream()):
            x = DLPackTensor(make_tensor((5,), device, torch.float32))
            from_dlpack(x)

    @skipMeta
    @onlyNativeDeviceTypes
    @dtypes(*get_all_dtypes(include_bool=False))
    def test_dlpack_tensor_invalid_stream(self, device, dtype):
        with self.assertRaises(TypeError):
            x = make_tensor((5,), device, dtype)
            x.__dlpack__(stream=object())

    @skipMeta
    def test_dlpack_error_on_bool_tensor(self):
        x = torch.tensor([True], dtype=torch.bool)
        with self.assertRaises(RuntimeError):
            to_dlpack(x)

    # TODO: increase tests once NumPy supports the `__dlpack__` protocol

    @skipMeta
    def test_dlpack_export_requires_grad(self):
        x = torch.zeros(10, dtype=torch.float32, requires_grad=True)
        with self.assertRaisesRegex(RuntimeError, r"require gradient"):
            x.__dlpack__()

    @skipMeta
    def test_dlpack_export_is_conj(self):
        x = torch.tensor([-1 + 1j, -2 + 2j, 3 - 3j])
        y = torch.conj(x)
        with self.assertRaisesRegex(RuntimeError, r"conjugate bit"):
            y.__dlpack__()

    @skipMeta
    def test_dlpack_export_non_strided(self):
        x = torch.sparse_coo_tensor([[0]], [1], size=(1,))
        y = torch.conj(x)
        with self.assertRaisesRegex(RuntimeError, r"strided"):
            y.__dlpack__()

    @onlyCUDA
    @unittest.skipIf(PYTORCH_CUDA_MEMCHECK, "is_pinned uses failure to detect pointer property")
    def test_pin_memory_from_constructor(self, device):
        def _get_like(t, **kwargs):
            return [
                torch.rand_like(t, **kwargs),
                torch.randn_like(t, **kwargs),
                torch.empty_like(t, **kwargs),
                torch.full_like(t, 4, **kwargs),
                torch.zeros_like(t, **kwargs),
                torch.ones_like(t, **kwargs),
            ]

        def _get_tensors(**kwargs):
            return [
                torch.tensor([10, 11], **kwargs),
                torch.randn(3, 5, **kwargs),
                torch.rand(3, **kwargs),
                # torch.randint(3, 5, **kwargs), // unsupported
                torch.zeros(3, **kwargs),
                torch.randperm(3, **kwargs),
                torch.empty(6, **kwargs),
                torch.ones(6, **kwargs),
                torch.eye(6, **kwargs),
                torch.arange(3, 5, **kwargs)]

        pinned_tensors = _get_tensors(pin_memory=True) + _get_like(torch.empty(5, dtype=torch.float64), pin_memory=True)
        for x in pinned_tensors:
            self.assertTrue(x.is_pinned())

        tensors = _get_tensors() + _get_like(torch.empty(5, dtype=torch.float64, pin_memory=True))
        for x in tensors:
            self.assertFalse(x.is_pinned())

    def test_storage_device(self, device):
        x = torch.tensor([], device=device)
        self.assertEqual(x.dtype, x.storage().dtype)

    @deviceCountAtLeast(2)
    @onlyCUDA
    def test_storage_multigpu(self, devices):
        for device in devices:
            x = torch.tensor([], device=device)
            self.assertEqual(x.dtype, x.storage().dtype)

    @dtypesIfCUDA(torch.float, torch.double, torch.half)
    @dtypes(torch.float, torch.double)
    def test_multinomial(self, device, dtype):
        def make_prob_dist(shape, is_contiguous):
            if is_contiguous:
                if dtype == torch.half:
                    return torch.zeros(shape, device=device).uniform_().to(dtype=torch.half)
                return torch.zeros(shape, device=device, dtype=dtype).uniform_()
            elif len(shape) == 1:
                if dtype == torch.half:
                    return torch.zeros((shape + [5]), device=device).uniform_().to(dtype=torch.half)[:, 2]
                return torch.zeros((shape + [5]), device=device, dtype=dtype).uniform_()[:, 2]
            else:
                # num dim = 2
                new_shape = [2, shape[1], 7, 1, shape[0], 1, 10]
                if dtype == torch.half:
                    prob_dist = torch.zeros(new_shape, device=device).uniform_().to(dtype=torch.half)
                else:
                    prob_dist = torch.zeros(new_shape, device=device, dtype=dtype).uniform_()
                prob_dist = prob_dist.transpose(1, 4)
                prob_dist = prob_dist[1, :, 5, 0, :, 0, 4]
                assert not prob_dist.is_contiguous()  # sanity check
                return prob_dist

        for is_contiguous in (True, False):
            # with replacement
            n_row = 3
            for n_col in range(4, 5 + 1):
                prob_dist = make_prob_dist([n_row, n_col], is_contiguous)
                # indices that shouldn't be sampled (<0 means none)
                zero_prob_indices = torch.LongTensor(n_row).random_(-2, n_col).tolist()
                for i, j in enumerate(zero_prob_indices):
                    if j >= 0:
                        prob_dist[i, j] = 0
                n_sample = n_col * 3
                sample_indices = torch.multinomial(prob_dist, n_sample, True)
                self.assertEqual(prob_dist.dim(), 2)
                self.assertEqual(sample_indices.size(1), n_sample)
                for i in range(n_row):
                    zero_prob_idx = zero_prob_indices[i]
                    if zero_prob_idx < 0:
                        continue
                    for j in range(n_sample):
                        self.assertNotEqual(sample_indices[i, j], zero_prob_idx,
                                            msg="sampled an index with zero probability")

            # without replacement
            n_row = 3
            for n_col in range(2, 10 + 1, 2):
                prob_dist = make_prob_dist([n_row, n_col], is_contiguous)
                # indices that shouldn't be sampled (<0 means none)
                zero_prob_indices = torch.LongTensor(n_row).random_(-1, n_col).tolist()
                for i, j in enumerate(zero_prob_indices):
                    if j >= 0:
                        prob_dist[i, j] = 0
                n_sample = max(1, n_col - 2)
                sample_indices = torch.multinomial(prob_dist, n_sample, False)
                self.assertEqual(prob_dist.dim(), 2)
                self.assertEqual(sample_indices.size(1), n_sample)
                for i in range(n_row):
                    row_samples = {}
                    zero_prob_idx = zero_prob_indices[i]
                    for j in range(n_sample):
                        sample_idx = sample_indices[i, j]
                        if zero_prob_idx >= 0:
                            self.assertNotEqual(sample_idx, zero_prob_idx,
                                                msg="sampled an index with zero probability")
                        self.assertNotIn(sample_idx, row_samples, "sampled an index twice")
                        row_samples[sample_idx] = True

            # vector
            n_col = 4
            prob_dist = make_prob_dist([n_col], is_contiguous).fill_(1)
            zero_prob_idx = 1  # index that shouldn't be sampled
            prob_dist[zero_prob_idx] = 0
            n_sample = 20
            sample_indices = torch.multinomial(prob_dist, n_sample, True)
            for sample_index in sample_indices:
                self.assertNotEqual(sample_index, zero_prob_idx, msg="sampled an index with zero probability")
            s_dim = sample_indices.dim()
            self.assertEqual(sample_indices.dim(), 1, msg="wrong number of dimensions")
            self.assertEqual(prob_dist.dim(), 1, msg="wrong number of prob_dist dimensions")
            self.assertEqual(sample_indices.size(0), n_sample, msg="wrong number of samples")

        # CUDA misalignment issue (#46702)
        n_row, n_col = 2, 3
        prob_dist = make_prob_dist([n_row, n_col], True)
        n_sample = 1
        sample_indices = torch.multinomial(prob_dist, n_sample, True)
        self.assertEqual(sample_indices.dim(), 2, msg="wrong number of dimensions")
        self.assertEqual(sample_indices.size(1), n_sample, msg="wrong number of samples")

    @onlyCUDA
    @dtypes(torch.float, torch.double, torch.half)
    def test_multinomial_deterministic(self, device, dtype):
        gen = torch.Generator(device=device)

        trials = 5
        seed = 0
        prob_dist = torch.rand(10000, 1000, device=device, dtype=dtype)
        n_sample = 1

        for i in range(trials):
            gen.manual_seed(seed)
            samples_1 = torch.multinomial(prob_dist, n_sample, True, generator=gen)

            gen.manual_seed(seed)
            samples_2 = torch.multinomial(prob_dist, n_sample, True, generator=gen)

            self.assertEqual(samples_1, samples_2)
            self.assertEqual(samples_1.dim(), 2, msg="wrong number of dimensions")
            self.assertEqual(samples_1.size(1), n_sample, msg="wrong number of samples")


    @slowTest
    @dtypes(torch.float)
    def test_multinomial_rng_state_advance(self, device, dtype):
        corpus_size = 100000
        freqs = torch.ones(corpus_size, dtype=torch.float, device=device)
        n_sample = 100
        samples1 = torch.multinomial(freqs, n_sample, replacement=True)
        samples2 = torch.multinomial(freqs, n_sample, replacement=True)
        samples = torch.cat([samples1, samples2])
        # expect no more than 1 repeating elements generated in 2 attempts
        # the probability of at least element being repeated is surprisingly large, 18%
        self.assertLessEqual(2 * n_sample - samples.unique().size(0), 2)
        samples1 = torch.multinomial(freqs, n_sample, replacement=False)
        samples2 = torch.multinomial(freqs, n_sample, replacement=False)
        samples = torch.cat([samples1, samples2])
        # expect no more than 1 repeating elements generated in 2 attempts
        self.assertLessEqual(2 * n_sample - samples.unique().size(0), 1)

    def _test_memory_format_transformations(self, device, input_generator_fn, transformation_fn,
                                            memory_format, compare_data=True, default_is_preserve=False):

        assert(memory_format == torch.channels_last or memory_format == torch.channels_last_3d)

        # xc is a channels last tensor
        xc = input_generator_fn(device)
        # xc is not memory dense, but looks like channels last
        if memory_format == torch.channels_last:
            xc = xc[..., ::2, ::2]
        else:
            xc = xc[..., ::2, ::2, ::2]

        clone = transformation_fn(xc, memory_format=torch.preserve_format)
        self.assertFalse(clone.is_contiguous())
        self.assertTrue(clone.is_contiguous(memory_format=memory_format))
        self.assertFalse(xc.is_contiguous())
        self.assertFalse(xc.is_contiguous(memory_format=memory_format))
        if compare_data:
            self.assertEqual(xc, clone.to(xc))

        xc = input_generator_fn(device)
        clone = transformation_fn(xc, memory_format=torch.contiguous_format)
        self.assertTrue(clone.is_contiguous())
        self.assertFalse(clone.is_contiguous(memory_format=memory_format))
        if compare_data:
            self.assertEqual(xc, clone.to(xc))

        xc = input_generator_fn(device)
        clone = transformation_fn(xc)

        if default_is_preserve:
            self.assertFalse(clone.is_contiguous())
            self.assertTrue(clone.is_contiguous(memory_format=memory_format))
        else:
            self.assertTrue(clone.is_contiguous())
            self.assertFalse(clone.is_contiguous(memory_format=memory_format))
        if compare_data:
            self.assertEqual(xc, clone.to(xc))

        x = torch.randn((3, 4, 5, 6, 7, 8, 9), device=device)
        for _ in range(10):
            permutation = list(range(len(x.shape)))
            random.shuffle(permutation)
            x = x.permute(permutation)
            self.assertEqual(x.stride(), transformation_fn(x, memory_format=torch.preserve_format).stride())

    def test_memory_format_to(self, device):
        def get_generator(memory_format, shape):
            def input_generator_fn(device):
                return torch.randn(shape, device=device, dtype=torch.float32).contiguous(memory_format=memory_format)
            return input_generator_fn

        def transformation_fn(tensor, **kwargs):
            return tensor.to(dtype=torch.float64, **kwargs)

        formats_shapes = (
            (torch.channels_last, (4, 3, 8, 8)),
            (torch.channels_last_3d, (4, 3, 8, 8, 8)))

        for mf, shape in formats_shapes:
            self._test_memory_format_transformations(
                device, get_generator(mf, shape), transformation_fn, mf, default_is_preserve=True)

    def test_memory_format_type(self, device):
        def get_generator(memory_format, shape):
            def input_generator_fn(device):
                return torch.randn(shape, device=device, dtype=torch.float32).contiguous(memory_format=memory_format)
            return input_generator_fn

        def transformation_fn(tensor, **kwargs):
            return tensor.to(torch.float64, **kwargs)

        formats_shapes = (
            (torch.channels_last, (4, 3, 8, 8)),
            (torch.channels_last_3d, (4, 3, 8, 8, 8)))

        for mf, shape in formats_shapes:
            self._test_memory_format_transformations(
                device, get_generator(mf, shape), transformation_fn, mf, default_is_preserve=True)

    def test_memory_format_clone(self, device):
        def get_generator(memory_format, shape):
            def input_generator_fn(device):
                return torch.randn(shape, device=device, dtype=torch.float32).contiguous(memory_format=memory_format)
            return input_generator_fn

        def transformation_fn(tensor, **kwargs):
            return tensor.clone(**kwargs)

        formats_shapes = (
            (torch.channels_last, (4, 3, 8, 8)),
            (torch.channels_last_3d, (4, 3, 8, 8, 8)))

        for mf, shape in formats_shapes:
            self._test_memory_format_transformations(
                device, get_generator(mf, shape), transformation_fn, mf, True, default_is_preserve=True)

    def test_memory_format_factory_like_functions_preserve(self, device):
        def get_generator(memory_format, shape):
            def input_generator_fn(device):
                return torch.randn(shape, device=device, dtype=torch.float32).contiguous(memory_format=memory_format)
            return input_generator_fn

        transformation_fns = [
            lambda t, **kwargs: torch.zeros_like(t, **kwargs),
            lambda t, **kwargs: torch.ones_like(t, **kwargs),
            lambda t, **kwargs: torch.randint_like(t, 10, 100, **kwargs),
            lambda t, **kwargs: torch.randint_like(t, 100, **kwargs),
            lambda t, **kwargs: torch.randn_like(t, **kwargs),
            lambda t, **kwargs: torch.rand_like(t, **kwargs),
            lambda t, **kwargs: torch.full_like(t, 7, **kwargs),
            lambda t, **kwargs: torch.empty_like(t, **kwargs)]

        formats_shapes = (
            (torch.channels_last, (4, 3, 8, 8)),
            (torch.channels_last_3d, (4, 3, 8, 8, 8)))

        for mf, shape, in formats_shapes:
            for transformation_fn in transformation_fns:
                self._test_memory_format_transformations(
                    device, get_generator(mf, shape), transformation_fn, mf, compare_data=False, default_is_preserve=True)

    def test_memory_format_type_shortcuts(self, device):
        def get_generator(memory_format, shape, dtype):
            def input_generator_fn(device):
                return torch.randn(shape, device=device, dtype=dtype).clamp(0, 1) \
                    .round().contiguous(memory_format=memory_format)
            return input_generator_fn


        def get_fn(fn_name):
            def transformation_fn(tensor, **kwargs):
                fn = getattr(tensor, fn_name)
                return fn(**kwargs)
            return transformation_fn

        shortcuts = ['byte', 'char', 'double', 'bool', 'half', 'int', 'long', 'short']
        if device == 'cpu':
            shortcuts += ['bfloat16']

        formats_shapes = (
            (torch.channels_last, (4, 3, 8, 8)),
            (torch.channels_last_3d, (4, 3, 8, 8, 8)))

        for mf, shape in formats_shapes:
            for fn_name in shortcuts:
                self._test_memory_format_transformations(
                    device, get_generator(mf, shape, torch.float32), get_fn(fn_name), mf, default_is_preserve=True)

        # Test 'float' separately to avoid float->float no-op.
        for mf, shape in formats_shapes:
            self._test_memory_format_transformations(
                device, get_generator(mf, shape, torch.float64), get_fn('float'), mf, default_is_preserve=True)

    @onlyCUDA
    def test_memory_format_cpu_and_cuda_ops(self, device):
        def get_generator(memory_format, shape):
            def input_generator_fn(device):
                return torch.randn(shape, device=device, dtype=torch.float32).contiguous(memory_format=memory_format)
            return input_generator_fn

        def transformation_cpu_fn(tensor, **kwargs):
            return tensor.cpu(**kwargs)

        def transformation_cuda_fn(tensor, **kwargs):
            return tensor.cuda(**kwargs)

        formats_shapes = (
            (torch.channels_last, (4, 3, 8, 8)),
            (torch.channels_last_3d, (4, 3, 8, 8, 8)))

        for mf, shape in formats_shapes:
            self._test_memory_format_transformations(
                'cuda', get_generator(mf, shape), transformation_cpu_fn, mf, default_is_preserve=True)
            self._test_memory_format_transformations(
                'cpu', get_generator(mf, shape), transformation_cuda_fn, mf, default_is_preserve=True)

    @dtypes(torch.complex64, torch.complex128)
    def test_complex_unsupported(self, device, dtype):
        t = torch.tensor((1 + 1j), device=device, dtype=dtype)
        # Note: this is consistent with NumPy
        with self.assertRaises(RuntimeError):
            torch.floor(t)
        with self.assertRaises(RuntimeError):
            torch.ceil(t)
        with self.assertRaises(RuntimeError):
            torch.trunc(t)

        # Tests min and max variants with complex inputs
        # Note: whether PyTorch should support min and max on complex
        # tensors is an open question.
        # See https://github.com/pytorch/pytorch/issues/36374
        with self.assertRaisesRegex(RuntimeError, '(.*not support.*)|(.*not implemented.*)'):
            torch.min(t)
        with self.assertRaisesRegex(RuntimeError, '(.*not support.*)|(.*not implemented.*)'):
            t.min()
        with self.assertRaisesRegex(RuntimeError, '(.*not support.*)|(.*not implemented.*)'):
            torch.min(t, dim=0)
        with self.assertRaisesRegex(RuntimeError, '(.*not support.*)|(.*not implemented.*)'):
            torch.min(t, t)
        with self.assertRaisesRegex(RuntimeError, '(.*not support.*)|(.*not implemented.*)'):
            torch.min(t, t, out=t)

        with self.assertRaisesRegex(RuntimeError, '(.*not support.*)|(.*not implemented.*)'):
            torch.max(t)
        with self.assertRaisesRegex(RuntimeError, '(.*not support.*)|(.*not implemented.*)'):
            t.max()
        with self.assertRaisesRegex(RuntimeError, '(.*not support.*)|(.*not implemented.*)'):
            torch.max(t, dim=0)
        with self.assertRaisesRegex(RuntimeError, '(.*not support.*)|(.*not implemented.*)'):
            torch.max(t, t)
        with self.assertRaisesRegex(RuntimeError, '(.*not support.*)|(.*not implemented.*)'):
            torch.max(t, t, out=t)

        with self.assertRaisesRegex(RuntimeError, '(.*not support.*)|(.*not implemented.*)'):
            torch.amin(t)
        with self.assertRaisesRegex(RuntimeError, '(.*not support.*)|(.*not implemented.*)'):
            t.amin()
        with self.assertRaisesRegex(RuntimeError, '(.*not support.*)|(.*not implemented.*)'):
            torch.amin(t, dim=0)

        with self.assertRaisesRegex(RuntimeError, '(.*not support.*)|(.*not implemented.*)'):
            torch.amax(t)
        with self.assertRaisesRegex(RuntimeError, '(.*not support.*)|(.*not implemented.*)'):
            t.amax()
        with self.assertRaisesRegex(RuntimeError, '(.*not support.*)|(.*not implemented.*)'):
            torch.amax(t, dim=0)

        # Tests _aminmax() variants with complex inputs,
        # which are currently not supported due to min & max being unsupported
        # for complex inputs, as per https://github.com/pytorch/pytorch/issues/36374
        # Test with a single-element tensor t, as well as a multi-element tensor x
        with self.assertRaisesRegex(RuntimeError, '(.*not support.*)|(.*not implemented.*)'):
            min_val, max_val = torch._aminmax(t)
        with self.assertRaisesRegex(RuntimeError, '(.*not support.*)|(.*not implemented.*)'):
            min_val = torch._aminmax(t, dim=0)[0]
        with self.assertRaisesRegex(RuntimeError, '(.*not support.*)|(.*not implemented.*)'):
            max_val = torch._aminmax(t, dim=0)[1]
        # Test _aminmax() with a multi-element tensor
        x = torch.tensor([(1 + 1j), (2 + 3j)], device=device, dtype=dtype)
        with self.assertRaisesRegex(RuntimeError, '(.*not support.*)|(.*not implemented.*)'):
            min_val, max_val = torch._aminmax(x)
        with self.assertRaisesRegex(RuntimeError, '(.*not support.*)|(.*not implemented.*)'):
            min_val = torch._aminmax(x, dim=0)[0]
        with self.assertRaisesRegex(RuntimeError, '(.*not support.*)|(.*not implemented.*)'):
            max_val = torch._aminmax(x, dim=0)[1]

        # Tests clamp variants with complex inputs
        # Note: whether PyTorch should support clamp on complex
        # tensors is an open question.
        # See https://github.com/pytorch/pytorch/issues/33568
        min_val = 1 + 1j
        max_val = 4 + 4j
        out = torch.empty((0,), device=device, dtype=dtype)
        with self.assertRaisesRegex(RuntimeError, '(.*not support.*)|(.*not implemented.*)'):
            torch.clamp(t, min=min_val)
        with self.assertRaisesRegex(RuntimeError, '(.*not support.*)|(.*not implemented.*)'):
            torch.clamp(t, max=max_val)
        with self.assertRaisesRegex(RuntimeError, '(.*not support.*)|(.*not implemented.*)'):
            torch.clamp(t, min_val, max_val)
        with self.assertRaisesRegex(RuntimeError, '(.*not support.*)|(.*not implemented.*)'):
            torch.clamp(t, min=min_val, out=out)
        with self.assertRaisesRegex(RuntimeError, '(.*not support.*)|(.*not implemented.*)'):
            torch.clamp(t, max=max_val, out=out)
        with self.assertRaisesRegex(RuntimeError, '(.*not support.*)|(.*not implemented.*)'):
            torch.clamp(t, min_val, max_val, out=out)

    def test_pickle_gradscaler(self, device):
        # This test is not in test_cuda.py because it should pass in 3 cases:
        #  1. cuda is not available.
        #  2. cuda is available but device is not cuda.
        #  3. cuda is available and device is cuda.
        # In case 1, a and b disable themselves on construction and shouldn't try to pickle workhorse attributes.
        # In case 2, a and b are enabled.  Workhorse attributes participate in pickling, but none are lazy-inited
        # to cuda Tensors, because I don't want to do cuda things if device is not cuda.
        # In case 3, a and b are enabled and we may also try lazy-initing _scale to a cuda tensor.
        device = torch.device(device)
        try_lazy_inits = (True, False) if device.type == "cuda" else (False,)
        for lazy_init_scale in try_lazy_inits:
            a = torch.cuda.amp.GradScaler(init_scale=3., growth_factor=4., backoff_factor=.5, growth_interval=2)
            self.assertTrue(not a.is_enabled() if torch.cuda.amp.common.amp_definitely_not_available() else a.is_enabled())
            if lazy_init_scale:
                # Dummy a.scale() call lazy-inits a._scale Tensor.
                a.scale(torch.tensor([4.0], dtype=torch.float32, device=device))
                self.assertTrue(isinstance(a._scale, torch.cuda.FloatTensor))
            # The following three lines should work whether or not cuda is available.
            serialized = pickle.dumps(a)
            b = pickle.loads(serialized)
            self.assertEqual(b.is_enabled(), a.is_enabled())
            if a.is_enabled():
                self.assertEqual(b.get_scale(), 3.)
                self.assertEqual(b.get_growth_factor(), 4.)
                self.assertEqual(b.get_backoff_factor(), .5)
                self.assertEqual(b.get_growth_interval(), 2)
                self.assertEqual(b._init_growth_tracker, 0)
                # supplies a dummy key to test the defaultdict's default_factory
                self.assertEqual(b._per_optimizer_states["fdsa"],
                                 torch.cuda.amp.grad_scaler._refresh_per_optimizer_state())
                if lazy_init_scale:
                    self.assertEqual(b.scale(torch.tensor([4.0], dtype=torch.float32, device=device)), 12.0)

    def test_multinomial_invalid(self, device):
        def test(probs):
            with self.assertRaisesRegex(RuntimeError,
                                        'probability tensor contains either `inf`, `nan` or element < 0'):
                torch.multinomial(probs.to(device), 2)
                torch.cuda.synchronize()

        test(torch.tensor([1., -1., 1.]))
        test(torch.tensor([1., inf, 1.]))
        test(torch.tensor([1., -inf, 1.]))
        test(torch.tensor([1., 1., nan]))

    def test_multinomial_invalid_distribution(self, device):
        def test(probs, replacement):
            with self.assertRaisesRegex(RuntimeError,
                                        r"invalid multinomial distribution \(sum of probabilities <= 0\)"):
                torch.multinomial(probs, 2, replacement)
                torch.cuda.synchronize()

        x = torch.zeros(3, device=device)
        y = torch.zeros(3, 3, device=device)
        z = torch.zeros(3, 3, device=device)
        z[1, :] = 1

        test(x, False)
        test(y, False)
        test(z, False)

        # Verify only for CPU as replacement=True
        # throws device side assert triggered.
        if self.device_type == 'cpu':
            test(x, True)
            test(y, True)
            test(z, True)

    def _test_multinomial_empty(self, device, replacement, num_samples):
        probs = torch.ones(0, 3, device=device)
        expected = torch.empty(0, num_samples, dtype=torch.int64)
        out = torch.multinomial(probs, num_samples=num_samples, replacement=replacement)
        self.assertEqual(out, expected)

    def test_multinomial_empty_w_replacement(self, device):
        self._test_multinomial_empty(device, True, 1)
        self._test_multinomial_empty(device, True, 2)

    def test_multinomial_empty_wo_replacement(self, device):
        self._test_multinomial_empty(device, False, 1)
        self._test_multinomial_empty(device, False, 2)

    def _generate_input(self, shape, dtype, device, with_extremal):
        if shape == ():
            x = torch.tensor((), dtype=dtype, device=device)
        else:
            if dtype.is_floating_point or dtype.is_complex:
                # work around torch.randn not being implemented for bfloat16
                if dtype == torch.bfloat16:
                    x = torch.randn(*shape, device=device) * random.randint(30, 100)
                    x = x.to(torch.bfloat16)
                else:
                    x = torch.randn(*shape, dtype=dtype, device=device) * random.randint(30, 100)
                x[torch.randn(*shape) > 0.5] = 0
                if with_extremal and dtype.is_floating_point:
                    # Use extremal values
                    x[torch.randn(*shape) > 0.5] = float('nan')
                    x[torch.randn(*shape) > 0.5] = float('inf')
                    x[torch.randn(*shape) > 0.5] = float('-inf')
                elif with_extremal and dtype.is_complex:
                    x[torch.randn(*shape) > 0.5] = complex('nan')
                    x[torch.randn(*shape) > 0.5] = complex('inf')
                    x[torch.randn(*shape) > 0.5] = complex('-inf')
            elif dtype == torch.bool:
                x = torch.zeros(shape, dtype=dtype, device=device)
                x[torch.randn(*shape) > 0.5] = True
            else:
                x = torch.randint(15, 100, shape, dtype=dtype, device=device)

        return x

    def _test_where_scalar_template(self, device, dtype, exec_fn):
        for with_extremal in [True, False]:
            for ndims in range(0, 4):
                shape = self._rand_shape(ndims, min_size=5, max_size=10)
                for n in range(ndims + 1):
                    for c in combinations(list(range(ndims)), n):
                        for scalar_type in [int, float, complex]:
                            if dtype.is_complex:
                                condition = self._generate_input(shape, dtype, device, with_extremal).abs() > 0.5
                            else:
                                condition = self._generate_input(shape, dtype, device, with_extremal) > 0.5

                            x = self._generate_input(shape, dtype, device, with_extremal)

                            if not dtype.is_complex and scalar_type == complex:
                                continue

                            scalar_1 = scalar_type(random.random())

                            exec_fn(scalar_type, dtype, condition, x, scalar_1)

    # For current implementation,
    # below are the valid `TensorDtype` and `ScalarType` combinations.
    def _where_valid_scalar_tensor_combination(self, scalar_type, dtype):
        if (scalar_type == int and dtype == torch.long):
            return True
        elif (scalar_type == float and dtype == torch.double):
            return True
        elif (scalar_type == complex and dtype == torch.complex128):
            return True
        return False

    @onlyNativeDeviceTypes
    @dtypes(*(get_all_int_dtypes() + get_all_fp_dtypes() +
              get_all_complex_dtypes()))
    def test_where_scalar_invalid_combination_raises(self, device, dtype):

        def checkRaises(scalar_type, dtype, condition, x, scalar_1):
            if not self._where_valid_scalar_tensor_combination(scalar_type, dtype):
                # Note: This should fail once `where` supports type promotion.
                with self.assertRaisesRegex(RuntimeError, "expected scalar type"):
                    torch.where(condition, x, scalar_1)

        self._test_where_scalar_template(device, dtype, checkRaises)

    @skipCUDAVersionIn([(11, 2)])  # test fails for 11.2, see https://github.com/pytorch/pytorch/issues/51980
    @dtypes(*(get_all_int_dtypes() + get_all_fp_dtypes() +
              get_all_complex_dtypes()))
    def test_where_scalar_valid_combination(self, device, dtype):

        def checkResult(scalar_type, dtype, condition, x, scalar_1):
            if self._where_valid_scalar_tensor_combination(scalar_type, dtype):
                def x_like(scalar, without_dtype=False):
                    return torch.tensor(scalar, dtype=dtype, device=device).expand_as(x)

                # X = Tensor, Y = Scalar
                scalar_out = torch.where(condition, x, scalar_1)
                tensor_out = torch.where(condition, x, x_like(scalar_1))
                self.assertEqual(scalar_out, tensor_out)

                # X = Scalar, Y = Tensor
                scalar_out = torch.where(condition, scalar_1, x)
                tensor_out = torch.where(condition, x_like(scalar_1), x)
                self.assertEqual(scalar_out, tensor_out)

        self._test_where_scalar_template(device, dtype, checkResult)

    # As the test fails with Runtime Error not raised on XLA
    @onlyNativeDeviceTypes
    def test_where_scalar_scalar(self, device):
        # Scalar-Scalar Version
        height = 5
        width = 5
        default_dtype = torch.get_default_dtype()
        for test_default_dtype in [torch.float, torch.double]:
            torch.set_default_dtype(test_default_dtype)
            for scalar_type_1 in [int, float, complex]:
                for scalar_type_2 in [int, float, complex]:
                    x1 = scalar_type_1(random.random() * random.randint(10, 20))
                    x2 = scalar_type_2(random.random() * random.randint(20, 30))
                    condition = torch.randn(height, width, device=device) > 0.5
                    if scalar_type_1 != scalar_type_2:
                        self.assertRaisesRegex(RuntimeError, "expected scalar type", lambda: torch.where(condition, x1, x2))
                    else:
                        def get_dtype(scalar_type):
                            complex_dtype = torch.complex64 if torch.float == torch.get_default_dtype() else torch.complex128
                            type_map = {int: torch.long, float: torch.get_default_dtype(), complex: complex_dtype}
                            return type_map[scalar_type]
                        expected = torch.zeros((height, width), dtype=get_dtype(scalar_type_1))
                        expected[condition] = x1
                        expected[~condition] = x2
                        result = torch.where(condition, x1, x2)
                        self.assertEqual(expected, result)

        # Reset the original dtype
        torch.set_default_dtype(default_dtype)

    def test_hook_remove(self, device):
        # Reference: https://github.com/pytorch/pytorch/issues/58354
        def _test_helper(remove_hook):
            def install_hook(tensor):
                handle = None

                def hook(tensor):
                    if remove_hook:
                        handle.remove()
                    return torch.zeros_like(tensor)
                handle = tensor.register_hook(hook)

            t = torch.ones((1, 5), device=device, requires_grad=True)
            install_hook(t)

            # First call to backward
            t.mean().backward()
            self.assertEqual(t.grad, torch.zeros_like(t))

            # Second call to backward
            t.mean().backward()
            if remove_hook:
                # After removing the hook, make sure the usual gradient is returned
                self.assertEqual(t.grad, 0.2 * torch.ones_like(t))
            else:
                self.assertEqual(t.grad, torch.zeros_like(t))

        _test_helper(remove_hook=True)
        _test_helper(remove_hook=False)

    # This test should ideally be in test_testing.py,
    # but since pytorch/xla runs tests from test_torch.py, we have it here.
    @skipXLA
    def test_skip_xla(self, device):
        if self.device_type == 'xla':
            # Should not reach here!
            self.assertTrue(False)

    # This test should ideally be in test_testing.py,
    # but since pytorch/xla runs tests from test_torch.py, we have it here.
    @expectedFailureXLA
    def test_expected_failure_xla(self, device):
        if self.device_type == 'xla':
            self.assertTrue(False)

    # This test should ideally be in test_testing.py,
    # but since pytorch/xla runs tests from test_torch.py, we have it here.
    def test_assertRaisesRegex_ignore_msg_non_native_device(self, device):
        # Verify that self.assertRaisesRegex only checks the Error and ignores
        # message for non-native devices.
        x = torch.randn((10, 3), device=device)
        t = torch.empty(10, dtype=torch.int64, device=device).random_(0, 3)
        invalid_weight = torch.randn(4, device=device)
        msg = "weight tensor should be defined either for all 3 classes or no classes"

        # XLA raises RuntimeError with a different message.
        with self.assertRaisesRegex(RuntimeError, msg):
            torch.nn.functional.nll_loss(x, t, weight=invalid_weight)


# Tests that compare a device's computation with the (gold-standard) CPU's.
class TestDevicePrecision(TestCase):
    exact_dtype = True

    @onlyCUDA
    def test_index_add_bfloat16(self, device):
        inp_tensor = torch.randn(5, 3, device='cpu').bfloat16()
        t = torch.tensor([[1, 2, 3], [4, 5, 6], [7, 8, 9]], dtype=torch.bfloat16, device='cpu')
        index = torch.tensor([0, 4, 2], device='cpu')
        out_cpu = inp_tensor.index_add(0, index, t)

        inp_tensor = inp_tensor.to(device=device)
        t = t.to(device=device)
        index = index.to(device=device)
        out_gpu = inp_tensor.index_add(0, index, t)

        self.assertEqual(out_cpu, out_gpu, atol=1e-2, rtol=0)

    def test_device_serialization(self, device):
        x = torch.randn(4, 4, device=device)

        with tempfile.NamedTemporaryFile() as f:
            torch.save(x, f)
            f.seek(0)
            x_copy = torch.load(f)

        self.assertEqual(x_copy, x)
        self.assertIs(type(x_copy), type(x))
        self.assertEqual(x_copy.device, x.device)

    @deviceCountAtLeast(2)
    def test_multidevice_serialization(self, devices):
        x = [torch.randn(4, 4, device=devices[0]),
             torch.randn(4, 4, device=devices[1])]

        with tempfile.NamedTemporaryFile() as f:
            torch.save(x, f)
            f.seek(0)
            x_copy = torch.load(f)

        for original, cp in zip(x, x_copy):
            self.assertEqual(cp, original)
            self.assertIs(type(cp), type(original))
            self.assertEqual(cp.device, original.device)

    @deviceCountAtLeast(1)
    def test_copy_noncontig(self, devices):
        def do_test(d0, d1):
            x = torch.tensor([1.5, 2.5, 3.5, 4.5, 5.5, 6.5], device=d0)
            y = torch.tensor([0, 0, 0, 0, 0, 0], device=d1)
            self.assertNotEqual(x.dtype, y.dtype)

            y[::2].copy_(x[::2])
            self.assertEqual(y, [1, 0, 3, 0, 5, 0])

        do_test('cpu', devices[0])
        do_test(devices[0], 'cpu')

        if len(devices) > 1:
            do_test(devices[0], devices[1])

    @deviceCountAtLeast(2)
    def test_type_conversions_same_device(self, devices):
        x = torch.randn(5, 5, device=devices[1])
        self.assertEqual(x.int().device, torch.device(devices[1]))
        self.assertEqual(x.type(torch.int).device, torch.device(devices[1]))
        self.assertEqual(x.to(torch.int).device, torch.device(devices[1]))

    @dtypesIfCUDA(torch.half, torch.float, torch.double,
                  torch.int8, torch.short, torch.int, torch.long,
                  torch.uint8)
    @dtypes(torch.float, torch.double,
            torch.int8, torch.short, torch.int, torch.long,
            torch.uint8)
    def test_from_sequence(self, device, dtype):
        seq = [list(range(i * 4, i * 4 + 4)) for i in range(5)]
        reference = torch.arange(0, 20).resize_(5, 4)
        self.assertEqual(torch.tensor(seq, dtype=dtype, device=device), reference, exact_dtype=False)

    @deviceCountAtLeast(1)
    def test_advancedindex_mixed_cpu_devices(self, devices) -> None:
        def test(x: torch.Tensor, ia: torch.Tensor, ib: torch.Tensor) -> None:
            # test getitem
            self.assertEqual(x[:, ia, None, ib, 0].cpu(),
                             x.cpu()[:, ia.cpu(), None, ib.cpu(), 0])
            self.assertEqual(x[ia], x.cpu()[ia.cpu()])
            # test setitem
            x_clone1 = x.clone()
            x_clone2 = x.clone()
            first_shape = x[:, ia, None, ib, 0].shape
            second_shape = x[ia].shape
            x_clone1[:, ia, None, ib, 0] = torch.randn(first_shape).to(x_clone1)
            x_clone2[ia] = torch.randn(second_shape).to(x_clone2)

        cpu = torch.device('cpu')
        for device in devices:
            # Index cpu tensor with device tensor
            x = torch.randn(3, 4, 4, 4, 3)
            ia = torch.tensor([0, 2, 1]).to(device)
            ib = torch.tensor([0, 2, 1]).to(device)
            test(x, ia, ib)

            # Index device tensor with cpu tensor
            x = x.to(device)
            ia = ia.to(cpu)
            ib = ib.to(cpu)
            test(x, ia, ib)

            # Index cpu tensor with mixed cpu, device tensors
            x = x.to(cpu)
            ia = ia.to(cpu)
            ib = ib.to(device)
            test(x, ia, ib)

            # Index device tensor with mixed cpu, device tensors
            x = x.to(device)
            ia = ia.to(cpu)
            ib = ib.to(device)
            test(x, ia, ib)

            if len(devices) > 1:
                other_device = devices[0]
                if device == devices[0]:
                    other_device = devices[1]
                # Index device tensor with mixed cpu, device tensors on different devices
                x = x.to(device)
                ia = ia.to(cpu)
                ib = ib.to(other_device)
                test(x, ia, ib)

    def test_copy_broadcast(self, device) -> None:
        x = torch.randn(10, 5)
        y = torch.randn(5, device=device)
        x.copy_(y)
        self.assertEqual(x[3], y)

        x = torch.randn(10, 5, device=device)
        y = torch.randn(5)
        x.copy_(y)
        self.assertEqual(x[3], y)

    @dtypes(torch.int64, torch.float32, torch.float64)
    def test_clamp(self, device, dtype):
        test_args = [
            *product(
                [(100, 50), (10, 64), (97,)],  # shape
                (True, False),  # non-contiguous
            )
        ]

        for shape, noncontig in test_args:
            x = make_tensor(shape, device=device, dtype=dtype,
                            noncontiguous=noncontig)
            ub = make_tensor(shape, device=device, dtype=dtype,
                             noncontiguous=noncontig)
            lb = make_tensor(shape, device=device, dtype=dtype,
                             noncontiguous=noncontig)

            expect = x.max(lb).min(ub)
            actual = x.clamp(lb, ub)
            self.assertEqual(expect, actual)

            expect = np.clip(x.cpu().numpy(), lb.cpu().numpy(), ub.cpu().numpy())
            self.assertEqual(expect, actual)

            expect = x.max(lb)
            actual = x.clamp(min=lb)
            self.assertEqual(expect, actual)

            expect = x.min(ub)
            actual = x.clamp(max=ub)
            self.assertEqual(expect, actual)

            # Test broadcasting min & max
            expect = x.max(lb[0]).min(ub[..., :1])
            actual = x.clamp(lb[0], ub[..., :1])
            self.assertEqual(expect, actual)

            # Test broadcasting x
            expect = x[..., :1].max(lb).min(ub)
            actual = x[..., :1].clamp(lb, ub)
            self.assertEqual(expect, actual)


# we implemented custom deallocation for subclasses, so it behooves
# us to make sure all of these bits work.  We'll use __del__ to
# track if objects die or not
class Tracker:
    def __init__(self, marker):
        self.marker = marker

    @staticmethod
    def make():
        marker = [False]
        return marker, Tracker(marker)

    def __del__(self):
        self.marker[0] = True

@contextlib.contextmanager
def disable_gc():
    if gc.isenabled():
        try:
            gc.disable()
            yield
        finally:
            gc.enable()
    else:
        yield

class TestTorch(AbstractTestCases._TestTorchMixin):
    exact_dtype = True

    def test_tensor_ctor_scalar(self):
        x = torch.Tensor(torch.tensor(1.0))
        self.assertEqual(x, torch.tensor(1.0))

    def test_deepcopy_gradient(self):
        from copy import deepcopy
        a = torch.zeros(10)
        a.grad = torch.ones(10)
        self.assertEqual(a.grad, deepcopy(a).grad)
        s = torch.zeros(10).to_sparse()
        s.grad = torch.ones(10).to_sparse()
        self.assertEqual(s.grad, deepcopy(s).grad)

        # ensure sharing is not broken
        c = deepcopy([a, a.grad])
        self.assertTrue(c[0].grad is c[1])

    def test_tensor_base_init(self):
        # Direct construction not OK
        self.assertRaises(RuntimeError, lambda: torch._C._TensorBase())

        # But construction of subclass is OK
        class T(torch._C._TensorBase):
            pass

        T()

    def test_tensor_base_new(self):

        # OK to call super().__new__, see
        # https://github.com/pytorch/pytorch/issues/57421
        class TestTensor(torch._C._TensorBase):
            @staticmethod
            def __new__(cls, x, *args, **kwargs):
                return super().__new__(cls, x, *args, **kwargs)

        x = torch.ones(5)
        test_tensor = TestTensor(x)

    def test_pyobj_preserved(self):
        x = torch.empty(2)
        x.foo = 2  # put something on __dict__
        y = torch.empty(2)
        y.grad = x
        del x  # x is dead in Python
        self.assertEqual(y.grad.foo, 2)
        z = y.grad  # it's live
        del z  # it's dead again
        self.assertEqual(y.grad.foo, 2)

    def test_subclass_preserved(self):
        class MyTensor(torch.Tensor):
            pass

        x = MyTensor(torch.empty(2))
        y = torch.empty(2)
        y.grad = x
        del x  # x is dead in Python
        self.assertEqual(type(y.grad), MyTensor)
        z = y.grad  # it's live
        del z  # it's dead again
        self.assertEqual(type(y.grad), MyTensor)

    def test_tensor_slot_dealloc(self):

        class SlotTensor1(torch._C._TensorBase):
            __slots__ = ['slot1']

        class SlotTensor2(SlotTensor1):
            __slots__ = ['slot2']

        m1, t1 = Tracker.make()
        m2, t2 = Tracker.make()
        slot_tensor = SlotTensor2(torch.empty(2))
        slot_tensor.slot1 = t1
        slot_tensor.slot2 = t2
        del t1
        del t2
        self.assertFalse(m1[0])
        self.assertFalse(m2[0])
        del slot_tensor
        self.assertTrue(m1[0])
        self.assertTrue(m2[0])

    def test_tensor_dict_dealloc(self):
        m, t = Tracker.make()
        x = torch.empty(2)
        x.arf = t
        del t
        self.assertFalse(m[0])
        del x
        self.assertTrue(m[0])

    def test_tensor_finalizer_dealloc(self):
        m = [False]

        class FinalizerTensor(torch._C._TensorBase):
            def __del__(self):
                m[0] = True

        fin_tensor = FinalizerTensor(torch.empty(2))
        self.assertFalse(m[0])
        del fin_tensor
        self.assertTrue(m[0])

    def test_tensor_weakref_dealloc(self):

        x = torch.empty(2)
        m = [False]

        def cb(r):
            m[0] = True

        wref = weakref.ref(x, cb)
        del x
        self.assertTrue(m[0])
        self.assertEqual(wref(), None)

    def test_tensor_cycle_via_dict(self):
        m1, t1 = Tracker.make()
        x = torch.empty(2)
        x._tracker = t1
        del t1

        m2, t2 = Tracker.make()
        y = torch.empty(2)
        y._tracker = t2
        del t2

        x._loop = y
        y._loop = x

        # C++ reference should keep the cycle live!
        # This exercise THPVariable_subtype_traverse
        # NB: Because z.grad is a reference done entirely in C++, cycles
        # involving it directly are NOT broken by Python GC; you've
        # set up a good old C++ reference cycle which we cannot safely
        # break (because C++ references are allowed to be accessed
        # multithreaded-ly) (TODO: except maybe if you can prove that
        # only Python has access to the C++ object, in which case you can
        # also prove that no multithreaded access occurs)
        z = torch.empty(2)
        z.grad = x

        del x
        del y

        gc.collect()
        self.assertFalse(m1[0])
        self.assertFalse(m2[0])

        with disable_gc():
            del z
            self.assertFalse(m1[0])
            self.assertFalse(m2[0])

        gc.collect()
        self.assertTrue(m1[0])
        self.assertTrue(m2[0])

    def test_tensor_cycle_via_slots(self):
        m1 = [False]
        m2 = [False]

        class SlotTensor1(torch._C._TensorBase):
            __slots__ = ['slot1']

            def __del__(self):
                m1[0] = True

        class SlotTensor2(SlotTensor1):
            __slots__ = ['slot2']

            def __del__(self):
                m2[0] = True

        x = SlotTensor1(torch.empty(2))
        y = SlotTensor2(torch.empty(2))

        x.slot1 = y
        y.slot2 = x

        del x
        with disable_gc():
            del y
            self.assertFalse(m1[0])
            self.assertFalse(m2[0])

        gc.collect()
        self.assertTrue(m1[0])
        self.assertTrue(m2[0])

    def test_backward_hooks_traverse(self):
        m1, t1 = Tracker.make()
        m2, t2 = Tracker.make()
        x = torch.empty(2, requires_grad=True)
        x._tracker = t1
        y = torch.empty(2, requires_grad=True)
        y._tracker = t2
        del t1
        del t2

        # this hits a special setter, it's not just a __dict__ entry
        x._backward_hooks = y
        y._backward_hooks = x

        del x
        with disable_gc():
            del y
            self.assertFalse(m1[0])
            self.assertFalse(m2[0])

        gc.collect()

        self.assertTrue(m1[0])
        self.assertTrue(m2[0])

    def test_dead_weak_ref(self):
        x = torch.empty(2)
        w_x = weakref.ref(x)
        y = torch.empty(2)
        y.grad = x
        del x

        x = w_x()
        # Ideally, x would keep the tensor live.  But CPython doesn't
        # provide enough hooks to do this.  So it will go dead and x
        # will transmute into an undefined tensor.  Not great, but the
        # best we can do.
        del y

        self.assertRaises(RuntimeError, lambda: x.sigmoid())

    def test_resurrected_weak_ref(self):
        x = torch.empty(2)
        w_x = weakref.ref(x)
        y = torch.empty(2)
        y.grad = x
        del x

        x = w_x()
        # Use this to manually fix weak references after dereferencing them
        x._fix_weakref()
        del y
        x.sigmoid()

    @torch.inference_mode()
    def test_bmm_multithreaded(self):
        device = 'cpu'
        num_threads = torch.get_num_threads()

        torch.set_num_threads(4)
        batch_sizes = [1, 10]
        M, N, O = 23, 8, 12
        dtype = torch.float32
        numpy_dtype = dtype

        def invert_perm(p):
            d = {x: i for i, x in enumerate(p)}
            return (d[0], d[1], d[2])

        def generate_inputs(num_batches):
            # transposed tensors
            for perm1, perm2 in itertools.product(itertools.permutations((0, 1, 2)), repeat=2):
                b1 = make_tensor((num_batches, M, N), device, dtype, low=-1, high=1)
                b2 = make_tensor((num_batches, N, O), device, dtype, low=-1, high=1)
                b1 = b1.permute(perm1).contiguous().permute(invert_perm(perm1))
                b2 = b2.permute(perm2).contiguous().permute(invert_perm(perm2))
                yield b1, b2
            # broadcasting tensors
            for b1, b2, b3, b4, b5, b6 in itertools.product((True, False), repeat=6):
                shape1 = (num_batches if b1 else 1, M if b2 else 1, N if b3 else 1)
                shape2 = (num_batches if b4 else 1, N if b5 else 1, O if b6 else 1)
                b1 = make_tensor(shape1, device, dtype, low=-1, high=1).expand(num_batches, M, N)
                b2 = make_tensor(shape2, device, dtype, low=-1, high=1).expand(num_batches, N, O)
                yield b1, b2
            # zero-sized tensors
            for z1, z2, z3, z4 in itertools.product((True, False), repeat=4):
                shape1 = (num_batches if z1 else 0, M if z2 else 0, N if z3 else 0)
                shape2 = (num_batches if z1 else 0, N if z3 else 0, O if z4 else 0)
                b1 = torch.randn(shape1, dtype=dtype, device=device)
                b2 = torch.randn(shape2, dtype=dtype, device=device)
                yield b1, b2

        try:
            for num_batches in batch_sizes:
                for (b1, b2), perm3 in itertools.product(generate_inputs(num_batches), itertools.permutations((0, 1, 2))):
                    res1 = torch.bmm(b1, b2)
                    res2 = torch.full((num_batches, M, O), math.nan, dtype=dtype, device=device) \
                        .permute(perm3).contiguous().permute(invert_perm(perm3))
                    torch.bmm(b1, b2, out=res2)
                    expect = torch.from_numpy(
                        b1.to(numpy_dtype).cpu().numpy() @ b2.to(numpy_dtype).cpu().numpy()).to(device=device, dtype=dtype)
                    self.assertEqual(expect, res1)
                    self.assertEqual(expect, res2)
        finally:
            torch.set_num_threads(num_threads)

    def test_conj_neg_tolist(self):
        x = torch.randn(2, dtype=torch.cfloat)
        y1 = x.conj()
        y1_expect = x.conj_physical()
        y2 = y1.imag
        self.assertEqual(y1, y1_expect.tolist())
        self.assertEqual(y2, y1_expect.imag.tolist())

# TODO: these empy classes are temporarily instantiated for XLA compatibility
#   once XLA updates their test suite it should be removed
class TestViewOps(TestCase):
    pass

class TestTensorDeviceOps(TestCase):
    pass

# Generates tests
# Note: test generation must be done at file scope, not within main, or
# pytest will fail.
add_neg_dim_tests()
instantiate_device_type_tests(TestViewOps, globals())
instantiate_device_type_tests(TestVitalSignsCuda, globals())
instantiate_device_type_tests(TestTensorDeviceOps, globals())
instantiate_device_type_tests(TestTorchDeviceType, globals())
instantiate_device_type_tests(TestDevicePrecision, globals(), except_for='cpu')

if __name__ == '__main__':
    run_tests()
