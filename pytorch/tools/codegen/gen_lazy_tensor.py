import pathlib
import argparse
import os
import yaml
from collections import namedtuple
from typing import List, Dict, Union, Sequence, Optional, Callable, Iterable, Iterator, Tuple
from tools.codegen.gen import get_grouped_native_functions, parse_native_yaml
from tools.codegen.model import (DispatchKey, FunctionSchema,
                                 NativeFunction, NativeFunctionsGroup, OperatorName)
from tools.codegen.selective_build.selector import SelectiveBuilder
from tools.codegen.utils import concatMap, YamlLoader, FileManager
import tools.codegen.dest as dest
from .gen_backend_stubs import (parse_backend_yaml, error_on_missing_kernels,
                                gen_dispatchkey_nativefunc_headers,
                                gen_dispatcher_registrations)

# Parses the external backend's yaml, and adds a new BackendIndex for the backend's dispatch key.
# Returns a Tuple of (backend_key, autograd_key, cpp_namespace, updated BackendIndex mapping, full_codegen)
ParsedExternalYaml = namedtuple('ParsedExternalYaml', [
    'backend_key', 'autograd_key', 'cpp_namespace', 'backend_indices', 'full_codegen'])


def parse_full_codegen_ops(
        backend_yaml_path: str,
        grouped_native_functions: Sequence[Union[NativeFunction, NativeFunctionsGroup]],
) -> List[OperatorName]:

    native_functions_map: Dict[OperatorName, NativeFunction] = {
        f.func.name: f
        for f in concatMap(lambda f: [f] if isinstance(f, NativeFunction) else list(f.functions()), grouped_native_functions)
    }

    with open(backend_yaml_path, 'r') as f:
        yaml_values = yaml.load(f, Loader=YamlLoader)
    assert isinstance(yaml_values, dict)

    full_codegen = yaml_values.pop('full_codegen', [])
    assert isinstance(full_codegen, list), f'expected "full_codegen" to be a list, but got: {full_codegen}'
    full_codegen = [OperatorName.parse(name) for name in full_codegen]

    return full_codegen


def main() -> None:
    parser = argparse.ArgumentParser(description='Generate Lazy Tensor backend files')
    parser.add_argument(
        '-s',
        '--source_yaml',
        help='path to source yaml file containing operator external definitions')
    parser.add_argument(
        '-o', '--output_dir', help='output directory')
    parser.add_argument(
        '--dry_run', type=bool, default=False, help='output directory')
    parser.add_argument(
        '--impl_path', type=str, default=None, help='path to the source C++ file containing kernel definitions')
    parser.add_argument(
        '--gen_ts_lowerings', action="store_true", help='Generate TorchScript lowerings in addition to Lazy IR and NativeFunctions')
    parser.add_argument(
        '--node_base', type=str, default="Node", help='Name of backend specific custom Lazy IR Node base class')
    parser.add_argument(
        '--node_base_hdr', type=str, default=None, help='Path to header file defining custom Lazy IR Node base class')
    parser.add_argument(
        '--tensor_class', type=str, default="LazyTensor", help='Name of backend specific custom Lazy Tensor class')
    parser.add_argument(
        '--tensor_class_hdr', type=str, default="lazy_tensor_core/csrc/tensor.h",
        help='Path to header file defining custom Lazy Tensor class')
    options = parser.parse_args()

    run(options.source_yaml, options.output_dir, options.dry_run, options.impl_path,
        options.gen_ts_lowerings, options.node_base, options.node_base_hdr,
        options.tensor_class, options.tensor_class_hdr)


def run(source_yaml: str, output_dir: str, dry_run: bool, impl_path: Optional[str],
        gen_ts_lowerings: bool, node_base: str, node_base_hdr: Optional[str],
        tensor_class: str, tensor_class_hdr: str) -> None:

    # Assumes that this file lives at PYTORCH_ROOT/tools/codegen/gen_backend_stubs.py
    pytorch_root = pathlib.Path(__file__).parent.parent.parent.absolute()
    template_dir = os.path.join(pytorch_root, "aten/src/ATen/templates")

    def make_file_manager(install_dir: str) -> FileManager:
        return FileManager(install_dir=install_dir, template_dir=template_dir, dry_run=dry_run)

    fm = make_file_manager(output_dir)

    native_yaml_path = os.path.join(pytorch_root, 'aten/src/ATen/native/native_functions.yaml')
    parsed_yaml = parse_native_yaml(native_yaml_path)
    native_functions, backend_indices = parsed_yaml.native_functions, parsed_yaml.backend_indices
    grouped_native_functions = get_grouped_native_functions(native_functions)

    def sort_native_function(f: Union[NativeFunctionsGroup, NativeFunction]) -> str:
        """
        We sort the native function because of the note in concat_map_codegen.
        TODO(alanwaketan): Remove this sorting hack once all ops are grouped properly.
        """
        func = f.functional.func if isinstance(f, NativeFunctionsGroup) else f.func
        return str(func.name.name)

    grouped_native_functions = sorted(grouped_native_functions, key=sort_native_function)
    parsed_backend_yaml = parse_backend_yaml(source_yaml, grouped_native_functions, backend_indices)
    backend_key = parsed_backend_yaml.backend_key
    autograd_key = parsed_backend_yaml.autograd_key
    cpp_namespace = parsed_backend_yaml.cpp_namespace
    backend_indices = parsed_backend_yaml.backend_indices
    full_codegen = parse_full_codegen_ops(source_yaml, grouped_native_functions)

    def concat_map_codegen(func: Callable[[NativeFunction], Sequence[str]],
                           xs: Iterable[Union[NativeFunctionsGroup, NativeFunction]],
                           *, codegenInplaceVariant: bool = False) -> Iterator[str]:
        """
        We code-gen for the functional variant, which is all we need for IR classes/lowerings/shape inferences, but we
        only code-gen additional entries for the inplace variant for the native functions.
        Note: If xs is not sorted, there may be an edge case when generating IR classes. Considering relu and relu_, if
        we encounter relu_ before relu. we will then generate an IR class with op = at::aten::relu_ for both relu and
        relu_ which will cause problems for relu.
        TODO(alanwaketan): Once all ops are grouped properly, we should no longer need this hack.
        """
        generated = set()

        def gen_key(func: FunctionSchema) -> Tuple[str, str]:
            # we want to generate unique entries for overloads of functional variants,
            # but not for inplace variants unless explicitly told `codegenInplaceVariant`
            return (func.name.name.base, func.name.overload_name)

        for x in xs:
            f = x.functional if isinstance(x, NativeFunctionsGroup) else x
            # For the 'or'd terms:
            # 1. codegenInplaceVariant means we can generate the in-place variant corresponding items.
            # 2. not f.func.name.name.inplace means the op is not a in-place variant, so we can generate the item.
            # 3. f.func.name.name.base not in generated means even for in-place ops we still need to generate the item
            # as if they were the functional variants for one time.
            if f.func.name in full_codegen and \
               (codegenInplaceVariant or not f.func.name.name.inplace or gen_key(f.func) not in generated):
                generated.add(gen_key(f.func))
                for r in func(f):
                    yield r

    selector = SelectiveBuilder.get_nop_selector()

    # TODO: handle cases when yaml contains zero ops properly in a later PR.
    if backend_key is not None and autograd_key is not None:
        backend_dispatch_key: DispatchKey = backend_key
        autograd_dispatch_key: DispatchKey = autograd_key
        class_name = backend_indices[backend_dispatch_key].native_function_class_name()

        if impl_path is not None:
            error_on_missing_kernels(native_functions, backend_indices, backend_key,
                                     autograd_key, impl_path, full_codegen)

        assert class_name is not None

        # Generate nativefunction declarations
        gen_dispatchkey_nativefunc_headers(fm, class_name, cpp_namespace, backend_indices,
                                           grouped_native_functions, backend_dispatch_key, autograd_dispatch_key)

        # Generate Dispatcher registrations which hook up the nativefunctions
        for dispatch_key in [backend_dispatch_key, autograd_dispatch_key]:
            gen_dispatcher_registrations(fm, output_dir, cpp_namespace, backend_indices, grouped_native_functions,
                                         backend_dispatch_key, dispatch_key, selector)

        # Generate native function impls that build IR nodes
        fm.write_with_template(f'{backend_dispatch_key}NativeFunctions.cpp', 'DispatchKeyNativeFunctions.cpp', lambda: {
            'includes': [f'#include <{path}>' for path in [
                tensor_class_hdr,
                "ATen/MetaFunctions.h",
                "torch/csrc/lazy/core/shape.h",
                "lazy_tensor_core/csrc/aten_ltc_bridge.h",
                "lazy_tensors/computation_client/metrics.h",
                f"{output_dir}/{backend_key}NativeFunctions.h",
                f"{output_dir}/{backend_key}LazyIr.h",
                f"{output_dir}/{backend_key}ShapeInference.h",
            ]],
            'native_functions_include': '',
            'backend_namespace': 'torch_lazy_tensors',  # this is wrong
            'native_function_definitions':
            list(concat_map_codegen(
                dest.GenLazyNativeFuncDefinition(f'{backend_dispatch_key}NativeFunctions',
                                                 backend_indices[backend_dispatch_key],
                                                 tensor_class),
                grouped_native_functions,
                codegenInplaceVariant=True
            )),
        })
        # Generate headers for shape/dtype funcs for non-meta kernels
        fm.write_with_template(f'{backend_dispatch_key}ShapeInference.h', 'ShapeInference.h', lambda: {
            'lazy_ir_sysinc': [f'#include <{path}>' for path in [
                "ATen/Tensor.h",
                "c10/core/ScalarType.h",
                "c10/util/Optional.h",
                "torch/csrc/lazy/core/ir.h",
                "torch/csrc/lazy/core/shape.h",
                "vector",
            ]],
            'lazy_ir_inc': [],
            'DispatchKey': backend_dispatch_key,
            'dispatch_namespace': backend_dispatch_key.lower(),
            'func_declarations': list(concat_map_codegen(
                dest.GenLazyShapeInferenceDefinition(backend_indices[backend_dispatch_key],
                                                     tensor_class),
                grouped_native_functions
            )),
        })
        # Generate IR node classes
        fm.write_with_template(f'{backend_dispatch_key}LazyIr.h', 'LazyIr.h', lambda: {
            'lazy_ir_sysinc': [f'#include <{path}>' for path in [
                "c10/core/ScalarType.h",
                "c10/util/Optional.h",
                "torch/csrc/lazy/core/hash.h",
                "torch/csrc/lazy/core/ir.h",
                "vector",
            ]],
            'lazy_ir_inc': [f'#include "{path}"' for path in [
                "lazy_tensor_core/csrc/ops/scalar.h",
                node_base_hdr if node_base_hdr is not None else None
            ] if path is not None],
            'external_backend_headers': f'#include "{output_dir}/{backend_key}NativeFunctions.h"',
            'namespaced_headers': '',
            'DispatchKey': backend_dispatch_key,
            'dispatch_namespace': backend_dispatch_key.lower(),
            'ir_declarations': list(concat_map_codegen(
                dest.LazyIR(backend_indices[backend_dispatch_key], node_base),
                grouped_native_functions
            )),
        })


if __name__ == '__main__':
    main()
