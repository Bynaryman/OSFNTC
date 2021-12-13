#include <ATen/native/vulkan/ops/Common.h>

namespace at {
namespace native {
namespace vulkan {
namespace ops {

uint32_t batch_size(const Tensor& tensor) {
  const IntArrayRef sizes = tensor.sizes();
  const uint32_t dims = sizes.size();
  if (dims < 4) {
    return 1;
  }
  return sizes[dims - 4];
}

uint32_t channels_size(const Tensor& tensor) {
  const IntArrayRef sizes = tensor.sizes();
  const uint32_t dims = sizes.size();
  if (dims < 3) {
    return 1;
  }
  return sizes[dims - 3];
}

uint32_t height_size(const Tensor& tensor) {
  const IntArrayRef sizes = tensor.sizes();
  const uint32_t dims = sizes.size();
  if (dims < 2) {
    return 1;
  }
  return sizes[dims - 2];
}

uint32_t width_size(const Tensor& tensor) {
  const IntArrayRef sizes = tensor.sizes();
  const uint32_t dims = sizes.size();
  if (dims < 1) {
    return 1;
  }
  return sizes[dims - 1];
}

api::Shader::WorkGroup adaptive_work_group_size(const api::Shader::WorkGroup& global_work_group) {
  api::Shader::WorkGroup local_group_size = {4, 4, 4};
  if (global_work_group.data[2u] == 1) {
    if (global_work_group.data[1u] < 8) {
      local_group_size.data[0u] = 16;
      local_group_size.data[1u] = 4;
      local_group_size.data[2u] = 1;
    }
    else {
      local_group_size.data[0u] = 8;
      local_group_size.data[1u] = 8;
      local_group_size.data[2u] = 1;
    }
  }
  return local_group_size;
}

} // namespace ops
} // namespace vulkan
} // namespace native
} // namespace at
