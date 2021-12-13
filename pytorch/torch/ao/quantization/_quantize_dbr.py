import torch

from ._dbr.auto_trace import add_auto_observation, add_auto_convert
from ._dbr.fusion import get_module_fusion_fqns


def prepare(model, example_inputs, inplace=False, allow_list=None,
            observer_non_leaf_module_list=None,
            prepare_custom_config_dict=None,
            fuse_modules=True):
    r"""A wrapper around `torch.quantization.prepare` which prepares the
    model for quantization using dynamic tracing. Requires `example_inputs` to build
    the graph before calibration or quantization aware training can proceed.

    TODO(future PR): better docblock
    """
    assert example_inputs is not None, 'example_inputs must be specified'

    if fuse_modules:
        # automatically fuse modules
        old_class = model.__class__
        # For now, need to propagate qconfig before observing, because
        # AutoQuantizationState needs a qconfig to work
        torch.quantization.propagate_qconfig_(model)
        model = add_auto_observation(model, example_inputs)
        module_fusion_fqns = get_module_fusion_fqns(model)
        if len(module_fusion_fqns):
            model = torch.quantization.fuse_modules(model, module_fusion_fqns)
        # TODO: also delete _auto_quant_staate of all children
        if hasattr(model, '_auto_quant_state'):
            del model._auto_quant_state
        # the model hierarchy might have changed during fusion, so we
        # have to delete the cached module hook types
        for k, v in model.named_modules():
            if hasattr(v, '_auto_quant_module_hook_type'):
                del v._auto_quant_module_hook_type

        model.__class__ = old_class

    # Automatically assign qconfigs for modules where the defaults do not
    # work.
    # TODO(future PR): clean this up and align with other APIs
    for name, child in model.named_modules():
        if isinstance(child, (torch.nn.Embedding, torch.nn.EmbeddingBag)):
            # pass
            # child.qconfig = torch.quantization.float_qparams_weight_only_qconfig
            # uncomment below to unbreak attention_is_all_you_need
            # TODO write up issue, maybe fix
            child.qconfig = None  # type: ignore[assignment]
        elif isinstance(child, torch.nn.LSTM):
            # TODO: fix LSTM handling in eager mode static quant and remove this
            child.qconfig = None

    model = torch.quantization.prepare(
        model, inplace, allow_list, observer_non_leaf_module_list,
        prepare_custom_config_dict)
    assert not inplace
    model = add_auto_observation(model, example_inputs)
    return model


def convert(
        module, mapping=None, inplace=False, remove_qconfig=False,
        convert_custom_config_dict=None):
    r"""A wrapper around `torch.quantization.convert` which converts the model
    to a quantized form using dynamic tracing.

    TODO(future PR): better docblock
    """
    # TODO: currently remove_qconfig deletes observers, two things need
    # to be fixed to enable this:
    # 1. scale/zp of non-module observers need to be saved before
    #    observers are deleted
    # 2. current observer deletion logic does not know about AutoQuantState,
    #    this needs to update to work the same way for modules and non-modules
    assert remove_qconfig is False
    model = torch.quantization.convert(
        module, mapping, inplace, remove_qconfig, convert_custom_config_dict)
    assert not inplace
    model = add_auto_convert(model)
    return model
