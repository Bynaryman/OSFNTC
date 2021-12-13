#include <torch/csrc/jit/codegen/cuda/ir_utils.h>
#include <torch/csrc/jit/codegen/cuda/lower2device.h>
#include <torch/csrc/jit/codegen/cuda/partial_split_map.h>

namespace torch {
namespace jit {
namespace fuser {
namespace cuda {

void PartialSplitMap::build(Fusion* fusion) {
  const auto gpu_lower = GpuLower::current();
  auto used_vals = ir_utils::allTvs(fusion);

  for (auto tv : ir_utils::filterByType<TensorView>(used_vals)) {
    auto exprs = ExprSort::getExprs(
        fusion, {tv->domain()->domain().begin(), tv->domain()->domain().end()});
    for (auto split : ir_utils::filterByType<Split>(exprs)) {
      // Only needs to check root domains as partial split is only
      // allowed with root domains
      if (std::find(
              tv->getRootDomain().begin(),
              tv->getRootDomain().end(),
              split->in()) == tv->getRootDomain().end()) {
        continue;
      }
      auto root_domain = split->in();
      auto kir_root_domain =
          gpu_lower->lowerValue(split->in())->as<kir::IterDomain>();
      auto start_offset = split->startOffset();
      start_offset_map_.insert({root_domain, start_offset});
      kir_start_offset_map_.insert(
          {kir_root_domain,
           gpu_lower->lowerValue(start_offset)->as<kir::Val>()});
      auto stop_offset = split->stopOffset();
      stop_offset_map_.insert({root_domain, stop_offset});
      kir_stop_offset_map_.insert(
          {kir_root_domain,
           gpu_lower->lowerValue(stop_offset)->as<kir::Val>()});
    }
  }
}

Val* PartialSplitMap::getStartOffset(IterDomain* root_domain) const {
  auto it = start_offset_map_.find(root_domain);
  if (it == start_offset_map_.end()) {
    return nullptr;
  } else {
    return it->second;
  }
}

kir::Val* PartialSplitMap::getStartOffset(kir::IterDomain* root_domain) const {
  auto it = kir_start_offset_map_.find(root_domain);
  if (it == kir_start_offset_map_.end()) {
    return nullptr;
  } else {
    return it->second;
  }
}

Val* PartialSplitMap::getStopOffset(IterDomain* root_domain) const {
  auto it = stop_offset_map_.find(root_domain);
  if (it == stop_offset_map_.end()) {
    return nullptr;
  } else {
    return it->second;
  }
}

kir::Val* PartialSplitMap::getStopOffset(kir::IterDomain* root_domain) const {
  auto it = kir_stop_offset_map_.find(root_domain);
  if (it == kir_stop_offset_map_.end()) {
    return nullptr;
  } else {
    return it->second;
  }
}

} // namespace cuda
} // namespace fuser
} // namespace jit
} // namespace torch
