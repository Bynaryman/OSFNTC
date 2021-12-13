# Owner(s): ["oncall: distributed"]

import sys
import unittest

import torch
from torch import distributed as dist
from torch.distributed._fsdp.flatten_params_wrapper import FlattenParamsWrapper
from torch.testing._internal.common_utils import run_tests, TestCase


if not dist.is_available():
    print("Distributed not available, skipping tests", file=sys.stderr)
    sys.exit(0)


class TestFlattenParams(TestCase):
    """Base test class and used for CPU case."""

    def _get_empty_module(self, seed=0):
        torch.manual_seed(seed)  # keep everything deterministic

        class Test(torch.nn.Module):
            def forward(self, x):
                return x + 1

        module = Test()

        def get_input(device, dtype):
            torch.manual_seed(1)  # keep everything deterministic
            return torch.rand(1).to(device=device, dtype=dtype)

        module.get_input = get_input
        return module

    def _get_transformer(self, seed=0):
        torch.manual_seed(seed)  # keep everything deterministic
        module = torch.nn.Transformer(
            d_model=32,
            num_encoder_layers=2,
            num_decoder_layers=2,
            dim_feedforward=128,
            dropout=0.1,
        )
        module.register_buffer("dummy_buffer", torch.tensor(1.0))

        def get_input(device, dtype):
            torch.manual_seed(1)  # keep everything deterministic
            src = torch.rand(20, 8, 32).to(device=device, dtype=dtype)  # T x B x C
            tgt = torch.rand(10, 8, 32).to(device=device, dtype=dtype)  # T x B x C
            return (src, tgt)

        module.get_input = get_input
        return module

    def _get_shared_params_transformer(self, seed=0):
        module = self._get_transformer(seed=seed)
        # share the FFNs
        for enc_layer, dec_layer in zip(module.encoder.layers, module.decoder.layers):
            dec_layer.linear1.weight = enc_layer.linear1.weight
            dec_layer.linear2.weight = enc_layer.linear2.weight
        return module

    def _get_output(self, module):
        device = next(module.parameters()).device
        dtype = next(module.parameters()).dtype
        input = module.get_input(device, dtype)
        return module(*input)

    def _get_pnorm_after_step(self, module):
        optim = torch.optim.SGD(module.parameters(), lr=0.01)
        loss = self._get_output(module).sum()
        loss.backward()
        optim.step()
        return torch.norm(torch.stack([p.detach().norm() for p in module.parameters()]))

    def _test_num_params(self, module):
        ref_num_params = sum(p.numel() for p in module.parameters())

        params_to_flatten = list(module.parameters())
        flat_module = FlattenParamsWrapper(module, params_to_flatten)
        flat_num_params = sum(p.numel() for p in flat_module.parameters())

        self.assertEqual(ref_num_params, flat_num_params)
        self.assertEqual(flat_num_params, flat_module.flat_param.numel())

    def _test_output(self, module):
        ref_output = self._get_output(module)

        params_to_flatten = list(module.parameters())
        flat_module = FlattenParamsWrapper(module, params_to_flatten)
        flat_output = self._get_output(flat_module)
        self.assertEqual(ref_output, flat_output)

    def test_partial_flattening(self):
        module = self._get_transformer()
        num_params = sum(p.numel() for p in module.parameters())

        params_to_flatten = list(module.encoder.layers[1].parameters()) + list(
            module.decoder.layers[0].parameters()
        )
        num_params_to_flatten = sum(p.numel() for p in params_to_flatten)

        module = FlattenParamsWrapper(module, param_list=params_to_flatten)
        self.assertEqual(module.flat_param.numel(), num_params_to_flatten)
        self.assertEqual(sum(p.numel() for p in module.parameters()), num_params)

        # flattened parameters are removed
        self.assertEqual(len(list(module.encoder.layers[1].parameters())), 0)
        self.assertEqual(len(list(module.decoder.layers[0].parameters())), 0)

        # non-flattened parameters remain
        self.assertGreater(len(list(module.encoder.layers[0].parameters())), 0)
        self.assertGreater(len(list(module.decoder.layers[1].parameters())), 0)

        # test that changing the module dtype works properly
        orig_dtype = params_to_flatten[0].dtype
        new_dtype = torch.float32 if orig_dtype == torch.float16 else torch.float16
        self.assertEqual(module.flat_param.dtype, orig_dtype)
        self.assertTrue(
            all(p.dtype == orig_dtype for p in module.encoder.layers[0].parameters())
        )
        module = module.to(dtype=new_dtype)
        self.assertEqual(module.flat_param.dtype, new_dtype)
        self.assertTrue(
            all(p.dtype == new_dtype for p in module.encoder.layers[0].parameters())
        )

    def test_flatten_nothing(self):
        module = self._get_transformer()
        module = FlattenParamsWrapper(module, param_list=[])
        self.assertIsNone(module.flat_param)

    def test_empty_module(self):
        module = self._get_empty_module()
        in_data = torch.rand(1)
        ref_out = module(in_data)
        module = FlattenParamsWrapper(module, param_list=[])
        self.assertEqual(len(list(module.parameters())), 0)
        self.assertIsNone(module.flat_param)
        fpw_out = module(in_data)
        self.assertEqual(ref_out, fpw_out)

    def test_num_params(self):
        module = self._get_transformer()
        self._test_num_params(module)

    def test_shared_params_num_params(self):
        module = self._get_shared_params_transformer()
        self._test_num_params(module)

    def test_output(self):
        module = self._get_transformer()
        self._test_output(module)

    def test_shared_params_output(self):
        module = self._get_shared_params_transformer()
        self._test_output(module)

    def test_shared_params_pnorm_after_step(self):
        # incorrect parameter sharing is likely to cause problems after an
        # optimization step
        module = self._get_shared_params_transformer()
        ref_pnorm_after_step = self._get_pnorm_after_step(module)

        module = self._get_shared_params_transformer()  # recreate
        params_to_flatten = list(module.parameters())
        flat_module = FlattenParamsWrapper(module, params_to_flatten)
        flat_pnorm_after_step = self._get_pnorm_after_step(flat_module)

        self.assertEqual(ref_pnorm_after_step, flat_pnorm_after_step)


@unittest.skipIf(not torch.cuda.is_available(), "test requires a GPU")
class TestFlattenParamsCUDA(TestFlattenParams):
    def _get_transformer(self, seed=0):
        module = super()._get_transformer(seed=seed)
        return module.cuda()


@unittest.skipIf(not torch.cuda.is_available(), "test requires a GPU")
class TestFlattenParamsCUDAHalf(TestFlattenParams):
    def _get_transformer(self, seed=0):
        module = super()._get_transformer(seed=seed)
        return module.cuda().half()


if __name__ == "__main__":
    run_tests()
