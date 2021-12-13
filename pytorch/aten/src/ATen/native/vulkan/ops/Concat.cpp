#include <ATen/native/vulkan/api/Helper.h>
#include <ATen/native/vulkan/ops/Common.h>
#include <torch/library.h>

namespace at {
namespace native {
namespace vulkan {
namespace ops {
namespace {

using namespace api::utils;

namespace {
inline int64_t normalize_dim(int64_t d, int64_t n) {
  return (d % n + n) % n;
}
} // namespace

Tensor cat_batch(const TensorList tensors, vTensor& v_output) {
  TORCH_CHECK(false, "Vulkan cat not implemented for batch dimension!");
}

Tensor cat_feature(const TensorList tensors, vTensor& v_output) {
  api::Context* const context = api::context();
  api::Command::Pool& command_pool = context->command().pool;
  api::Command::Buffer& command_buffer = command_pool.stream();

  int64_t ch_size_allprior = 0;
  int64_t ch_interval = 0;
  for (const auto& tensor : tensors) {
    ch_interval += tensor.sizes()[1];
  }

  auto dst_image = v_output.image(
    command_buffer,
    vTensor::Stage::Compute,
    vTensor::Access::Read | vTensor::Access::Write);

  for (const auto& tensor : tensors) {
    const Tensor self = tensor.is_vulkan() ? tensor : tensor.vulkan();
    const vTensor& v_self = convert(self);
    if C10_LIKELY(v_output.has_image() && v_self.has_image()) {
      auto src_image = v_self.image(
              command_buffer,
              vTensor::Stage::Compute);

      const struct Block final {
        uvec3 size;                // output texture size
        uint32_t fill_0;           // dummy
        uvec3 isize;               // input texture size
        uint32_t fill_1;           // dummy
        uint32_t batch_size;       // input tensor's batch size
        uint32_t ch_size;          // input tensor's channel size
        uint32_t ch_interval;      // channel interval (total # of channels for all tensors)
        uint32_t ch_size_allprior; // # of channels for tensor 0 to i-1 at ith tensor
      } block {
        v_output.extents(),
        0u,
        v_self.extents(),
        0u,
        safe_downcast<uint32_t>(v_self.sizes()[0]),
        safe_downcast<uint32_t>(v_self.sizes()[1]),
        safe_downcast<uint32_t>(ch_interval),
        safe_downcast<uint32_t>(ch_size_allprior),
      };

      ch_size_allprior += v_self.sizes()[1];

      context->dispatch(
          command_buffer,
          {
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
          },
          VK_KERNEL(cat_feature),
          v_self.extents(),
          context->gpu().adapter->local_work_group_size(),
          // Read/Write access bypasses synchronization but inserts appropriate
          // barriers if necessary.
          dst_image,
          // Read-only access is implied on const tensors and triggers an async
          // synchronization if necessary.
          src_image,
          // Object lifetime is managed by the resource pool.
          // It is OK not to keep track of the handle.
          context->resource().pool.uniform(block).object);
    }
    else {
      TORCH_CHECK(false, "Not implemented!");
    }
  }

  command_pool.submit(context->gpu().queue, command_buffer);

  return convert(v_output);
}

Tensor cat_feature_mult4ch(const TensorList tensors, vTensor& v_output) {
  api::Context* const context = api::context();
  api::Command::Pool& command_pool = context->command().pool;
  api::Command::Buffer& command_buffer = command_pool.stream();

  int64_t depth_size_allprior = 0;
  int64_t ch_interval = 0;
  for (const auto& tensor : tensors) {
    ch_interval += tensor.sizes()[1];
  }
  const int64_t depth_interval = ch_interval / 4;

  auto dst_image = v_output.image(
    command_buffer,
    vTensor::Stage::Transfer,
    vTensor::Access::Write);
  uvec3 src_offset{};
  uvec3 dst_offset{};

  for (const auto& tensor : tensors) {
    const Tensor self = tensor.is_vulkan() ? tensor : tensor.vulkan();
    const vTensor& v_self = convert(self);
    if C10_LIKELY(v_output.has_image() && v_self.has_image()) {
      auto src_image = v_self.image(
              command_buffer,
              vTensor::Stage::Transfer);

      const uint32_t depth_slice = safe_downcast<uint32_t>(tensor.sizes()[1] / 4);
      uvec3 copy_extents {v_self.extents().data[0u],
        v_self.extents().data[1u],
        depth_slice};

      for (int b = 0; b < tensor.sizes()[0]; ++b) {
        src_offset.data[2u] = safe_downcast<uint32_t>(depth_slice * b);
        dst_offset.data[2u] = depth_size_allprior + safe_downcast<uint32_t>(depth_interval * b);
        api::helper::copy_texture_to_texture(command_buffer,
          src_image,
          dst_image,
          copy_extents,
          src_offset,
          dst_offset);
      }

      depth_size_allprior += depth_slice;
    }
    else {
      TORCH_CHECK(false, "Not implemented!");
    }
  }

  command_pool.submit(context->gpu().queue, command_buffer);

  return convert(v_output);
}

Tensor cat_width(const TensorList tensors, vTensor& v_output) {
  TORCH_CHECK(false, "Vulkan cat not implemented for width dimension!");
}

Tensor cat_height(const TensorList tensors, vTensor& v_output) {
  api::Context* const context = api::context();
  api::Command::Pool& command_pool = context->command().pool;
  api::Command::Buffer& command_buffer = command_pool.stream();

  auto dst_image = v_output.image(
    command_buffer,
    vTensor::Stage::Transfer,
    vTensor::Access::Write);

  uvec3 src_offset{};
  uvec3 dst_offset{};
  for (const auto& tensor : tensors) {
    const Tensor self = tensor.is_vulkan() ? tensor : tensor.vulkan();
    const vTensor& v_self = convert(self);
    if C10_LIKELY(v_output.has_image() && v_self.has_image()) {
      auto src_image = v_self.image(
              command_buffer,
              vTensor::Stage::Transfer);

      api::helper::copy_texture_to_texture(command_buffer,
        src_image,
        dst_image,
        v_self.extents(),
        src_offset,
        dst_offset);

      // Increment by height
      dst_offset.data[1u] += v_self.extents().data[1u];
    }
    else {
      TORCH_CHECK(false, "Not implemented!");
    }
  }

  command_pool.submit(context->gpu().queue, command_buffer);

  return convert(v_output);
}

Tensor cat(
  const at::TensorList tensors,
  const int64_t dim) {
  const auto norm_dim = normalize_dim(dim, 4);
  TORCH_CHECK(
    tensors.size() > 0,
    "Vulkan cat expects at least one tensor");

  at::Tensor tensor = tensors[0];
  int64_t cat_dim_size = 0;
  bool is_mult4ch = true;

  for (const auto & t : tensors) {
     TORCH_INTERNAL_ASSERT(
      t.dim() == 4, "Vulkan cat expects 4 dimensional inputs");

    if (t.sizes()[1] % 4 != 0) {
      is_mult4ch = false;
    }

    for (int d = 0; d < 4; ++d) {
      if (d == dim) {
        continue;
      }
      TORCH_INTERNAL_ASSERT(
        t.size(d) == tensor.size(d),
        "Vulkan cat inputs must have matching sizes except concatenated dimension");
    }
    cat_dim_size += t.size(dim);
  }

  auto result_size = tensor.sizes().vec();
  result_size[dim] = cat_dim_size;

  vTensor v_output{
    api::context(),
    result_size,
    tensor.options()};

  if (dim == 3) {
    return cat_width(tensors, v_output);
  }
  if (dim == 2) {
    return cat_height(tensors, v_output);
  }
  else if (dim == 1) {
    if (is_mult4ch) {
      return cat_feature_mult4ch(tensors, v_output);
    }
    return cat_feature(tensors, v_output);
  }
  return cat_batch(tensors, v_output);
}

#ifdef USE_VULKAN_API

TORCH_LIBRARY_IMPL(aten, Vulkan, m) {
  m.impl(TORCH_SELECTIVE_NAME("aten::_cat"), TORCH_FN(cat));
}

#endif /* USE_VULKAN_API */

} // namespace
} // namespace ops
} // namespace vulkan
} // namespace native
} // namespace at
