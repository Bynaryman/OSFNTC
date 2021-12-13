# Owner(s): ["oncall: quantization"]

import torch
import torch.fx.experimental.fx_acc.acc_tracer as acc_tracer
from torch.fx.experimental.fx2trt import (
    TRTInterpreter,
    InputTensorSpec,
    TRTModule,
)
from torch.ao.quantization import (
    default_qconfig
)
from torch.ao.quantization.quantize_fx import (
    prepare_fx,
    get_tensorrt_backend_config_dict,
)
from torch.ao.quantization._quantize_fx_do_not_use import (
    _convert_fx_do_not_use,
)
from torch.testing._internal.common_quantization import (
    QuantizationTestCase,
)
import torch.nn.functional as F

from torch.testing._internal.common_cuda import TEST_CUDA
from torch.testing._internal.common_utils import run_tests
from torch.testing._internal.common_quantization import NodeSpec as ns
import unittest
import itertools

def lower_to_trt(model, inputs, shape_ranges):
    """ Lower a quantized model to TensorRT
    """
    assert len(inputs) == 1, "lower_to_trt only works for one input currently"
    model = acc_tracer.trace(model, inputs)  # type: ignore[attr-defined]
    # TODO: test multiple inputs setting and enable multiple inputs
    input_specs = [
        InputTensorSpec(
            torch.Size([-1, *inputs[0].shape[1:]]), torch.float,
            shape_ranges=shape_ranges, has_batch_dim=True)
    ]

    interp = TRTInterpreter(
        model,
        input_specs,
        explicit_batch_dimension=True, explicit_precision=True)
    result = interp.run(fp16_mode=False, int8_mode=True)
    trt_mod = TRTModule(result.engine, result.input_names, result.output_names)
    return trt_mod

class TestConvertFxDoNotUse(QuantizationTestCase):
    def setUp(self):
        super().setUp()
        self.trt_backend_config_dict = get_tensorrt_backend_config_dict()

    def _test_quantized_inputs_outputs(
            self, prepare_custom_config_dict, prepare_count_check,
            convert_count_check):
        """
        Test the option to have inputs and outputs of the graph quantized
        """
        class M(torch.nn.Module):
            def __init__(self):
                super().__init__()
                self.conv1 = torch.nn.Conv2d(1, 1, 1)
                self.conv2 = torch.nn.Conv2d(1, 1, 1)

            def forward(self, x):
                x = self.conv1(x)
                x = self.conv2(x)
                return x

        # quantized input, quantized output
        m = M()
        qconfig_dict = {'': torch.ao.quantization.default_qconfig}
        m.eval()
        mp = torch.ao.quantization.quantize_fx.prepare_fx(
            m, qconfig_dict,
            prepare_custom_config_dict=prepare_custom_config_dict)
        self.checkGraphModuleNodes(mp, expected_node_occurrence=prepare_count_check)
        mp(torch.randn(1, 1, 4, 4))
        mq = _convert_fx_do_not_use(
            mp, is_reference=True, backend_config_dict=self.trt_backend_config_dict)
        self.checkGraphModuleNodes(mq, expected_node_occurrence=convert_count_check)

    def test_quantized_input_quantized_output(self):
        prepare_custom_config_dict = {
            'input_quantized_idxs': [0], 'output_quantized_idxs': [0]}
        prepare_count_check = {
            ns.call_module(torch.ao.quantization.MinMaxObserver): 2,
        }
        convert_count_check = {
            # output of conv1 and output of conv2
            ns.call_function(torch.quantize_per_tensor): 2,
            # input of conv2
            ns.call_method('dequantize'): 1,
        }
        self._test_quantized_inputs_outputs(
            prepare_custom_config_dict, prepare_count_check, convert_count_check)

    def test_fp32_input_quantized_output(self):
        prepare_custom_config_dict = {
            'output_quantized_idxs': [0]}
        prepare_count_check = {
            ns.call_module(torch.ao.quantization.MinMaxObserver): 3,
        }
        convert_count_check = {
            # input, output of conv1 and output of conv2
            ns.call_function(torch.quantize_per_tensor): 3,
            # input of conv1, conv2
            ns.call_method('dequantize'): 2,
        }
        self._test_quantized_inputs_outputs(
            prepare_custom_config_dict, prepare_count_check, convert_count_check)

    def test_quantized_input_fp32_output(self):
        prepare_custom_config_dict = {
            'input_quantized_idxs': [0]}
        prepare_count_check = {
            ns.call_module(torch.ao.quantization.MinMaxObserver): 2,
        }
        convert_count_check = {
            # output of conv1, conv2
            ns.call_function(torch.quantize_per_tensor): 2,
            # input of conv2, final output
            ns.call_method('dequantize'): 2,
        }
        self._test_quantized_inputs_outputs(
            prepare_custom_config_dict, prepare_count_check, convert_count_check)

    def test_fp32_input_fp32_output(self):
        prepare_custom_config_dict = {}
        prepare_count_check = {
            ns.call_module(torch.ao.quantization.MinMaxObserver): 3,
        }
        convert_count_check = {
            ns.call_function(torch.quantize_per_tensor): 3,
            ns.call_method('dequantize'): 3,
        }
        self._test_quantized_inputs_outputs(
            prepare_custom_config_dict, prepare_count_check, convert_count_check)

@unittest.skipIf(not TEST_CUDA, "gpu is not available.")
class TestQuantizeFxTRTOps(QuantizationTestCase):
    """ Test TensorRT operator support
    """
    def setUp(self):
        super().setUp()
        self.qconfig = torch.ao.quantization.QConfig(
            activation=torch.ao.quantization.observer.HistogramObserver.with_args(
                qscheme=torch.per_tensor_symmetric, dtype=torch.qint8
            ),
            weight=torch.ao.quantization.default_weight_observer
        )
        self.trt_backend_config_dict = get_tensorrt_backend_config_dict()

    def _test_module(
            self,
            m,
            inputs,
            shape_ranges,
            no_prepare=None,
            no_convert=None):
        """
        Args:
          m: the float module we want to test
          inputs: list of inputs for the module
          shape_ranges: a list of shape_range, where every shape_range is a tuple of
          three tuples
          ((min_input_shape), (optimized_input_shape), (max_input_shape)).
          Each shape_range is used to populate a TensorRT optimization profile.
          e.g. If the input shape varies from (1, 224) to (100, 224) and we want to optimize
          for (25, 224) because it's the most common input shape, then we set shape_ranges to
          ((1, 224), (25, 225), (100, 224))
          no_prepare: node occurrence after prepare
          no_convert: node occurrence after convert
        """
        m = m.eval()
        prepared = prepare_fx(m, {"": self.qconfig}, backend_config_dict=self.trt_backend_config_dict)
        self.checkGraphModuleNodes(prepared, expected_node_occurrence=no_prepare)
        # calibration
        prepared(*inputs)
        quantized = _convert_fx_do_not_use(
            prepared, is_reference=True, backend_config_dict=self.trt_backend_config_dict)
        self.checkGraphModuleNodes(quantized, expected_node_occurrence=no_convert)
        # lower to trt
        trt_mod = lower_to_trt(quantized, inputs, shape_ranges)
        inputs_cuda = [i.cuda() for i in inputs]
        # make sure it runs
        trt_mod(*inputs_cuda)


    def test_conv_relu_module(self):
        conv_module = {1 : torch.nn.Conv1d, 2 : torch.nn.Conv2d, 3 : torch.nn.Conv3d}

        conv1d_input = torch.rand(1, 3, 10)
        conv2d_input = torch.rand(1, 3, 10, 10)
        conv3d_input = torch.rand(1, 3, 10, 10, 10)
        conv_input = {1: conv1d_input, 2: conv2d_input, 3: conv3d_input}

        class ConvNdModule(torch.nn.Module):
            def __init__(self, dim, has_relu=False, f_relu=False):
                super().__init__()
                self.conv = conv_module[dim](3, 3, 3).float()
                if has_relu:
                    if f_relu:
                        self.relu = F.relu
                    else:
                        self.relu = torch.nn.ReLU()
                else:
                    self.relu = torch.nn.Identity()

            def forward(self, x):
                return self.relu(self.conv(x))

        # just testing conv2d since conv1d and conv3d are not supported in fx2trt
        for dim, has_relu, f_relu in itertools.product([2], [True, False], [True, False]):
            # when has_relu=False, we have torch.nn.Identity, which would introduce
            # extra quant-dequat pair
            no_convert = {
                ns.call_function(torch.quantize_per_tensor): 2 + int(not has_relu),
                ns.call_method("dequantize"): 2 + int(not has_relu),
            }
            self._test_module(
                ConvNdModule(dim, has_relu, f_relu),
                [conv_input[dim]],
                [((1, *conv_input[dim].shape[1:]),
                  (5, *conv_input[dim].shape[1:]),
                  (10, *conv_input[dim].shape[1:]))],
                no_convert=no_convert)

    def test_linear_relu_module(self):
        class LinearModule(torch.nn.Module):
            def __init__(self, has_relu=False, f_relu=False):
                super().__init__()
                self.linear = torch.nn.Linear(5, 10).float()
                if has_relu:
                    if f_relu:
                        self.relu = F.relu
                    else:
                        self.relu = torch.nn.ReLU()
                else:
                    self.relu = torch.nn.Identity()

            def forward(self, x):
                return self.relu(self.linear(x))

        linear_input = torch.rand(8, 5)

        shape_ranges = [
            ((1, 5),
             (5, 5),
             (10, 5))
        ]
        for has_relu, f_relu in itertools.product([True, False], [True, False]):
            # when has_relu=False, we have torch.nn.Identity, which would introduce
            # extra quant-dequat pair
            no_convert = {
                ns.call_function(torch.quantize_per_tensor): 2 + int(not has_relu),
                ns.call_method("dequantize"): 2 + int(not has_relu),
            }
            self._test_module(
                LinearModule(has_relu, f_relu),
                [linear_input],
                shape_ranges,
                no_convert=no_convert)

    def test_ops(self):
        class M(torch.nn.Module):
            def __init__(self):
                super().__init__()
                self.conv = torch.nn.Conv2d(3, 3, 3)
                self.linear = torch.nn.Linear(5, 5)
                self.relu = torch.nn.ReLU()

            def forward(self, x):
                x = self.conv(x)
                x = self.linear(x)
                x = x + 3
                x = self.relu(x)
                x = x + 6
                return x

        m = M().eval()
        m = prepare_fx(m, {"": default_qconfig})
        m = _convert_fx_do_not_use(
            m, is_reference=True, backend_config_dict=self.trt_backend_config_dict)
        expected_occurrence = {
            ns.call_function(torch.quantize_per_tensor): 5,
            ns.call_method("dequantize"): 5,
            ns.call_module(torch.nn.quantized._reference.Linear): 1,
            ns.call_module(torch.nn.quantized._reference.Conv2d): 1,
        }
        self.checkGraphModuleNodes(
            m,
            expected_node_occurrence=expected_occurrence)

    def test_unsupported_qconfig(self):
        """ Check that we won't quantize the model if the qconfig is not supported
        """
        class LinearModule(torch.nn.Module):
            def __init__(self):
                super().__init__()
                self.linear = torch.nn.Linear(5, 10)

            def forward(self, x):
                return self.linear(x)

        linear_module_input = torch.rand(8, 5)

        m = LinearModule().eval()
        trt_unsupported_qconfig = default_qconfig
        prepared = prepare_fx(m, {"": trt_unsupported_qconfig}, backend_config_dict=self.trt_backend_config_dict)
        # calibration
        prepared(linear_module_input)
        quantized = _convert_fx_do_not_use(
            prepared, is_reference=True, backend_config_dict=self.trt_backend_config_dict)
        node_occurrence = {
            ns.call_function(torch.quantize_per_tensor): 0,
            ns.call_method("dequantize"): 0,
            ns.call_module(torch.nn.Linear): 1,
            ns.call_module(torch.nn.quantized._reference.Linear): 0,
        }
        # check model is not quantized
        self.checkGraphModuleNodes(quantized, expected_node_occurrence=node_occurrence)

    def test_cat(self):
        class M(torch.nn.Module):
            def __init__(self):
                super().__init__()

            def forward(self, x):
                return torch.cat([x, x], 1)

        m = M().eval()
        prepared = prepare_fx(
            m, {"": self.qconfig}, backend_config_dict=self.trt_backend_config_dict)
        self.assertTrue(len(dict(prepared.named_children())) == 1)
        quantized = _convert_fx_do_not_use(
            prepared, is_reference=True, backend_config_dict=self.trt_backend_config_dict)
        node_occurrence = {
            ns.call_function(torch.quantize_per_tensor): 2,
            ns.call_function(torch.cat): 1,
            ns.call_method("dequantize"): 2,
        }
        self.checkGraphModuleNodes(quantized, expected_node_occurrence=node_occurrence)

    def test_addmm(self):
        class M(torch.nn.Module):
            def __init__(self):
                super().__init__()
                self.weight = torch.randn(5, 5)
                self.bias = torch.randn(5)

            def forward(self, x):
                return torch.addmm(self.bias, x, self.weight)

        m = M().eval()
        prepared = prepare_fx(
            m, {"": self.qconfig}, backend_config_dict=self.trt_backend_config_dict)
        node_occurrence = {
            # weight
            ns.call_module(torch.ao.quantization.MinMaxObserver): 1,
            # activation
            ns.call_module(torch.ao.quantization.HistogramObserver): 2,
        }
        self.checkGraphModuleNodes(prepared, expected_node_occurrence=node_occurrence)
        quantized = _convert_fx_do_not_use(
            prepared, is_reference=True, backend_config_dict=self.trt_backend_config_dict)
        node_occurrence = {
            # input activation, output activation and weight
            ns.call_function(torch.quantize_per_tensor): 3,
            ns.call_function(torch.addmm): 1,
            ns.call_method("dequantize"): 3,
        }
        self.checkGraphModuleNodes(quantized, expected_node_occurrence=node_occurrence)

if __name__ == "__main__":
    run_tests()
