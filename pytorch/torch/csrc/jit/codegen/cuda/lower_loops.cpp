#include <torch/csrc/jit/codegen/cuda/lower_loops.h>

#include <torch/csrc/jit/codegen/cuda/arith.h>
#include <torch/csrc/jit/codegen/cuda/ir_iostream.h>
#include <torch/csrc/jit/codegen/cuda/ir_utils.h>
#include <torch/csrc/jit/codegen/cuda/iter_visitor.h>
#include <torch/csrc/jit/codegen/cuda/kernel_expr_evaluator.h>
#include <torch/csrc/jit/codegen/cuda/kernel_ir_printer.h>
#include <torch/csrc/jit/codegen/cuda/lower2device.h>
#include <torch/csrc/jit/codegen/cuda/lower_utils.h>
#include <torch/csrc/jit/codegen/cuda/transform_replay.h>

#include <algorithm>
#include <deque>
#include <numeric>

namespace torch {
namespace jit {
namespace fuser {
namespace cuda {

std::vector<kir::Expr*> LoopNestGenerator::loweredExprs(
    const std::vector<Expr*>& exprs) {
  FUSER_PERF_SCOPE("GpuLower::Lower::LoopNestGenerator::loweredExprs");
  TORCH_INTERNAL_ASSERT(FusionGuard::getCurFusion() != nullptr);
  LoopNestGenerator generator(exprs);
  return generator.lowered_exprs_;
}

LoopNestGenerator::LoopNestGenerator(const std::vector<Expr*>& exprs) {
  generate(exprs);
}

namespace {

kir::ForLoop* openForHelper(kir::ForLoop* scope, kir::IterDomain* kir_id) {
  const auto gpu_lower = GpuLower::current();
  kir::IrBuilder ir_builder(gpu_lower->kernel());
  auto extent_with_halo = gpu_lower->haloInfo().getExtent(kir_id);
  kir::ForLoop* new_scope = nullptr;
  if (extent_with_halo) {
    // When an axis is extended with halo, unrolling and vectorization
    // are assumed to not be used for now.
    TORCH_INTERNAL_ASSERT(
        kir_id->parallelType() != ParallelType::Unroll &&
        !isParallelTypeVectorize(kir_id->parallelType()));
    // Use the extent that's extended by halo
    new_scope = ir_builder.create<kir::ForLoop>(
        kir_id,
        kir_id->isBroadcast() ? ir_builder.zeroVal()
                              : ir_builder.create<kir::Int>(c10::nullopt),
        nullptr,
        extent_with_halo,
        nullptr,
        false,
        nullptr,
        false);
  } else {
    new_scope = ir_builder.create<kir::ForLoop>(kir_id);
  }
  if (scope != nullptr) {
    scope->body().insert(0, new_scope);
  }
  return new_scope;
}

} // namespace

void LoopNestGenerator::openFor(kir::IterDomain* kir_iter_domain) {
  if (for_loops_.size() > 0) {
    const auto new_scope = openForHelper(for_loops_.back(), kir_iter_domain);
    // for_loop_allocations_.insert({new_scope, 0});
    for_loops_.push_back(new_scope);
  } else {
    for_loops_.push_back(openForHelper(nullptr, kir_iter_domain));
    lowered_exprs_.insert(lowered_exprs_.begin(), for_loops_.back());
  }
}

void LoopNestGenerator::closeFor() {
  TORCH_INTERNAL_ASSERT(!for_loops_.empty());
  for_loops_.pop_back();
}

void LoopNestGenerator::pushFront(kir::Expr* expr) {
  if (for_loops_.size() == 0) {
    lowered_exprs_.insert(lowered_exprs_.begin(), expr);
  } else {
    for_loops_.back()->body().insert(0, expr);
  }
}

void LoopNestGenerator::handle(Expr* expr) {
  const auto gpu_lower = GpuLower::current();
  kir::IrBuilder ir_builder(gpu_lower->kernel());

  // Check if it's a tensor view expression we need to place in the loop nest
  // structure
  if (!ir_utils::isTVOp(expr)) {
    // Close all the loops, scalar operations cannot be inside for loops based
    // on expr sorting.
    while (!for_loops_.empty()) {
      closeFor();
    }
    pushFront(gpu_lower->lowerExpr(expr));

    for (auto out : expr->outputs()) {
      TORCH_INTERNAL_ASSERT(
          out->getValType().value() == ValType::Scalar,
          "Unrecognized output type found in expr ",
          expr,
          " cannot lower ",
          out->getValType().value());

      pushFront(ir_builder.create<kir::Allocate>(
          gpu_lower->lowerValue(out),
          MemoryType::Local,
          ir_builder.create<kir::Int>(1)));
    }
    return;
  }

  TensorView* out_tv = expr->output(0)->as<TensorView>();

  // Grab the loop structure
  TORCH_INTERNAL_ASSERT(
      loop_structures_.find(out_tv) != loop_structures_.end(),
      "Could not find loop structure of ",
      out_tv);

  // Figure out what the entire loop structure should look like.
  std::vector<IterDomain*> loop_structure = loop_structures_.at(out_tv);
  std::vector<kir::IterDomain*> kir_loop_structure;

  std::transform(
      loop_structure.begin(),
      loop_structure.end(),
      std::back_inserter(kir_loop_structure),
      [&gpu_lower](IterDomain* id) {
        return gpu_lower->lowerValue(id)->as<kir::IterDomain>();
      });
  // Ordering of loop_structure is global, so simply close loops we don't need,
  // and open the ones we do.

  while (!for_loops_.empty() &&
         std::find(
             kir_loop_structure.begin(),
             kir_loop_structure.end(),
             for_loops_.back()->iter_domain()) == kir_loop_structure.end()) {
    closeFor();
  }

  for (auto loop : kir_loop_structure) {
    auto find_it = std::find_if(
        for_loops_.begin(), for_loops_.end(), [loop](kir::ForLoop* fl) {
          return fl->iter_domain() == loop;
        });
    if (find_it == for_loops_.end()) {
      openFor(loop);
    }
  }

  pushFront(gpu_lower->lowerExpr(expr));
}

namespace {
// Copied verbatim from lower_expr_sort EXCEPT map is parallel map, not loop
// map, and direction is reversed
struct LocalDomainSorter {
  LocalDomainSorter(
      const std::unordered_map<IterDomain*, std::unordered_set<IterDomain*>>&
          concrete_id_dependencies)
      : concrete_id_dependencies_(concrete_id_dependencies) {}

  // Return if id0 should be before id1
  inline bool operator()(IterDomain* id0, IterDomain* id1) {
    auto concrete_id_0 =
        GpuLower::current()->caParallelMap().getConcreteMappedID(id0);
    auto concrete_id_1 =
        GpuLower::current()->caParallelMap().getConcreteMappedID(id1);

    if (concrete_id_dependencies_.find(concrete_id_0) !=
        concrete_id_dependencies_.end()) {
      const auto& dependencies_0 = concrete_id_dependencies_.at(concrete_id_0);
      // if id0 depends on id1 it means id1 is outside id0, so id1 < id0
      return !dependencies_0.count(concrete_id_1);
    }

    if (concrete_id_dependencies_.find(concrete_id_1) !=
        concrete_id_dependencies_.end()) {
      const auto& dependencies_1 = concrete_id_dependencies_.at(concrete_id_1);
      // if id1 depends on id0 it means id1 is inside id0, so id0 < id1
      return dependencies_1.count(concrete_id_0);
    }

    return true;
  }

  const std::unordered_map<IterDomain*, std::unordered_set<IterDomain*>>&
      concrete_id_dependencies_;
};
} // namespace

// Generate the loop nest structure and place it in lowered_exprs_
void LoopNestGenerator::generate(const std::vector<Expr*>& exprs) {
  TORCH_INTERNAL_ASSERT(lowered_exprs_.empty());

  // Figure out loop structure of each expression. This can be a bit convoluted,
  // for an example why see FusionAdvancedLowering6

  // Grab iteration domain dependencies, similar to the logic in
  // lower_expr_sort, EXCEPT it is based on parallel map not loop map, and
  // dependencies are in opposite order, inner loops are dependant on outer
  // loops.

  const auto& parallel_map = GpuLower::current()->caParallelMap();

  std::unordered_map<IterDomain*, std::unordered_set<IterDomain*>>
      concrete_id_dependencies;
  for (auto tv : ir_utils::allTvs(FusionGuard::getCurFusion())) {
    std::unordered_set<IterDomain*> dependencies;

    for (auto tv_id : tv->domain()->domain()) {
      auto concrete_id = parallel_map.getConcreteMappedID(tv_id);

      if (concrete_id_dependencies.find(concrete_id) ==
          concrete_id_dependencies.end()) {
        concrete_id_dependencies[concrete_id] = dependencies;
      } else {
        concrete_id_dependencies[concrete_id].insert(
            dependencies.begin(), dependencies.end());
      }

      // Loops after tv_id are dependent on tv_id
      dependencies.emplace(parallel_map.getConcreteMappedID(tv_id));
    }
  }

  // Generate loop structure for each tensor view
  for (auto tv : ir_utils::allTvs(FusionGuard::getCurFusion())) {
    // Zero dim tensor support
    if (tv->nDims() == 0) {
      loop_structures_[tv] = std::vector<IterDomain*>();
      continue;
    }

    auto last_id_concrete =
        parallel_map.getConcreteMappedID(tv->axis((int)(tv->nDims() - 1)));
    auto all_loops_it = concrete_id_dependencies.find(last_id_concrete);
    TORCH_INTERNAL_ASSERT(
        all_loops_it != concrete_id_dependencies.end(),
        "Should have processed all id's in all tvs.");
    std::vector<IterDomain*> loop_structure(
        all_loops_it->second.begin(), all_loops_it->second.end());
    // Dependencies of last domain doesn't include last domain, include it
    // manually
    loop_structure.emplace_back(last_id_concrete);
    std::sort(
        loop_structure.begin(),
        loop_structure.end(),
        LocalDomainSorter(concrete_id_dependencies));
    loop_structures_[tv] = loop_structure;
  }

  // Process the carefully ordered expressions
  for (auto it = exprs.rbegin(); it != exprs.rend(); ++it) {
    handle(*it);
  }
}

} // namespace cuda
} // namespace fuser
} // namespace jit
} // namespace torch
