
#include <torch/csrc/jit/codegen/cuda/utils.h>

#include <c10/util/string_view.h>

#include <cstdlib>
#include <iostream>
#include <unordered_map>

namespace torch {
namespace jit {
namespace fuser {
namespace cuda {

namespace {

auto parseDebugDumpOptions() {
  std::unordered_map<DebugDumpOption, bool> options_map = {
      {DebugDumpOption::FusionIr, false},
      {DebugDumpOption::FusionIrMath, false},
      {DebugDumpOption::KernelIr, false},
      {DebugDumpOption::CudaKernel, false},
      {DebugDumpOption::CudaFull, false},
      {DebugDumpOption::CudaToFile, false},
      {DebugDumpOption::LaunchParam, false},
      {DebugDumpOption::FusionSegments, false},
      {DebugDumpOption::PrintRuntimeArgs, false},
      {DebugDumpOption::EffectiveBandwidth, false},
      {DebugDumpOption::FusionSegmentsDrawing, false},
      {DebugDumpOption::PrintPtxasLog, false},
      {DebugDumpOption::BufferReuseInfo, false},
      {DebugDumpOption::SchedulerDebug, false},
      {DebugDumpOption::ParallelDimensions, false},
      {DebugDumpOption::Halo, false}};

  if (const char* dump_options = std::getenv("PYTORCH_NVFUSER_DUMP")) {
    c10::string_view options_view(dump_options);
    while (!options_view.empty()) {
      const auto end_pos = options_view.find_first_of(',');
      const auto token = options_view.substr(0, end_pos);
      if (token == "fusion_ir") {
        options_map[DebugDumpOption::FusionIr] = true;
      } else if (token == "fusion_ir_math") {
        options_map[DebugDumpOption::FusionIrMath] = true;
      } else if (token == "kernel_ir") {
        options_map[DebugDumpOption::KernelIr] = true;
      } else if (token == "cuda_kernel") {
        options_map[DebugDumpOption::CudaKernel] = true;
      } else if (token == "cuda_full") {
        options_map[DebugDumpOption::CudaFull] = true;
      } else if (token == "cuda_to_file") {
        options_map[DebugDumpOption::CudaToFile] = true;
      } else if (token == "launch_param") {
        options_map[DebugDumpOption::LaunchParam] = true;
      } else if (token == "segmented_fusion") {
        options_map[DebugDumpOption::FusionSegments] = true;
      } else if (token == "print_args") {
        options_map[DebugDumpOption::PrintRuntimeArgs] = true;
      } else if (token == "dump_eff_bandwidth") {
        options_map[DebugDumpOption::EffectiveBandwidth] = true;
      } else if (token == "draw_segmented_fusion") {
        options_map[DebugDumpOption::FusionSegmentsDrawing] = true;
      } else if (token == "ptxas_verbose") {
        options_map[DebugDumpOption::PrintPtxasLog] = true;
      } else if (token == "buffer_reuse_verbose") {
        options_map[DebugDumpOption::BufferReuseInfo] = true;
      } else if (token == "scheduler_params") {
        options_map[DebugDumpOption::SchedulerDebug] = true;
      } else if (token == "parallel_dimensions") {
        options_map[DebugDumpOption::ParallelDimensions] = true;
      } else if (token == "halo") {
        options_map[DebugDumpOption::Halo] = true;
      } else {
        TORCH_CHECK(
            false,
            "Invalid debug dump option: '",
            token,
            "'\nAvailable options:\n",
            "\tfusion_ir, fusion_ir_math, kernel_ir, cuda_kernel, cuda_full,\n",
            "\tcuda_to_file, launch_param, segmented_fusion, print_args,\n",
            "\tdump_eff_bandwidth, draw_segmented_fusion, scheduler_params\n",
            "\tparallel_dimensions, buffer_reuse_verbose, ptxas_verbose\n",
            "\thalo\n");
      }
      options_view = (end_pos != c10::string_view::npos)
          ? options_view.substr(end_pos + 1)
          : "";
    }
  }

  return options_map;
}

} // namespace

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
void debugPrint(const c10::TensorTypePtr& type) {
  std::stringstream sizes_s;
  if (auto sizes = type->symbolic_sizes().sizes()) {
    for (const auto& shape_symbol : *sizes) {
      if (shape_symbol.is_static()) {
        sizes_s << shape_symbol.static_size() << ", ";
      } else {
        sizes_s << "s(" << *reinterpret_cast<const int64_t*>(&shape_symbol)
                << "), ";
      }
    }
  } else {
    sizes_s << "no size available";
  }
  std::cout << "sizes:" << sizes_s.str() << std::endl;
  if (const auto& stride_properties = type->stride_properties().sizes()) {
    std::stringstream stride_s;
    std::stringstream index_s;
    std::stringstream contig_s;

    for (const auto& stride_property : *stride_properties) {
      if (stride_property.has_value() && stride_property->stride_.has_value()) {
        stride_s << *stride_property->stride_ << ", ";
      } else {
        stride_s << "?, ";
      }
      if (stride_property.has_value() &&
          stride_property->stride_index_.has_value()) {
        index_s << *stride_property->stride_index_ << ", ";
      } else {
        index_s << "?, ";
      }
      if (stride_property.has_value() &&
          stride_property->contiguous_.has_value()) {
        contig_s << *stride_property->contiguous_ << ", ";
      } else {
        contig_s << "?, ";
      }
    }
    std::cout << "stride: " << stride_s.str() << std::endl;
    std::cout << "stride index: " << index_s.str() << std::endl;
    std::cout << "contiguous: " << contig_s.str() << std::endl;
  } else {
    std::cout << "no stride properties available" << std::endl;
  }
}
#pragma clang diagnostic pop

bool isDebugDumpEnabled(DebugDumpOption option) {
  const static auto dump_options = parseDebugDumpOptions();
  return dump_options.at(option);
}

bool useFallback() {
  const char* disable_fb_env = getenv("PYTORCH_NVFUSER_DISABLE_FALLBACK");
  return !(disable_fb_env ? atoi(disable_fb_env) : false);
}

bool disableRNGUnrolling() {
  const char* disable_rng_unroll = getenv("PYTORCH_NVFUSER_DISABLE_RNG_UNROLL");
  return disable_rng_unroll ? atoi(disable_rng_unroll) : false;
}

} // namespace cuda
} // namespace fuser
} // namespace jit
} // namespace torch
