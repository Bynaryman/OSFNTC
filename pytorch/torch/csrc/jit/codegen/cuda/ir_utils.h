#pragma once

#include <torch/csrc/jit/codegen/cuda/ir_all_nodes.h>
#include <torch/csrc/jit/codegen/cuda/type.h>

#include <iterator>
#include <unordered_map>

namespace torch {
namespace jit {
namespace fuser {
namespace cuda {
namespace ir_utils {

template <typename FilterType, typename Iterator>
class FilterIterator {
 public:
  using iterator_category = std::forward_iterator_tag;
  using difference_type = std::ptrdiff_t;
  using value_type = FilterType*;
  using pointer = value_type*;
  using reference = value_type&;

  FilterIterator(Iterator begin, Iterator end) : current_(begin), end_(end) {
    advance();
  }

  FilterType* operator*() const {
    return (*current_)->template as<FilterType>();
  }

  FilterType* operator->() const {
    return (*this);
  }

  FilterIterator& operator++() {
    ++current_;
    advance();
    return *this;
  }

  FilterIterator operator++(int) {
    const auto before_increment = *this;
    ++current_;
    advance();
    return before_increment;
  }

  bool operator==(const FilterIterator& other) const {
    TORCH_INTERNAL_ASSERT(
        end_ == other.end_,
        "Comparing two FilteredViews that originate from different containers");
    return current_ == other.current_;
  }

  bool operator!=(const FilterIterator& other) const {
    return !(*this == other);
  }

 private:
  void advance() {
    current_ = std::find_if(current_, end_, [](const auto& val) {
      return dynamic_cast<const FilterType*>(val) != nullptr;
    });
  }

 private:
  Iterator current_;
  Iterator end_;
};

// An iterable view to a given container of Val pointers. Only returns
// Vals of a given Val type.
// NOTE: Add a non-const iterator if needed.
template <typename FilterType, typename InputIt>
class FilteredView {
 public:
  using value_type = FilterType*;
  using const_iterator = FilterIterator<FilterType, InputIt>;

  FilteredView(InputIt first, InputIt last) : input_it_(first), last_(last) {}

  const_iterator cbegin() const {
    return const_iterator(input_it_, last_);
  }

  const_iterator begin() const {
    return cbegin();
  }

  const_iterator cend() const {
    return const_iterator(last_, last_);
  }

  const_iterator end() const {
    return cend();
  }

  bool empty() const {
    return begin() == end();
  }

 private:
  const InputIt input_it_;
  const InputIt last_;
};

template <typename FilterType, typename InputIt>
auto filterByType(InputIt first, InputIt last) {
  return FilteredView<FilterType, InputIt>(first, last);
}

template <typename FilterType, typename ContainerType>
auto filterByType(const ContainerType& inputs) {
  return filterByType<FilterType>(inputs.cbegin(), inputs.cend());
}

//! Returns a list of new-to-old mappings.
//!
//! The input map does not need to be complete. Missing axes are
//! assumed not to be affected.
//!
//! This is used to preprocess broadcast and transpose arguments.
//!
//! Example: (N := ndims)
//!   {{0, 1}} -> [1, 0, ...., N-1]
//!   Transposes the first two axes with no other change.
//!
//!   {{0, -1}} -> [N-1, ...., 0]
//!   Swaps the first and last axes.
std::vector<int> normalizeOld2New(
    const std::unordered_map<int, int>& old2new_in,
    size_t ndims);

// Replace all uses of reference with substitute in expr. Return the Expr.
// Warning: Invalidates provided Expr.
// Warning: Removes connection of reference through provided Expr.
// Warning: Creates new Expr connecting substitue.
// Reference is found through direct pointer comparison.
Expr* replaceValInExpr(Expr* expr, Val* reference, Val* substitute);

// Makes rfactor generic with reduction ops and Welford
TORCH_CUDA_CU_API TensorView* rfactorHelper(
    TensorView* red_tv,
    const std::vector<int>& axes);

// Return immediate producers of tv
TORCH_CUDA_CU_API std::vector<TensorView*> producerTvsOf(TensorView* tv);

// Return immediate consumers of tv
TORCH_CUDA_CU_API std::vector<TensorView*> consumerTvsOf(TensorView* tv);

// Return immediate producers of tvs (can return tvs input)
TORCH_CUDA_CU_API std::vector<TensorView*> producerTvsOf(
    const std::vector<TensorView*>& tvs);

// Return immediate consumers of tvs (can return tvs input)
TORCH_CUDA_CU_API std::vector<TensorView*> consumerTvsOf(
    const std::vector<TensorView*>& tvs);

// Returns producers of tv that are inputs of fusion
TORCH_CUDA_CU_API std::vector<TensorView*> inputTvsOf(TensorView* tv);

// Returns consumers of tv that are outputs of fusion
TORCH_CUDA_CU_API std::vector<TensorView*> outputTvsOf(TensorView* tv);

// Returns producers of tvs that are inputs of fusion
TORCH_CUDA_CU_API std::vector<TensorView*> inputTvsOf(
    std::vector<TensorView*> tvs);

// Returns consumers of tvs that are outputs of fusion
TORCH_CUDA_CU_API std::vector<TensorView*> outputTvsOf(
    std::vector<TensorView*> tvs);

// returns all tensor views in fusion that are used between outputs and inputs.
TORCH_CUDA_CU_API std::vector<TensorView*> allTvs(Fusion* fusion);

// Returns the history of expressions applied to the domains of td
TORCH_CUDA_CU_API std::vector<Expr*> historyOf(TensorDomain* td);

// Returns the history of expressions applied to the domains of tv
TORCH_CUDA_CU_API std::vector<Expr*> historyOf(TensorView* tv);

} // namespace ir_utils
} // namespace cuda
} // namespace fuser
} // namespace jit
} // namespace torch
