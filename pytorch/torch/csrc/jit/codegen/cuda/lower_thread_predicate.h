
#pragma once

#include <torch/csrc/WindowsTorchApiMacro.h>

#include <torch/csrc/jit/codegen/cuda/ir_all_nodes.h>
#include <torch/csrc/jit/codegen/cuda/lower_utils.h>
#include <torch/csrc/jit/codegen/cuda/parallel_type_bitmap.h>

#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace torch {
namespace jit {
namespace fuser {
namespace cuda {

//! Maps TensorViews to a { ParallelTypeBitmap, SourceMap } pair
//!
//! Map from TensorView to bit set represnting <BIDx, BIDy, BIDz, TIDx, TIDy,
//! TIDz> If any dependency of TV had a parallelized reduction, we will track
//! it here. This will be used for predicate generation to prevent
//! parallelization on that axis. This is important if we have a reduction on
//! for example TIDx, as the reduced value is only valid on threadIdx.x == 0
//! therefore if we use that value later in the kernel we have that predicate.
//! If we follow a reduction parallelized on TIDx with a broadcast on TIDx we
//! no longer need the predicate and can reset the bit accordingly
//!
//! In addition, if a parallel thread type is not used, it is
//! redundant to use all threads/blocks. That isn't a problem
//! generally although it can be inefficient, but when an aliased smem
//! buffer is used as an output, redundant writes can be invalid (see issue
//! #1110). PredicateInfo::redundant_types track which parallel types
//! are redundant for each tensor and is used to let only one
//! thread/block of a redundant type execute the expression for a
//! tensor.
class TORCH_CUDA_CU_API ThreadPredicateMap {
 public:
  using SourceMap = std::unordered_map<
      ParallelType,
      std::unordered_set<const TensorView*>,
      TypeHash>;

  //! Thread predicate information for each tensor
  struct PredicateInfo {
    // Parallel types where only one thread/block is valid.
    ParallelTypeBitmap limited_types;
    // Source tensors to grid reductions.
    SourceMap source_map;
    // Parallel types where only one thread/block is enough.
    ParallelTypeBitmap redundant_types;
  };

  using MapType = std::unordered_map<const TensorView*, PredicateInfo>;

  using const_iterator = MapType::const_iterator;

  //! Build a map from each tensor to PredicateInfo.
  void build(Fusion* fusion);

  //! Get a PredicateInfo for a given tensor. If it's an output of
  //! a parallel broadcast, unmask the limited_types_ bit of the
  //! corresponding parallel type since it must join the broadcast
  //! operation although the valid input is only available at one of
  //! the threads/blocks.
  PredicateInfo getPredicateInfo(const TensorView* tv) const;

  //! Returns a flag set that indicates which parallel types should be
  //! predicated.
  ParallelTypeBitmap getPredicatedParallelTypes(const TensorView* tv) const;

  //! Returns a Bool predicate for a given TensorView.
  kir::Bool* getPredicate(const TensorView* tv) const;

  //! Returns a ParallelTypeBitmap representing which domain needs
  //! blockBroadcast.
  //!
  //! Even when a domain is broadcast and parallelized, it does not need
  //! blockBroadcast unless it is predicated by limited_types_
  ParallelTypeBitmap getParallelBroadcastDomains(const TensorView* tv) const;

  void print() const;

  //! Generate a Bool value from PredicateInfo.
  static kir::Bool* getPredicateFromPredicateInfo(
      const ThreadPredicateMap::PredicateInfo& pred_info);

 private:
  // Update the thread_predicates bitset based on provided Expr
  void updateBitSet(const Expr*);

  const_iterator find(const TensorView* tv) const;
  const_iterator end() const;

  const PredicateInfo& at(const TensorView* tv) const;
  PredicateInfo& at(const TensorView* tv);

  //! Insert a new mapping
  void insert(
      const TensorView* tv,
      const ParallelTypeBitmap& valid_types,
      const SourceMap& src_map,
      const ParallelTypeBitmap& redundant_types);

  //! Insert a new mapping
  void insert(const TensorView* tv, const PredicateInfo& pred_and_src);

 private:
  MapType thread_predicates_;
};

} // namespace cuda
} // namespace fuser
} // namespace jit
} // namespace torch
