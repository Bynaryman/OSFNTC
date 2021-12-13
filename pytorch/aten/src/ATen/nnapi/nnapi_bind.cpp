#include <vector>

#include <ATen/ATen.h>
#include <ATen/nnapi/nnapi_bind.h>
#include <ATen/nnapi/nnapi_wrapper.h>
#include <ATen/nnapi/nnapi_model_loader.h>

namespace torch {
namespace nnapi {
namespace bind {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
nnapi_wrapper* nnapi;
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
nnapi_wrapper* check_nnapi;

void load_platform_library() {
  static int run_once = [](){
    nnapi_wrapper_load(&nnapi, &check_nnapi);
    CAFFE_ENFORCE(nnapi);
    CAFFE_ENFORCE(nnapi->Model_free);
    CAFFE_ENFORCE(nnapi->Compilation_free);
    CAFFE_ENFORCE(nnapi->Execution_free);
    return 0;
  }();
  (void)run_once;
}

// NnapiCompilation functon definitions:

// Could possibly call load_platform_library in constructor, but error reporting
// can be complicated if the constructor is called during model loading.
// Instead, delay all work until the explicit init call.
void NnapiCompilation::init(
    at::Tensor serialized_model_tensor,
    std::vector<at::Tensor> parameter_buffers) {
  TORCH_CHECK(!model_, "Attempted to re-initialize NnapiCompilation.");

  load_platform_library();

  std::vector<const void*> buffers;
  std::vector<int32_t> buffer_sizes;
  for (auto& t : parameter_buffers) {
    TORCH_CHECK(t.is_contiguous());
    buffers.push_back(t.data_ptr());
    buffer_sizes.push_back(t.nbytes());
  }

  TORCH_CHECK(serialized_model_tensor.is_contiguous());
  // This is currently always int32_t, but support uint8_t for old models
  // and possible future changes to the generator.
  uint8_t* ser_model_ptr =
    serialized_model_tensor.scalar_type() == at::ScalarType::Byte
      ? serialized_model_tensor.data_ptr<uint8_t>()
      : reinterpret_cast<uint8_t*>(serialized_model_tensor.data_ptr<int32_t>());
  c10::ArrayRef<uint8_t> ser_model = {
    ser_model_ptr,
    serialized_model_tensor.nbytes()
  };
  TORCH_CHECK(ser_model.size() > 0);

  // NOLINTNEXTLINE(cppcoreguidelines-init-variables)
  ANeuralNetworksModel* model;
  check_nnapi->Model_create(&model);
  CAFFE_ENFORCE(model);
  model_.reset(model);

  int load_result = ::caffe2::nnapi::load_nnapi_model(
      nnapi,
      model_.get(),
      ser_model.data(),
      ser_model.size(),
      buffers.size(),
      buffers.data(),
      buffer_sizes.data(),
      0,
      nullptr,
      nullptr,
      &num_inputs_,
      &num_outputs_,
      nullptr);
  CAFFE_ENFORCE(load_result == 0);

  check_nnapi->Model_finish(model_.get());

  // NOLINTNEXTLINE(cppcoreguidelines-init-variables)
  ANeuralNetworksCompilation* compilation;
  check_nnapi->Compilation_create(model_.get(), &compilation);
  // TODO: Make this configurable.
  check_nnapi->Compilation_setPreference(compilation, ANEURALNETWORKS_PREFER_SUSTAINED_SPEED);
  check_nnapi->Compilation_finish(compilation);
  compilation_.reset(compilation);
}

void NnapiCompilation::run(
    std::vector<at::Tensor> inputs,
    std::vector<at::Tensor> outputs) {
  // NOLINTNEXTLINE(cppcoreguidelines-init-variables)
  ANeuralNetworksExecution* execution;
  check_nnapi->Execution_create(compilation_.get(), &execution);
  ExecutionPtr execution_unique_ptr(execution);

  TORCH_CHECK((int32_t)inputs.size() == num_inputs_);
  TORCH_CHECK((int32_t)outputs.size() == num_outputs_);

  for (size_t i = 0; i < inputs.size(); i++) {
    auto& t = inputs[i];
    // TODO: Check contiguous and dtype.
    ANeuralNetworksOperandType op_type;
    std::vector<uint32_t> dim;
    get_operand_type(t, &op_type, &dim);
    check_nnapi->Execution_setInput(
        execution,
        i,
        &op_type,
        t.data_ptr(),
        t.nbytes());
  }

  for (size_t i = 0; i < outputs.size(); i++) {
    auto& t = outputs[i];
    // TODO: Check contiguous and dtype.
    check_nnapi->Execution_setOutput(
        execution,
        i,
        nullptr,
        t.data_ptr(),
        t.nbytes());
  }

  check_nnapi->Execution_compute(execution);

  // TODO: Maybe skip this for fixed-size outputs?
  for (size_t i = 0; i < outputs.size(); i++) {
    auto& t = outputs[i];
    // NOLINTNEXTLINE(cppcoreguidelines-init-variables)
    uint32_t rank;
    check_nnapi->Execution_getOutputOperandRank(execution, i, &rank);
    std::vector<uint32_t> dims(rank);
    check_nnapi->Execution_getOutputOperandDimensions(execution, i, dims.data());
    std::vector<int64_t> long_dims(dims.begin(), dims.end());
    // TODO: Maybe check that only the batch dimension is changed?
    t.resize_(long_dims);
  }
}

void NnapiCompilation::get_operand_type(const at::Tensor& t, ANeuralNetworksOperandType* operand, std::vector<uint32_t>* dims) {
  operand->dimensionCount = t.dim();
  TORCH_CHECK(operand->dimensionCount == t.dim()); // Check for overflow.
  dims->resize(t.dim());
  operand->dimensions = dims->data();
  for (size_t i = 0; i < dims->size(); i++) {
    (*dims)[i] = t.sizes()[i];
    TORCH_CHECK((*dims)[i] == t.sizes()[i]); // Check for overflow.
  }
  if (t.scalar_type() == c10::kFloat) {
    operand->type = ANEURALNETWORKS_TENSOR_FLOAT32;
    operand->scale = 0;
    operand->zeroPoint = 0;
    return;
  }
  if (t.scalar_type() == c10::kQUInt8) {
    TORCH_CHECK(t.is_quantized());
    operand->type = ANEURALNETWORKS_TENSOR_QUANT8_ASYMM;
    // NOLINTNEXTLINE(cppcoreguidelines-narrowing-conversions,bugprone-narrowing-conversions)
    operand->scale = t.q_scale();
    operand->zeroPoint = t.q_zero_point();
    return;
  }
  if (t.scalar_type() == c10::kInt) {
    operand->type = ANEURALNETWORKS_TENSOR_INT32;
    operand->scale = 0;
    operand->zeroPoint = 0;
    return;
  }
  // TODO: Support more dtypes.
  CAFFE_THROW("Bad dtype");
}

} // namespace bind
} // namespace nnapi
} // namespace torch
