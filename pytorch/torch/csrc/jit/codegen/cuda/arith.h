#pragma once

#include <torch/csrc/WindowsTorchApiMacro.h>

#include <torch/csrc/jit/codegen/cuda/ir_interface_nodes.h>
#include <torch/csrc/jit/codegen/cuda/type.h>
#include <torch/csrc/jit/codegen/cuda/type_promotion.h>

class Val;

/*
 * The operations defined in this header is intended as user facing functions.
 * Generally users should not directly instantiate temporary TensorViews they
 * should instead use the functions below which will automatically create IR
 * nodes, and return a resulting TensorView of correctly tracked shapes.
 */

namespace torch {
namespace jit {
namespace fuser {
namespace cuda {

// Insertion of casting op to dtype, returns new resulting val
TORCH_CUDA_CU_API Val* castOp(DataType dtype, Val* v1);
TORCH_CUDA_CU_API TensorView* castOp(DataType dtype, TensorView* v1);

// Perform unary op type and return the output
TORCH_CUDA_CU_API Val* unaryOp(UnaryOpType type, Val* v1);
TORCH_CUDA_CU_API TensorView* unaryOp(UnaryOpType type, TensorView* v1);

TORCH_CUDA_CU_API Val* unaryOp(
    UnaryOpType type,
    Val* v1,
    const TypePromotionConfig& config);
TORCH_CUDA_CU_API TensorView* unaryOp(
    UnaryOpType type,
    TensorView* v1,
    const TypePromotionConfig& config);

// Perform binary op type on v1 and v2 and return a type promoted output.
// Mod, CeilDiv, and LT are considered Int only output operations for now.
TORCH_CUDA_CU_API Val* binaryOp(
    BinaryOpType type,
    Val* v1,
    Val* v2,
    DataType out_dtype = DataType::Null);
TORCH_CUDA_CU_API TensorView* binaryOp(
    BinaryOpType type,
    TensorView* v1,
    Val* v2,
    DataType out_dtype = DataType::Null);
TORCH_CUDA_CU_API TensorView* binaryOp(
    BinaryOpType type,
    Val* v1,
    TensorView* v2,
    DataType out_dtype = DataType::Null);
TORCH_CUDA_CU_API TensorView* binaryOp(
    BinaryOpType type,
    TensorView* v1,
    TensorView* v2,
    DataType out_dtype = DataType::Null);

TORCH_CUDA_CU_API Val* binaryOp(
    BinaryOpType type,
    Val* v1,
    Val* v2,
    const TypePromotionConfig& config);
TORCH_CUDA_CU_API TensorView* binaryOp(
    BinaryOpType type,
    TensorView* v1,
    Val* v2,
    const TypePromotionConfig& config);
TORCH_CUDA_CU_API TensorView* binaryOp(
    BinaryOpType type,
    Val* v1,
    TensorView* v2,
    const TypePromotionConfig& config);
TORCH_CUDA_CU_API TensorView* binaryOp(
    BinaryOpType type,
    TensorView* v1,
    TensorView* v2,
    const TypePromotionConfig& config);

// Perform a reduction operation on v1, initial value for reduction is init,
// reduces across axes, and reduction operation defined by BinaryOp.
TORCH_CUDA_CU_API TensorView* reductionOp(
    BinaryOpType reduction_op_type,
    const std::vector<int>& axes,
    Val* init,
    TensorView* v1,
    bool keep_dim = false);

//! Auxiliary Struct holding result of
//! a single welford op in ternsorview
class TORCH_CUDA_CU_API WelfordResult {
 public:
  TensorView* avg;
  TensorView* var_sum;
  TensorView* n;

  explicit WelfordResult(
      TensorView* in_avg,
      TensorView* in_var_sum,
      TensorView* in_n);

  WelfordResult rFactor(const std::vector<int>& axes);
};

//! Welford operator on specified axes. This is currently the only scan op with
//! multiple outputs that is supported. May consider generalization if more scan
//! ops are added.
TORCH_CUDA_CU_API WelfordResult Welford(
    TensorView* tv,
    const std::vector<int>& axes,
    TensorView* init_avg = nullptr,
    TensorView* init_var = nullptr,
    Int* init_N = new Int(0));

// UNARY OPERATIONS
// abs
TORCH_CUDA_CU_API Val* abs(Val*);
TORCH_CUDA_CU_API TensorView* abs(TensorView*);
// acos
TORCH_CUDA_CU_API Val* acos(Val*);
TORCH_CUDA_CU_API TensorView* acos(TensorView*);
// asin
TORCH_CUDA_CU_API Val* asin(Val*);
TORCH_CUDA_CU_API TensorView* asin(TensorView*);
// atan
TORCH_CUDA_CU_API Val* atan(Val*);
TORCH_CUDA_CU_API TensorView* atan(TensorView*);
// atanh
TORCH_CUDA_CU_API Val* atanh(Val*);
TORCH_CUDA_CU_API TensorView* atanh(TensorView*);
// ceil
TORCH_CUDA_CU_API Val* ceil(Val*);
TORCH_CUDA_CU_API TensorView* ceil(TensorView*);
// cos
TORCH_CUDA_CU_API Val* cos(Val*);
TORCH_CUDA_CU_API TensorView* cos(TensorView*);
// cosh
TORCH_CUDA_CU_API Val* cosh(Val*);
TORCH_CUDA_CU_API TensorView* cosh(TensorView*);
// exp
TORCH_CUDA_CU_API Val* exp(Val*);
TORCH_CUDA_CU_API TensorView* exp(TensorView*);
// expm1
TORCH_CUDA_CU_API Val* expm1(Val*);
TORCH_CUDA_CU_API TensorView* expm1(TensorView*);
// erf
TORCH_CUDA_CU_API Val* erf(Val*);
TORCH_CUDA_CU_API TensorView* erf(TensorView*);
// erfc
TORCH_CUDA_CU_API Val* erfc(Val*);
TORCH_CUDA_CU_API TensorView* erfc(TensorView*);
// floor
TORCH_CUDA_CU_API Val* floor(Val*);
TORCH_CUDA_CU_API TensorView* floor(TensorView*);
// frac
TORCH_CUDA_CU_API Val* frac(Val*);
TORCH_CUDA_CU_API TensorView* frac(TensorView*);
// gelu
TORCH_CUDA_CU_API Val* gelu(Val*);
TORCH_CUDA_CU_API TensorView* gelu(TensorView*);
// silu
TORCH_CUDA_CU_API Val* silu(Val*);
TORCH_CUDA_CU_API TensorView* silu(TensorView*);
// lgamma
TORCH_CUDA_CU_API Val* lgamma(Val*);
TORCH_CUDA_CU_API TensorView* lgamma(TensorView*);
// log
TORCH_CUDA_CU_API Val* log(Val*);
TORCH_CUDA_CU_API TensorView* log(TensorView*);
// log10
TORCH_CUDA_CU_API Val* log10(Val*);
TORCH_CUDA_CU_API TensorView* log10(TensorView*);
// log1p
TORCH_CUDA_CU_API Val* log1p(Val*);
TORCH_CUDA_CU_API TensorView* log1p(TensorView*);
// log2
TORCH_CUDA_CU_API Val* log2(Val*);
TORCH_CUDA_CU_API TensorView* log2(TensorView*);
// neg
TORCH_CUDA_CU_API Val* neg(Val*);
TORCH_CUDA_CU_API TensorView* neg(TensorView*);
// randlike
TORCH_CUDA_CU_API Val* randlike(Val*);
TORCH_CUDA_CU_API TensorView* randlike(TensorView*);
// reciprocal
TORCH_CUDA_CU_API Val* reciprocal(Val*);
TORCH_CUDA_CU_API TensorView* reciprocal(TensorView*);
// relu
TORCH_CUDA_CU_API Val* relu(Val*);
TORCH_CUDA_CU_API TensorView* relu(TensorView*);
// rsqrt
TORCH_CUDA_CU_API Val* rsqrt(Val*);
TORCH_CUDA_CU_API TensorView* rsqrt(TensorView*);
// round
TORCH_CUDA_CU_API Val* round(Val*);
TORCH_CUDA_CU_API TensorView* round(TensorView*);
// set
TORCH_CUDA_CU_API Val* set(Val*);
TORCH_CUDA_CU_API TensorView* set(TensorView*);
// sigmoid
TORCH_CUDA_CU_API Val* sigmoid(Val*);
TORCH_CUDA_CU_API TensorView* sigmoid(TensorView*);
// sin
TORCH_CUDA_CU_API Val* sin(Val*);
TORCH_CUDA_CU_API TensorView* sin(TensorView*);
// sinh
TORCH_CUDA_CU_API Val* sinh(Val*);
TORCH_CUDA_CU_API TensorView* sinh(TensorView*);
// sqrt
TORCH_CUDA_CU_API Val* sqrt(Val*);
TORCH_CUDA_CU_API TensorView* sqrt(TensorView*);
// tan
TORCH_CUDA_CU_API Val* tan(Val*);
TORCH_CUDA_CU_API TensorView* tan(TensorView*);
// tanh
TORCH_CUDA_CU_API Val* tanh(Val*);
TORCH_CUDA_CU_API TensorView* tanh(TensorView*);
// trunc
TORCH_CUDA_CU_API Val* trunc(Val*);
TORCH_CUDA_CU_API TensorView* trunc(TensorView*);
// not
TORCH_CUDA_CU_API Val* notOp(Val*);
TORCH_CUDA_CU_API TensorView* notOp(TensorView*);

// Broadcasts v1 based on bool vector. Size of broadcast bool vector should be
// the number of dims desired in the broadcasted tensor. This vector should be
// true if output dim should be a broadcasted dim, and false if it is not a
// broadcasted dim. Number of false entires must match the number of input dims.
TORCH_CUDA_CU_API TensorView* broadcast(
    TensorView* inp,
    const std::vector<bool>& is_broadcast_dim);

//! Transpose a tensor as specified by axis mappings.
//!
//! The transposition mapping is specified with a list of pairs from
//! old to new positions. Positions are relative to the noReduction
//! domain.
//!
//! \param inp Tensor to transpose
//! \param old2new Pairs of mapping from old to new positions.
TORCH_CUDA_CU_API TensorView* transpose(
    TensorView* inp,
    const std::unordered_map<int, int>& old2new);

// BINARY OPERATIONS
// add
TORCH_CUDA_CU_API Val* add(Val* v1, Val* v2);
TORCH_CUDA_CU_API TensorView* add(TensorView* v1, Val* v2);
TORCH_CUDA_CU_API TensorView* add(Val* v1, TensorView* v2);
TORCH_CUDA_CU_API TensorView* add(TensorView* v1, TensorView* v2);
// atan2
TORCH_CUDA_CU_API Val* atan2(Val* v1, Val* v2);
TORCH_CUDA_CU_API TensorView* atan2(TensorView* v1, Val* v2);
TORCH_CUDA_CU_API TensorView* atan2(Val* v1, TensorView* v2);
TORCH_CUDA_CU_API TensorView* atan2(TensorView* v1, TensorView* v2);
// div
TORCH_CUDA_CU_API Val* div(Val* v1, Val* v2);
TORCH_CUDA_CU_API TensorView* div(TensorView* v1, Val* v2);
TORCH_CUDA_CU_API TensorView* div(Val* v1, TensorView* v2);
TORCH_CUDA_CU_API TensorView* div(TensorView* v1, TensorView* v2);
// fmod
TORCH_CUDA_CU_API Val* fmod(Val* v1, Val* v2);
TORCH_CUDA_CU_API TensorView* fmod(TensorView* v1, Val* v2);
TORCH_CUDA_CU_API TensorView* fmod(Val* v1, TensorView* v2);
TORCH_CUDA_CU_API TensorView* fmod(TensorView* v1, TensorView* v2);
// mul
TORCH_CUDA_CU_API Val* mul(Val* v1, Val* v2);
TORCH_CUDA_CU_API TensorView* mul(TensorView* v1, Val* v2);
TORCH_CUDA_CU_API TensorView* mul(Val* v1, TensorView* v2);
TORCH_CUDA_CU_API TensorView* mul(TensorView* v1, TensorView* v2);
// pow
TORCH_CUDA_CU_API Val* pow(Val* v1, Val* v2);
TORCH_CUDA_CU_API TensorView* pow(TensorView* v1, Val* v2);
TORCH_CUDA_CU_API TensorView* pow(Val* v1, TensorView* v2);
TORCH_CUDA_CU_API TensorView* pow(TensorView* v1, TensorView* v2);
// remainder
TORCH_CUDA_CU_API Val* remainder(Val* v1, Val* v2);
TORCH_CUDA_CU_API TensorView* remainder(TensorView* v1, Val* v2);
TORCH_CUDA_CU_API TensorView* remainder(Val* v1, TensorView* v2);
TORCH_CUDA_CU_API TensorView* remainder(TensorView* v1, TensorView* v2);
// sub
TORCH_CUDA_CU_API Val* sub(Val* v1, Val* v2);
TORCH_CUDA_CU_API TensorView* sub(TensorView* v1, Val* v2);
TORCH_CUDA_CU_API TensorView* sub(Val* v1, TensorView* v2);
TORCH_CUDA_CU_API TensorView* sub(TensorView* v1, TensorView* v2);
// Integer binary ops
// mod
TORCH_CUDA_CU_API Val* mod(Val* v1, Val* v2);
TORCH_CUDA_CU_API TensorView* mod(TensorView* v1, Val* v2);
TORCH_CUDA_CU_API TensorView* mod(Val* v1, TensorView* v2);
TORCH_CUDA_CU_API TensorView* mod(TensorView* v1, TensorView* v2);
// ceilDiv
TORCH_CUDA_CU_API Val* ceilDiv(Val* v1, Val* v2);
TORCH_CUDA_CU_API TensorView* ceilDiv(TensorView* v1, Val* v2);
TORCH_CUDA_CU_API TensorView* ceilDiv(Val* v1, TensorView* v2);
TORCH_CUDA_CU_API TensorView* ceilDiv(TensorView* v1, TensorView* v2);
// lshift
TORCH_CUDA_CU_API Val* lshift(Val* v1, Val* v2);
TORCH_CUDA_CU_API TensorView* lshift(TensorView* v1, Val* v2);
TORCH_CUDA_CU_API TensorView* lshift(Val* v1, TensorView* v2);
TORCH_CUDA_CU_API TensorView* lshift(TensorView* v1, TensorView* v2);
// rshift
TORCH_CUDA_CU_API Val* rshift(Val* v1, Val* v2);
TORCH_CUDA_CU_API TensorView* rshift(TensorView* v1, Val* v2);
TORCH_CUDA_CU_API TensorView* rshift(Val* v1, TensorView* v2);
TORCH_CUDA_CU_API TensorView* rshift(TensorView* v1, TensorView* v2);
// Logical binary ops
// eq
TORCH_CUDA_CU_API Val* eq(Val* v1, Val* v2);
TORCH_CUDA_CU_API TensorView* eq(TensorView* v1, Val* v2);
TORCH_CUDA_CU_API TensorView* eq(Val* v1, TensorView* v2);
TORCH_CUDA_CU_API TensorView* eq(TensorView* v1, TensorView* v2);
// ge
TORCH_CUDA_CU_API Val* ge(Val* v1, Val* v2);
TORCH_CUDA_CU_API TensorView* ge(TensorView* v1, Val* v2);
TORCH_CUDA_CU_API TensorView* ge(Val* v1, TensorView* v2);
TORCH_CUDA_CU_API TensorView* ge(TensorView* v1, TensorView* v2);
// gt
TORCH_CUDA_CU_API Val* gt(Val* v1, Val* v2);
TORCH_CUDA_CU_API TensorView* gt(TensorView* v1, Val* v2);
TORCH_CUDA_CU_API TensorView* gt(Val* v1, TensorView* v2);
TORCH_CUDA_CU_API TensorView* gt(TensorView* v1, TensorView* v2);
// le
TORCH_CUDA_CU_API Val* le(Val* v1, Val* v2);
TORCH_CUDA_CU_API TensorView* le(TensorView* v1, Val* v2);
TORCH_CUDA_CU_API TensorView* le(Val* v1, TensorView* v2);
TORCH_CUDA_CU_API TensorView* le(TensorView* v1, TensorView* v2);
// lt
TORCH_CUDA_CU_API Val* lt(Val* v1, Val* v2);
TORCH_CUDA_CU_API TensorView* lt(TensorView* v1, Val* v2);
TORCH_CUDA_CU_API TensorView* lt(Val* v1, TensorView* v2);
TORCH_CUDA_CU_API TensorView* lt(TensorView* v1, TensorView* v2);
// ne
TORCH_CUDA_CU_API Val* ne(Val* v1, Val* v2);
TORCH_CUDA_CU_API TensorView* ne(TensorView* v1, Val* v2);
TORCH_CUDA_CU_API TensorView* ne(Val* v1, TensorView* v2);
TORCH_CUDA_CU_API TensorView* ne(TensorView* v1, TensorView* v2);

// andOp
TORCH_CUDA_CU_API Val* andOp(Val* v1, Val* v2);
TORCH_CUDA_CU_API TensorView* andOp(TensorView* v1, Val* v2);
TORCH_CUDA_CU_API TensorView* andOp(Val* v1, TensorView* v2);
TORCH_CUDA_CU_API TensorView* andOp(TensorView* v1, TensorView* v2);
// orOp
TORCH_CUDA_CU_API Val* orOp(Val* v1, Val* v2);
TORCH_CUDA_CU_API TensorView* orOp(TensorView* v1, Val* v2);
TORCH_CUDA_CU_API TensorView* orOp(Val* v1, TensorView* v2);
TORCH_CUDA_CU_API TensorView* orOp(TensorView* v1, TensorView* v2);
// xorOp
TORCH_CUDA_CU_API Val* xorOp(Val* v1, Val* v2);
TORCH_CUDA_CU_API TensorView* xorOp(TensorView* v1, Val* v2);
TORCH_CUDA_CU_API TensorView* xorOp(Val* v1, TensorView* v2);
TORCH_CUDA_CU_API TensorView* xorOp(TensorView* v1, TensorView* v2);

// REDUCTION OPERATIONS
TORCH_CUDA_CU_API TensorView* sum(
    TensorView* v1,
    const std::vector<int>& reduction_axes,
    bool keep_dim = false);

TORCH_CUDA_CU_API TensorView* max(
    TensorView* v1,
    const std::vector<int>& reduction_axes,
    bool keep_dim = false);

TORCH_CUDA_CU_API TensorView* min(
    TensorView* v1,
    const std::vector<int>& reduction_axes,
    bool keep_dim = false);

// COMPOUND OPERATIONS
// add_alpha
TORCH_CUDA_CU_API Val* add_alpha(Val* v1, Val* v2, Val* s);
TORCH_CUDA_CU_API TensorView* add_alpha(TensorView* v1, Val* v2, Val* s);
TORCH_CUDA_CU_API TensorView* add_alpha(Val* v1, TensorView* v2, Val* s);
TORCH_CUDA_CU_API TensorView* add_alpha(TensorView* v1, TensorView* v2, Val* s);
// sub_alpha
TORCH_CUDA_CU_API Val* sub_alpha(Val* v1, Val* v2, Val* s);
TORCH_CUDA_CU_API TensorView* sub_alpha(TensorView* v1, Val* v2, Val* s);
TORCH_CUDA_CU_API TensorView* sub_alpha(Val* v1, TensorView* v2, Val* s);
TORCH_CUDA_CU_API TensorView* sub_alpha(TensorView* v1, TensorView* v2, Val* s);
// lerp
TORCH_CUDA_CU_API Val* lerp(Val* start, Val* end, Val* weight);
TORCH_CUDA_CU_API TensorView* lerp(TensorView* start, Val* end, Val* weight);
TORCH_CUDA_CU_API TensorView* lerp(Val* start, TensorView* end, Val* weight);
TORCH_CUDA_CU_API TensorView* lerp(Val* start, Val* end, TensorView* weight);
TORCH_CUDA_CU_API TensorView* lerp(
    TensorView* start,
    TensorView* end,
    Val* weight);
TORCH_CUDA_CU_API TensorView* lerp(
    TensorView* start,
    Val* end,
    TensorView* weight);
TORCH_CUDA_CU_API TensorView* lerp(
    Val* start,
    TensorView* end,
    TensorView* weight);
TORCH_CUDA_CU_API TensorView* lerp(
    TensorView* start,
    TensorView* end,
    TensorView* weight);
// addcmul
TORCH_CUDA_CU_API Val* addcmul(Val* v1, Val* v2, Val* v3, Val* s);
TORCH_CUDA_CU_API TensorView* addcmul(TensorView* v1, Val* v2, Val* v3, Val* s);
TORCH_CUDA_CU_API TensorView* addcmul(Val* v1, TensorView* v2, Val* v3, Val* s);
TORCH_CUDA_CU_API TensorView* addcmul(Val* v1, Val* v2, TensorView* v3, Val* s);
TORCH_CUDA_CU_API TensorView* addcmul(
    TensorView* v1,
    TensorView* v2,
    Val* v3,
    Val* s);
TORCH_CUDA_CU_API TensorView* addcmul(
    TensorView* v1,
    Val* v2,
    TensorView* v3,
    Val* s);
TORCH_CUDA_CU_API TensorView* addcmul(
    Val* v1,
    TensorView* v2,
    TensorView* v3,
    Val* s);
TORCH_CUDA_CU_API TensorView* addcmul(
    TensorView* v1,
    TensorView* v2,
    TensorView* v3,
    Val* s);

// TERNARY OPERATIONS
// where
TORCH_CUDA_CU_API Val* where(Val* c, Val* v1, Val* v2);
TORCH_CUDA_CU_API TensorView* where(TensorView* c, Val* v1, Val* v2);
TORCH_CUDA_CU_API TensorView* where(Val* c, TensorView* v1, Val* v2);
TORCH_CUDA_CU_API TensorView* where(Val* c, Val* v1, TensorView* v2);
TORCH_CUDA_CU_API TensorView* where(TensorView* c, TensorView* v1, Val* v2);
TORCH_CUDA_CU_API TensorView* where(TensorView* c, Val* v1, TensorView* v2);
TORCH_CUDA_CU_API TensorView* where(Val* c, TensorView* v1, TensorView* v2);
TORCH_CUDA_CU_API TensorView* where(
    TensorView* c,
    TensorView* v1,
    TensorView* v2);
// threshold
TORCH_CUDA_CU_API Val* threshold(Val* in, Val* thresh, Val* value);
TORCH_CUDA_CU_API TensorView* threshold(
    TensorView* in,
    Val* thresh,
    Val* value);
// clamp
TORCH_CUDA_CU_API Val* clamp(Val* in, Val* min_val, Val* max_val);
TORCH_CUDA_CU_API TensorView* clamp(TensorView* in, Val* min_val, Val* max_val);

//! Internal operator for supporting backward graphs
//!
//! example:
//!   v1 = T1 [I0(10),I1(20),I2(30),I3(40)]
//!   v2 = sum_to(v1,{30,1}) ------> v2 = T2[I2,R3 (keep_dim)]
//!
//!  This operator will return v1* directly if sizes of v1 root domain
//!  is already the same as shape.
//!
//!  Name of sum_to is different from NV fuser naming,
//!  this is to align with the operator name of at::sum_to.

TORCH_CUDA_CU_API TensorView* sum_to(
    TensorView* v1,
    const std::vector<Int*>& sum_to_size);

TORCH_CUDA_CU_API TensorView* sum_to(
    TensorView* v1,
    const std::vector<int64_t>& sum_to_size);

//! Shift a tensor to a direction specified by offsets.
//!
//! Example:
//!   t0: 2D tensor of size N by M
//!   t1 = shift(t0, {1, -1});
//!
//!   then:
//!     t1[i, j] = t0[i-1, j+1] for 1 <= i < N and 0 <= j < M-1.
//!     t1[i, j] = 0, otherwise
//!
//! The pad option controls how out-of-boundary accesses are
//! handled. When pad is true, shifting works as if the source tensor
//! is padded by zero. Otherwise, it does not modify the output tensor
//! region whose source coordinates are out-of-boundry. In both cases,
//! the size of output tensor does not change. However, when pad is
//! false, the start or stop value of the shifted axis is adjusted
//! accordingly. For example, when a shift offset is one, the axis start
//! value would be incremented by one.
//!
//! \param pad If true, out-of-boundary access returns zero.
TORCH_CUDA_CU_API TensorView* shift(
    TensorView* inp,
    const std::vector<int>& offsets,
    bool pad = true);

//! Gather a window of nearby elements for each element.
//!
//! Each window of size window_shape is stored as a additional
//! innermost domain, meaning that the number of dimensions of the
//! output tensor doubles. The pad_width parameter specifies the
//! padding width of each side of each axis.
//!
//! Example:
//!   t0: 2D tensor of [N, M]
//!   t1 = gather(t0, {1, 3}, {{0, 0}, {1, 1}});
//!
//!   then:
//!     t1: [N, M, 1, 3]
//!     t1[i, j, k, l] = The value at the window position of [k, l]
//!                      for t0[i, j]
TORCH_CUDA_CU_API TensorView* gather(
    TensorView* inp,
    const std::vector<int>& window_shape,
    const std::vector<std::vector<int>>& pad_width);

//! Gather a window of nearby elements for each element.
//!
//! Same as the another gather interface but with Int* parameters.
TORCH_CUDA_CU_API TensorView* gather(
    TensorView* inp,
    const std::vector<Int*>& window_shape,
    const std::vector<std::vector<Int*>>& pad_width);

} // namespace cuda
} // namespace fuser
} // namespace jit
} // namespace torch
