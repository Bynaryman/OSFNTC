#include <sstream>
#include <string>

#include <ATen/core/jit_type.h>
#include <c10/core/ScalarType.h>
#include <torch/csrc/jit/backends/backend.h>
#include <torch/csrc/jit/backends/backend_detail.h>
#include <torch/csrc/jit/backends/backend_preprocess.h>
#include <torch/csrc/jit/mobile/nnc/aot_compiler.h>
#include <torch/csrc/jit/passes/constant_propagation.h>
#include <torch/csrc/jit/passes/dead_code_elimination.h>
#include <torch/csrc/jit/passes/freeze_module.h>
#include <torch/csrc/jit/passes/frozen_graph_optimizations.h>
#include <torch/csrc/jit/passes/peephole.h>
#include <torch/csrc/jit/passes/remove_mutation.h>
#include <torch/csrc/jit/passes/shape_analysis.h>
#include <torch/csrc/jit/passes/symbolic_shape_analysis.h>
#include <torch/csrc/jit/serialization/export.h>
#include <torch/csrc/jit/serialization/import.h>
#include <torch/csrc/jit/tensorexpr/graph_opt.h>
#include <torch/csrc/jit/tensorexpr/kernel.h>
#include <torch/script.h>

C10_DEFINE_string(model, "", "The torch script model to optimize.");
C10_DEFINE_string(model_name, "", "The name of the model.");
C10_DEFINE_string(model_version, "", "The version of the model.");
C10_DEFINE_string(
    input_dims,
    "",
    "The dimensions of input TensorCPUs using comma separated numbers."
    "If multiple inputs needed, use semicolon to separate "
    "the dimension of different tensors.");
C10_DEFINE_string(
    input_types,
    "float",
    "The dtype of input TensorCPUs."
    "If multiple inputs needed, use semicolon to separate "
    "the dtype of different tensors."
    "Supported dtypes: float, int64, uint8");
C10_DEFINE_string(method_name, "forward", "The name of the method.");
C10_DEFINE_string(
    output_llvm,
    "",
    "Name of the output llvm assembly to be saved.");
C10_DEFINE_string(output_model, "", "Name of the output model to be saved.");

namespace {

std::vector<std::string> split(
    char separator,
    const std::string& string,
    bool ignore_empty = true) {
  std::vector<std::string> pieces;
  std::stringstream ss(string);
  std::string item;
  while (getline(ss, item, separator)) {
    if (!ignore_empty || !item.empty()) {
      pieces.push_back(std::move(item));
    }
  }
  return pieces;
}

std::vector<std::vector<int64_t>> parseInputShapes() {
  CAFFE_ENFORCE_GE(FLAGS_input_dims.size(), 0, "Input dims must be specified.");
  std::vector<std::string> input_dims_list = split(';', FLAGS_input_dims);
  std::vector<std::vector<int64_t>> inputs;
  for (const auto& input_dims_item : input_dims_list) {
    auto input_dims_str = split(',', input_dims_item);
    std::vector<int64_t> input_dims;
    input_dims.reserve(input_dims_str.size());
    for (const auto& s : input_dims_str) {
      input_dims.push_back(c10::stoi(s));
    }
    inputs.push_back(input_dims);
  }
  return inputs;
}

std::vector<at::ScalarType> parseInputTypes() {
  std::vector<std::string> inputTypes = split(';', FLAGS_input_types);
  std::vector<at::ScalarType> scalarTypes;
  for (const auto& inputType : inputTypes) {
    at::ScalarType scalarType;
    if (inputType == "float") {
      scalarType = at::ScalarType::Float;
    } else if (inputType == "uint8") {
      scalarType = at::ScalarType::Byte;
    } else if (inputType == "int64") {
      scalarType = at::ScalarType::Long;
    } else {
      CAFFE_THROW("Unsupported input type: ", inputType);
    }
    scalarTypes.push_back(scalarType);
  }
  return scalarTypes;
}

c10::Dict<c10::IValue, c10::IValue> createCompileSpec() {
  c10::Dict<c10::IValue, c10::IValue> compile_spec(
      c10::StringType::get(), c10::AnyType::get());
  c10::Dict<c10::IValue, c10::IValue> method_spec(
      c10::StringType::get(), c10::AnyType::get());
  auto inputShapes = parseInputShapes();
  auto inputTypes = parseInputTypes();
  method_spec.insert("sizes", inputShapes);
  method_spec.insert("types", inputTypes);
  compile_spec.insert(FLAGS_method_name, method_spec);
  return compile_spec;
}

std::vector<std::vector<int64_t>> getInputSizes(
    const c10::Dict<c10::IValue, c10::IValue>& compile_spec) {
  auto input_shapes = compile_spec.at(FLAGS_method_name).toGenericDict().at("sizes").toList();
  std::vector<std::vector<int64_t>> inputSizes;
  for (const auto& input_shape : input_shapes) {
    auto sizes = ((c10::IValue) input_shape).toIntVector();
    inputSizes.emplace_back(sizes);
  }
  return inputSizes;
}

std::vector<at::ScalarType> getInputTypes(
    const c10::Dict<c10::IValue, c10::IValue>& compile_spec) {
  auto inputTypesList = compile_spec.at(FLAGS_method_name).toGenericDict().at("types").toList();
  std::vector<at::ScalarType> inputTypes;
  for (const auto& inputType : inputTypesList) {
    auto type = ((c10::IValue) inputType).toScalarType();
    inputTypes.emplace_back(type);
  }
  return inputTypes;
}

std::string getNncKernelId() {
  // TODO: calculate the version_token.
  const std::string version_token = "VERTOKEN";
  return FLAGS_model_name + ":" + FLAGS_model_version + ":" + FLAGS_method_name +
      ":" + version_token;
}

std::string getNncKernelFuncName(const std::string& method_name) {
  return "nnc_" + FLAGS_model_name + "_" + FLAGS_model_version + "_" + method_name;
}

void writeOutputLlvmAssembly(const std::string& asm_code) {
  std::string output_llvm_file_name = FLAGS_output_llvm;
  if (output_llvm_file_name.empty()) {
    output_llvm_file_name =
        FLAGS_model.substr(0, FLAGS_model.find('.')) + ".compiled.ll";
  }

  std::ofstream output(output_llvm_file_name);
  output << asm_code;
  std::cout << "The compiled llvm assembly code was saved to " << output_llvm_file_name
            << std::endl;
}

c10::IValue preprocess(
    const torch::jit::Module& mod,
    const c10::Dict<c10::IValue, c10::IValue>& compile_spec,
    const torch::jit::BackendDebugHandleGenerator& generate_debug_handles) {

  auto method = mod.get_method(FLAGS_method_name);
  auto graph = toGraphFunction(method.function()).graph()->copy();
  auto sizes = getInputSizes(compile_spec);
  auto types = getInputTypes(compile_spec);
  auto kernel_func_name = getNncKernelFuncName(FLAGS_method_name);

  auto compiled = torch::jit::mobile::nnc::aotCompile(
      FLAGS_method_name, graph, sizes, types, kernel_func_name);
  writeOutputLlvmAssembly(compiled.second);

  auto func = std::move(compiled.first);
  func->set_nnc_kernel_id(getNncKernelId());

  torch::jit::mobile::nnc::CompilationUnit cu;
  cu.register_function(std::move(func));
  return cu.serialize();
}

static auto reg = torch::jit::backend_preprocess_register("nnc", preprocess);

} // namespace

int main(int argc, char** argv) {
  c10::SetUsageMessage(
      "Run NNC AOT compiler for pytorch model. Example usage:\n"
      "build/bin/aot_model_compiler"
      " --model=<model file>"
      " --model_name=<model name>"
      " --model_version=<model version>"
      " --input_dims=<input dimensions like '1,3,224,224;2,2'>"
      " --input_types=<input dtypes like 'float;float'>"
      " [--method_name=<method name>]"
      " [--output_llvm=<llvm assembly output file path>]"
      " [--output_model=<output model file path>]");

  if (!c10::ParseCommandLineFlags(&argc, &argv)) {
    std::cerr << "Failed to parse command line flags!" << std::endl;
    std::cout << c10::UsageMessage() << std::endl;
    return 1;
  }

  CAFFE_ENFORCE(!FLAGS_model.empty(), c10::UsageMessage());
  CAFFE_ENFORCE(!FLAGS_model_name.empty(), c10::UsageMessage());
  CAFFE_ENFORCE(!FLAGS_model_version.empty(), c10::UsageMessage());
  CAFFE_ENFORCE(!FLAGS_input_dims.empty(), c10::UsageMessage());
  CAFFE_ENFORCE(split(';', FLAGS_input_dims).size() == split(';', FLAGS_input_types).size(),
      "Number of input_dims and input_types should be the same");

  std::string output_model_name = FLAGS_output_model;
  if (output_model_name.empty()) {
    output_model_name =
        FLAGS_model.substr(0, FLAGS_model.find('.')) + ".compiled.pt";
  }

  auto m = torch::jit::load(FLAGS_model);
  m.eval();
  auto frozen_m = torch::jit::freeze_module(m.clone());
  auto graph = frozen_m.get_method(FLAGS_method_name).graph();
  auto inputShapes = parseInputShapes();
  auto inputTypes = parseInputTypes();
  std::vector<c10::optional<at::Tensor>> example_inputs;
  example_inputs.reserve(inputShapes.size());
  for (int i = 0; i < inputShapes.size(); ++i) {
    example_inputs.emplace_back(at::rand(inputShapes[i]).to(at::dtype(inputTypes[i])));
  }

  torch::jit::RemoveTensorMutation(graph);
  torch::jit::EliminateDeadCode(graph->block());
  graph = torch::jit::tensorexpr::removeUnusedSelfArgument(graph);

  torch::jit::tensorexpr::annotateInputShapes(graph, example_inputs);
  torch::jit::OptimizeFrozenGraph(graph, true);
  torch::jit::PropagateShapesOnGraph(graph);
  torch::jit::PeepholeOptimize(graph, false);
  torch::jit::ConstantPropagation(graph);
  torch::jit::PropagateShapesOnGraph(graph);
  torch::jit::PeepholeOptimize(graph, false);
  torch::jit::ConstantPropagation(graph);

  auto compile_spec = createCompileSpec();
  auto any_dict_ty =
      c10::DictType::create(c10::StringType::get(), c10::AnyType::get());
  auto compiled_module = torch::jit::detail::codegen_backend_module(
      "nnc", frozen_m, compile_spec, any_dict_ty);
  compiled_module._save_for_mobile(output_model_name);
  std::cout << "The compiled model was saved to " << output_model_name
            << std::endl;
  return 0;
}
