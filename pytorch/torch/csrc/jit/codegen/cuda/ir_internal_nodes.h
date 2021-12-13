#pragma once

#include <torch/csrc/WindowsTorchApiMacro.h>

#include <torch/csrc/jit/codegen/cuda/fusion.h>
#include <torch/csrc/jit/codegen/cuda/ir_base_nodes.h>
#include <torch/csrc/jit/codegen/cuda/ir_interface_nodes.h>

//! Nodes in here should generally not be used by users. They should be behind
//! the scenes and users shouldn't have to be aware of what they do to use the
//! code generator
//!
//! \todo improve implementation bool IterDomain::sameAs(const IterDomain*)
//! \todo Add testing of sameAs functions for these nodes
//!

namespace torch {
namespace jit {
namespace fuser {
namespace cuda {

//! Returns true if both v1 and v2 are scalars, are the same type of scalars,
//! and dispatches to the inherited Val type's `->sameAs` call. e.g. if both
//! vals are `Int` will dispatch to v1->as<Int>()->sameAs(v2.as<Int>())
bool areEqualScalars(Val* v1, Val* v2);

//! A specialization for Unary operations. Unary operations take in a single
//! input and produce a single output. Examples include:
//!   1) Casting operation i.e. float(a_val)
//!   2) Negation i.e. val * -1
//!   3) Reduction across a dimension i.e. val.sum(axis=2)
//!   4) split/merge
class TORCH_CUDA_CU_API UnaryOp : public Expr {
 public:
  UnaryOp(UnaryOpType type, Val* out, Val* in);

  UnaryOp(const UnaryOp* src, IrCloner* ir_cloner);

  Val* out() const {
    return out_;
  }
  Val* in() const {
    return in_;
  }

  UnaryOpType getUnaryOpType() const {
    return unary_op_type_;
  }

  bool sameAs(const Statement* other) const override;

 private:
  const UnaryOpType unary_op_type_;
  Val* const out_ = nullptr;
  Val* const in_ = nullptr;
};

//! A specialization for Binary operations. Binary operations take in two inputs
//! and produce a single output. Examples include:
//!  1) Add/mul/div/mod/sub (A * B)
//!  2) LT (A < B)
class TORCH_CUDA_CU_API BinaryOp : public Expr {
 public:
  BinaryOp(BinaryOpType type, Val* out, Val* lhs, Val* rhs);

  BinaryOp(const BinaryOp* src, IrCloner* ir_cloner);

  Val* out() const {
    return out_;
  }
  Val* lhs() const {
    return lhs_;
  }
  Val* rhs() const {
    return rhs_;
  }

  BinaryOpType getBinaryOpType() const {
    return binary_op_type_;
  }

  bool sameAs(const Statement* other) const override;

 private:
  const BinaryOpType binary_op_type_;
  Val* const out_ = nullptr;
  Val* const lhs_ = nullptr;
  Val* const rhs_ = nullptr;
};

//! Broadcast in to match out. is_broadcast_dims are relative to out. Where
//! is_broadcast_dims.size() == out->nDims().
class TORCH_CUDA_CU_API BroadcastOp : public Expr {
 public:
  //! \param out The output tensor
  //! \param in The input tensor
  //! \param is_broadcast_dims True when output dim is a new broadcast domain
  BroadcastOp(Val* out, Val* in, std::vector<bool> is_broadcast_dims);

  BroadcastOp(const BroadcastOp* src, IrCloner* ir_cloner);

  Val* out() const {
    return out_;
  }
  Val* in() const {
    return in_;
  }

  bool isBroadcastDim(size_t dim) const {
    return is_broadcast_dims_.at(dim);
  }

  const std::vector<bool>& getBroadcastDimFlags() const {
    return is_broadcast_dims_;
  }

  bool sameAs(const Statement* other) const override;

 private:
  Val* const out_ = nullptr;
  Val* const in_ = nullptr;

  //! The same list passed to the broadcast arithmetic op. Each
  //! element corresponds to an IterDomain of the output tensor and is
  //! true when the IterDomain is a new broadcast domain. Note
  //! that the output tensor may have other broadcast domains whose
  //! flags are false because the input tensor may already have
  //! broadcast domains.
  const std::vector<bool> is_broadcast_dims_;
};

//! Reduction operation. Out is first initialized to _init. Then
//! reduction_op_type is used to update out as out = reductionOp(out, in).
//! Output's axes marked as reduction will be reduced to produce an output
//! tensor. The output tensors size will be the size of all
//! non-reduction/non-broadcast dimensions.
class TORCH_CUDA_CU_API ReductionOp : public Expr {
 public:
  ReductionOp(BinaryOpType reduction_op_type, Val* init, Val* out, Val* in);

  ReductionOp(const ReductionOp* src, IrCloner* ir_cloner);

  Val* out() const {
    return out_;
  }
  Val* in() const {
    return in_;
  }
  Val* init() const {
    return init_;
  }

  BinaryOpType getReductionOpType() const {
    return reduction_op_type_;
  }

  bool sameAs(const Statement* other) const override;

 private:
  const BinaryOpType reduction_op_type_;
  Val* const init_ = nullptr;
  Val* const out_ = nullptr;
  Val* const in_ = nullptr;
};

//! Welford Scan operation.
class TORCH_CUDA_CU_API WelfordOp : public Expr {
 public:
  WelfordOp(
      Val* out_avg,
      Val* out_var,
      Val* out_N,
      Val* init_avg,
      Val* init_var,
      Val* init_N,
      Val* in_avg,
      Val* in_var,
      Val* in_N);

  WelfordOp(const WelfordOp* src, IrCloner* ir_cloner);

  Val* out() const {
    return out_avg_;
  }

  Val* in() const {
    return in_avg_;
  }

  Val* init() const {
    return init_avg_;
  }

  bool sameAs(const Statement* const other) const override;

  // Welford Accessors
  // TODO clean up
  Val* outAvg() const {
    return out_avg_;
  }

  Val* outVar() const {
    return out_var_;
  }

  Val* outN() const {
    return out_N_;
  }

  Val* inAvg() const {
    return in_avg_;
  }

  Val* inVar() const {
    return in_var_;
  }

  Val* inN() const {
    return in_N_;
  }

  Val* initAvg() const {
    return init_avg_;
  }

  Val* initVar() const {
    return init_var_;
  }

  Val* initN() const {
    return init_N_;
  }

  bool singleValue() const {
    return in_N_->isOneInt();
  }

  bool hasInit() const {
    return !init_N_->isZeroInt();
  }

 private:
  Val* const out_avg_;
  Val* const out_var_;
  Val* const out_N_;
  Val* const init_avg_;
  Val* const init_var_;
  Val* const init_N_;
  Val* const in_avg_;
  Val* const in_var_;
  Val* const in_N_;
};

class TORCH_CUDA_CU_API TransposeOp : public Expr {
 public:
  TransposeOp(TensorView* out, TensorView* in, std::vector<int> new2old);

  TransposeOp(const TransposeOp* src, IrCloner* ir_cloner);

  TensorView* out() const {
    return out_;
  }

  TensorView* in() const {
    return in_;
  }

  const std::vector<int>& new2old() const {
    return new2old_;
  }

 private:
  TensorView* const out_ = nullptr;
  TensorView* const in_ = nullptr;
  const std::vector<int> new2old_;
};

class TORCH_CUDA_CU_API TernaryOp : public Expr {
 public:
  TernaryOp(TernaryOpType type, Val* out, Val* in1, Val* in2, Val* in3);

  TernaryOp(const TernaryOp* src, IrCloner* ir_cloner);

  Val* out() const {
    return out_;
  }

  Val* in1() const {
    return in1_;
  }
  Val* in2() const {
    return in2_;
  }
  Val* in3() const {
    return in3_;
  }

  TernaryOpType getTernaryOpType() const {
    return ternary_op_type_;
  }

  bool sameAs(const Statement* other) const override;

 private:
  const TernaryOpType ternary_op_type_;
  Val* const out_ = nullptr;
  Val* const in1_ = nullptr;
  Val* const in2_ = nullptr;
  Val* const in3_ = nullptr;
};

//! Shift
class TORCH_CUDA_CU_API ShiftOp : public Expr {
 public:
  //! \param out
  //! \param in
  //! \param offsets
  ShiftOp(Val* out, Val* in, std::vector<int> offsets, bool pad);

  ShiftOp(const ShiftOp* src, IrCloner* ir_cloner);

  Val* out() const {
    return out_;
  }
  Val* in() const {
    return in_;
  }

  int offset(size_t dim) const {
    return offsets_.at(dim);
  }

  const std::vector<int>& offsets() const {
    return offsets_;
  }

  bool pad() const {
    return pad_;
  }

  bool sameAs(const Statement* other) const override;

 private:
  Val* const out_ = nullptr;
  Val* const in_ = nullptr;
  //! Each of the root axes is shifted by the corresponding value of
  //! offsets_. The sign of each value indicates the direction of
  //! shifting.
  const std::vector<int> offsets_;
  const bool pad_;
};

//! Gather a window around each element.
class TORCH_CUDA_CU_API GatherOp : public Expr {
 public:
  GatherOp(
      Val* out,
      Val* in,
      std::vector<Int*> window_shape,
      std::vector<std::vector<Int*>> pad_width);

  GatherOp(const GatherOp* src, IrCloner* ir_cloner);

  Val* out() const {
    return out_;
  }
  Val* in() const {
    return in_;
  }

  const auto& windowShape() const {
    return window_shape_;
  }

  //! Returns the gather axis that corresponds to an input axis
  int gatherAxis(int axis) const;

  const auto& padWidth() const {
    return pad_width_;
  }

  bool sameAs(const Statement* other) const override;

 private:
  Val* const out_ = nullptr;
  Val* const in_ = nullptr;
  //! Shape of a window gathered for each element.
  std::vector<Int*> window_shape_;
  //! The size of zero-padding of each axis.
  std::vector<std::vector<Int*>> pad_width_;
};

// Friends for direct access to split
class TensorDomain;
class ReplayTransformations;
class IndexReferenceReplay;
//! Simply a representation of an annotated 1D iterable from start to extent.
//! TensorDomains which represent how to iterate over a tensor is made up of
//! IterDomains to form an ND iterable. We directly set parallization strategies
//! on IterDomains.
class TORCH_CUDA_CU_API IterDomain : public Val {
 public:
  IterDomain(
      Val* start,
      Val* extent,
      ParallelType parallel_type = ParallelType::Serial,
      IterType iter_type = IterType::Iteration,
      bool is_rfactor_domain = false);

  IterDomain(
      Val* start,
      Val* extent,
      Val* stop_offset,
      ParallelType parallel_type = ParallelType::Serial,
      IterType iter_type = IterType::Iteration,
      bool is_rfactor_domain = false);

  IterDomain(const IterDomain* src, IrCloner* ir_cloner);

  bool sameAs(const Statement* other) const override;

  // Returns a new IterDomain matching properties of this
  // TODO: parallel_method->getParallelType
  IterDomain* clone() const {
    auto cloned = new IterDomain(
        start(),
        extent(),
        stopOffset(),
        getParallelType(),
        getIterType(),
        isRFactorProduct());

    cloned->is_padded_dimension_ = is_padded_dimension_;
    cloned->padded_to_size_ = padded_to_size_;
    return cloned;
  }

  //! Clone a vector domains
  static std::vector<IterDomain*> clone(
      const std::vector<IterDomain*>& domains);

  static IterDomain* merge(IterDomain* outer, IterDomain* inner);

  bool isReduction() const {
    return getIterType() == IterType::Reduction;
  }

  bool isRFactorProduct() const {
    return is_rfactor_domain_;
  }

  bool isBroadcast() const {
    return getIterType() == IterType::BroadcastWithStride ||
        getIterType() == IterType::BroadcastWithoutStride;
  }

  bool isGather() const {
    return getIterType() == IterType::Gather;
  }

  bool isParallelized() const {
    return getParallelType() != ParallelType::Serial;
  }

  //! Return if this iter domain is mapped to a grid dimension
  bool isBlockDim() const {
    return isParallelTypeBlockDim(getParallelType());
  }

  //! Return if this iter domain is mapped to a block dimension
  bool isThreadDim() const {
    return isParallelTypeThreadDim(getParallelType());
  }

  //! Return if this iter domain is either mapped to a block or grid dimension
  bool isThread() const {
    return (isBlockDim() || isThreadDim());
  }

  //! Convert to strided broadcast, used for supporting broadcast on output
  void toStridedBroadcast() {
    TORCH_INTERNAL_ASSERT(
        isBroadcast(),
        "toStridedBroadCast: converting an non-broadcast iterdomain",
        this);
    iter_type_ = IterType::BroadcastWithStride;
  }

  // Convert a serial iterdomain to broadcast, used for implicit broadcast
  void convertToBroadcast() {
    TORCH_INTERNAL_ASSERT(
        !isBroadcast() && !isReduction(),
        "convertToBroadcast: converting an non-serial iterdomain",
        this);

    iter_type_ = IterType::BroadcastWithStride;
  }

  void parallelize(ParallelType t);

  ParallelType getParallelType() const {
    return parallel_type_;
  }

  IterType getIterType() const {
    return iter_type_;
  }

  Val* start() const {
    return start_;
  }

  Val* stop() const;

  Val* stopOffset() const;

  Val* extent() const {
    TORCH_INTERNAL_ASSERT(extent_ != nullptr);
    return extent_;
  }

  //! Dimension padding interface:
  //!  2 modes are currently supported:
  //!
  //!   - mode 1: if to_size is given as a positive number,
  //!      the dimension will be padded to the size so that
  //!      this iterdomain will be compile-time constant
  //!      size and it is the scheduler's responsibility
  //!      to ensure no input larger than the padded size
  //!      will be observed
  //!
  //!   - mode 2: if no to_size is given, this dimension
  //!      is "dynamically" padded to next smallest multiple
  //!      of a warp size, i.e. 17 padded to 32, 33 padded to 64
  //!      based on the given input.
  void padToMultipleOfWarp(c10::optional<int64_t> maybe_to_size = {}) {
    // Currently only restricted to TIDx to generate warp reduce
    TORCH_CHECK(
        parallel_type_ == ParallelType::TIDx,
        "padToMultipleOfWarp : warp padding only supported on TIDx parallel dimension");
    is_padded_dimension_ = true;
    if (maybe_to_size.has_value()) {
      if (maybe_to_size.value() > 0) {
        padded_to_size_ = maybe_to_size.value();
      }
    }
  }

  //! Indicates if this iterdomain had padding
  //!  dynamical or statical
  bool hasPaddingToMultipleOfWarp() const {
    return is_padded_dimension_;
  }

  //! Returns a concrete value if this iterdomain
  //!  has been padded to a statical size.
  c10::optional<int64_t> getMaybeSizeAfterPadding() const {
    return padded_to_size_;
  }

  //! True if range of iteration domain isn't across the full extent
  bool maybePartial() const;

  //! Check if IterDomain is a broadcast axis with compile-time
  //! known extent. This is the case with all size-1 IterDomains on
  //! a TensorView's root domain when the TensorView is created.
  bool isImplicitBroadcast() const {
    return isBroadcast() && extent()->isOneInt();
  }

  //! Check if IterDomain is a reduction axis with size of 1, i.e.
  //! a "squeeze" operator.
  //!
  //! NOTE: Detection of trivial reduction here is not
  //! comprehensive. See detectTrivialReductionDerivedDomains for more
  //! comprehensive analysis. We typically use this for root domain trivial
  //! reduction checks. So we ship to the correct scheduler. It may
  //! not be incredibly robust, but it makes sense to keep it for now.
  bool isTrivialReduction() const {
    return isReduction() && extent()->isOneInt();
  }

 protected:
  friend TensorDomain;
  friend ReplayTransformations;
  friend IndexReferenceReplay;

  //! start_offset and stop_offset defines partial split. Only root
  //! domains are allowed to have non-zero start and stop offsets.
  static std::pair<IterDomain*, IterDomain*> split(
      IterDomain* in,
      Val* factor,
      bool inner_split,
      Val* start_offset = nullptr,
      Val* stop_offset = nullptr);

  //! trim_out_of_bounds controls how the values outside start and stop
  //! positions are treated. The option is only valid with root
  //! domains as non-root domains do not have valid start and stop
  //! positions.
  //!
  //! \param trim_out_of_bounds Trims [0, start_] and [-stop_offset_, extent_]
  static std::pair<IterDomain*, IterDomain*> split(
      IterDomain* in,
      Val* factor,
      bool inner_split,
      bool trim_out_of_bounds);

 private:
  //! Valid range is defined as [start:-stop_offset]
  Val* const start_ = nullptr;
  Val* const extent_ = nullptr;
  //! Distance of stop from the end
  Val* const stop_offset_ = nullptr;
  ParallelType parallel_type_ = ParallelType::Serial;
  IterType iter_type_ = IterType::Iteration;
  bool is_rfactor_domain_ = false;
  bool is_padded_dimension_ = false;
  c10::optional<int64_t> padded_to_size_ = c10::nullopt;
};

//! TensorDomain holds a vector of IterDomains. It holds an IterDomain for every
//! logical axis in its associated tensor. TensorDomain does not directly hold
//! the Tensor it is associated with, and in theory could be associated with
//! multiple tensors. TensorDomain's primary responsibility is to provide a
//! mechanism to access history of transformations that were used to generate
//! it. This is done through the normal interaction of Expr/Val in Fusion. i.e.
//! if we want to know the previous operation generating a particular
//! TensorDomain we can simply call:
//!
//!     FusionGuard::getCurFusion()->definition(a_tensor_domain)
//!
//! which should give us an operation in the list [split, merge] or similar
//! operations that take in a TensorDomain, applies a transformation and outputs
//! a tensor domain.
class TORCH_CUDA_CU_API TensorDomain : public Val {
 public:
  explicit TensorDomain(
      std::vector<IterDomain*> root_domain,
      std::vector<bool> contiguity = std::vector<bool>());

  TensorDomain(
      std::vector<IterDomain*> root_domain,
      std::vector<IterDomain*> domain,
      std::vector<bool> contiguity = std::vector<bool>());

  TensorDomain(
      std::vector<IterDomain*> root_domain,
      std::vector<IterDomain*> rfactor_domain,
      std::vector<IterDomain*> domain,
      std::vector<bool> contiguity = std::vector<bool>());

  TensorDomain(const TensorDomain* src, IrCloner* ir_cloner);

  bool operator==(const TensorDomain& other) const;
  bool operator!=(const TensorDomain& other) const {
    return !(*this == other);
  }

  std::vector<IterDomain*>::size_type nDims() const {
    return domain_.size();
  }

  bool sameAs(const Statement* other) const override;

  static bool sameAs(
      const std::vector<IterDomain*>& lhs,
      const std::vector<IterDomain*>& rhs);

  const std::vector<IterDomain*>& domain() const {
    return domain_;
  }

  const std::vector<bool>& contiguity() const {
    return contiguity_;
  }

  void setContiguity(const std::vector<bool>& contig);

  std::string getContiguityString() const {
    std::stringstream ss;
    for (auto b : contiguity()) {
      ss << (b ? "t" : "f");
    }
    return ss.str();
  }

  bool hasReduction() const;
  bool hasBlockReduction() const;
  bool hasGridReduction() const;
  bool hasBroadcast() const;
  bool hasRFactor() const;
  bool hasVectorize() const;

  c10::optional<unsigned int> getReductionAxis() const;

  const std::vector<IterDomain*>& noReductions() const {
    return no_reduction_domain_;
  }

  const std::vector<IterDomain*>& noBroadcasts() const {
    return no_bcast_domain_;
  }

  const std::vector<IterDomain*>& getRootDomain() const {
    return root_domain_;
  };

  const std::vector<IterDomain*>& getRFactorDomain() const {
    return rfactor_domain_;
  };

  // If rfactor domain exists in domain() return it, otherwise return root
  // domain.
  const std::vector<IterDomain*>& getMaybeRFactorDomain() const {
    return hasRFactor() ? getRFactorDomain() : getRootDomain();
  }

  void resetDomains() {
    no_reduction_domain_ = noReductions(domain_);
    no_bcast_domain_ = noBroadcasts(domain_);
    has_nontrivial_reduction_ = hasNontrivialReduction(domain_);
  }

  // i here is int, as we want to accept negative value and ::size_type can be a
  // uint.
  IterDomain* axis(int i) const;

  size_t posOf(IterDomain* id) const;

  //! Returns a position of a root domain
  size_t rootPosOf(IterDomain* id) const;

  // Split "axis" into 2 axes
  //! inner_split dictates if the factor section of the split should be inside
  //! the
  //! remainer or outside.
  //! e.g. split(0, 4, inner_split = true) will result in:
  //! tv[id{extent}] -> tv[id{ceilDiv(extent, factor)}, id{factor}]
  //! e.g. split(0, 4, inner_split = false) will result in:
  //! tv[id{extent}] -> tv[id{factor}, id{ceilDiv(extent, factor)}]
  void split(
      int axis_,
      Val* factor,
      bool inner_split,
      bool trim_out_of_bounds = false);

  // Merge axis_o and axis_i. axis_i is the fast changing dimension. Resulting
  // axis is by default placed at original position axis_o
  void merge(int axis_o, int axis_i);

  // Reorder axes according to map[old_pos] = new_pos
  void reorder(const std::unordered_map<int, int>& old2new);

  static std::vector<IterDomain*> orderedAs(
      const std::vector<IterDomain*>& td,
      const std::unordered_map<int, int>& old2new);

  static std::vector<IterDomain*> noReductions(const std::vector<IterDomain*>&);
  static std::vector<IterDomain*> noBroadcasts(const std::vector<IterDomain*>&);

  static bool hasBroadcast(const std::vector<IterDomain*>&);
  static bool hasReduction(const std::vector<IterDomain*>&);
  static bool hasNontrivialReduction(const std::vector<IterDomain*>&);

  // pair is in order where second is the consumer of first
  std::pair<TensorDomain*, TensorDomain*> rFactor(const std::vector<int>& axes);

 private:
  const std::vector<IterDomain*> root_domain_;
  std::vector<IterDomain*> domain_;
  std::vector<IterDomain*> no_bcast_domain_;
  std::vector<IterDomain*> no_reduction_domain_;
  const std::vector<IterDomain*> rfactor_domain_;
  std::vector<bool> contiguity_;
  bool has_nontrivial_reduction_;
};

//! Representation a split on an IterDomain by "factor"
//! inner_split dictates if the factor section of the split should be inside the
//! remainer or outside.
class TORCH_CUDA_CU_API Split : public Expr {
 public:
  // start_offset and stop_offset are used to express partial
  // split. Only the partial domain from start_offset to stop_offset
  // is split and the outer sub-regions are ignored. Note that both
  // start_offset and stop_offset are distance from the left end and
  // right ends, respectively.
  Split(
      IterDomain* outer,
      IterDomain* inner,
      IterDomain* in,
      Val* factor,
      bool inner_split = true,
      Val* start_offset = nullptr,
      Val* stop_offset = nullptr);

  Split(const Split* src, IrCloner* ir_cloner);

  IterDomain* outer() const {
    return outer_;
  }
  IterDomain* inner() const {
    return inner_;
  }
  IterDomain* in() const {
    return in_;
  }
  Val* factor() const {
    return factor_;
  }

  bool innerSplit() const {
    return inner_split_;
  }

  Val* startOffset() const {
    TORCH_INTERNAL_ASSERT(start_offset_ != nullptr);
    return start_offset_;
  }

  Val* stopOffset() const {
    TORCH_INTERNAL_ASSERT(stop_offset_ != nullptr);
    return stop_offset_;
  }

  //! Utility function to compute the split extent.
  static Val* extent(Val* in_extent, Val* start_offset, Val* stop_offset);

  bool sameAs(const Statement* other) const override;

 private:
  IterDomain* const outer_ = nullptr;
  IterDomain* const inner_ = nullptr;
  IterDomain* const in_ = nullptr;
  Val* const factor_ = nullptr;
  bool inner_split_ = true;
  //! Start position of the input domain. Non-zero means partial
  //! split. Elements until this offset are ignored.
  Val* const start_offset_ = nullptr;
  //! Offset from extent of the input domain. Non-zero means partial
  //! split. Elements after this offset are ignored.
  Val* const stop_offset_ = nullptr;
};

//! Merge the IterDomains outer and inner into one domain, outer and inner
//! dictate which will be traversed first (inner). Both IterDomains must be of
//! the same iter or reduction type, as well as the same parallelization
//! strategy if there is one
//!
//! \todo Should this be a unary op type?
//!
class TORCH_CUDA_CU_API Merge : public Expr {
 public:
  Merge(IterDomain* out, IterDomain* outer, IterDomain* inner);

  Merge(const Merge* src, IrCloner* ir_cloner);

  IterDomain* out() const {
    return out_;
  }
  IterDomain* outer() const {
    return outer_;
  }
  IterDomain* inner() const {
    return inner_;
  }

  bool sameAs(const Statement* other) const override;

 private:
  IterDomain* const out_ = nullptr;
  IterDomain* const outer_ = nullptr;
  IterDomain* const inner_ = nullptr;
};

//! Integer value which has a special name
//!
//! These could be:
//! - threadIdx.x
//! - blockIdx.y
//! - blockDim.z
//! - T3.stride[2]
//!
class TORCH_CUDA_CU_API NamedScalar : public Val {
 public:
  // NOLINTNEXTLINE(modernize-pass-by-value)
  NamedScalar(std::string name, DataType dtype)
      : Val(ValType::NamedScalar, dtype), name_(name) {}

  NamedScalar(const NamedScalar* src, IrCloner* ir_cloner);

  const std::string& name() const {
    return name_;
  }

  bool sameAs(const Statement* other) const override;

  //! Return the named scalar extent of a parallel dimension (e.g. blockDim.x)
  static NamedScalar* getParallelDim(ParallelType p_type);

  //! Return the named scalar index of a parallel dimension (e.g. threadIdx.x)
  static NamedScalar* getParallelIndex(ParallelType p_type);

  //! Return the parallel type of this NamedScalar if it is an extent of a
  //! parallel dimension
  c10::optional<ParallelType> getParallelDim() const;

  //! Return the parallel type of this NamedScalar if it is an index of a
  //! parallel dimension
  c10::optional<ParallelType> getParallelIndex() const;

 private:
  std::string name_;
};

} // namespace cuda
} // namespace fuser
} // namespace jit
} // namespace torch
