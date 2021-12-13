#pragma once

#include <torch/csrc/WindowsTorchApiMacro.h>

#include <torch/csrc/jit/codegen/cuda/ir_all_nodes.h>

namespace torch {
namespace jit {
namespace fuser {
namespace cuda {

void validateIr(Fusion* fusion);

void validateVectorize(Fusion* fusion);

//! Validates all tensors are consistently parallelized. Basically,
//! when a producer axis is threaded, either with threadIdx or
//! blockIdx, there must be a mapped consumer axis with the
//! same ParallelType with some exceptions.
//!
//! This function assumes Loop and Parallel ComputeAtMaps are already
//! built as they are used to validate consistency.
void validateParallelize(Fusion* fusion);

//! Validates partial split expressions. Partial split only uses an
//! inner subdomain specified by start and stop offsets, ignoring the
//! values outside the range. It's designed to be used with non-padded
//! shift, which introduces non-zero start and stop smaller than the
//! extent. This function makes sure all tensors have all values
//! calculated that are necessary for output values.
void validatePartialSplit(Fusion* fusion);

//! If a tensor depends on multiple grid reduction outputs, it may not
//! be computed at all unless a single thread block happens hold the
//! valid outputs of all producer tensors.
void validateThreadPredicates(Fusion* fusion);

} // namespace cuda
} // namespace fuser
} // namespace jit
} // namespace torch
