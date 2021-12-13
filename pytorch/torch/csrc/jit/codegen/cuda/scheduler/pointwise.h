#pragma once

#include <ATen/core/ivalue.h>

#include <torch/csrc/jit/codegen/cuda/fusion.h>
#include <torch/csrc/jit/codegen/cuda/scheduler/pointwise_heuristic.h>

namespace torch {
namespace jit {
namespace fuser {
namespace cuda {

class SchedulerRuntimeInfo;
class HeuristicSummary;

TORCH_CUDA_CU_API c10::optional<PointwiseParams> getPointwiseHeuristics(
    Fusion* fusion,
    const at::ArrayRef<c10::IValue>& runtime_inputs,
    HeuristicSummary* data_cache = nullptr);

TORCH_CUDA_CU_API c10::optional<PointwiseParams> getPointwiseHeuristics(
    Fusion* fusion,
    SchedulerRuntimeInfo& runtime_info,
    HeuristicSummary* data_cache = nullptr);

TORCH_CUDA_CU_API void schedulePointwise(
    Fusion* fusion,
    const PointwiseParams& params);

TORCH_CUDA_CU_API LaunchParams schedulePointwise(
    Fusion* fusion,
    const at::ArrayRef<c10::IValue>& runtime_inputs);

} // namespace cuda
} // namespace fuser
} // namespace jit
} // namespace torch
