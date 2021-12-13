#pragma once
#include <torch/csrc/jit/codegen/cuda/executor_launch_params.h>
#include <torch/csrc/jit/codegen/cuda/executor_utils.h>
#include <torch/csrc/jit/codegen/cuda/fusion.h>
#include <torch/csrc/jit/codegen/cuda/ir_all_nodes.h>
#include <torch/csrc/jit/codegen/cuda/ir_cloner.h>
#include <torch/csrc/jit/codegen/cuda/ir_printer.h>
#include <torch/csrc/jit/codegen/cuda/kernel_expr_evaluator.h>
#include <torch/csrc/jit/codegen/cuda/lower2device.h>
#include <torch/csrc/jit/codegen/cuda/utils.h>

#include <c10/core/DeviceType.h>

namespace torch {
namespace jit {
namespace fuser {
namespace cuda {

// TODO: Should this actually be in launch params?
struct TORCH_CUDA_CU_API CompileOptions {
  c10::Device device = c10::Device(c10::DeviceType::CUDA, 0);
  KernelIndexMode index_mode = KernelIndexMode::INT64;
};

class TORCH_CUDA_CU_API FusionExecutor : public NonCopyable {
 public:
  // Unsafe compilation that's useful for debugging kernels, iterating over
  // slight modifications of a generated kernel
  void debugCompileFusionFromStr(
      Fusion* fusion,
      const std::string& code,
      const std::string& name,
      int id,
      CompileOptions options = CompileOptions());

  void compileFusion(
      Fusion* fusion,
      CompileOptions options = CompileOptions(),
      const at::ArrayRef<IValue>& inputs = {},
      const LaunchParams& launch_constraints = LaunchParams());

  std::vector<at::Tensor> runFusion(
      const at::ArrayRef<IValue>& inputs,
      const std::vector<at::Tensor>& outputs,
      const LaunchParams& launch_constraints = LaunchParams(),
      const c10::optional<size_t>& opt_code = c10::nullopt);

  std::vector<at::Tensor> runFusion(
      const at::ArrayRef<IValue>& inputs,
      const LaunchParams& launch_constraints = LaunchParams(),
      const c10::optional<size_t>& opt_code = c10::nullopt) {
    return runFusion(inputs, {}, launch_constraints, opt_code);
  }

  // function to query whether a `FusionExecutor` has a compiled kernel to
  // execute
  bool compiled() const {
    return fusion_id_ != -1;
  };

  void evictCache(size_t cache_id) {
    executor_entry_lookup_.erase(cache_id);
  }

  // struct used to hold necessary information to launch compiled kernel on a
  // given input set.
  //
  // TODO: strides would also be important when we handle permutations in
  //       codegen.
  //
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
  struct ExecutorEntry {
    bool init = false;
    LaunchParams launch_params;
    std::vector<std::pair<int, int>> io_alias_indices;
    std::vector<std::vector<int64_t>> output_sizes;
    std::vector<at::ScalarType> output_types;
    std::vector<std::vector<int64_t>> buffer_sizes;
    std::vector<at::ScalarType> buffer_types;
    std::vector<bool> buffer_zero_init;
    uint64_t rand_offset;
  };

  using ExecutorCompileTimeInfoCache =
      executor_utils::caching::ExecutorCompileTimeInfoCache;

  kir::Kernel* kernel() const {
    return lowered_.kernel();
  }

  //! Internal knob used for debugging/profiling only
  void setExecuteKernelFlag(bool execute_kernel) {
    execute_kernel_ = execute_kernel;
  }

  //! Internal knob used for debugging/profiling only
  void setMeasureKernelTimeFlag(bool measure_kernel_time) {
    measure_kernel_time_ = measure_kernel_time;
  }

  //! Returns the last kernel execution time, in milliseconds
  //!
  //! \note The kernel time is only tracked if enabled by calling
  //!    setMeasureKernelTimeFlag(true)
  //!
  float kernelTimeMs() const {
    return measure_kernel_time_ ? kernel_time_ms_ : 0;
  }

  //! Internal tests only. Compiles CUDA code with NVRTC directly from
  //! string. This util provides a path to test runtime code, i.e. the resource
  //! strings.
  void compileRtc(
      const std::string& code,
      const std::string& name,
      bool structured = false);

  //! Internal tests only. Runs the compiled CUDA kernel from compileRtc.
  void runRtc(
      const LaunchParams& launch_params,
      const std::vector<at::Tensor>& args);

  //! Internal knob used for debugging/profiling only
  void disableLaunchParamCache() {
    disable_parameter_cache_ = true;
  }

 private:
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
  struct GlobalBuffers {
    std::vector<at::Tensor> buffers;
    std::vector<bool> zero_init;
  };

  std::string kernelName() const {
    std::stringstream ss;
    ss << "kernel" << fusion_id_;
    return ss.str();
  }

  static std::string kernelNamespace() {
    return "CudaCodeGen";
  }

  // Add preamble and wrap in namespace
  std::string getStructuredCode(const std::string& kernel);

  LaunchParams computeLaunchParams(
      const LaunchParams& launch_constraints,
      kir::ExpressionEvaluator& expr_eval,
      const int warp_size);

  uint64_t computeSharedMemory(
      kir::ExpressionEvaluator& expr_eval,
      const std::vector<const kir::Allocate*>& buffers,
      bool align_padding = false,
      uint64_t total = 0);

  // return a pair of vector of tensors, where tensors in the first vector are
  // not initialized, while the second vector contains zero-initiliazed tensors
  GlobalBuffers allocGlobalVals(kir::ExpressionEvaluator& expr_eval);

  // alias_index: index of outputs that are aliases to inputs, hence we should
  // skip allocating real storage for those, but still maintain its spot to
  // maintain the indexing from output aliases to inputs
  std::vector<at::Tensor> allocOutputs(
      kir::ExpressionEvaluator& expr_eval,
      const std::unordered_set<int>& alias_indices = {});

  void setUsedTVs();

  const std::vector<TensorView*>& getUsedTVs() const {
    return used_tvs_;
  };

  ExecutorCompileTimeInfoCache* compileTimeDataCache() {
    return &compile_time_info_cache_;
  }

 private:
  Fusion fusion_;

  CompileOptions options_;
  size_t max_device_smem = std::numeric_limits<size_t>().max();
  int warp_size_ = 0;
  executor_utils::NvrtcFunction compiled_kernel_;

  // TensorViews actually used in the kernel.
  std::vector<TensorView*> used_tvs_;

  // Counter to be used for kernel name.
  int fusion_id_ = -1;
  static int fusion_id_counter_;

  GpuLower lowered_;

  // lookup table to take short cut to retrieve recorded information in order to
  // launch kernels without re-inference parameters.
  std::unordered_map<size_t, ExecutorEntry> executor_entry_lookup_;

  // Profiling support: knob to control wheter we actually execute the
  // kernel on the GPU or not
  bool execute_kernel_ = true;

  // Profiling support: knob to enable measuring kernel execution time
  bool measure_kernel_time_ = false;

  // The last kernel execution time, if measure_kernel_time_ is true
  float kernel_time_ms_ = 0;

  // Profiling support: knob to disable caching of launch params
  bool disable_parameter_cache_ = false;

  // Compile time information caching. This is used for shape inference
  //  support. The cache stores graph information that are available
  //  without shape information so that each shape inference call will
  //  not need to re-compute them.
  ExecutorCompileTimeInfoCache compile_time_info_cache_;

  // Cached expr eval
  std::unique_ptr<KernelPrecomputedIntegers> evaluator_precomputed_integers_ =
      nullptr;
};

} // namespace cuda
} // namespace fuser
} // namespace jit
} // namespace torch
