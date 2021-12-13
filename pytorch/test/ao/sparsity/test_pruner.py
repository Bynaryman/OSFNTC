# -*- coding: utf-8 -*-
# Owner(s): ["module: unknown"]


import copy
import logging

import torch
from torch import nn
from torch.ao.sparsity import BasePruner, PruningParametrization, ZeroesParametrization
from torch.nn.utils import parametrize

from torch.testing._internal.common_utils import TestCase

logging.basicConfig(format='%(asctime)s - %(name)s - %(levelname)s - %(message)s', level=logging.INFO)

DEVICES = {
    torch.device("cpu"),
    torch.device("cuda") if torch.cuda.is_available() else torch.device("cpu")
}

NEEDS_ZEROS = {  # these layers should have pruned indices zero-ed, not removed
    nn.BatchNorm2d
}


class Linear(nn.Module):
    r"""Model with Linear layers, in Sequential and outside, without biases"""
    def __init__(self):
        super().__init__()
        self.seq = nn.Sequential(
            nn.Linear(16, 16, bias=False)
        )
        self.linear = nn.Linear(16, 16, bias=False)

    def forward(self, x):
        x = self.seq(x)
        x = self.linear(x)
        return x


class LinearB(nn.Module):
    r"""Model with Linear layers, in Sequential and outside, with biases"""
    def __init__(self):
        super().__init__()
        self.seq = nn.Sequential(
            nn.Linear(16, 16, bias=True)
        )
        self.linear = nn.Linear(16, 16, bias=True)

    def forward(self, x):
        x = self.seq(x)
        x = self.linear(x)
        return x


class MultipleLinear(nn.Module):
    r"""Model with multiple Linear layers, in Sequential and outside, without biases
    and with activation functions"""
    def __init__(self):
        super().__init__()
        self.seq = nn.Sequential(
            nn.Linear(7, 5, bias=False),
            nn.ReLU(),
            nn.Linear(5, 8, bias=False),
            nn.ReLU(),
            nn.Linear(8, 6, bias=False)
        )
        self.linear = nn.Linear(6, 4, bias=False)

    def forward(self, x):
        x = self.seq(x)
        x = self.linear(x)
        return x


class MultipleLinearB(nn.Module):
    r"""Model with multiple Linear layers, in Sequential and outside, with biases
    and with activation functions"""
    def __init__(self):
        super().__init__()
        self.seq = nn.Sequential(
            nn.Linear(7, 5, bias=True),
            nn.ReLU(),
            nn.Linear(5, 8, bias=True),
            nn.ReLU(),
            nn.Linear(8, 6, bias=True)
        )
        self.linear = nn.Linear(6, 4, bias=True)

    def forward(self, x):
        x = self.seq(x)
        x = self.linear(x)
        return x


class MultipleLinearMixed(nn.Module):
    r"""Model with multiple Linear layers, in Sequential and outside, some with biases
    and with activation functions"""
    def __init__(self):
        super().__init__()
        self.seq = nn.Sequential(
            nn.Linear(7, 5, bias=True),
            nn.ReLU(),
            nn.Linear(5, 8, bias=False),
            nn.ReLU(),
            nn.Linear(8, 6, bias=True)
        )
        self.linear = nn.Linear(6, 4, bias=False)

    def forward(self, x):
        x = self.seq(x)
        x = self.linear(x)
        return x


class Conv2dA(nn.Module):
    r"""Model with Conv2d layers, in Sequential and outside, without biases"""
    def __init__(self):
        super().__init__()
        self.seq = nn.Sequential(
            nn.Conv2d(1, 32, 3, 1, bias=False),
        )
        self.conv2d = nn.Conv2d(32, 64, 3, 1, bias=False)

    def forward(self, x):
        x = self.seq(x)
        x = self.conv2d(x)
        return x


class Conv2dB(nn.Module):
    r"""Model with Conv2d layers, in Sequential and outside, with biases"""
    def __init__(self):
        super().__init__()
        self.seq = nn.Sequential(
            nn.Conv2d(1, 32, 3, 1, bias=True),
        )
        self.conv2d = nn.Conv2d(32, 64, 3, 1, bias=True)

    def forward(self, x):
        x = self.seq(x)
        x = self.conv2d(x)
        return x


class Conv2dC(nn.Module):
    r"""Model with Conv2d layers, in Sequential and outside, with and without biases"""
    def __init__(self):
        super().__init__()
        self.seq = nn.Sequential(
            nn.Conv2d(1, 32, 3, 1, bias=True),
        )
        self.conv2d = nn.Conv2d(32, 64, 3, 1, bias=False)

    def forward(self, x):
        x = self.seq(x)
        x = self.conv2d(x)
        return x


class Conv2dBN(nn.Module):
    r"""Model with Conv2d layers and BatchNorms"""
    def __init__(self):
        super().__init__()
        self.seq = nn.Sequential(
            nn.Conv2d(1, 32, 3, 1, bias=True),
            nn.BatchNorm2d(32)
        )
        self.conv2d = nn.Conv2d(32, 64, 3, 1, bias=True)
        self.bn = nn.BatchNorm2d(64)

    def forward(self, x):
        x = self.seq(x)
        x = self.conv2d(x)
        x = self.bn(x)
        return x


class SimplePruner(BasePruner):
    def update_mask(self, layer, **kwargs):
        layer.parametrizations.weight[0].pruned_outputs.add(1)


class MultiplePruner(BasePruner):
    def update_mask(self, layer, **kwargs):
        layer.parametrizations.weight[0].pruned_outputs.update([1, 2])


class TestBasePruner(TestCase):
    def _check_pruner_prepared(self, model, pruner, device):
        for config in pruner.module_groups:
            modules = []
            if type(config['module']) is tuple:
                for module in config['module']:
                    modules.append(module)
            else:
                module = config['module']
                modules.append(module)
            for module in modules:
                assert module.weight.device.type == device.type
                # Check mask exists
                assert hasattr(module, 'mask')
                # Check parametrization exists and is correct
                assert parametrize.is_parametrized(module)
                assert hasattr(module, "parametrizations")
                # Assume that this is the 1st/only parametrization
                if isinstance(module, tuple(NEEDS_ZEROS)):
                    assert type(module.parametrizations.weight[0]) == ZeroesParametrization
                else:
                    assert type(module.parametrizations.weight[0]) == PruningParametrization

    def _check_pruner_mask_squashed(self, model, pruner, device):
        for config in pruner.module_groups:
            modules = []
            if type(config['module']) is tuple:
                for module in config['module']:
                    modules.append(module)
            else:
                module = config['module']
                modules.append(module)
            for module in modules:
                assert module.weight.device.type == device.type
                assert not hasattr(module, "parametrizations")
                assert not hasattr(module, 'mask')

    def _check_pruner_valid_before_step(self, model, pruner, device):
        for config in pruner.module_groups:
            modules = []
            if type(config['module']) is tuple:
                for module in config['module']:
                    modules.append(module)
            else:
                module = config['module']
                modules.append(module)
            for module in modules:
                assert module.weight.device.type == device.type
                assert module.parametrizations.weight[0].pruned_outputs == set()

    def _check_pruner_valid_after_step(self, model, pruner, pruned_set, device):
        for config in pruner.module_groups:
            modules = []
            if type(config['module']) is tuple:
                for module in config['module']:
                    modules.append(module)
            else:
                module = config['module']
                modules.append(module)
            for module in modules:
                assert module.weight.device.type == device.type
                assert module.parametrizations.weight[0].pruned_outputs == pruned_set

    def _test_constructor_on_device(self, model, device):
        self.assertRaisesRegex(TypeError, 'BasePruner .* update_mask',
                               BasePruner)
        model1 = copy.deepcopy(model).to(device)
        pruner = SimplePruner(None)
        pruner.prepare(model1, None)
        for g in pruner.module_groups:
            module = g['module']
            assert module.weight.device.type == device.type
        assert len(pruner.module_groups) == 2
        pruner.step()
        # Can instantiate the model with configs
        model2 = copy.deepcopy(model).to(device)
        pruner = SimplePruner({'test': 3})
        pruner.prepare(model2, [model2.linear])
        assert len(pruner.module_groups) == 1
        assert pruner.module_groups[0]['fqn'] == 'linear'
        assert 'test' in pruner.module_groups[0]
        assert pruner.module_groups[0]['test'] == 3

    def test_constructor(self):
        model = Linear()
        for device in DEVICES:
            self._test_constructor_on_device(model, torch.device(device))

    def _test_prepare_linear_on_device(self, model, device):
        model = copy.deepcopy(model).to(device)
        x = torch.ones(128, 16, device=device)
        pruner = SimplePruner(None)
        pruner.prepare(model, None)
        self._check_pruner_prepared(model, pruner, device)
        assert model(x).shape == (128, 16)

    def test_prepare_linear(self):
        models = [Linear(), LinearB()]  # without and with bias
        for device in DEVICES:
            for model in models:
                self._test_prepare_linear_on_device(model, torch.device(device))

    def _test_prepare_conv2d_on_device(self, model, config, device):
        x = torch.ones((1, 1, 28, 28), device=device)
        pruner = SimplePruner(None)
        pruner.prepare(model, config)
        self._check_pruner_prepared(model, pruner, device)
        assert model(x).shape == (1, 64, 24, 24)

    def test_prepare_conv2d(self):
        bn_model = Conv2dBN()
        bn_config = [(bn_model.seq[0], bn_model.seq[1]), (bn_model.conv2d, bn_model.bn)]

        models = [Conv2dA(), Conv2dB(), Conv2dC(), bn_model]
        configs = [None, None, None, bn_config]
        for device in DEVICES:
            for model, config in zip(models, configs):
                model = model.to(device)
                self._test_prepare_conv2d_on_device(model, config, torch.device(device))

    def _test_squash_mask_linear_on_device(self, model, device):
        model = copy.deepcopy(model).to(device)
        x = torch.ones(128, 16, device=device)
        pruner = SimplePruner(None)
        pruner.prepare(model, None)
        pruner.squash_mask()
        self._check_pruner_mask_squashed(model, pruner, device)
        assert model(x).shape == (128, 16)

    def test_squash_mask_linear(self):
        models = [Linear(), LinearB()]  # without and with bias
        for device in DEVICES:
            for model in models:
                self._test_squash_mask_linear_on_device(model, torch.device(device))

    def _test_squash_mask_conv2d_on_device(self, model, config, device):
        model = copy.deepcopy(model).to(device)
        x = torch.ones((1, 1, 28, 28), device=device)
        pruner = SimplePruner(None)
        pruner.prepare(model, config)
        pruner.squash_mask()
        self._check_pruner_mask_squashed(model, pruner, device)
        assert model(x).shape == (1, 64, 24, 24)

    def test_squash_mask_conv2d(self):
        bn_model = Conv2dBN()
        bn_config = [(bn_model.seq[0], bn_model.seq[1]), (bn_model.conv2d, bn_model.bn)]

        models = [Conv2dA(), Conv2dB(), Conv2dC(), bn_model]
        configs = [None, None, None, bn_config]
        for device in DEVICES:
            for model, config in zip(models, configs):
                model = model.to(device)
                self._test_squash_mask_conv2d_on_device(model, config, torch.device(device))

    def _test_step_linear_on_device(self, model, is_basic, device):
        model = model.to(device)
        if is_basic:
            x = torch.ones(16, 16)
            pruner = SimplePruner(None)
            pruner.prepare(model, None)
            self._check_pruner_valid_before_step(model, pruner, device)
            pruner.step()
            self._check_pruner_valid_after_step(model, pruner, {1}, device)
        else:
            x = torch.ones(7, 7)
            pruner = MultiplePruner(None)
            pruner.prepare(model, None)
            self._check_pruner_valid_before_step(model, pruner, device)
            pruner.step()
            self._check_pruner_valid_after_step(model, pruner, {1, 2}, device)

    def test_step_linear(self):
        basic_models = [Linear(), LinearB()]
        complex_models = [MultipleLinear(), MultipleLinearB(), MultipleLinearMixed()]
        for device in DEVICES:
            for model in basic_models:
                self._test_step_linear_on_device(model, True, torch.device(device))
            for model in complex_models:
                self._test_step_linear_on_device(model, False, torch.device(device))

    def _test_step_conv2d_on_device(self, model, config, device):
        model = model.to(device)
        x = torch.ones((1, 1, 28, 28)).to(device)
        pruner = SimplePruner(None)
        pruner.prepare(model, config)
        self._check_pruner_valid_before_step(model, pruner, device)
        pruner.step()
        if type(model) is Conv2dBN:
            assert pruner.get_module_pruned_outputs(model.seq[1]) == pruner.get_module_pruned_outputs(model.seq[0])
            assert pruner.get_module_pruned_outputs(model.bn) == pruner.get_module_pruned_outputs(model.conv2d)
        self._check_pruner_valid_after_step(model, pruner, {1}, device)
        assert model(x).shape == (1, 64, 24, 24)

    def test_step_conv2d(self):
        bn_model = Conv2dBN()
        bn_config = [(bn_model.seq[0], bn_model.seq[1]),
                     (bn_model.conv2d, bn_model.bn)]

        models = [Conv2dA(), Conv2dB(), Conv2dC(), bn_model]
        configs = [None, None, None, None, bn_config]
        for device in DEVICES:
            for model, config in zip(models, configs):
                self._test_step_conv2d_on_device(model, config, torch.device(device))
