#include <torch/csrc/jit/codegen/cuda/scheduler/reduction.h>

#include <torch/csrc/jit/codegen/cuda/executor_utils.h>
#include <torch/csrc/jit/codegen/cuda/instrumentation.h>
#include <torch/csrc/jit/codegen/cuda/ir_all_nodes.h>
#include <torch/csrc/jit/codegen/cuda/ir_utils.h>
#include <torch/csrc/jit/codegen/cuda/scheduler/reduction_utils.h>
#include <torch/csrc/jit/codegen/cuda/scheduler/registry.h>
#include <torch/csrc/jit/codegen/cuda/scheduler/utils.h>
#include <torch/csrc/jit/codegen/cuda/transform_replay.h>

#include <torch/csrc/jit/codegen/cuda/ir_iostream.h>

#include <ATen/cuda/CUDAContext.h>

namespace torch {
namespace jit {
namespace fuser {
namespace cuda {

namespace {

ReductionParams innerReductionHeuristic(
    const int64_t total_reduction_numel,
    const int64_t total_iteration_numel,
    const int64_t inner_most_dimension_numel,
    const int64_t n_tensor_inputs,
    const int64_t max_input_dtype_size,
    const size_t vectorize_factor) {
  // Set some targets for parallelization

  const int64_t n_elems = total_reduction_numel * total_iteration_numel;

  // WARNING: At some point we may want to generate heuristics for another
  // device that is not the current device.
  const int64_t device_max_threads_per_multiprocessor =
      (int64_t)at::cuda::getCurrentDeviceProperties()
          ->maxThreadsPerMultiProcessor;

  const int64_t device_multiprocessor_count =
      (int64_t)at::cuda::getCurrentDeviceProperties()->multiProcessorCount;

  auto const max_unroll = ceilDiv(
      // Available unrolling based on size of data type
      (int64_t)16 / (int64_t)max_input_dtype_size,
      // Reduce unrolling if we have many inputs, start reduction at 4 inputs
      scheduler_utils::lastPow2(
          std::max((int64_t)n_tensor_inputs >> 2, (int64_t)1)));

  // Conservative value, could be set to larger based on arch if necessary.
  constexpr int64_t l1_cache = 32 * 1024;
  // Could change per generation, but for l1 we want to consider active threads,
  // not resident
  constexpr int64_t active_threads = 1024;

  // if data fits in l2 and we need more parallelization in the reduction dim,
  // we can use a smaller warp size. While thread local data fits in l1, and
  // reduction dim is really small, we can use <32 threads per warp.
  const bool fits_in_l2 = n_elems * max_input_dtype_size * n_tensor_inputs <
      at::cuda::getCurrentDeviceProperties()->l2CacheSize;

  // If it fits in l2, we just want to make sure each warp uses 32Bytes. Set
  // minimum warp as 16 threads instead of 32 as if we have a small reduction
  // dim going a bit smaller than 32 usually helps.
  const int64_t warp_size_based_on_l2 =
      fits_in_l2 ? (int64_t)32 / max_input_dtype_size : 16;

  // Check how many elements it would take per thread to start thrashing l1
  // set that to minimum number we want to reduce per thread.
  const int64_t warp_size_based_on_l1 = std::min(
      ceilDiv(
          total_reduction_numel,
          std::max(
              l1_cache /
                  (n_tensor_inputs * max_input_dtype_size * active_threads),
              (int64_t)1)),
      (int64_t)16);

  // Take the smaller
  const int64_t warp_size =
      std::min(warp_size_based_on_l1, warp_size_based_on_l2);

  // Initialization
  int64_t target_blocks = 1;
  int64_t target_unroll = 1;
  int64_t target_iterations = 1;

  // Try to set a minmum amount of work for each thread, as cross thread
  // communication is slow so it shouldn't be done for every element in the
  // reduction.
  int64_t min_target_iterations =
      std::max((int64_t)32 / (int64_t)max_input_dtype_size, (int64_t)1);

  // Start trying to break parallelization up across threads,
  // unrolling/iterations, and blocks.

  // max_threads_in_block is the cap on a thread block, the minimum is based on
  // warp_size
  int64_t max_threads_in_block = std::max(
      warp_size, ceilDiv(total_reduction_numel, min_target_iterations));

  // If we have one warp per block, check if that's enough to saturate the SMs
  target_blocks = ceilDiv(n_elems, warp_size);

  // If we have more than a wave of blocks, put parallelism into unrolling and
  // target iterations
  if (target_blocks > device_multiprocessor_count) {
    auto available_unroll = std::max(
        n_elems / (warp_size * device_multiprocessor_count), (int64_t)1);

    // Spread across unrolling and iterations, want a balance of the two so flip
    // back and forth to alternate adding to them.
    bool flip = true;

    while (available_unroll > 1 &&
           (target_unroll < max_unroll ||
            // Prefer unrolling
            target_iterations < max_unroll)) {
      if (target_unroll * 2 <= max_unroll && flip) {
        target_unroll *= 2;
      }

      if (target_iterations * 2 <= max_unroll && !flip) {
        target_iterations *= 2;
      }

      available_unroll = std::max(
          n_elems /
              (warp_size * device_multiprocessor_count * target_unroll *
               target_iterations),
          (int64_t)1);

      flip = !flip;
    }

    // Recompute target blocks
    target_blocks =
        ceilDiv(n_elems, warp_size * target_unroll * target_iterations);
  }

  // Cap target blocks to 4 waves
  target_blocks = std::min(target_blocks, device_multiprocessor_count * 4);

  if (target_blocks * target_unroll * target_iterations < n_elems) {
    // targetting 4 waves, so try to use a quarter of available threads
    max_threads_in_block = std::min(
        ceilDiv(n_elems, target_blocks * target_unroll),
        ceilDiv(device_max_threads_per_multiprocessor, (int64_t)4));
  }

  // Round up to nearest warp.
  if (max_threads_in_block % warp_size != 0) {
    max_threads_in_block += warp_size - max_threads_in_block % warp_size;
  }

  // To get to target threads:
  // Prioritize
  // (1) x dim in reduction
  // (2) unrolling in reduction
  // (3) y in output
  // To get target blocks:
  // Prioritize
  // (1) x dim in multiple outputs
  // (2) y dim in multiple reductions

  // Cross grid inner reduction, number of blocks to cross-grid on
  int64_t gridim = 1;
  // Cross grid outer reduction, number of blocks to cross-grid on
  int64_t grodim = 1;
  // Blocks for outputs
  int64_t godim = 1;

  // Threads for reduction
  int64_t bdimx = 1;
  // Threads for outputs
  int64_t bdimy = 1;
  // Threads for outer reduction dimension
  int64_t bdimz = 1;

  // Unroll amount
  int64_t inner_reduction_unroll_factor = 1;
  int64_t outer_reduction_unroll_factor = 1;
  int64_t iter_unroll_factor = 1;

  inner_reduction_unroll_factor =
      vectorize_factor > 1 ? (int64_t)vectorize_factor : 1;

  // Grab what we can out of reduction domain, but don't go over a warp size yet
  bdimx = std::min(
      std::max(
          ceilDiv(inner_most_dimension_numel, inner_reduction_unroll_factor),
          (int64_t)warp_size),
      max_threads_in_block);

  // If we're not just barely covering the dimension, round to a more friendly
  // number
  if (bdimx * inner_reduction_unroll_factor != inner_most_dimension_numel) {
    // Round bdimx down to multiple of warp size or power 2
    if (bdimx < warp_size) {
      bdimx = scheduler_utils::lastPow2(bdimx);
    } else {
      bdimx = bdimx - bdimx % warp_size;
    }
  }

  // Put everything else in bdimy for now
  bdimy = std::max(warp_size / bdimx, (int64_t)1);

  // If 3D fill the rest of the threads into bdimz
  bdimz = std::min(
      std::min(
          std::max(max_threads_in_block / (bdimx * bdimy), (int64_t)1),
          ceilDiv(total_reduction_numel, inner_most_dimension_numel)),
      scheduler_utils::z_block_limit);

  // If 3D doesn't fill out the threads, adjust to add to bdimy
  bdimy = std::max(max_threads_in_block / (bdimx * bdimz), (int64_t)1);

  // If we don't have a full warp and have an unroll factor, move unroll into
  // bdimx
  if (bdimx * bdimy * bdimz < warp_size && inner_reduction_unroll_factor > 1) {
    bdimx = std::min(
        std::max(inner_most_dimension_numel, warp_size), max_threads_in_block);

    inner_reduction_unroll_factor =
        std::min(ceilDiv(inner_most_dimension_numel, bdimx), max_unroll);

    // Readjust bdimy and bdimz
    bdimy = std::max(warp_size / bdimx, (int64_t)1);

    bdimz = std::min(
        std::max(max_threads_in_block / (bdimx * bdimy), (int64_t)1),
        ceilDiv(total_reduction_numel, inner_most_dimension_numel));

    bdimy = std::max(max_threads_in_block / (bdimx * bdimz), (int64_t)1);
  }

  godim = ceilDiv(total_iteration_numel, bdimy);

  bool vectorize = false;

  // Move unrolling factor into vectorization upto vectorization limit.
  if (vectorize_factor > 1 && inner_reduction_unroll_factor > 1) {
    vectorize = true;
    inner_reduction_unroll_factor = std::min(
        scheduler_utils::lastPow2(inner_reduction_unroll_factor),
        (int64_t)vectorize_factor);
  }

  // Attempt to put some unrolling into the outer reduction if inner hasn't
  // taken the max unrolling
  if (inner_reduction_unroll_factor < max_unroll) {
    outer_reduction_unroll_factor = std::min(
        ceilDiv(max_unroll, inner_reduction_unroll_factor),
        ceilDiv(
            ceilDiv(total_reduction_numel, inner_most_dimension_numel), bdimz));
  }

  int64_t remainder_in_reduction = ceilDiv(
      total_reduction_numel,
      bdimx * inner_reduction_unroll_factor * bdimz *
          outer_reduction_unroll_factor * target_iterations);

  int64_t remainder_in_inner_dim = ceilDiv(
      inner_most_dimension_numel,
      bdimx * inner_reduction_unroll_factor * target_iterations);

  // If we haven't gotten to the max_unroll case, try to take it out of the
  // iteration domain
  if (inner_reduction_unroll_factor * outer_reduction_unroll_factor <
      max_unroll) {
    // Don't go over a combined inner/outer unroll of max_unroll
    auto unroll_available = ceilDiv(
        max_unroll,
        inner_reduction_unroll_factor * outer_reduction_unroll_factor);

    if (unroll_available > 1 && godim > 2 * device_multiprocessor_count) {
      unroll_available = std::min(
          unroll_available, ceilDiv(godim, 2 * device_multiprocessor_count));
      iter_unroll_factor = unroll_available;
    }
  }

  godim = ceilDiv(total_iteration_numel, bdimy * iter_unroll_factor);

  // Clang tidy
  constexpr int64_t kEight = 8;
  // Cross grid reduction if we haven't hit our target blocks, and we have manyr
  // reduction elements.
  if ((godim < target_blocks && remainder_in_reduction >= 0) ||
      (remainder_in_reduction >= kEight)) {
    auto grdim = std::min(remainder_in_reduction, bdimx * bdimy * kEight);

    gridim = remainder_in_inner_dim;
    grodim = std::max(grdim / gridim, (int64_t)1);
    grodim = std::max(
        std::min(remainder_in_reduction / remainder_in_inner_dim, grodim),
        (int64_t)1);
  }

  // Try to do some cleanup of ragged waves on device, don't do this if we're
  // trying to do a 3D schedule. godim is a remainder of a split, so can only
  // control gridim
  if (grodim == 1 &&
      // If we have less than 8 waves of blocks
      gridim * godim < device_multiprocessor_count * kEight &&
      // And we don't have an even divisible number of blocks
      (gridim * godim) % device_multiprocessor_count != 0 &&
      // And we have more than one wave
      gridim * godim > device_multiprocessor_count) {
    // round waves down
    auto waves =
        std::max((godim * gridim) / device_multiprocessor_count, (int64_t)1);
    auto new_gridim =
        std::max((waves * device_multiprocessor_count) / godim, (int64_t)1);
    if (
        // If difference is less than 25% of the original gridim
        (new_gridim - gridim) * 4 < gridim &&
        // and difference is less than 25% of the original number of blocks
        ((new_gridim * godim) - (gridim * godim)) * 4 < gridim * godim) {
      gridim = new_gridim;
    }
  }

  if (grodim > 1 || gridim > 1) {
    // Grid reductions do not support unrolling iteration dimension, revert if
    // set.
    if (iter_unroll_factor) {
      iter_unroll_factor = 1;
    }
    // This could mess up parallelization which could be redone, but that would
    // require iterating over this entire function.
  }

  ReductionParams rparams;
  rparams.fastest_dim = true;
  rparams.cross_block_inner_reduce = true;
  rparams.block_dim_inner_reduction = ParallelType::TIDx;
  rparams.cross_grid_inner_reduce = gridim > 1;
  rparams.multiple_reds_per_blk = bdimy > 1;
  bool pad_bdimx = bdimx > 16 &&
      bdimx * bdimy <
          (int64_t)at::cuda::getCurrentDeviceProperties()->maxThreadsPerBlock;
  // If barely just covering reduction dim, don't pad to the next warp
  pad_bdimx = pad_bdimx &&
      bdimx * inner_reduction_unroll_factor != inner_most_dimension_numel;
  rparams.pad_inner_reduction_to_warp = pad_bdimx;

  if (rparams.pad_inner_reduction_to_warp) {
    // Adjust bdimx based on padding
    auto warp_size = (int64_t)at::cuda::getCurrentDeviceProperties()->warpSize;
    bdimx =
        bdimx % warp_size == 0 ? bdimx : bdimx + warp_size - bdimx % warp_size;
  }

  if (inner_reduction_unroll_factor || iter_unroll_factor == 1) {
    rparams.unroll_inner_reduction = true;
    rparams.unroll_factor_inner_reduction = inner_reduction_unroll_factor;
    rparams.vectorize_inner_reduction = vectorize;
  }

  rparams.block_dim_iter_dom = ParallelType::TIDy;
  if (iter_unroll_factor > 1) {
    rparams.unroll_iter_dom = true;
    rparams.unroll_factor_iter_dom = iter_unroll_factor;
  }

  rparams.schedule_3D = total_reduction_numel != inner_most_dimension_numel;
  // Outer reduction domain
  if (rparams.schedule_3D) {
    rparams.cross_grid_outer_reduce = grodim > 1;
    if (bdimz > 1) {
      rparams.block_dim_outer_reduction = ParallelType::TIDz;
      rparams.cross_block_outer_reduce = true;
    }
    rparams.unroll_outer_reduction = outer_reduction_unroll_factor > 1;
    rparams.unroll_factor_outer_reduction = outer_reduction_unroll_factor;
  }

  int64_t gdimx = LaunchParams::UNINITIALIZED_VAL;
  int64_t gdimy = LaunchParams::UNINITIALIZED_VAL;
  int64_t gdimz = LaunchParams::UNINITIALIZED_VAL;

  // If we have a cross grid case we want to have gdimy assigned to godim and
  // gdimx assigned to grdim. Otherwise it's helpful to pull godim into gdimx in
  // case it's larger than gdimy can hold, as not doing so can thrash the cache.

  if (rparams.cross_grid_inner_reduce) {
    rparams.grid_dim_inner_reduction = ParallelType::BIDx;
    gdimx = gridim;
    rparams.split_grid_dim_inner_reduction =
        gdimx > scheduler_utils::x_grid_limit;

    rparams.grid_dim_iter_dom = ParallelType::BIDy;
    gdimy = godim;
    rparams.split_grid_dim_iter_dom = gdimy > scheduler_utils::y_grid_limit;

  } else {
    gdimx = godim;
    rparams.grid_dim_iter_dom = ParallelType::BIDx;
    rparams.split_grid_dim_iter_dom = gdimx > scheduler_utils::x_grid_limit;
  }

  if (rparams.cross_grid_outer_reduce) {
    if (rparams.cross_block_inner_reduce) {
      gdimz = grodim;
      rparams.grid_dim_outer_reduction = ParallelType::BIDz;
    } else {
      gdimy = grodim;
      rparams.grid_dim_outer_reduction = ParallelType::BIDy;
    }
  }

  rparams.lparams = LaunchParams(
      rparams.grid_dim_iter_dom == ParallelType::BIDx
          ? LaunchParams::UNINITIALIZED_VAL
          : gdimx,
      rparams.grid_dim_iter_dom == ParallelType::BIDy
          ? LaunchParams::UNINITIALIZED_VAL
          : gdimy,
      gdimz,
      bdimx,
      bdimy > 1 ? bdimy : LaunchParams::UNINITIALIZED_VAL,
      bdimz > 1 ? bdimz : LaunchParams::UNINITIALIZED_VAL);

  if (isDebugDumpEnabled(DebugDumpOption::SchedulerDebug)) {
    std::cerr << "\n===== Reduction Stats ========\n"
              << "total_reduction_numel: "
              << total_reduction_numel / inner_most_dimension_numel << " * "
              << inner_most_dimension_numel << "\n"
              << "total_iteration_numel: " << total_iteration_numel << "\n"
              << "vectorize_factor: " << vectorize_factor << "\n"
              << "n_tensor_inputs: " << n_tensor_inputs << "\n"
              << "max_input_dtype_size: " << max_input_dtype_size << "\n"
              << "block(" << bdimx << ", " << bdimy << ", " << bdimz << ")"
              << std::endl;
    std::cerr << rparams.toString() << std::endl;
  }

  // If 3d, check if it's supported by the scheduler, otherwise force 1D
  // schedule
  if (rparams.schedule_3D) {
    if (rparams.multiple_reds_per_blk &&
        (rparams.cross_grid_inner_reduce || rparams.cross_grid_outer_reduce)) {
      if (isDebugDumpEnabled(DebugDumpOption::SchedulerDebug)) {
        std::cerr << "\n===== UNSUPPORTED REDUCTION HEURISTIC ========\n";
        std::cerr << rparams.multiple_reds_per_blk << ", "
                  << rparams.unroll_inner_reduction << ", "
                  << rparams.cross_grid_inner_reduce << std::endl;
      }
      return innerReductionHeuristic(
          total_reduction_numel,
          total_iteration_numel,
          total_reduction_numel,
          n_tensor_inputs,
          max_input_dtype_size,
          vectorize_factor);
    }
  }

  return rparams;
}

ReductionParams OuterReductionHeuristic(
    const int64_t total_reduction_numel,
    const int64_t total_iteration_numel,
    const int64_t n_tensor_inputs,
    const int64_t max_input_dtype_size,
    const size_t vectorize_factor) {
  // Set some targets for parallelization

  const int64_t n_elems = total_reduction_numel * total_iteration_numel;
  const int64_t l2_cache_size =
      at::cuda::getCurrentDeviceProperties()->l2CacheSize;

  const int64_t warp_size =
      n_elems * max_input_dtype_size * n_tensor_inputs < l2_cache_size
      ? (int64_t)32 / max_input_dtype_size
      : 32;

  int64_t target_blocks = 1;
  int64_t target_unroll = 1;
  int64_t max_threads_in_block = warp_size;

  // WARNING: Current device for codegen may not be the target device
  const int64_t device_max_threads_per_multiprocessor =
      (int64_t)at::cuda::getCurrentDeviceProperties()
          ->maxThreadsPerMultiProcessor;

  const int64_t device_multiprocessor_count =
      (int64_t)at::cuda::getCurrentDeviceProperties()->multiProcessorCount;

  auto const max_unroll = ceilDiv(
      // Available unrolling based on size of data type
      (int64_t)16 / (int64_t)max_input_dtype_size,
      // Reduce unrolling if we have many inputs, start reduction at 4 inputs
      scheduler_utils::lastPow2(
          std::max((int64_t)n_tensor_inputs >> 2, (int64_t)1)));

  // If we have one warp per block, how many blocks would that be?
  target_blocks = ceilDiv(n_elems, (int64_t)warp_size);

  // If we have more than a wave, put parallelism into unrolling
  if (target_blocks > device_multiprocessor_count) {
    target_unroll = std::min(
        max_unroll, ceilDiv(target_blocks, device_multiprocessor_count));
    target_blocks = ceilDiv(target_blocks, target_unroll);
  }

  // Cap target blocks to 4 waves
  target_blocks = std::min(target_blocks, device_multiprocessor_count * 4);

  if (target_blocks * target_unroll * max_threads_in_block < n_elems) {
    // targetting 4 waves, so try to use a quarter of available threads
    max_threads_in_block = std::min(
        ceilDiv(n_elems, target_blocks * target_unroll),
        ceilDiv(device_max_threads_per_multiprocessor, (int64_t)4));
  }

  // Round up to nearest warp.
  if (max_threads_in_block % warp_size != 0) {
    max_threads_in_block += warp_size - max_threads_in_block % warp_size;
  }

  // To get to target threads:
  // Prioritize
  // (1) x dim in iter domain
  // (2) unrolling in iter domain
  // (3) y in reduction domain
  // To get target blocks:
  // Prioritize
  // (1) x dim in multiple outputs
  // (2) y dim in multiple reductions - need to flip unrolling to reduction
  // domain for this

  // Blocks for reductions
  int64_t gdimy = 1;
  // Blocks for outputs
  int64_t gdimx = 1;

  // Threads for reduction
  int64_t bdimy = 1;
  // Threads for output
  int64_t bdimx = 1;

  // Unroll amount
  int64_t inner_reduction_unroll_factor = 1;
  int64_t iter_unroll_factor = 1;

  // Start bdimx as a warp
  bdimx = std::min(warp_size, total_iteration_numel);

  // If we didn't hit a warp, round down to pow 2
  bdimx = scheduler_utils::lastPow2(bdimx);

  // Prioritie unrolling on iteration domain, maintaining a wave on the device
  if (ceilDiv(total_iteration_numel, bdimx) > 2 * device_multiprocessor_count) {
    iter_unroll_factor =
        std::min(max_unroll, ceilDiv(device_multiprocessor_count, bdimx));
  }

  // If there's 2 waves, continue to fill bdimx
  if (ceilDiv(total_iteration_numel, bdimx * iter_unroll_factor) >=
      2 * device_multiprocessor_count) {
    // Put more into bdimx
    bdimx = std::min(
        // Leave a full wave of blocks
        ceilDiv(
            total_iteration_numel,
            iter_unroll_factor * device_multiprocessor_count),
        // Don't exceed max thread count
        max_threads_in_block);

    // Round bdimx down to power of 2 or multiple of warp
    bdimx = bdimx > warp_size ? bdimx - bdimx % warp_size
                              : scheduler_utils::lastPow2(bdimx);
  }

  // Fill bdimy with left over threads
  bdimy = std::min(
      std::max(max_threads_in_block / bdimx, (int64_t)1),
      total_reduction_numel);

  bool vectorize = false;

  // Move unrolling factor into vectorization upto vectorization limit.
  if (vectorize_factor > 1 && iter_unroll_factor > 1) {
    vectorize = true;
    iter_unroll_factor = std::min(
        scheduler_utils::lastPow2(iter_unroll_factor),
        (int64_t)vectorize_factor);
  }

  // Since this is persistent and registers will have to be used anyways unroll
  // the reduction dim if it's available
  inner_reduction_unroll_factor =
      std::min(max_unroll, ceilDiv(total_reduction_numel, bdimy));

  // Go cross grid
  gdimy = ceilDiv(
      ceilDiv(total_reduction_numel, bdimy * inner_reduction_unroll_factor),
      (int64_t)4);

  gdimx = ceilDiv(total_iteration_numel, bdimx * iter_unroll_factor);

  // Clang tidy
  constexpr int64_t kEight = 8;
  constexpr int64_t kSixteen = 16;
  constexpr int64_t kThirtyTwo = 32;

  if (ceilDiv(total_reduction_numel, bdimy * inner_reduction_unroll_factor) >=
      kThirtyTwo) {
    // Many reduction elements, go cross grid
    int64_t min_gdimy = 1;
    if (gdimy > 1) {
      // already cross grid, don't go below target or what was already set
      min_gdimy = std::min(gdimy, ceilDiv(target_blocks, gdimx));
    }
    gdimy = std::max(
        min_gdimy,
        ceilDiv(
            ceilDiv(
                total_reduction_numel, bdimy * inner_reduction_unroll_factor),
            (int64_t)kSixteen));
    // Don't go too far above number of threads in a block since that's how many
    // threads are available to do final reduction iteration
    // This is good!
    gdimy = std::min(gdimy, bdimx * bdimy * kEight);
  }

  // Try to do some cleanup of ragged waves on device
  if (
      // If we have less than 8 waves of blocks
      gdimy * gdimx < device_multiprocessor_count * kEight &&
      // And we don't have an even divisible number of blocks
      (gdimy * gdimx) % device_multiprocessor_count != 0 &&
      // And we have more than one wave
      gdimy * gdimx > device_multiprocessor_count) {
    // round waves down
    auto waves =
        std::max((gdimx * gdimy) / device_multiprocessor_count, (int64_t)1);
    auto new_gdimy =
        std::max((waves * device_multiprocessor_count) / gdimx, (int64_t)1);
    if (
        // If difference is less than 25% of the original gdimy
        (new_gdimy - gdimy) * 4 < gdimy &&
        // and difference is less than 25% of the original number of blocks
        ((new_gdimy * gdimx) - (gdimy * gdimx)) * 4 < gdimy * gdimx) {
      gdimy = new_gdimy;
    }
  }

  // Cannot unroll with cross grid reductions
  if (gdimy > 1 && iter_unroll_factor > 1) {
    // Readjust the thread bindings, ideally we would repeat the block setup
    // without considering iter domain unrolling, but for now will simplify
    bdimx = std::min(max_threads_in_block, bdimx * iter_unroll_factor);
    // Round bdimx down to power of 2 or multiple of warp
    bdimx = bdimx > warp_size ? bdimx - bdimx % warp_size
                              : scheduler_utils::lastPow2(bdimx);
    // bdimy can only be reduced here from before
    bdimy = std::max(max_threads_in_block / bdimx, (int64_t)1);
    // Reset iteration unroll
    iter_unroll_factor = 1;
  }

  ReductionParams rparams;
  // cross grid implies cross block
  rparams.cross_block_inner_reduce = bdimy > 1 || gdimy > 1;
  rparams.cross_grid_inner_reduce = gdimy > 1;
  rparams.multiple_reds_per_blk = bdimx > 1 || iter_unroll_factor > 1;

  if (rparams.multiple_reds_per_blk) {
    rparams.block_dim_iter_dom = ParallelType::TIDx;
  }

  rparams.grid_dim_iter_dom = ParallelType::BIDx;
  rparams.split_grid_dim_iter_dom = gdimx > scheduler_utils::x_grid_limit;

  if (rparams.cross_grid_inner_reduce) {
    rparams.grid_dim_inner_reduction = ParallelType::BIDy;
    rparams.split_grid_dim_inner_reduction =
        gdimy > scheduler_utils::y_grid_limit;
  }

  if (rparams.cross_block_inner_reduce) {
    if (rparams.block_dim_iter_dom == ParallelType::TIDx) {
      rparams.block_dim_inner_reduction = ParallelType::TIDy;
    } else {
      rparams.block_dim_inner_reduction = ParallelType::TIDx;
    }
  }

  if (inner_reduction_unroll_factor > 1) {
    rparams.unroll_inner_reduction = true;
    rparams.unroll_factor_inner_reduction = inner_reduction_unroll_factor;
  }
  if (iter_unroll_factor > 1) {
    rparams.unroll_iter_dom = true;
    rparams.unroll_factor_iter_dom = iter_unroll_factor;
    rparams.vectorize_iter_dom = vectorize;
  }

  rparams.lparams = LaunchParams(
      LaunchParams::UNINITIALIZED_VAL,
      gdimy,
      LaunchParams::UNINITIALIZED_VAL,
      rparams.multiple_reds_per_blk ? bdimx : bdimy,
      rparams.multiple_reds_per_blk ? bdimy : LaunchParams::UNINITIALIZED_VAL,
      LaunchParams::UNINITIALIZED_VAL);

  if (isDebugDumpEnabled(DebugDumpOption::SchedulerDebug)) {
    std::cerr << "\n===== Reduction Stats ========\n"
              << "total_reduction_numel: " << total_reduction_numel << "\n"
              << "total_iteration_numel: " << total_iteration_numel << "\n"
              << "vectorize_factor: " << vectorize_factor << "\n"
              << "n_tensor_inputs: " << n_tensor_inputs << "\n"
              << "max_input_dtype_size: " << max_input_dtype_size << "\n"
              << "block(" << bdimx << ", " << bdimy << ", 1)" << std::endl;
    std::cerr << rparams.toString() << std::endl;
  }
  return rparams;
}

} // namespace

ReductionParams reductionHeuristic(
    const int64_t total_reduction_numel,
    const int64_t total_iteration_numel,
    const int64_t inner_most_dimension_numel,
    const bool fastest_dim_reduction,
    const size_t n_tensor_inputs,
    const size_t max_input_dtype_size,
    const size_t vectorize_factor) {
  if (fastest_dim_reduction) {
    return innerReductionHeuristic(
        total_reduction_numel,
        total_iteration_numel,
        inner_most_dimension_numel,
        n_tensor_inputs,
        max_input_dtype_size,
        vectorize_factor);
  } else {
    // 3D schedules not enabled for outer reductions
    return OuterReductionHeuristic(
        total_reduction_numel,
        total_iteration_numel,
        n_tensor_inputs,
        max_input_dtype_size,
        vectorize_factor);
  }
}

TORCH_CUDA_CU_API c10::optional<ReductionParams> getReductionHeuristics(
    Fusion* fusion,
    const at::ArrayRef<c10::IValue>& runtime_inputs,
    HeuristicSummary* data_cache) {
  FUSER_PERF_SCOPE("getReductionHeuristics");

  SchedulerRuntimeInfo runtime_info(fusion, runtime_inputs, true);

  return getReductionHeuristics(fusion, runtime_info, data_cache);
}

TORCH_CUDA_CU_API c10::optional<ReductionParams> getReductionHeuristics(
    Fusion* fusion,
    SchedulerRuntimeInfo& runtime_info,
    HeuristicSummary* data_cache) {
  FUSER_PERF_SCOPE("getReductionHeuristics");

  FusionGuard fg(fusion);

  auto reduction_tv_entry =
      HeuristicSummaryEntry<HeuristicCompileTime::ReductionTVs>(
          data_cache, [&fusion]() {
            return std::make_unique<std::vector<TensorView*>>(
                scheduler_utils::getReductionTvs(fusion));
          });

  auto& reduction_tvs = reduction_tv_entry.get();

  TORCH_INTERNAL_ASSERT(
      reduction_tvs.size() >= 1, "Need reduction tensor views to schedule.");

  auto reduction_tv = reduction_tvs[0];

  TORCH_INTERNAL_ASSERT(
      reduction_tv->hasReduction(), "TensorView doesn't have a reduction.");

  const auto red_expr = reduction_tv->definition();

  TORCH_INTERNAL_ASSERT(
      red_expr->getExprType() != c10::nullopt &&
          (red_expr->getExprType().value() == ExprType::ReductionOp ||
           red_expr->getExprType().value() == ExprType::WelfordOp),
      "TensorView doesn't have a reduction.");

  auto tv_inps = ir_utils::filterByType<TensorView>(fusion->inputs());
  TORCH_INTERNAL_ASSERT(
      !tv_inps.empty(),
      "Tried to schedule a fusion with no tensor inputs, currently not supported.");

  auto vectorizable_inputs_outputs_entry =
      HeuristicSummaryEntry<HeuristicCompileTime::VectorizableInputsAndOutputs>(
          data_cache, [&reduction_tv]() {
            return std::make_unique<std::vector<TensorView*>>(
                scheduler_utils::getInputsOutputsWithInnerDim(
                    reduction_tv, true));
          });

  auto& vectorizable_inputs_outputs = vectorizable_inputs_outputs_entry.get();

  auto unrollable_inputs_outputs_entry =
      HeuristicSummaryEntry<HeuristicCompileTime::UnrollableInputsAndOutputs>(
          data_cache, [&reduction_tv]() {
            return std::make_unique<std::vector<TensorView*>>(
                scheduler_utils::getInputsOutputsWithInnerDim(
                    reduction_tv, false));
          });

  auto& unrollable_inputs_outputs = unrollable_inputs_outputs_entry.get();

  TORCH_INTERNAL_ASSERT(unrollable_inputs_outputs.size() > 0);
  // Vectorize as much as we can
  size_t vectorize_factor = std::numeric_limits<size_t>::max();

  for (auto tv : vectorizable_inputs_outputs) {
    const auto tv_vectorize_factor = runtime_info.getVectorizableWidth(tv);
    vectorize_factor = std::min(vectorize_factor, tv_vectorize_factor);
  }

  if (vectorize_factor == std::numeric_limits<size_t>::max()) {
    vectorize_factor = 1;
  }

  // Base max dtype and n_tensor_inputs on tensors that are vectorizable (i.e.
  // share inner dimension with data pattern we're looking at).
  size_t max_dtype_size = 1;
  size_t n_tensor_inputs = 0;
  for (auto tv : unrollable_inputs_outputs) {
    if (!tv->isFusionInput()) {
      continue;
    }
    max_dtype_size =
        std::max(max_dtype_size, dataTypeSize(tv->getDataType().value()));
    n_tensor_inputs++;
  }

  auto properties =
      scheduler_utils::getProperties(fusion, runtime_info, reduction_tv);

  return reductionHeuristic(
      properties.total_reduction_numel,
      properties.total_iteration_numel,
      properties.inner_most_dimension_numel,
      properties.fastest_dim_reduction,
      n_tensor_inputs,
      max_dtype_size,
      vectorize_factor);
}

// fusion is the input IR that will be modified by this function
void scheduleReduction(Fusion* fusion, const ReductionParams& rparams) {
  FUSER_PERF_SCOPE("scheduleReduction");
  FusionGuard fg(fusion);

  bool unroll = rparams.unroll_inner_reduction || rparams.unroll_iter_dom;

  // Cache inputs if unrolled
  auto cached_inputs = scheduler_utils::cacheInputs(fusion, unroll);

  // Cache and fork  outputs
  std::vector<std::pair<TensorView*, TensorView*>> cached_outputs =
      scheduler_utils::cacheAndForkOutputs(fusion, unroll);

  // Make sure we don't have global memory set on intermediate tensors from
  // fusion segmentation
  scheduler_utils::clearMemorySpace(fusion);

  auto reduction_tvs = scheduler_utils::getReductionTvs(fusion);

  TORCH_INTERNAL_ASSERT(reduction_tvs.size());

  auto reduction_tv = reduction_tvs[0];

  auto dim_analysis = scheduler_utils::canonicalDimReduction(
      fusion, reduction_tv, rparams.fastest_dim && rparams.schedule_3D);

  bool has_iter_axis = dim_analysis.first;
  bool has_red_axis = dim_analysis.second;

  TORCH_INTERNAL_ASSERT(
      has_red_axis,
      "Could not find reduction axis in tensor used for reduction scheduler.");

  if (!has_iter_axis) {
    TORCH_INTERNAL_ASSERT(
        rparams.fastest_dim,
        "If all dims are reduction, should be sending it to fastest dim scheduler.");
  }

  TensorView* reference_tv = reduction_scheduler_utils::scheduleReductionTV(
      rparams, reduction_tv, has_iter_axis);

  // Reduction tensor views and rfactor tensor views are setup. Let's finish off
  // the scheduling, particularly inlining and unrolling.
  TORCH_INTERNAL_ASSERT(
      reference_tv != nullptr && reduction_tv != nullptr,
      "Need these two tensor views to finish the scheduling.");
  reduction_scheduler_utils::multiReductionInliner(
      fusion,
      rparams,
      reduction_tv,
      reference_tv,
      reduction_tvs,
      cached_inputs,
      cached_outputs);
}

} // namespace cuda
} // namespace fuser
} // namespace jit
} // namespace torch
