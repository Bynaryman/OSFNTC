import collections
import copy
import math
import unittest

import torch
import torch.nn as nn
import torch.nn.functional as F
import torch.nn.intrinsic as nni
from torch.testing._internal.common_quantization import (
    skipIfNoFBGEMM,
    skip_if_no_torchvision,
    QuantizationTestCase,
)
from torch.quantization import (
    ObserverBase,
    FakeQuantizeBase,
)
from torch.quantization.quantize_fx import (
    prepare_fx,
    convert_fx,
)

import torch.ao.quantization._quantize_dbr as _quantize_dbr
import torch.ao.ns._numeric_suite_dbr as ns
# TODO(future PR): move these utils out of the FX folder
import torch.ao.ns._numeric_suite_fx as ns_fx

def _allclose(a, b):
    if isinstance(a, tuple):
        assert isinstance(b, tuple)
        result = True
        for a_inner, b_inner in zip(a, b):
            result = result and torch.allclose(a_inner, b_inner)
        return result
    elif isinstance(a, torch.Tensor):
        assert isinstance(b, torch.Tensor)
        return torch.allclose(a, b)
    raise AssertionError('unhandled type')


class QuantizeDBRTestCase(QuantizationTestCase):
    def _test_auto_tracing(
        self,
        m,
        qconfig,
        example_args,
        fuse_modules=True,
        do_fx_comparison=True,
        do_torchscript_checks=True,
    ):
        m_copy = copy.deepcopy(m)

        m.qconfig = qconfig

        mp = _quantize_dbr.prepare(
            m, example_args, fuse_modules=fuse_modules)
        out_p = mp(*example_args)
        # print(mp)
        mq = _quantize_dbr.convert(mp)
        # print(mq)
        # verify it runs
        out_q = mq(*example_args)
        # print(out_q)

        # compare it against FX
        if do_fx_comparison:
            m_copy_p = prepare_fx(m_copy, {'': qconfig})
            out_m_copy_p = m_copy_p(*example_args)
            # print(m_copy_p)
            m_copy_q = convert_fx(m_copy_p)
            # print(m_copy_q)
            # print(m_copy_q.graph)
            out_q_fx = m_copy_q(*example_args)
            # print(out_q)
            # print(out_q_fx)
            self.assertTrue(_allclose(out_p, out_m_copy_p))
            # print(out_q)
            # print(out_q_fx)
            self.assertTrue(_allclose(out_q, out_q_fx))

        if do_torchscript_checks:
            # verify torch.jit.trace works
            mq_jit_traced = torch.jit.trace(
                mq, example_args, check_trace=False)
            # print(mq_jit_traced.graph)
            traced_out = mq_jit_traced(*example_args)
            self.assertTrue(_allclose(traced_out, out_q))

            # verify torch.jit.script works
            rewritten = mq.rewrite_for_scripting()
            rewritten_out = rewritten(*example_args)
            # print(rewritten)
            self.assertTrue(_allclose(rewritten_out, out_q))

            scripted_rewritten = torch.jit.script(rewritten)
            # print(scripted_rewritten.graph)
            scripted_rewritten_out = scripted_rewritten(*example_args)
            # print('scripted_rewritten_out', scripted_rewritten_out)
            self.assertTrue(_allclose(scripted_rewritten_out, out_q))

            traced_rewritten = torch.jit.trace(
                rewritten, example_args, check_trace=False)
            traced_rewritten_out = traced_rewritten(*example_args)
            self.assertTrue(_allclose(traced_rewritten_out, out_q))


@skipIfNoFBGEMM
class TestQuantizeDBR(QuantizeDBRTestCase):
    def test_fusion(self):
        class M(torch.nn.Module):
            def __init__(self):
                super().__init__()
                self.conv = torch.nn.Conv2d(1, 1, 1)
                self.relu = torch.nn.ReLU()
                self.child = nn.Sequential(
                    nn.Conv2d(1, 1, 1),
                    nn.ReLU(),
                )

            def forward(self, x):
                x = self.conv(x)
                x = self.relu(x)
                x = self.child(x)
                return x

        m = M().eval()
        m.qconfig = torch.quantization.default_qconfig
        mp = _quantize_dbr.prepare(m, (torch.randn(1, 1, 1, 1),))
        self.assertTrue(isinstance(mp.conv, nni.ConvReLU2d))
        self.assertTrue(isinstance(mp.child[0], nni.ConvReLU2d))

    def test_fusion2(self):
        class M(torch.nn.Module):
            def __init__(self):
                super().__init__()
                self.conv = torch.nn.Conv2d(1, 1, 1)
                self.bn = torch.nn.BatchNorm2d(1)
                # self.conv2 = torch.nn.Conv2d(1, 1, 1)
                self.relu = torch.nn.LeakyReLU()

            def forward(self, x):
                x = self.conv(x)
                x = self.bn(x)
                x = self.relu(x)
                return x

        m = M().eval()
        qconfig = torch.quantization.default_qconfig
        self._test_auto_tracing(m, qconfig, (torch.randn(1, 1, 2, 2),))

    def test_fusion_called_multiple_times(self):
        """
        Tests that fusion works if the modules to fuse get called multiple
        times in the same forward.
        """
        class M(torch.nn.Module):
            def __init__(self):
                super().__init__()
                self.conv = torch.nn.Conv2d(1, 1, 1)
                self.relu = torch.nn.ReLU()

            def forward(self, x):
                for _ in range(2):
                    x = self.conv(x)
                    x = self.relu(x)
                return x

        m = M().eval()
        qconfig = torch.quantization.default_qconfig
        self._test_auto_tracing(m, qconfig, (torch.randn(1, 1, 2, 2),))

    def test_observers_not_touched_by_tracing(self):
        """
        Verifies that running dynamic tracing does not change any data
        stored in observers and fake quants.
        """
        m = nn.Sequential(nn.Conv2d(1, 1, 1)).eval()
        m.qconfig = torch.quantization.default_qconfig
        mp = _quantize_dbr.prepare(m, (torch.randn(1, 1, 1, 1),))
        for _, mod in mp.named_modules():
            if isinstance(mod, (ObserverBase, FakeQuantizeBase)):
                scale, zp = mod.calculate_qparams()
                # Assume that if scale is 1.0 and zp is 0, no calibration
                # has happened.
                self.assertTrue(torch.allclose(scale, torch.ones(1)))
                self.assertTrue(torch.equal(zp, torch.zeros(1, dtype=torch.long)))

    def test_multiple_modules(self):
        m = nn.Sequential(
            nn.Sequential(nn.Conv2d(1, 1, 1)),
            nn.Sequential(nn.Conv2d(1, 1, 1)),
        ).eval()
        qconfig = torch.quantization.default_qconfig
        self._test_auto_tracing(m, qconfig, (torch.randn(1, 1, 2, 2),))

    def test_child_modules(self):
        m = nn.Sequential(nn.Sequential(nn.Conv2d(1, 1, 1))).eval()
        qconfig = torch.quantization.default_qconfig
        self._test_auto_tracing(m, qconfig, (torch.randn(1, 1, 2, 2),))

    def test_conv(self):
        class M(torch.nn.Module):
            def __init__(self):
                super().__init__()
                self.conv = torch.nn.Conv2d(1, 1, 1)

            def forward(self, x):
                x1 = self.conv(x)
                return x1

        m = M().eval()
        qconfig = torch.quantization.default_qconfig
        self._test_auto_tracing(m, qconfig, (torch.randn(1, 1, 2, 2),))

    def test_conv_mod_qat(self):
        class M(torch.nn.Module):
            def __init__(self):
                super().__init__()
                self.conv = torch.nn.Conv2d(1, 1, 1)

            def forward(self, x):
                x1 = self.conv(x)
                return x1

        m = M().eval()
        qconfig = torch.quantization.get_default_qat_qconfig('fbgemm')
        self._test_auto_tracing(
            copy.deepcopy(m), qconfig, (torch.randn(1, 1, 2, 2),))

        # test backprop does not crash
        m.qconfig = qconfig
        inputs = torch.randn(1, 1, 1, 1)
        inputs.requires_grad = True
        mp = _quantize_dbr.prepare(m, (inputs,))
        output = mp(inputs)
        labels = torch.randn(1, 1, 1, 1)
        loss = (output - labels).sum()
        loss.backward()
        optim = torch.optim.SGD(mp.parameters(), lr=0.01)
        optim.step()

    def test_conv_functional_qat(self):

        class M(torch.nn.Module):
            def __init__(self, weight2d, bias2d):
                super().__init__()
                self.weight2d = torch.nn.Parameter(weight2d)
                self.bias2d = torch.nn.Parameter(bias2d)
                self.stride2d = (1, 1)
                self.padding2d = (0, 0)
                self.dilation2d = (1, 1)
                self.groups = 1

            def forward(self, x):
                x = F.conv2d(
                    x, self.weight2d, self.bias2d, self.stride2d, self.padding2d,
                    self.dilation2d, self.groups)
                return x

        m = M(torch.randn(1, 1, 1, 1), torch.randn(1)).eval()
        qconfig = torch.quantization.get_default_qat_qconfig('fbgemm')
        self._test_auto_tracing(m, qconfig, (torch.randn(1, 1, 2, 2),))

        # test backprop does not crash
        m.qconfig = qconfig
        inputs = torch.randn(1, 1, 1, 1)
        inputs.requires_grad = True
        mp = _quantize_dbr.prepare(m, (inputs,))
        output = mp(inputs)
        labels = torch.randn(1, 1, 1, 1)
        loss = (output - labels).sum()
        loss.backward()
        optim = torch.optim.SGD(mp.parameters(), lr=0.01)
        optim.step()

    def test_dropout_conv(self):
        class M(torch.nn.Module):
            def __init__(self):
                super().__init__()
                self.dropout = nn.Dropout()
                self.conv = torch.nn.Conv2d(1, 1, 1)

            def forward(self, x):
                # this can be sometimes inplace
                x1 = self.dropout(x)
                x1 = self.conv(x)
                return x1

        m = M().eval()
        qconfig = torch.quantization.default_qconfig
        self._test_auto_tracing(m, qconfig, (torch.randn(1, 1, 2, 2),))

    # TODO(future PR): implement observer sharing to match FX
    def test_cat_fp32(self):
        class M(torch.nn.Module):
            def forward(self, x):
                x = torch.cat([x, x], dim=1)
                return x

        m = M().eval()
        qconfig = torch.quantization.default_qconfig
        self._test_auto_tracing(m, qconfig, (torch.randn(1, 1, 2, 2),))

        class M(torch.nn.Module):
            def forward(self, x):
                x = torch.cat((x, x), dim=1)
                return x

        m = M().eval()
        qconfig = torch.quantization.default_qconfig
        self._test_auto_tracing(m, qconfig, (torch.randn(1, 1, 2, 2),))

    def test_cat_int(self):
        class M(torch.nn.Module):
            def forward(self, x):
                x = torch.cat([x, x], dim=1)
                return x

        m = M().eval()
        qconfig = torch.quantization.default_qconfig
        for dtype in (torch.int32, torch.int64):
            self._test_auto_tracing(
                m, qconfig, (torch.zeros(1, 1, 1, 1, dtype=dtype),),
                # FX graph mode quant does not support this yet
                do_fx_comparison=False)

    @unittest.skip('FX graph mode is using fake_quantize with PTQ, TODO verify')
    def test_conv_unsupported_inplace_conv(self):
        """
        Verifies that having an quantizeable op which is inplace
        is handled well
        """
        class M(torch.nn.Module):
            def __init__(self):
                super().__init__()
                self.conv = torch.nn.Conv2d(1, 1, 1)
                self.conv2 = torch.nn.Conv2d(1, 1, 1)

            def forward(self, x):
                x = self.conv(x)
                x = F.hardsigmoid(x, inplace=True)
                x = self.conv2(x)
                return x

        m = M().eval()
        qconfig = torch.quantization.default_qconfig
        self._test_auto_tracing(m, qconfig, (torch.randn(1, 1, 2, 2),))

    def test_conv_flatten_linear(self):
        class M(torch.nn.Module):
            def __init__(self):
                super().__init__()
                self.conv = torch.nn.Conv2d(1, 1, 1)
                self.linear = torch.nn.Linear(1, 1)

            def forward(self, x):
                x1 = self.conv(x)
                # TODO(future PR): unbreak this
                # x1 = torch.nn.functional.adaptive_avg_pool2d(x, (1, 1))
                x1 = torch.nn.functional.adaptive_avg_pool2d(x1, (1, 1))
                x2 = torch.flatten(x1, 1)
                x3 = self.linear(x2)
                return x3

        m = M().eval()
        qconfig = torch.quantization.default_qconfig
        self._test_auto_tracing(m, qconfig, (torch.randn(1, 1, 1, 1),))

    def test_conv_add(self):
        class M(torch.nn.Module):
            def __init__(self):
                super().__init__()
                self.conv = torch.nn.Conv2d(1, 1, 1)

            def forward(self, x):
                x1 = self.conv(x)
                print(x)
                x2 = x1 + x
                return x2

        m = M().eval()
        qconfig = torch.quantization.default_qconfig
        self._test_auto_tracing(m, qconfig, (torch.randn(1, 1, 2, 2),))

    def test_conv_scalar_add(self):
        class M(torch.nn.Module):
            def __init__(self):
                super().__init__()
                self.conv = torch.nn.Conv2d(1, 1, 1)

            def forward(self, x):
                x = self.conv(x)
                x = x + 1.0
                return x

        model_fp32 = M().eval()
        qconfig = torch.quantization.default_qconfig
        self._test_auto_tracing(model_fp32, qconfig, (torch.randn(1, 1, 2, 2),))

    def test_conv_relu_add(self):
        class M(torch.nn.Module):
            def __init__(self):
                super().__init__()
                self.conv = torch.nn.Conv2d(1, 1, 1)
                self.relu = torch.nn.ReLU()

            def forward(self, x):
                x1 = self.conv(x)
                x2 = self.relu(x1)
                x3 = x1 + x
                return x3

        model_fp32 = M().eval()

        qconfig = torch.quantization.default_qconfig
        self._test_auto_tracing(model_fp32, qconfig, (torch.randn(1, 1, 2, 2),))

    def test_linear_torch_relu(self):
        class M(torch.nn.Module):
            def __init__(self):
                super().__init__()
                self.u1 = nn.Linear(1, 1)
                self.v1 = nn.Linear(1, 1)
                self.u2 = nn.Linear(1, 1)
                self.v2 = nn.Linear(1, 1)
                self.w = nn.Linear(1, 1)

            def forward(self, x):
                x = self.w(x)
                x = x + torch.relu(self.v1(torch.relu(self.u1(x))))
                return x + torch.relu(self.v2(torch.relu(self.u2(x))))

        model_fp32 = M().eval()

        qconfig = torch.quantization.default_qconfig
        self._test_auto_tracing(model_fp32, qconfig, (torch.randn(1, 1, 1, 1),))

    def test_conv_functional(self):

        class M(torch.nn.Module):
            def __init__(self, weight2d, bias2d):
                super().__init__()
                self.weight2d = torch.nn.Parameter(weight2d)
                self.bias2d = torch.nn.Parameter(bias2d)
                self.stride2d = (1, 1)
                self.padding2d = (0, 0)
                self.dilation2d = (1, 1)
                self.groups = 1

            def forward(self, x):
                x = F.conv2d(
                    x, self.weight2d, self.bias2d, self.stride2d, self.padding2d,
                    self.dilation2d, self.groups)
                return x

        model_fp32 = M(torch.randn(1, 1, 1, 1), torch.randn(1)).eval()
        qconfig = torch.quantization.default_qconfig
        self._test_auto_tracing(model_fp32, qconfig, (torch.randn(1, 1, 2, 2),))

    def test_conv_functional_dynamic_weights(self):
        class M(torch.nn.Module):
            def __init__(self, weight2d, bias2d):
                super().__init__()
                self.weight2d = torch.nn.Parameter(weight2d)
                self.bias2d = torch.nn.Parameter(bias2d)
                self.stride2d = (1, 1)
                self.padding2d = (0, 0)
                self.dilation2d = (1, 1)
                self.groups = 1

            def forward(self, x):
                updated_weight = self.weight2d * x
                x = F.conv2d(
                    x, updated_weight, self.bias2d, self.stride2d, self.padding2d,
                    self.dilation2d, self.groups)
                return x

        model_fp32 = M(torch.randn(1, 1, 1, 1), torch.randn(1)).eval()
        qconfig = torch.quantization.default_qconfig
        self._test_auto_tracing(
            model_fp32, qconfig, (torch.randn(1, 1, 2, 2),),
            # FX implements this functionality instead of skipping it
            do_fx_comparison=False,
            # TODO enable scripting support for this
            do_torchscript_checks=False)

    def test_linear_functional(self):
        class LinearFunctional(nn.Module):
            def __init__(self):
                super().__init__()
                self.w1 = nn.Parameter(torch.empty(4, 4))
                self.b1 = nn.Parameter(torch.ones(4))
                torch.nn.init.kaiming_uniform_(self.w1, a=math.sqrt(5))

            def forward(self, x):
                x = F.linear(x, self.w1, self.b1)
                return x

        model_fp32 = LinearFunctional().eval()
        qconfig = torch.quantization.default_qconfig
        self._test_auto_tracing(
            model_fp32, qconfig, (torch.randn(1, 1, 4, 4),))

    def test_gelu_linear(self):
        class M(torch.nn.Module):
            def __init__(self):
                super().__init__()
                self.gelu = torch.nn.GELU()
                self.linear = torch.nn.Linear(1, 1)

            def forward(self, x):
                x = self.linear(x)
                x = self.gelu(x)
                return x

        model_fp32 = M().eval()
        qconfig = torch.quantization.default_qconfig
        self._test_auto_tracing(model_fp32, qconfig, (torch.randn(1, 1, 1, 1),))

    def test_dropout(self):
        class M(torch.nn.Module):
            def __init__(self):
                super().__init__()
                self.dropout = nn.Dropout()
                self.linear = torch.nn.Linear(1, 1)
                self.linear2 = torch.nn.Linear(1, 1)

            def forward(self, x):
                x = self.linear(x)
                x = self.dropout(x)
                x = self.linear2(x)
                return x

        model_fp32 = M().eval()
        qconfig = torch.quantization.default_qconfig
        self._test_auto_tracing(model_fp32, qconfig, (torch.randn(1, 1, 1, 1),))

    def test_add(self):
        class M(torch.nn.Module):
            def forward(self, x):
                x = x + x
                x = x + 1.0
                x = 1.0 + x
                return x

        model_fp32 = M().eval()
        qconfig = torch.quantization.default_qconfig
        self._test_auto_tracing(model_fp32, qconfig, (torch.randn(1, 1, 2, 2),))

    def test_add_int32(self):
        class M(torch.nn.Module):
            def forward(self, x):
                x = x + x
                return x

        model_fp32 = M().eval()
        qconfig = torch.quantization.default_qconfig
        self._test_auto_tracing(
            model_fp32, qconfig, (torch.ones(1, 1, 2, 2, dtype=torch.int32),),
            # FX graph mode quantization does not automatically detect
            # tensor inputs in non-float dtypes.
            do_fx_comparison=False)

    def test_module_then_add(self):
        class M(torch.nn.Module):
            def __init__(self):
                super().__init__()
                self.linear = torch.nn.Linear(1, 1)

            def forward(self, x):
                x = self.linear(x)
                x = x + 1.0
                x = x + 1.0
                return x

        model_fp32 = M().eval()
        qconfig = torch.quantization.default_qconfig
        self._test_auto_tracing(model_fp32, qconfig, (torch.randn(1, 1, 1, 1),))

    def test_sub(self):
        class M(torch.nn.Module):
            def forward(self, x):
                x = x - x
                x = x - 1.0
                return x

        model_fp32 = M().eval()
        qconfig = torch.quantization.default_qconfig
        self._test_auto_tracing(model_fp32, qconfig, (torch.randn(1, 1, 2, 2),))

    def test_mul(self):
        class M(torch.nn.Module):
            def forward(self, x):
                x = x * x
                x = x * 1.0
                return x

        model_fp32 = M().eval()
        qconfig = torch.quantization.default_qconfig
        self._test_auto_tracing(model_fp32, qconfig, (torch.randn(1, 1, 2, 2),))

    def test_mul_int(self):
        # TODO: make all the math functions work correctly for integer types
        # TODO: make the same improvement in FX graph mode quant, if possible
        class M(torch.nn.Module):
            def forward(self, x):
                x = x * x
                return x

        model_fp32 = M().eval()
        qconfig = torch.quantization.default_qconfig
        for dtype in (torch.int32, torch.int64):
            self._test_auto_tracing(
                copy.deepcopy(model_fp32), qconfig,
                (torch.ones(1, 1, 2, 2, dtype=dtype),),
                # FX graph mode quant does not support this yet
                do_fx_comparison=False)

    def test_div(self):
        class M(torch.nn.Module):
            def forward(self, x):
                x = x / x
                x = x / 1.0
                return x

        model_fp32 = M().eval()
        qconfig = torch.quantization.default_qconfig
        self._test_auto_tracing(model_fp32, qconfig, (torch.randn(1, 1, 2, 2),))

    def test_method(self):
        class M(torch.nn.Module):
            def forward(self, x):
                x = x + x
                x = torch.relu(x)
                # x = x.relu()
                return x

        model_fp32 = M().eval()
        qconfig = torch.quantization.default_qconfig
        self._test_auto_tracing(model_fp32, qconfig, (torch.randn(1, 1, 2, 2),))

    def test_add_linear(self):
        class M(torch.nn.Module):
            def __init__(self):
                super().__init__()
                self.linear = nn.Linear(1, 1)

            def forward(self, x):
                x = x + x
                x = self.linear(x)
                return x

        model_fp32 = M().eval()
        qconfig = torch.quantization.default_qconfig
        self._test_auto_tracing(model_fp32, qconfig, (torch.randn(1, 1, 1, 1),))

    def test_module_created_during_forward(self):
        """Some BERT models have this pattern"""
        class M(torch.nn.Module):
            def forward(self, x):
                x = nn.Softmax(dim=-1)(x)
                return x

        model_fp32 = M().eval()
        qconfig = torch.quantization.default_qconfig
        self._test_auto_tracing(
            model_fp32, qconfig, (torch.randn(1, 1, 1, 1),),
            # This syntax is not supported by FX or TorchScript
            do_fx_comparison=False, do_torchscript_checks=False)

    def test_module_returns_namedtuple(self):
        NamedTuple = collections.namedtuple("NamedTuple", ["x0", "x1"])

        """Some hf models have this pattern"""
        class M1(torch.nn.Module):
            def forward(self, x):
                return NamedTuple(x, x)

        class M(torch.nn.Module):
            def __init__(self):
                super().__init__()
                self.m1 = M1()

            def forward(self, x):
                m1 = self.m1(x)
                return (m1.x0, m1.x1)

        model_fp32 = M().eval()
        qconfig = torch.quantization.default_qconfig
        self._test_auto_tracing(
            model_fp32, qconfig, (torch.randn(1, 1, 1, 1),),
            # TODO(future PR): add FX rewrite support
            do_fx_comparison=False, do_torchscript_checks=False)

    @unittest.skip('TODO build this')
    def test_module_input_types(self):
        class M(torch.nn.Module):
            def forward(self, x=None, y=None):
                print('x', x)
                print('y', y)
                assert x is not None and y is not None
                return (x, y)

        model_fp32 = M().eval()
        example_inputs = {'y': torch.randn(1), 'x': torch.randn(1)}
        ExampleInputsTupleCtr = collections.namedtuple('ExampleInputs', example_inputs)
        example_inputs_tuple = ExampleInputsTupleCtr(**example_inputs)
        ms = torch.jit.trace(model_fp32, example_inputs_tuple)

        return
        qconfig = torch.quantization.default_qconfig

        # dict
        kwargs = {'x': torch.randn(1, 1, 2, 2)}
        self._test_auto_tracing(model_fp32, qconfig, (), kwargs)

    def test_module_return_types(self):
        class M1(torch.nn.Module):
            def forward(self, x):
                return x, x

        class M2(torch.nn.Module):
            def __init__(self):
                super().__init__()
                self.m1 = M1()

            def forward(self, x):
                x1, x2 = self.m1(x)
                return x1

        model_fp32 = M2().eval()
        qconfig = torch.quantization.default_qconfig
        self._test_auto_tracing(model_fp32, qconfig, (torch.randn(1, 1, 2, 2),))

    def test_inplace_unquantizeable_op(self):
        class M(torch.nn.Module):
            def __init__(self):
                super().__init__()
                self.conv1 = nn.Conv2d(1, 1, 1)
                self.silu = nn.SiLU(inplace=True)
                # self.silu = nn.SiLU()
                self.conv2 = nn.Conv2d(1, 1, 1)

            def forward(self, x):
                x = self.conv1(x)
                x = self.silu(x)
                x = self.conv2(x)
                return x

        model_fp32 = M().eval()
        qconfig = torch.quantization.default_qconfig
        self._test_auto_tracing(model_fp32, qconfig, (torch.randn(1, 1, 2, 2),))

    @unittest.skip('this depends on unsupported syntax detection, currently disabled')
    def test_vovnet_sequential(self):

        class SequentialAppendList(nn.Sequential):
            def __init__(self, *args):
                super(SequentialAppendList, self).__init__(*args)

            def forward(self, x: torch.Tensor) -> torch.Tensor:
                concat_list = []
                for i, module in enumerate(self):
                    if i == 0:
                        concat_list.append(module(x))
                    else:
                        concat_list.append(module(concat_list[-1]))
                x = torch.cat(concat_list, dim=1)
                return x

        m = SequentialAppendList(torch.nn.Conv2d(1, 1, 1)).eval()
        qconfig = torch.quantization.default_qconfig
        self._test_auto_tracing(m, qconfig, (torch.randn(1, 1, 1, 1),))

    def test_unsupported_ops(self):
        class M(torch.nn.Module):
            def forward(self, x):
                x = F.tanhshrink(x)
                x = x + x
                x = F.tanhshrink(x)
                return x

        model_fp32 = M().eval()
        qconfig = torch.quantization.default_qconfig
        self._test_auto_tracing(model_fp32, qconfig, (torch.randn(1, 1, 2, 2),))

    def test_unknown_op_after_quantized(self):
        class M(torch.nn.Module):
            def forward(self, x):
                x = x + x
                std = x.std()
                return std

        model_fp32 = M().eval()
        qconfig = torch.quantization.default_qconfig
        self._test_auto_tracing(
            model_fp32, qconfig, (torch.randn(1, 1, 2, 2),),
            fuse_modules=False)

    def test_embedding(self):
        # Note: this test is just testing that models with embeddings
        # do not crash with a global qconfig defined. Embedding quantization
        # is not actually happening in this prototype yet.
        # TODO(future PR): fix this and update this code.

        # test subclass
        class EmbeddingSubclass(nn.Embedding):
            pass

        class M(torch.nn.Module):
            def __init__(self):
                super().__init__()
                self.embedding = EmbeddingSubclass(1, 1)

            def forward(self, x):
                x = self.embedding(x)
                return x

        model_fp32 = M().eval()
        qconfig = torch.quantization.default_dynamic_qconfig
        self._test_auto_tracing(
            model_fp32, qconfig, (torch.LongTensor([[0]]),),
            fuse_modules=False)

        # test regular embedding
        class M(torch.nn.Module):
            def __init__(self):
                super().__init__()
                self.embedding = nn.Embedding(1, 1)

            def forward(self, x):
                x = self.embedding(x)
                return x

        model_fp32 = M().eval()
        qconfig = torch.quantization.default_dynamic_qconfig
        self._test_auto_tracing(
            model_fp32, qconfig, (torch.LongTensor([[0]]),),
            fuse_modules=False)

    def test_inplace_add(self):
        class M(torch.nn.Module):
            def __init__(self):
                super().__init__()
                self.embedding1 = nn.Embedding(1, 1)
                self.embedding2 = nn.Embedding(1, 1)
                self.layernorm = nn.LayerNorm(1)

            def forward(self, x):
                x1 = self.embedding1(x)
                x1 += self.embedding2(x)
                x2 = self.layernorm(x1)
                return x

        model_fp32 = M().eval()
        qconfig = torch.quantization.default_qconfig
        self._test_auto_tracing(
            model_fp32, qconfig, (torch.LongTensor([[0]]),),
            fuse_modules=False)

    # this is broken because AutoQuantizationState appears in self.items
    @unittest.skip('TODO fix this')
    def test_module_calls_items(self):
        class M(torch.nn.ModuleDict):
            def __init__(self):
                super().__init__()
                for i in range(2):
                    layer = nn.ReLU()
                    self.add_module("layer_" + str(i), layer)

            def forward(self, x):
                layers = [x]
                for name, layer in self.items():
                    layers.append(layer(x))
                return torch.cat(layers, dim=1)

        model_fp32 = M().eval()
        qconfig = torch.quantization.default_qconfig
        self._test_auto_tracing(
            model_fp32, qconfig, (torch.randn(1, 1, 2, 2),))

    def test_subclass_of_quantizeable_module(self):
        """
        If a user creates a subclass of nn.BatchNorm2d, that subclass
        should not be quantized unless the user defines a custom module.
        """
        class BN2d(torch.nn.BatchNorm2d):
            pass

        class M(torch.nn.Module):
            def __init__(self):
                super().__init__()
                self.conv = torch.nn.Conv2d(1, 1, 1)
                self.bn = BN2d(1)
                self.conv2 = torch.nn.Conv2d(1, 1, 1)

            def forward(self, x):
                x = self.conv(x)
                x = self.bn(x)
                x = self.conv2(x)
                return x

        m = M().eval()
        qconfig = torch.quantization.default_qconfig
        self._test_auto_tracing(
            m, qconfig, (torch.randn(1, 1, 2, 2),),
            # the module is not symbolically traceable
            do_fx_comparison=False)

    def test_lstm(self):
        # building block of torchbenchmark/tts_angular
        class LSTMWithProjection(nn.Module):
            def __init__(self, input_size, hidden_size, proj_size):
                super().__init__()
                self.input_size = input_size
                self.hidden_size = hidden_size
                self.proj_size = proj_size
                self.lstm = nn.LSTM(input_size, hidden_size, batch_first=True)
                self.linear = nn.Linear(hidden_size, proj_size, bias=False)

            def forward(self, x):
                self.lstm.flatten_parameters()
                o, (_, _) = self.lstm(x)
                return self.linear(o)

        m = LSTMWithProjection(1, 1, 1).eval()
        qconfig = torch.quantization.default_qconfig
        self._test_auto_tracing(
            m, qconfig, (torch.randn(1, 1, 1),),
            # the module is not symbolically traceable
            do_fx_comparison=False)

    # TODO(future PR): move into a separate test file
    def test_numeric_suite(self):
        class M(torch.nn.Module):
            def __init__(self):
                super().__init__()
                self.conv = nn.Conv2d(1, 1, 1)
                self.conv2 = nn.Sequential(nn.Conv2d(1, 1, 1))

            def forward(self, x):
                x = self.conv(x)
                x = self.conv2(x)
                x = x + x
                return x

        m = M().eval()
        m.qconfig = torch.quantization.default_qconfig
        example_args = (torch.randn(1, 1, 2, 2),)
        mp = _quantize_dbr.prepare(m, example_args)
        out_p = mp(*example_args)
        mq = _quantize_dbr.convert(copy.deepcopy(mp))
        out_q = mq(*example_args)

        mp, mq = ns.add_loggers('mp', mp, 'mq', mq)

        mp(*example_args)
        mq(*example_args)

        act_comparison = ns.extract_logger_info(mp, mq, 'mq')
        ns_fx.extend_logger_results_with_comparison(
            act_comparison, 'mp', 'mq', torch.ao.ns.fx.utils.compute_sqnr,
            'sqnr')

        # TODO(future PR): enforce validity of the result above, using
        # NS for FX utils. Will need some refactoring.

        # TODO(future PR): consider adding a util for below
        to_print = []
        for idx, (layer_name, v) in enumerate(act_comparison.items()):
            to_print.append([
                layer_name,
                v['node_output']['mq'][0]['fqn'],
                v['node_output']['mq'][0]['ref_node_target_type'],
                v['node_output']['mq'][0]['sqnr']])

@skipIfNoFBGEMM
class TestQuantizeDBRModels(QuantizeDBRTestCase):
    @skip_if_no_torchvision
    def test_mobilenet_v2(self):
        import torchvision
        m = torchvision.models.__dict__['mobilenet_v2'](pretrained=False).eval().float()
        qconfig = torch.quantization.default_qconfig
        self._test_auto_tracing(
            m, qconfig, (torch.randn(1, 3, 224, 224),),
            # TODO fix this (reason TBD)
            do_torchscript_checks=False)
