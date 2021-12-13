import collections
import enum
from typing import Callable, Tuple, Any, List, Optional, Dict

import torch
import torch.nn.functional as F
toq = torch.ops.quantized

from .mappings import (
    functions_supported_by_quantization,
    module_types_supported_by_quantization,
    module_types_supported_by_quantization_preserves_dtype,
    functions_supported_by_quantization_preserves_dtype,
    fp32_to_int8_fun_mapping,
    add_and_mul_ops,
)

from torch.quantization import (
    ObserverBase,
    FakeQuantizeBase,
)

def _raise_obs_not_found_error(func):
    raise RuntimeError(
        f'Encountered arithmetic operation {torch.typename(func)} but we have '
        f'encountered fewer arithmetic operations in previous calibration runs. '
        f'This likely indicates that the program contains dynamic control flow. '
        f' Quantization is not defined over dynamic control flow!')

def _raise_obs_op_mismatch(func, prev_op):
    raise RuntimeError(
        f'Encountered arithmetic operation {torch.typename(func)} but previously '
        f'recorded operation was {torch.typename(prev_op)}!. This likely indicates '
        f'that the program contains dynamic control flow. Quantization is not '
        f'defined over dynamic control flow!')


# TODO(future PR): figure out if there is a better option than namedtuple
SeenOpInfo = collections.namedtuple(
    'SeenOpInfo',
    [
        # integer
        'idx',
        # Python type of the seen op. For modules, this is type(mod). For
        # functions, this is the target function.
        'type',
        # True if the type is a module, False otherwise (for functions/methods).
        'type_is_module',
        # Note: FQN refers to the current module for modules and to the parent
        # module for functions
        'fqn',
        # Information about the input tensors, List[QTensorInfo].
        # Non-tensor inputs are represented with None.
        'input_tensor_infos',
        # Information about the output tensors, List[QTensorInfo].
        # Non-tensor outputs are represented with None.
        'output_tensor_infos',
        # Information about tensors which will need to be packed,
        # Dict[int, str]
        # idx is the argument index in args
        # name is the name of this parameter in the parent module
        'packable_tensor_idx_to_name',
        # Information about non-tensors which will need to be packed,
        # Dict[int, Any]
        # idx is the argument index in args
        # arg is the argument value
        'packable_nontensor_idx_to_arg',
        # Information about tensors which will need to be packed from kwargs.
        # Dict[str, str]
        # kwarg_name is the kwarg name
        # name is the name of this parameter in the parent module
        'packable_tensor_kwarg_name_to_name',
        # This is True if all packable args are simple attributes, or there
        # are no packable args.
        # This is False if some packable args are results of other functions.
        # bool
        'op_packing_only_uses_module_attributes',
    ],
)
def seen_op_info_repr(self) -> str:
    s = f"(type): {self.type}\n"
    s += f"     (fqn): {self.fqn}\n"
    s += f"     (input_tensor_infos): {self.input_tensor_infos}\n"
    s += f"     (output_tensor_infos): {self.output_tensor_infos}"
    if len(self.packable_tensor_idx_to_name):
        s += f"\n     (packable_tensor_idx_to_name): {self.packable_tensor_idx_to_name}"
    if len(self.packable_nontensor_idx_to_arg):
        s += f"\n     (packable_nontensor_idx_to_arg): {self.packable_nontensor_idx_to_arg}"
    if len(self.packable_tensor_kwarg_name_to_name):
        s += f"\n     (packable_tensor_kwarg_name_to_name): {self.packable_tensor_kwarg_name_to_name}"
    return s

SeenOpInfo.__repr__ = seen_op_info_repr  # type: ignore[assignment]

QTensorInfo = collections.namedtuple(
    'QTensorInfo',
    [
        'id',  # tensor ID
        'inf_dtype',  # dtype at inference
    ],
)

def op_needs_quantization(op: Callable) -> bool:
    if op in functions_supported_by_quantization:
        return True
    elif type(op) in module_types_supported_by_quantization:
        return True
    else:
        return False

# TODO: fix lint
class ObserverWrapper(torch.nn.Identity):
    def __init__(self, child):
        super().__init__()
        self.child = child

def wrap_observers_in_placeholders(module: torch.nn.Module) -> None:
    """
    Wraps each child observer of `module` in a placeholder which prevents
    the execution of the observer during the forward. This is useful to prevent
    tracing the model with example inputs from contributing to calibration
    statistics.
    """
    for name, child in module.named_children():
        if isinstance(child, (ObserverBase, FakeQuantizeBase)):
            wrapper = ObserverWrapper(child)
            setattr(module, name, wrapper)
        else:
            wrap_observers_in_placeholders(child)

def unwrap_observers_from_placeholders(module: torch.nn.Module) -> None:
    """
    Restores observers back to their original state.
    """
    # Note: we cannot use module.named_children() because we can
    # have two different names refer to the same module, for example
    # when we are reusing observers for torch.add scalar version.
    for name, child in module._modules.items():
        if child is None:
            continue
        if isinstance(child, ObserverWrapper):
            unwrapped = child.child
            setattr(module, name, unwrapped)
        else:
            unwrap_observers_from_placeholders(child)

def trace_with_inputs(
    model: torch.nn.Module,
    example_args: Tuple[Any],
) -> None:
    with torch.no_grad():
        old_training = model.training
        model.eval()
        wrap_observers_in_placeholders(model)
        model(*example_args)
        unwrap_observers_from_placeholders(model)
        if old_training:
            model.train()

# TODO(future PR): verify correctness of this for all
# quantizeable modules
def is_leaf(m: torch.nn.Module) -> bool:
    return (
        # allowlist everything in torch.nn except nn.Sequential
        (m.__module__.startswith('torch.nn') and (
            not isinstance(m, torch.nn.Sequential)
        )) or
        # allowlist nni modules, as they inherit from nn.Sequential
        m.__module__.startswith('torch.nn.intrinsic')
    )

class FuncOutputObsType(enum.Enum):
    NONE = 0
    NEW_OBS = 1
    REUSES_FIRST_INPUT_OBS = 2

def get_func_output_obs_type(
    seen_op_info: SeenOpInfo,
) -> FuncOutputObsType:
    op_type = seen_op_info.type
    is_module = isinstance(op_type, type(torch.nn.Module))
    if is_module:
        return FuncOutputObsType.NONE

    # check for ops which need packed weights but the weights are
    # coming from another function
    if not seen_op_info.op_packing_only_uses_module_attributes:
        return FuncOutputObsType.NONE

    if op_type in add_and_mul_ops:
        if (
            len(seen_op_info.input_tensor_infos) > 0 and
            seen_op_info.input_tensor_infos[0].inf_dtype in (torch.int32, torch.int64)
        ):
            # this is handling ops on dtypes such as torch.int
            return FuncOutputObsType.NONE
        elif (
            len(seen_op_info.input_tensor_infos) > 1 and
            seen_op_info.input_tensor_infos[1] is None
        ):
            return FuncOutputObsType.REUSES_FIRST_INPUT_OBS
    elif op_type in (torch.relu, F.relu):
        return FuncOutputObsType.NONE
    elif op_type == torch.cat:
        if (
            len(seen_op_info.input_tensor_infos) > 0 and
            seen_op_info.input_tensor_infos[0].inf_dtype in (torch.int32, torch.int64)
        ):
            return FuncOutputObsType.NONE
    return FuncOutputObsType.NEW_OBS

def converted_func_needs_scale_zp(seen_op_info: SeenOpInfo) -> bool:
    op_type = seen_op_info.type
    is_module = isinstance(op_type, type(torch.nn.Module))
    if is_module:
        return False
    if op_type in add_and_mul_ops:
        # check if both arguments are tensors
        inputs = seen_op_info.input_tensor_infos
        both_args_tensors = len(inputs) == 2 and inputs[0] is not None and \
            inputs[1] is not None
        # disable quantization for torch.mul with int tensor arguments
        first_dtype_is_not_int = len(inputs) > 0 and \
            inputs[0].inf_dtype not in (torch.int32, torch.int64)
        return both_args_tensors and first_dtype_is_not_int
    elif op_type == torch.cat:
        inputs = seen_op_info.input_tensor_infos
        first_dtype_is_not_int = len(inputs) > 0 and \
            inputs[0].inf_dtype not in (torch.int32, torch.int64)
        return first_dtype_is_not_int
    elif op_type in (F.conv2d, F.linear):
        outputs = seen_op_info.output_tensor_infos
        is_int8 = outputs[0].inf_dtype == torch.quint8
        return is_int8
    return False

class FuncOutputDTypeType(enum.Enum):
    # for ops which are quantizeable and are configured by the qconfig,
    # for example F.conv2d
    DTYPE_DEPENDS_ON_QCONFIG = 0
    # for ops which are quantizeable and take the dtype of the previous
    # op, for example nn.Dropout
    DTYPE_EQUALS_INPUT_DTYPE = 1
    # for ops which may be quantizeable in some cases but are not
    # quantizeable due to observed syntax (for example, F.conv2d with
    # weights coming from another function).
    DTYPE_DEFAULT_BC_UNSUPPORTED_SYNTAX = 2

def get_func_output_dtype_type(
    op: Callable,
    args: Tuple[Any, ...],
    op_packing_only_uses_module_attributes: bool,
) -> FuncOutputDTypeType:
    if isinstance(op, torch.nn.Module):
        if type(op) in module_types_supported_by_quantization_preserves_dtype:
            return FuncOutputDTypeType.DTYPE_EQUALS_INPUT_DTYPE

    # check for ops which need packed weights but the weights are
    # coming from another function
    if not op_packing_only_uses_module_attributes:
        return FuncOutputDTypeType.DTYPE_DEFAULT_BC_UNSUPPORTED_SYNTAX

    if op in functions_supported_by_quantization_preserves_dtype:
        return FuncOutputDTypeType.DTYPE_EQUALS_INPUT_DTYPE
    elif op in add_and_mul_ops and len(args) > 0 and \
            args[0].dtype in (torch.int32, torch.int64):
        # binary ops with torch.int arguments do not support quantization
        return FuncOutputDTypeType.DTYPE_EQUALS_INPUT_DTYPE
    elif op == torch.cat and len(args) > 0 and \
            args[0][0].dtype in (torch.int32, torch.int64):
        return FuncOutputDTypeType.DTYPE_EQUALS_INPUT_DTYPE

    return FuncOutputDTypeType.DTYPE_DEPENDS_ON_QCONFIG

def get_op_packing_only_uses_module_attributes(
    op: Callable,
    args: Tuple[Any, ...],
    module: torch.nn.Module,
) -> bool:
    """
    Returns True if all arguments of this op which are weights are module
    attributes on the root module, and False otherwise.

    For example, for `F.linear(input, weight, bias)`, this would return
    True if `weight` is stored directly on the parent module (the common case),
    and False if `weight` was an output of a different op.
    """
    # check for ops which need packed weights but the weights are
    # coming from another function
    packable_tensor_arg_idxs = get_packable_tensor_arg_idxs(op)
    if packable_tensor_arg_idxs is not None:
        for arg_idx in packable_tensor_arg_idxs:
            arg_name_in_root = get_param_name(module, args[arg_idx])
            if arg_name_in_root is None:
                return False
    return True

def get_quantized_op(
    seen_op_info: SeenOpInfo,
) -> Optional[Callable]:
    """
    Given a `seen_op_info`, returns the quantized version of the seen function.
    If the `seen_op_info` corresponds to a module, returns `None`.
    If the function does need quantizing, returns `None`.
    """
    op_type = seen_op_info.type
    is_module = isinstance(op_type, type(torch.nn.Module))
    if is_module:
        return None
    if seen_op_info.output_tensor_infos[0].inf_dtype != torch.quint8:
        return None

    if (
        (op_type in add_and_mul_ops or op_type == torch.cat) and
        seen_op_info.input_tensor_infos[0].inf_dtype in (torch.int32, torch.int64)
    ):
        # handle torch.mul with int tensor arguments
        return None
    elif op_type in fp32_to_int8_fun_mapping:
        return fp32_to_int8_fun_mapping[op_type]
    return None

def get_input_observed_arg_idxs(
    op: Callable,
) -> Optional[List[int]]:
    if isinstance(op, torch.nn.Module):
        # TODO(future PR): handle RNNs
        return [0]
    if op == F.conv2d:
        return [0, 1]
    elif op == F.linear:
        return [0, 1]
    # None means "observe all Tensor args"
    return None

def get_packable_tensor_arg_idxs(op: Callable) -> Optional[List[int]]:
    """
    Returns tensor arg idxs which correspond to parameters which will need
    to be packed.
    """
    if op == F.conv2d:
        return [1, 2]
    elif op == F.linear:
        return [1]
    return None

def get_packable_tensor_kwarg_names(op: Callable) -> Optional[List[str]]:
    """
    Returns tensor kwarg names which correspond to parameters which will
    need to be packed.
    """
    if op == F.linear:
        return ['bias']
    return None

def get_param_name(module: torch.nn.Module, arg: Any) -> Optional[str]:
    """
    Returns the name of arg with respect to the current module.
    """
    for name, param in module.named_parameters():
        if arg is param:
            return name
    return None
    # raise AssertionError(f"arg {arg} not found in module {module}")

def get_packable_nontensor_arg_idxs(op: Callable) -> Optional[List[int]]:
    """
    Returns nontensor arg idxs which correspond to arguments which will need
    to be packed.
    """
    if op == F.conv2d:
        # stride, padding, dilation, groups
        return [3, 4, 5, 6]
    return None

def get_packable_arg_idxs(op: Callable) -> Optional[List[int]]:
    if op == F.conv2d:
        # weight, bias, stride, padding, dilation, groups
        return [1, 2, 3, 4, 5, 6]
    elif op == F.linear:
        # weight
        return [1]
    return None

def get_weight_arg_idx(op: Callable) -> Optional[int]:
    if op == F.conv2d:
        return 1
    elif op == F.linear:
        return 1
    return None

def iterate_and_apply(
    args: Any,
    flattened_tensor_infos: List[Optional[QTensorInfo]],
    func: Callable,
    flattened_tensor_infos_idx=None
) -> Any:
    """
    Inputs:
      `args`: arguments to a function, may contain nested types, for example:

        ([torch.Tensor, torch.Tensor], int, (int, int))

      `flattened_tensor_infos`: tensor information containers for each tensor
        in `args`, flattened, for example corresponding with above:

        ({...}, {...}, None, None, None)

      `func`: function to apply to each tensor in `args` to create `new_args`

    Returns `new_args`, where each tensor has been transformed by `func`.
    """
    arg_idx = 0
    if flattened_tensor_infos_idx is None:
        flattened_tensor_infos_idx = [0]

    if isinstance(args, tuple):
        new_args = []
        for arg in args:
            new_arg = iterate_and_apply(
                arg, flattened_tensor_infos, func, flattened_tensor_infos_idx)
            new_args.append(new_arg)
        return tuple(new_args)
    elif isinstance(args, list):
        for idx in range(len(args)):
            new_arg = iterate_and_apply(
                args[idx], flattened_tensor_infos, func, flattened_tensor_infos_idx)
            args[idx] = new_arg
        return args
    else:
        # individual element
        cur_flattened_tensor_info = \
            flattened_tensor_infos[flattened_tensor_infos_idx[0]]
        flattened_tensor_infos_idx[0] += 1

        if cur_flattened_tensor_info is not None:
            return func(args, cur_flattened_tensor_info)
        else:
            return args

def get_producer_of_seen_op_info(
    idx_to_seen_op_info: Dict[int, SeenOpInfo],
    cur_seen_op_info: SeenOpInfo,
) -> Optional[SeenOpInfo]:
    """
    Input: cur_seen_op_info, all seen ops
    Output: the SeenOpInfo which created the input to the current SeenOpInfo
    """
    input_tensor_id = cur_seen_op_info.input_tensor_infos[0].id
    for idx, seen_op_info in idx_to_seen_op_info.items():
        for output_tensor_info in seen_op_info.output_tensor_infos:
            if output_tensor_info is not None:
                if input_tensor_id == output_tensor_info.id:
                    return seen_op_info
    return None

def get_users_of_seen_op_info(
    idx_to_seen_op_info: Dict[int, SeenOpInfo],
    cur_seen_op_info: SeenOpInfo,
) -> List[SeenOpInfo]:
    """
    Input: cur_seen_op_info
    Output: list of all seen_op_infos which use the output of the cur_seen_op_info,
    """
    if len(cur_seen_op_info.output_tensor_infos) != 1:
        return []
    output_tensor_id = cur_seen_op_info.output_tensor_infos[0].id
    results = []
    for idx, seen_op_info in idx_to_seen_op_info.items():
        for input_tensor_info in seen_op_info.input_tensor_infos:
            if input_tensor_info is not None:
                if output_tensor_id == input_tensor_info.id:
                    results.append(seen_op_info)
    return results

class HookType(enum.Enum):
    """
    Describes the various types of function and module hooks that are used
    to implement quantization syntax transforms.
    """
    # Hooks which are run before, during and after a quantizeable op.
    # Usually used for op input and output observation, subsituating
    # quantized kernels, and dynamically looking up arguments to quantized
    # kernels.
    OP_HOOKS = 0
    # Hooks which are run before or after a `torch.nn.Module` which
    # is a non-leaf. Usually used for dtype transforms if the user requests
    # that the inputs or outputs of a certain module are of some dtype.
    MODULE_IO_HOOKS = 1
    # Hooks which are run before a non-quantizeable op which requires
    # `torch.float` inputs. Any inputs which are not floats are converted
    # back to floats.
    ARG_DEQUANTS = 2
    # Everything else
    NONE = 3

def get_torch_function_hook_type(
    parent_module: Optional[torch.nn.Module],
    func: Callable,
) -> HookType:
    # the direct __dict__ accesses are for performance, because
    # the default `torch.nn.Module.__getattr__` has overhead.
    parent_module_has_qstate = parent_module is not None and \
        '_modules' in parent_module.__dict__ and \
        '_auto_quant_state' in parent_module.__dict__['_modules']
    needs_op_hooks = parent_module_has_qstate and \
        parent_module.__dict__['_modules']['_auto_quant_state'].cur_op_needs_hooks(func)  # type: ignore[union-attr, operator]

    if needs_op_hooks:
        return HookType.OP_HOOKS
    elif parent_module_has_qstate:
        return HookType.ARG_DEQUANTS
    else:
        return HookType.NONE

def get_module_hook_type(
    parent_module: Optional[torch.nn.Module],
    cur_module: torch.nn.Module,
) -> HookType:
    cached_hook_type = getattr(cur_module, '_auto_quant_module_hook_type', None)
    if cached_hook_type is not None:
        return cached_hook_type
    parent_module_has_qstate = parent_module is not None and \
        '_modules' in parent_module.__dict__ and \
        '_auto_quant_state' in parent_module.__dict__['_modules']
    needs_op_hooks = parent_module_has_qstate and \
        parent_module.__dict__['_modules']['_auto_quant_state'].cur_op_needs_hooks(cur_module)  # type: ignore[union-attr, operator]
    # We need IO hooks if
    # * we are calling forward on a module (always True here)
    # * that module has quant state
    # * that module does not need op hooks for the parent
    needs_io_hooks = (
        '_modules' in cur_module.__dict__ and
        '_auto_quant_state' in cur_module.__dict__['_modules'] and
        (not needs_op_hooks)
    )
    needs_arg_dequants = parent_module_has_qstate and not needs_op_hooks

    if needs_op_hooks:
        result = HookType.OP_HOOKS
    elif needs_io_hooks:
        result = HookType.MODULE_IO_HOOKS
    elif needs_arg_dequants:
        result = HookType.ARG_DEQUANTS
    else:
        result = HookType.NONE
    cur_module._auto_quant_module_hook_type = result  # type: ignore[assignment]
    return result

def clone_detach_tensor_without_dispatch(x: torch.Tensor) -> torch.Tensor:
    """
    Creates a detached clone of `x`, unwrapping x from any dispatched
    type before performing the copy.
    This is necessary to not leak dispatched types to debugging logic
    such as numeric suite.
    TODO(future PR): figure out why is_quantized returns False for
    the dispatched types, even though the underlying tensor is quantized.
    """
    old_class = x.__class__
    x.__class__ = torch.Tensor
    x_copy = x.clone().detach()
    x.__class__ = old_class
    return x_copy

def get_input_args_quant_dequant_info(
    seen_op_info: SeenOpInfo,
    tensor_id_to_scale_zp: Dict[int, Tuple[torch.Tensor, torch.Tensor]],
) -> Tuple[List[Optional[Tuple[float, int]]], List[bool], bool]:
    """
    Returns a list of information about the tensor inputs to the current op.

    Quant list:
    For each tensor input:
    * if the tensor input needs a quant, the list will contain
      (scale, zero_point)
    * if the tensor input does not need a quant, the list will contain None

    Dequant list:
    For each tensor input:
    * if the tensor input needs a dequant, True, otherwise, False

    any_arg_quant_or_dequant_needed:
    If True, at least one of quants or dequants is needed. If False,
    there are no quants or dequants needed.

    For example, if there are two tensor inputs to the current op, and the
    first input needs a quant, this function will return

      # quants
      [(scale0, zero_point0), None],
      # dequants
      [False, False]
    """
    quant_infos: List[Optional[Tuple[float, int]]] = []
    dequant_infos: List[bool] = []

    # determine the expected output dtype
    output_dtype = seen_op_info.output_tensor_infos[0].inf_dtype
    packable_arg_idxs = get_packable_arg_idxs(seen_op_info.type)
    any_arg_quant_or_dequant_needed = False

    for input_arg_idx, input_arg in enumerate(seen_op_info.input_tensor_infos):
        arg_will_be_packed = packable_arg_idxs is not None and \
            input_arg_idx in packable_arg_idxs and \
            seen_op_info.op_packing_only_uses_module_attributes
        if input_arg is not None and not arg_will_be_packed:
            tensor_id = input_arg.id
            if input_arg.inf_dtype != output_dtype:
                any_arg_quant_or_dequant_needed = True
                if output_dtype == torch.quint8:
                    assert tensor_id in tensor_id_to_scale_zp
                    scale, zp = tensor_id_to_scale_zp[tensor_id]
                    # TODO: return this to the caller
                    quant_infos.append((scale, zp,))  # type: ignore[arg-type]
                    dequant_infos.append(False)
                else:
                    quant_infos.append(None)
                    dequant_infos.append(True)
            else:
                quant_infos.append(None)
                dequant_infos.append(False)
        else:
            quant_infos.append(None)
            dequant_infos.append(False)
    return quant_infos, dequant_infos, any_arg_quant_or_dequant_needed
