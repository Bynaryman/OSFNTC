#pragma once

#include <torch/csrc/WindowsTorchApiMacro.h>

#include <torch/csrc/jit/codegen/cuda/compute_at_map.h>
#include <torch/csrc/jit/codegen/cuda/index_compute.h>
#include <torch/csrc/jit/codegen/cuda/kernel_ir.h>
#include <torch/csrc/jit/codegen/cuda/lower2device.h>
#include <torch/csrc/jit/codegen/cuda/reference_tensor.h>

#include <vector>

namespace torch {
namespace jit {
namespace fuser {
namespace cuda {

class IndexReferenceReplay : public OptInDispatch {
 private:
  IndexReferenceReplay(const std::vector<kir::ForLoop*>& loop_structure)
      : loop_structure_(loop_structure),
        ca_map_(GpuLower::current()->caIndexMap()) {}

  // Generate the replay.
  TensorDomain* computeReplay();

  // Given a concrete_id return the reference id associated with it, or generate
  // one to associate with it.
  IterDomain* concreteToRefId(IterDomain* concrete_id);

  // Given a reference id return the concrete id associated with it.
  IterDomain* refIdToConcrete(IterDomain* ref_id);

  // Make a new id for the reference replay based on the provided id
  IterDomain* idCopy(IterDomain* id);

  // Use the compute at map to get the fusion IterDomain from the
  // kir::IterDomain
  IterDomain* toFusionID(kir::IterDomain* kir_id);

  // Return the concrete entry of the non-reference id
  IterDomain* toConcrete(IterDomain* id);

  using OptInDispatch::handle;

  void handle(Split* split) override;
  void handle(Merge* merge) override;
  void handle(Expr* e) override;

 private:
  // Hold the loop structure we're generating a reference for.
  const std::vector<kir::ForLoop*>& loop_structure_;

  // Hold the compute at map used for the replay (index map)
  const ComputeAtMap& ca_map_;

  // Keep a vector of all iteration domains used in the reference (includes all
  // transformations)
  std::vector<IterDomain*> replayed_ids_;

  // Maps from reference and concrete id's in the compute at map.
  std::unordered_map<IterDomain*, IterDomain*> ref_id_to_concrete_;
  std::unordered_map<IterDomain*, IterDomain*> concrete_to_ref_id_;

  // Keep track of which reference id's were used as an input into a
  // transformation during replay
  std::unordered_set<IterDomain*> ref_id_consumed_;

  // Keep track of which reference id's were used as an output of a
  // transformation during replay
  std::unordered_set<IterDomain*> ref_id_produced_;

 public:
  // Generate the reference of the provided loop nest structure
  static ReferenceTensor getReference(
      const std::vector<kir::ForLoop*>& loop_structure) {
    auto replay = IndexReferenceReplay(loop_structure);
    ReferenceTensor ref;
    ref.domain = replay.computeReplay();
    ref.concrete_to_id = replay.concrete_to_ref_id_;
    ref.id_to_concrete = replay.ref_id_to_concrete_;
    return ref;
  }
};

// Index into the reference based on the provided index map.
IndexCompute getReferenceIndexing(
    const std::vector<kir::ForLoop*>& loop_structure,
    TensorDomain* reference_domain,
    std::unordered_map<kir::IterDomain*, kir::Val*> index_map,
    std::unordered_set<kir::IterDomain*> zero_domains,
    std::unordered_set<IterDomain*> preferred_path,
    std::unordered_map<kir::IterDomain*, kir::Val*> halo_extent_map = {});

// Short cut for global TVs. Index into the reference based on all loop indicies
// in the loop structure.
IndexCompute getReferenceIndexing(
    const std::vector<kir::ForLoop*>& loop_structure,
    TensorDomain* reference_domain);

// When indexing there are sometimes an option to propagate an index down
// multiple paths. This will return the IterDomains in the history of the
// reference domain and mark which paths should be taken (if there's a
// preference) to reach the roots provided in preferred_roots.
std::unordered_set<IterDomain*> buildPreferredPaths(
    TensorDomain* reference_domain,
    const std::unordered_set<IterDomain*>& preferred_roots);

} // namespace cuda
} // namespace fuser
} // namespace jit
} // namespace torch
