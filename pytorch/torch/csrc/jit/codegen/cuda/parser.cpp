#include <torch/csrc/jit/codegen/cuda/parser.h>

#include <torch/csrc/jit/codegen/cuda/arith.h>
#include <torch/csrc/jit/codegen/cuda/instrumentation.h>
#include <torch/csrc/jit/codegen/cuda/ir_all_nodes.h>
#include <torch/csrc/jit/codegen/cuda/ir_iostream.h>
#include <torch/csrc/jit/codegen/cuda/ops/all_ops.h>
#include <torch/csrc/jit/codegen/cuda/type_promotion.h>
#include <torch/csrc/jit/codegen/cuda/utils.h>

#include <torch/csrc/jit/frontend/function_schema_parser.h>
#include <torch/csrc/jit/ir/constants.h>

#include <unordered_map>
#include <utility>

namespace torch {
namespace jit {

typedef Value JitValue;
typedef Node JitOp;

namespace fuser {
namespace cuda {

constexpr auto kNumUnaryOps = 10;
constexpr auto kNumUnaryFloatOps = 23;

constexpr auto kNumBinaryFloatOps = 3;
constexpr auto kNumBinaryComparisonOps = 12;
constexpr auto kNumBinaryCastOps = 14;

constexpr auto kNumBinaryOpsWithAlpha = 4;
constexpr auto kNumLerpOps = 2;
constexpr auto kNumLayernormFwd = 2;
constexpr auto kNumBatchnormFwd = 3;
constexpr auto kNumInstancenormFwd = 1;
constexpr auto kNumSumToSize = 2;
constexpr auto kNumAutocastOps = 2;

namespace {

#define REGISTER_PARSE_RULE(op, func_body, ...)                                \
  registerParseRule(                                                           \
      op,                                                                      \
      [](const Node* node, std::unordered_map<size_t, ValueHolder>& value_map) \
          -> void func_body,                                                   \
      __VA_ARGS__)

const auto& sizeAttr = Symbol::attr("profiled_size");
const auto& intListAttr = Symbol::attr("profiled_int_list");
const auto& intAttr = Symbol::attr("profiled_int");
const auto& boolListAttr = Symbol::attr("profiled_bool_list");
const auto& boolAttr = Symbol::attr("profiled_bool");

typedef Val* CgValue;
typedef Expr* CgOp;

// Note [ Format Bookkeeping and Propagation in Parser ]
//
// The goal in supporting format propagation in parser is to:
//   1. resolves conflicts and propagate format to output;
//   2. bookkeeping of format on existing tensors;
//
// The requirement right now is that all parsing rules should support
// `contiguous` inputs with few operation supports `channels_last` inputs. In
// case where "wrong" inputs are fed to an operation, we should transpose it to
// proper format. This allows us to progressively expand `channels_last`
// support. Currently we bind all formats of a codegen Val in `ValueHolder`.
// This saves unnecessary transpose (not sure if it actually helps).
//
// Parsing rule pattern:
// a. format agnostic ops (e.g. PW unary op like aten::add)
//
//    // getConsistentValues -> return target format and copies of operands in
//    // the same format
//    auto [format, lhs, rhs] = getConsistentValues(
//    c10::nullopt,
//    value_map[node->inputs()[0]->unique()],
//    value_map[node->inputs()[1]->unique()]);
//
//    // compute out
//    auto out = binaryOp(op_mapping[node->kind()], lhs, rhs);
//    // specify `format` for out when adding it to `value_map_`
//    value_map.emplace(node->output()->unique(), ValueHolder(out, format));
//
// b. op that doesn't support `channels_last` yet (e.g. sum)
//
//    // Specifying `MemoryFormat::Contiguous` here to force all inputs to be in
//    // `Contiguous`
//    auto [format, self] = getConsistentValues(
//        MemoryFormat::Contiguous,
//        value_map[node->inputs()[0]->unique()]);
//    // ... use self
//
// c. diverged path (e.g. aten::batch_norm)

// lower number has higher precedence, so order matters here and we currently
// prioritize `ChannelsLast`
enum class MemoryFormat { ChannelsLast = 0, Contiguous = 1 };

// return format with higher precedence, this is used in folding expression
MemoryFormat operator+(const MemoryFormat& a, const MemoryFormat& b) {
  return a <= b ? a : b;
};

class ValueHolder {
 public:
  // checks if given Val in target format exists.
  bool hasValue(MemoryFormat format) const {
    return vals_.count(format) != 0;
  }

  // returns Val in target format.
  CgValue value(MemoryFormat format) const {
    auto iter_val = vals_.find(format);
    TORCH_INTERNAL_ASSERT(
        iter_val != vals_.end(), "accessing non existing c_last_value()");
    return iter_val->second;
  }

  // returns Val in target format if it exists, otherwise, transpose an existing
  // copy and add that to bookkeeping.
  CgValue maybeConvertValue(MemoryFormat format) {
    auto iter_val = vals_.find(format);
    if (iter_val != vals_.end()) {
      return iter_val->second;
    }
    // patching scalar value, because memory format doesn't carry real meaning.
    if (!is_tensor_view_) {
      return std::get<1>(getEntry());
    }
    MemoryFormat format_s = MemoryFormat::Contiguous;
    CgValue value_s = nullptr;
    std::tie(format_s, value_s) = getEntry();
    auto val = convertValue(format, format_s, value_s);
    vals_[format] = val;
    return val;
  }

  int rank() const {
    if (!is_tensor_view_) {
      return -1;
    } else {
      auto v = std::get<1>(getEntry());
      TORCH_INTERNAL_ASSERT(
          v->isA<TensorView>(), "can only access rank of TensorView");
      return static_cast<int>(v->as<TensorView>()->nDims());
    }
  }

  // TODO: delete this and update accessor for value_map(_)
  ValueHolder() {
    TORCH_INTERNAL_ASSERT(false, "can't default constructor ValueHolder");
  }

  ValueHolder(CgValue val, MemoryFormat format = MemoryFormat::Contiguous) {
    vals_[format] = val;
    if (val->isA<TensorView>()) {
      is_tensor_view_ = true;
    }
  }

  // returns the MemoryFormat and codegen Val with the highest precedence among
  // existing copies.
  std::tuple<MemoryFormat, CgValue> getEntry() const {
    static auto formats = {
        MemoryFormat::ChannelsLast, MemoryFormat::Contiguous};
    for (const auto& format : formats) {
      auto iter_val = vals_.find(format);
      if (iter_val != vals_.end()) {
        return {format, iter_val->second};
      }
    }
    TORCH_CHECK(false, "accessing empty ValueHolder");
  }

  // TODO: code cleaning in parser so we don't need these.
  // returns Val*, keeping them here just so we have less code change.
  CgValue operator*() const {
    return std::get<1>(getEntry());
  }
  CgValue operator->() const {
    return std::get<1>(getEntry());
  }
  operator CgValue() const {
    return std::get<1>(getEntry());
  }

 private:
  // helper function to convert value_s @ format_s to format_d
  CgValue convertValue(
      MemoryFormat format_d,
      MemoryFormat format_s,
      CgValue value_s) {
    TORCH_INTERNAL_ASSERT(
        value_s->isA<TensorView>(), "cannot convert non-TensorView");
    auto tv = value_s->as<TensorView>();
    CgValue value_d = nullptr;
    auto n_dim = tv->nDims();
    switch (switch_pair(format_d, format_s)) {
      case switch_pair(MemoryFormat::ChannelsLast, MemoryFormat::Contiguous): {
        std::unordered_map<int, int> permutation_axes;
        for (const auto i : c10::irange(n_dim - 2)) {
          permutation_axes[n_dim - 1 - i] = n_dim - 2 - i;
        }
        permutation_axes[1] =
            n_dim - 1; // {{n-1, n-2}, {n-2, n-3}, ... {1, n-1}}
        value_d = transpose(tv, permutation_axes);
        break;
      }
      case switch_pair(MemoryFormat::Contiguous, MemoryFormat::ChannelsLast): {
        std::unordered_map<int, int> permutation_axes;
        for (const auto i : c10::irange(n_dim - 2)) {
          permutation_axes[1 + i] = 2 + i;
        }
        permutation_axes[n_dim - 1] = 1; // {{1, 2}, {2, 3}, ... {n-1, 1}}
        value_d = transpose(tv, permutation_axes);
        break;
      }
      default:
        TORCH_INTERNAL_ASSERT(false, "unrecognized format conversion pair");
        break;
    }
    return value_d;
  }

 private:
  // container to hold all copies of value in different MemoryFormat
  std::unordered_map<MemoryFormat, CgValue> vals_;

  // identify scalar Val
  bool is_tensor_view_ = false;
};

template <class Func, class... Values>
auto iterate(Func f, ValueHolder& val) {
  return f(val);
}

template <class Func, class... Values>
auto iterate(Func f, ValueHolder& val, Values&... vals) {
  return f(val, iterate(f, vals...));
}

// iterate through all vals and return the output MemoryFormat and copies of
// vals.
//   1. When `forced_format == c10::nullopt`, target MemoryFormat returns the
//   highest precedenc among `vals`.
//   2. The target can be overwritten vias specifying `forced_format`.
//
// Note: take `Values&` by reference, since `maybeConvertValue` needs to modify
// the entry and we want that to be updated in `value_map_`
template <class... Values>
std::pair<MemoryFormat, std::list<CgValue>> getConsistentValues(
    c10::optional<MemoryFormat> forced_format,
    Values&... vals) {
  MemoryFormat format = MemoryFormat::Contiguous;
  if (forced_format.has_value()) {
    format = forced_format.value();
  } else {
    // check for identical nDim on vals
    auto rank_func = [](const ValueHolder& val, int rank = 0) {
      int v_rank = val.rank();
      v_rank = std::max(0, v_rank);
      if (rank == 0) {
        return v_rank;
      } else if (v_rank == 0) {
        return rank;
      } else if (rank == -1 || v_rank != rank) {
        return -1;
      }
      return rank;
    };
    int rank = iterate(rank_func, vals...);

    // only go channels_last when all inputs are of identical rank.
    // Consider pointwise operation between two tensor [N, C, H, W] + [H, W]
    if (rank > 0) {
      auto format_func = [](const ValueHolder& val,
                            MemoryFormat f = MemoryFormat::Contiguous) {
        return std::get<0>(val.getEntry()) + f;
      };
      format = iterate(format_func, vals...);
    }
  }

  auto convert_func = [format](
                          ValueHolder& val, std::list<CgValue> list_val = {}) {
    list_val.push_front(val.maybeConvertValue(format));
    return list_val;
  };
  auto list_val = iterate(convert_func, vals...);

  return std::make_pair(format, list_val);
}

typedef void (
    *ParseFuncPtr)(const Node*, std::unordered_map<size_t, ValueHolder>&);
typedef bool (*MergeQueryFuncPtr)(const Node*);

// TODO: add a mutex to make it thread safe.
class IrParser {
  enum class OperatorType {
    ElementWise,
    Reduction,
    ReductionToSize,
    Normalization
  };
  typedef OperatorType (*OperatorTypeFuncPtr)(const Node*);

  class RegistrationEntry {
   public:
    RegistrationEntry(
        ParseFuncPtr parse_f,
        MergeQueryFuncPtr merge_f = nullptr,
        OperatorTypeFuncPtr type_f = nullptr)
        : parse_f_(parse_f), merge_f_(merge_f), type_f_(type_f) {}

    void parse(
        const Node* node,
        std::unordered_map<size_t, ValueHolder>& values) const {
      parse_f_(node, values);
    }

    bool isCompatible(const Node* node) const {
      if (merge_f_ == nullptr) {
        return true;
      }
      return merge_f_(node);
    }

    bool isType(const Node* node, OperatorType type) const {
      auto n_type =
          type_f_ == nullptr ? OperatorType::ElementWise : type_f_(node);
      return n_type == type;
    }

   private:
    ParseFuncPtr parse_f_;
    MergeQueryFuncPtr merge_f_;
    OperatorTypeFuncPtr type_f_;
  };

 public:
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
  IrParser(std::shared_ptr<Graph> graph) : graph_(std::move(graph)) {
    initRegistry();
  }

  std::unique_ptr<Fusion> parse() {
    auto fusion = std::make_unique<Fusion>();
    // NOLINTNEXTLINE(cppcoreguidelines-init-variables)
    FusionGuard fg(fusion.get());
    auto block = graph_->block();

    std::unordered_set<Val*> c_last_tensors;
    // register all inputs;
    for (auto val : block->inputs()) {
      TORCH_INTERNAL_ASSERT(
          registerValue(val),
          "Failure when register value: ",
          *(val->node()),
          " with type: ",
          val->type());
      MemoryFormat format = MemoryFormat::Contiguous;
      Val* operand = nullptr;
      std::tie(format, operand) = value_map_[val->unique()].getEntry();
      fusion->addInput(operand);

      // mark input tensor as channels last;
      if (format == MemoryFormat::ChannelsLast) {
        c_last_tensors.insert(operand);
      }

      auto opt_dtype = operand->getDataType();
      // computation promotion, we cast fp16 or bf16 inputs to fp32 and use
      // promoted type in the computation.
      if (opt_dtype.has_value() &&
          (opt_dtype.value() == DataType::Half ||
           opt_dtype.value() == DataType::BFloat16)) {
        Val* promoted_val = castOp(DataType::Float, operand);
        // value_map_.emplace(val->unique(), ValueHolder(promoted_val, format));
        value_map_[val->unique()] = ValueHolder(promoted_val, format);
      }
    }

    // compose nodes in topo order;
    for (const JitOp* node : block->nodes()) {
      processJitNode(node);
    }

    // mark output;
    for (auto jit_output : block->outputs()) {
      auto& value_holder = value_map_[jit_output->unique()];
      TensorView* out = value_holder->as<TensorView>();
      // demote output dtype to be match PyTorch JIT graph.
      auto tensor_type = jit_output->type()->cast<TensorType>();
      TORCH_INTERNAL_ASSERT(
          tensor_type, "output of fusion group is not TensorType.");
      if (tensor_type->scalarType() == at::ScalarType::Half) {
        // No need to update value_map_ after this point.
        out = castOp(DataType::Half, out)->as<TensorView>();
      }
      if (tensor_type->scalarType() == at::ScalarType::BFloat16) {
        // No need to update value_map_ after this point.
        out = castOp(DataType::BFloat16, out)->as<TensorView>();
      }
      fusion->addOutput(out);

      // mark output tensor as channels last;
      if (value_holder.hasValue(MemoryFormat::ChannelsLast)) {
        c_last_tensors.insert(out);
      }
    }

    for (const auto& i : c10::irange(fusion->inputs().size())) {
      if (c_last_tensors.count(fusion->inputs()[i]) != 0) {
        fusion->setChannelsLastOnInput(i);
      }
    }
    for (const auto& i : c10::irange(fusion->outputs().size())) {
      if (c_last_tensors.count(fusion->outputs()[i]) != 0) {
        fusion->setChannelsLastOutputIndices(i);
      }
    }
    return fusion;
  }

  static bool lookupInSymbolSet(const Node* node) {
    initRegistry();

    return parser_symbol_set_.count(node->kind()) != 0;
  }

  // return nullptr if entry does not exist
  static const RegistrationEntry* lookupInRegistry(const Node* node) {
    // we need to use maybeSchema for nodes like prim::Constant, which doesn't
    // have a schema
    auto schema_ptr = node->maybeSchema();
    if (schema_ptr != nullptr) {
      // search cached entry first
      auto cache_it = cached_registry_lookup_.find(schema_ptr);
      if (cache_it != cached_registry_lookup_.end()) {
        return cache_it->second;
      } else {
        // match signature
        auto schema_str = canonicalSchemaString(*schema_ptr);

        auto iter = jit_operator_registry_.find(schema_str);
        if (iter != jit_operator_registry_.end()) {
          // update cache entry
          cached_registry_lookup_.insert(cache_it, {schema_ptr, &iter->second});
          return &iter->second;
        }
      }
    }
    return nullptr;
  }

  static void initRegistry() {
    if (init_registry_) {
      // TODO: mutex this guy;
      registerJitOperator();
      init_registry_ = false;
    }
  }

  static bool canParseNode(const Node* node) {
    initRegistry();

    // match signature.
    auto schema_ptr = node->maybeSchema();
    if (schema_ptr == nullptr) {
      return false;
    }
    auto reg_entry = lookupInRegistry(node);
    return reg_entry != nullptr && reg_entry->isCompatible(node);
  }

  static bool isReductionToSizeNode(const Node* node) {
    initRegistry();

    auto reg_entry = lookupInRegistry(node);
    return reg_entry != nullptr &&
        reg_entry->isType(node, OperatorType::ReductionToSize);
  }

  static bool isReductionNode(const Node* node) {
    initRegistry();

    auto reg_entry = lookupInRegistry(node);
    return reg_entry != nullptr &&
        (reg_entry->isType(node, OperatorType::Reduction) ||
         reg_entry->isType(node, OperatorType::ReductionToSize));
  }

  static bool isNormalizationNode(const Node* node) {
    initRegistry();

    auto reg_entry = lookupInRegistry(node);
    return reg_entry != nullptr &&
        reg_entry->isType(node, OperatorType::Normalization);
  }

  static bool isElementWiseNode(const Node* node) {
    initRegistry();

    auto reg_entry = lookupInRegistry(node);
    return reg_entry != nullptr &&
        reg_entry->isType(node, OperatorType::ElementWise);
  }

  // TODO: is_reduction is too hacky here. we should categorize operation types
  //       based on their memory accessing pattern, which would affect fusion
  //       strategy and partition logic.
  static void registerParseRule(
      std::shared_ptr<Operator>& op,
      ParseFuncPtr parse_fn,
      MergeQueryFuncPtr merge_query_fn = nullptr,
      OperatorTypeFuncPtr type_fn = nullptr) {
    auto op_name = op->schema().name();
    parser_symbol_set_.insert(c10::Symbol::fromQualString(op_name));
    // We blindly attempt to profile the inplace version of supported op, this
    // is to ensure that in-place removal in fusion partition would have the
    // profile information for them readily available after the pass.
    parser_symbol_set_.insert(c10::Symbol::fromQualString(op_name + '_'));
    jit_operator_registry_.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(canonicalSchemaString(op->schema())),
        std::forward_as_tuple(parse_fn, merge_query_fn, type_fn));
  }

 private:
  static void registerJitOperator() {
    // Register parse-function for each JIT operator;
    // This is a one-time look up, our hash registry indexes on the pointer in
    // OperatorRegistry.

    std::array<const char*, kNumBinaryOpsWithAlpha> BinaryOpWithAlpha = {
        "aten::add(Tensor self, Tensor other, *, Scalar alpha) -> Tensor",
        "aten::add(Tensor self, Scalar other, Scalar alpha) -> Tensor",
        "aten::sub(Tensor self, Tensor other, *, Scalar alpha) -> Tensor",
        "aten::sub(Tensor self, Scalar other, Scalar alpha) -> Tensor"};
    for (auto signature : BinaryOpWithAlpha) {
      auto ptr_op = getOperatorForLiteral(signature);
      REGISTER_PARSE_RULE(
          ptr_op,
          {
            using BinaryOpWithAlphaType = Val* (*)(Val*, Val*, Val*);
            static std::unordered_map<
                Symbol,
                std::pair<BinaryOpType, BinaryOpWithAlphaType>>
                op_mapping(
                    {{aten::add,
                      std::make_pair(
                          BinaryOpType::Add,
                          static_cast<BinaryOpWithAlphaType>(&add_alpha))},
                     {aten::sub,
                      std::make_pair(
                          BinaryOpType::Sub,
                          static_cast<BinaryOpWithAlphaType>(&sub_alpha))}});
            // TODO: handle scaling factor when it's not constant 1;
            MemoryFormat format;
            std::list<Val*> list_val;
            std::tie(format, list_val) = getConsistentValues(
                c10::nullopt,
                value_map[node->inputs()[0]->unique()],
                value_map[node->inputs()[1]->unique()]);
            auto lhs = list_val.front();
            list_val.pop_front();
            auto rhs = list_val.front();
            list_val.pop_front();
            Val* alpha = value_map[node->inputs()[2]->unique()];

            auto out = alpha->isOneInt()
                ? binaryOp(
                      op_mapping[node->kind()].first,
                      lhs,
                      rhs,
                      TypePromotion::default_op_config)
                : op_mapping[node->kind()].second(lhs, rhs, alpha);
            value_map.emplace(
                node->output()->unique(), ValueHolder(out, format));
          },
          nullptr,
          nullptr);
    }

    std::array<const char*, kNumBinaryFloatOps> BinaryFloatOp = {
        "aten::div(Tensor self, Tensor other) -> Tensor",
        "aten::div(Tensor self, Scalar other) -> Tensor",
        "aten::atan2(Tensor self, Tensor other) -> Tensor"};
    for (auto signature : BinaryFloatOp) {
      auto ptr_op = getOperatorForLiteral(signature);
      REGISTER_PARSE_RULE(
          ptr_op,
          {
            static std::unordered_map<Symbol, BinaryOpType> op_mapping(
                {{aten::div, BinaryOpType::Div},
                 {aten::atan2, BinaryOpType::Atan2}});

            MemoryFormat format;
            std::list<Val*> list_val;
            std::tie(format, list_val) = getConsistentValues(
                c10::nullopt,
                value_map[node->inputs()[0]->unique()],
                value_map[node->inputs()[1]->unique()]);
            auto lhs = list_val.front();
            list_val.pop_front();
            auto rhs = list_val.front();
            list_val.pop_front();

            auto out = binaryOp(
                op_mapping[node->kind()],
                lhs,
                rhs,
                TypePromotion::float_op_config);
            value_map.emplace(
                node->output()->unique(), ValueHolder(out, format));
          },
          nullptr,
          nullptr);
    }

    std::array<const char*, kNumBinaryCastOps> BinaryCastOp = {
        "aten::mul(Tensor self, Tensor other) -> Tensor",
        "aten::mul(Tensor self, Scalar other) -> Tensor",
        "aten::max(Tensor self, Tensor other) -> Tensor",
        "aten::min(Tensor self, Tensor other) -> Tensor",
        "aten::pow(Tensor self, Tensor exponent) -> Tensor",
        "aten::pow(Tensor self, Scalar exponent) -> Tensor",
        "aten::pow(Scalar self, Tensor exponent) -> Tensor",
        "aten::remainder(Tensor self, Tensor other) -> Tensor",
        "aten::fmod(Tensor self, Tensor other) -> Tensor",
        "aten::__and__(Tensor self, Tensor other) -> Tensor",
        "aten::__or__(Tensor self, Tensor other) -> Tensor",
        "aten::__xor__(Tensor self, Tensor other) -> Tensor",
        "aten::__lshift__(Tensor self, Tensor other) -> Tensor",
        "aten::__rshift__(Tensor self, Tensor other) -> Tensor"};
    for (auto signature : BinaryCastOp) {
      auto ptr_op = getOperatorForLiteral(signature);
      REGISTER_PARSE_RULE(
          ptr_op,
          {
            static std::unordered_map<Symbol, BinaryOpType> op_mapping(
                {{aten::mul, BinaryOpType::Mul},
                 {aten::min, BinaryOpType::Min},
                 {aten::max, BinaryOpType::Max},
                 {aten::pow, BinaryOpType::Pow},
                 {aten::remainder, BinaryOpType::Remainder},
                 {aten::fmod, BinaryOpType::Fmod},
                 {aten::__and__, BinaryOpType::And},
                 {aten::__or__, BinaryOpType::Or},
                 {aten::__xor__, BinaryOpType::Xor},
                 {aten::__lshift__, BinaryOpType::Lshift},
                 {aten::__rshift__, BinaryOpType::Rshift}});

            MemoryFormat format;
            std::list<Val*> list_val;
            std::tie(format, list_val) = getConsistentValues(
                c10::nullopt,
                value_map[node->inputs()[0]->unique()],
                value_map[node->inputs()[1]->unique()]);
            auto lhs = list_val.front();
            list_val.pop_front();
            auto rhs = list_val.front();
            list_val.pop_front();

            auto out = binaryOp(
                op_mapping[node->kind()],
                lhs,
                rhs,
                TypePromotion::default_op_config);
            value_map.emplace(
                node->output()->unique(), ValueHolder(out, format));
          },
          nullptr,
          nullptr);
    }

    std::array<const char*, kNumBinaryComparisonOps> BinaryOp = {
        "aten::eq(Tensor self, Tensor other) -> Tensor",
        "aten::eq(Tensor self, Scalar other) -> Tensor",
        "aten::ne(Tensor self, Tensor other) -> Tensor",
        "aten::ne(Tensor self, Scalar other) -> Tensor",
        "aten::ge(Tensor self, Tensor other) -> Tensor",
        "aten::ge(Tensor self, Scalar other) -> Tensor",
        "aten::gt(Tensor self, Tensor other) -> Tensor",
        "aten::gt(Tensor self, Scalar other) -> Tensor",
        "aten::le(Tensor self, Tensor other) -> Tensor",
        "aten::le(Tensor self, Scalar other) -> Tensor",
        "aten::lt(Tensor self, Tensor other) -> Tensor",
        "aten::lt(Tensor self, Scalar other) -> Tensor"};
    for (auto signature : BinaryOp) {
      auto ptr_op = getOperatorForLiteral(signature);
      REGISTER_PARSE_RULE(
          ptr_op,
          {
            static std::unordered_map<Symbol, BinaryOpType> op_mapping(
                {{aten::lt, BinaryOpType::LT},
                 {aten::le, BinaryOpType::LE},
                 {aten::gt, BinaryOpType::GT},
                 {aten::ge, BinaryOpType::GE},
                 {aten::ne, BinaryOpType::NE},
                 {aten::eq, BinaryOpType::Eq}});

            MemoryFormat format;
            std::list<Val*> list_val;
            std::tie(format, list_val) = getConsistentValues(
                c10::nullopt,
                value_map[node->inputs()[0]->unique()],
                value_map[node->inputs()[1]->unique()]);
            auto lhs = list_val.front();
            list_val.pop_front();
            auto rhs = list_val.front();
            list_val.pop_front();

            auto out = binaryOp(
                op_mapping[node->kind()],
                lhs,
                rhs,
                TypePromotion::comparison_op_config);
            value_map.emplace(
                node->output()->unique(), ValueHolder(out, format));
          },
          nullptr,
          nullptr);
    }

    std::array<const char*, kNumUnaryOps> UnaryOp = {
        "aten::abs(Tensor self) -> Tensor",
        "aten::bitwise_not(Tensor self) -> Tensor",
        "aten::ceil(Tensor self) -> Tensor",
        "aten::floor(Tensor self) -> Tensor",
        "aten::frac(Tensor self) -> Tensor",
        "aten::neg(Tensor self) -> Tensor",
        "aten::relu(Tensor self) -> Tensor",
        "aten::round(Tensor self) -> Tensor",
        "aten::silu(Tensor self) -> Tensor",
        "aten::trunc(Tensor self) -> Tensor",
    };
    for (auto signature : UnaryOp) {
      auto ptr_op = getOperatorForLiteral(signature);
      REGISTER_PARSE_RULE(
          ptr_op,
          {
            static std::unordered_map<Symbol, UnaryOpType> op_mapping({
                {aten::abs, UnaryOpType::Abs},
                {aten::bitwise_not, UnaryOpType::Not},
                {aten::ceil, UnaryOpType::Ceil},
                {aten::floor, UnaryOpType::Floor},
                {aten::frac, UnaryOpType::Frac},
                {aten::neg, UnaryOpType::Neg},
                {aten::relu, UnaryOpType::Relu},
                {aten::round, UnaryOpType::Round},
                {aten::silu, UnaryOpType::Silu},
                {aten::trunc, UnaryOpType::Trunc},
            });
            MemoryFormat format;
            std::list<Val*> list_val;
            std::tie(format, list_val) = getConsistentValues(
                c10::nullopt, value_map[node->inputs()[0]->unique()]);
            auto operand = list_val.front();
            list_val.pop_front();
            auto out = unaryOp(op_mapping[node->kind()], operand);
            value_map.emplace(
                node->output()->unique(), ValueHolder(out, format));
          },
          nullptr,
          nullptr);
    }

    std::array<const char*, kNumUnaryFloatOps> UnaryFloatOp = {
        "aten::log(Tensor self) -> Tensor",
        "aten::log10(Tensor self) -> Tensor",
        "aten::log1p(Tensor self) -> Tensor",
        "aten::log2(Tensor self) -> Tensor",
        "aten::lgamma(Tensor self) -> Tensor",
        "aten::exp(Tensor self) -> Tensor",
        "aten::expm1(Tensor self) -> Tensor",
        "aten::erf(Tensor self) -> Tensor",
        "aten::erfc(Tensor self) -> Tensor",
        "aten::cos(Tensor self) -> Tensor",
        "aten::acos(Tensor self) -> Tensor",
        "aten::cosh(Tensor self) -> Tensor",
        "aten::sin(Tensor self) -> Tensor",
        "aten::asin(Tensor self) -> Tensor",
        "aten::sinh(Tensor self) -> Tensor",
        "aten::tan(Tensor self) -> Tensor",
        "aten::atan(Tensor self) -> Tensor",
        "aten::tanh(Tensor self) -> Tensor",
        "aten::atanh(Tensor self) -> Tensor",
        "aten::sqrt(Tensor self) -> Tensor",
        "aten::rsqrt(Tensor self) -> Tensor",
        "aten::reciprocal(Tensor self) -> Tensor",
        "aten::sigmoid(Tensor self) -> Tensor"};
    for (auto signature : UnaryFloatOp) {
      auto ptr_op = getOperatorForLiteral(signature);
      REGISTER_PARSE_RULE(
          ptr_op,
          {
            static std::unordered_map<Symbol, UnaryOpType> op_mapping({
                {aten::log, UnaryOpType::Log},
                {aten::log10, UnaryOpType::Log10},
                {aten::log1p, UnaryOpType::Log1p},
                {aten::log2, UnaryOpType::Log2},
                {aten::lgamma, UnaryOpType::Lgamma},
                {aten::exp, UnaryOpType::Exp},
                {aten::expm1, UnaryOpType::Expm1},
                {aten::erf, UnaryOpType::Erf},
                {aten::erfc, UnaryOpType::Erfc},
                {aten::cos, UnaryOpType::Cos},
                {aten::acos, UnaryOpType::Acos},
                {aten::cosh, UnaryOpType::Cosh},
                {aten::sin, UnaryOpType::Sin},
                {aten::asin, UnaryOpType::Asin},
                {aten::sinh, UnaryOpType::Sinh},
                {aten::tan, UnaryOpType::Tan},
                {aten::tanh, UnaryOpType::Tanh},
                {aten::atan, UnaryOpType::Atan},
                {aten::atanh, UnaryOpType::Atanh},
                {aten::sqrt, UnaryOpType::Sqrt},
                {aten::rsqrt, UnaryOpType::Rsqrt},
                {aten::reciprocal, UnaryOpType::Reciprocal},
                {aten::sigmoid, UnaryOpType::Sigmoid},
            });
            MemoryFormat format;
            std::list<Val*> list_val;
            std::tie(format, list_val) = getConsistentValues(
                c10::nullopt, value_map[node->inputs()[0]->unique()]);
            auto operand = list_val.front();
            list_val.pop_front();
            auto out = unaryOp(
                op_mapping[node->kind()],
                operand,
                TypePromotion::float_op_config);
            value_map.emplace(
                node->output()->unique(), ValueHolder(out, format));
          },
          nullptr,
          nullptr);
    }

    {
      auto ptr_op = getOperatorForLiteral(
          "aten::rand_like(Tensor self, *, ScalarType? dtype=None, Layout? layout=None, Device? device=None, bool? pin_memory=None, MemoryFormat? memory_format=None) -> Tensor");
      REGISTER_PARSE_RULE(
          ptr_op,
          {
            MemoryFormat format;
            std::list<Val*> list_val;
            std::tie(format, list_val) = getConsistentValues(
                MemoryFormat::Contiguous,
                value_map[node->inputs()[0]->unique()]);
            auto operand = list_val.front();
            list_val.pop_front();

            auto out = randlike(operand);
            value_map.emplace(node->output()->unique(), out);
          },
          nullptr,
          nullptr);
    }

    {
      auto ptr_op = getOperatorForLiteral(
          "aten::softplus(Tensor self, Scalar beta, Scalar threshold) -> Tensor");
      REGISTER_PARSE_RULE(
          ptr_op,
          {
            MemoryFormat format;
            std::list<Val*> list_val;
            std::tie(format, list_val) = getConsistentValues(
                MemoryFormat::Contiguous,
                value_map[node->inputs()[0]->unique()]);
            auto operand = list_val.front();
            list_val.pop_front();
            auto& beta = value_map[node->inputs()[1]->unique()];
            auto& threshold = value_map[node->inputs()[2]->unique()];
            auto out = softplus(operand, beta, threshold);
            value_map.emplace(node->output()->unique(), out);
          },
          nullptr,
          nullptr);
    }

    {
      auto ptr_op = getOperatorForLiteral(
          "aten::threshold(Tensor self, Scalar threshold, Scalar value) -> Tensor");
      REGISTER_PARSE_RULE(
          ptr_op,
          {
            MemoryFormat format;
            std::list<Val*> list_val;
            std::tie(format, list_val) = getConsistentValues(
                MemoryFormat::Contiguous,
                value_map[node->inputs()[0]->unique()]);
            auto operand = list_val.front();
            list_val.pop_front();
            auto& th = value_map[node->inputs()[1]->unique()];
            auto& value = value_map[node->inputs()[2]->unique()];

            auto out = threshold(operand, th, value);
            value_map.emplace(node->output()->unique(), out);
          },
          nullptr,
          nullptr);
    }

    {
      auto ptr_op = getOperatorForLiteral(
          "aten::clamp(Tensor self, Scalar? min, Scalar? max) -> Tensor");
      REGISTER_PARSE_RULE(
          ptr_op,
          {
            MemoryFormat format;
            std::list<Val*> list_val;
            std::tie(format, list_val) = getConsistentValues(
                c10::nullopt, value_map[node->inputs()[0]->unique()]);
            auto operand = list_val.front();
            list_val.pop_front();
            Val* low = value_map.count(node->inputs()[1]->unique()) != 0
                ? *value_map[node->inputs()[1]->unique()]
                : new Double(std::numeric_limits<float>::min());
            Val* high = value_map.count(node->inputs()[2]->unique()) != 0
                ? *value_map[node->inputs()[2]->unique()]
                : new Double(std::numeric_limits<float>::max());

            auto out = clamp(operand, low, high);
            value_map.emplace(node->output()->unique(), out);
          },
          nullptr,
          nullptr);
    }

    {
      auto ptr_op = getOperatorForLiteral(
          "aten::where(Tensor condition, Tensor self, Tensor other) -> Tensor");
      REGISTER_PARSE_RULE(
          ptr_op,
          {
            MemoryFormat format;
            std::list<Val*> list_val;
            std::tie(format, list_val) = getConsistentValues(
                MemoryFormat::Contiguous,
                value_map[node->inputs()[0]->unique()],
                value_map[node->inputs()[1]->unique()],
                value_map[node->inputs()[2]->unique()]);
            auto condition = list_val.front();
            list_val.pop_front();
            auto x = list_val.front();
            list_val.pop_front();
            auto y = list_val.front();
            list_val.pop_front();

            auto out = where(condition, x, y);
            value_map.emplace(
                node->output()->unique(), ValueHolder(out, format));
          },
          nullptr,
          nullptr);
    }

    {
      std::array<const char*, kNumLerpOps> LerpOp = {
          "aten::lerp(Tensor self, Tensor end, Scalar weight) -> Tensor",
          "aten::lerp(Tensor self, Tensor end, Tensor weight) -> Tensor"};
      for (auto signature : LerpOp) {
        auto ptr_op = getOperatorForLiteral(signature);
        REGISTER_PARSE_RULE(
            ptr_op,
            {
              MemoryFormat format;
              std::list<Val*> list_val;
              std::tie(format, list_val) = getConsistentValues(
                  MemoryFormat::Contiguous,
                  value_map[node->inputs()[0]->unique()],
                  value_map[node->inputs()[1]->unique()],
                  value_map[node->inputs()[2]->unique()]);
              auto self = list_val.front();
              list_val.pop_front();
              auto end = list_val.front();
              list_val.pop_front();
              auto weight = list_val.front();
              list_val.pop_front();

              auto out = lerp(self, end, weight);
              value_map.emplace(
                  node->output()->unique(), ValueHolder(out, format));
            },
            nullptr,
            nullptr);
      }
    }

    {
      auto ptr_op = getOperatorForLiteral(
          "aten::addcmul(Tensor self, Tensor tensor1, Tensor tensor2, *, Scalar value=1) -> Tensor");
      REGISTER_PARSE_RULE(
          ptr_op,
          {
            MemoryFormat format;
            std::list<Val*> list_val;
            std::tie(format, list_val) = getConsistentValues(
                c10::nullopt,
                value_map[node->inputs()[0]->unique()],
                value_map[node->inputs()[1]->unique()],
                value_map[node->inputs()[2]->unique()],
                value_map[node->inputs()[3]->unique()]);
            auto self = list_val.front();
            list_val.pop_front();
            auto tensor1 = list_val.front();
            list_val.pop_front();
            auto tensor2 = list_val.front();
            list_val.pop_front();
            auto value = list_val.front();
            list_val.pop_front();

            auto out = addcmul(self, tensor1, tensor2, value);
            value_map.emplace(
                node->output()->unique(), ValueHolder(out, format));
          },
          nullptr,
          nullptr);
    }

#if false // temporarily disable this for dropout changes
    {
      auto ptr_op = getOperatorForLiteral(
          "aten::native_dropout(Tensor input, float p, float scale, bool train) -> (Tensor, Tensor)");
      REGISTER_PARSE_RULE(
          ptr_op,
          {
            MemoryFormat format;
            std::list<Val*> list_val;
            std::tie(format, list_val) = getConsistentValues(
                MemoryFormat::Contiguous,
                value_map[node->inputs()[0]->unique()],
                value_map[node->inputs()[1]->unique()],
                value_map[node->inputs()[2]->unique()]);
            auto input = list_val.front();
            list_val.pop_front();
            auto prob = list_val.front();
            list_val.pop_front();
            auto scale = list_val.front();
            list_val.pop_front();
            auto train = constant_as<bool>(node->input(3));

            TORCH_INTERNAL_ASSERT(
                train.has_value() and train.value(),
                "Train parameter is incorrectly set to false!");

            auto result = dropout(input->as<TensorView>(), prob, scale);

            value_map.emplace(node->output(0)->unique(), result.output);
            value_map.emplace(node->output(1)->unique(), result.mask);
          },
          nullptr,
          nullptr);
    }
#endif

    {
      auto ptr_op = getOperatorForLiteral(
          "aten::dropout(Tensor input, float p, bool train) -> Tensor");
      REGISTER_PARSE_RULE(
          ptr_op,
          {
            MemoryFormat format;
            std::list<Val*> list_val;
            std::tie(format, list_val) = getConsistentValues(
                MemoryFormat::Contiguous,
                value_map[node->inputs()[0]->unique()],
                value_map[node->inputs()[1]->unique()]);
            auto input = list_val.front();
            list_val.pop_front();
            auto prob = list_val.front();
            list_val.pop_front();

            auto train = constant_as<bool>(node->input(2));
            TORCH_INTERNAL_ASSERT(
                train.has_value(), "dropout needs constant `train` flag");

            if (train.value()) {
              auto result = dropout(input->as<TensorView>(), prob);

              value_map.emplace(node->output()->unique(), result.output);
            } else {
              value_map.emplace(node->output()->unique(), input);
            }
          },
          nullptr,
          nullptr);
    }

#if false // temporarily disable this for dropout changes
    {
      auto ptr_op = getOperatorForLiteral(
          "aten::native_dropout_backward(Tensor grad, Tensor mask, float scale) -> Tensor");
      REGISTER_PARSE_RULE(
          ptr_op,
          {
            MemoryFormat format;
            std::list<Val*> list_val;
            std::tie(format, list_val) = getConsistentValues(
                MemoryFormat::Contiguous,
                value_map[node->inputs()[0]->unique()],
                value_map[node->inputs()[1]->unique()],
                value_map[node->inputs()[2]->unique()]);
            auto grad = list_val.front();
            list_val.pop_front();
            auto mask = list_val.front();
            list_val.pop_front();
            auto scale = list_val.front();
            list_val.pop_front();

            auto output = dropout_backward(
                grad->as<TensorView>(), mask->as<TensorView>(), scale);
            value_map.emplace(node->output()->unique(), output);
          },
          nullptr,
          nullptr);
    }
#endif

    {
      std::array<const char*, kNumInstancenormFwd> InstanceNormFwd = {
          "aten::instance_norm(Tensor input, Tensor? weight, Tensor? bias, Tensor? running_mean, Tensor? running_var, bool use_input_stats, float momentum, float eps, bool cudnn_enabled) -> Tensor"};
      for (auto signature : InstanceNormFwd) {
        auto ptr_op = getOperatorForLiteral(signature);
        REGISTER_PARSE_RULE(
            ptr_op,
            {
              auto fusion = FusionGuard::getCurFusion();

              // TODO: handle channels last
              MemoryFormat format;
              std::list<Val*> list_val;
              std::tie(format, list_val) = getConsistentValues(
                  MemoryFormat::Contiguous,
                  value_map[node->inputs()[0]->unique()]);
              auto input_t = list_val.front();
              list_val.pop_front();
              auto input = input_t->as<TensorView>();

              TensorView* weight = nullptr;
              if (!node->input(1)->type()->isSubtypeOf(
                      static_cast<c10::TypePtr>(NoneType::get()))) {
                weight = value_map[node->input(1)->unique()]->as<TensorView>();
              }

              TensorView* bias = nullptr;
              if (!node->input(2)->type()->isSubtypeOf(
                      static_cast<c10::TypePtr>(NoneType::get()))) {
                bias = value_map[node->input(2)->unique()]->as<TensorView>();
              }

              TensorView* running_mean = nullptr;
              if (!node->input(3)->type()->isSubtypeOf(
                      static_cast<c10::TypePtr>(NoneType::get()))) {
                running_mean =
                    value_map[node->input(3)->unique()]->as<TensorView>();
                TORCH_INTERNAL_ASSERT(
                    fusion->hasInput(running_mean),
                    "IO_tensor `instance_norm::running_mean` can only be input tensor to fusion");
              }

              TensorView* running_var = nullptr;
              if (!node->input(4)->type()->isSubtypeOf(
                      static_cast<c10::TypePtr>(NoneType::get()))) {
                running_var =
                    value_map[node->input(4)->unique()]->as<TensorView>();
                TORCH_INTERNAL_ASSERT(
                    fusion->hasInput(running_var),
                    "IO_tensor `instance_norm::running_var` can only be input tensor to fusion");
              }

              // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
              auto use_input_stats = constant_as<bool>(node->input(5));
              TORCH_INTERNAL_ASSERT(
                  use_input_stats.has_value(),
                  "The use_input_stats (bool) parameter is required.");
              const bool kUseInputStats = use_input_stats.value();

              Val* momentum_ptr = nullptr;
              // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
              if (auto momentum = constant_as<float>(node->input(6))) {
                momentum_ptr = new Double(momentum.value());
              } else {
                // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
                momentum_ptr = value_map[node->input(6)->unique()];
              }

              Val* eps_ptr = nullptr;
              // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
              if (auto eps = constant_as<float>(node->input(7))) {
                eps_ptr = new Double(eps.value());
              } else {
                // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
                eps_ptr = value_map[node->input(7)->unique()];
              }

              auto result = instance_norm(
                  input,
                  weight,
                  bias,
                  running_mean,
                  running_var,
                  kUseInputStats,
                  momentum_ptr,
                  eps_ptr);

              if (node->kind() ==
                  c10::Symbol::fromQualString("aten::instance_norm")) {
                value_map.emplace(node->output()->unique(), result.output);
              }
            },
            [](const Node* node) -> bool { return true; },
            [](const Node* node) -> OperatorType {
              return OperatorType::Normalization;
            });
      }
    }

    {
      std::array<const char*, kNumBatchnormFwd> BatchNormFwd = {
          "aten::_batch_norm_impl_index(Tensor input, Tensor? weight, Tensor? bias, Tensor? running_mean, Tensor? running_var, bool training, float momentum, float eps, bool cudnn_enabled) -> (Tensor, Tensor, Tensor, Tensor, int)",
          "aten::native_batch_norm(Tensor input, Tensor? weight, Tensor? bias, Tensor? running_mean, Tensor? running_var, bool training, float momentum, float eps) -> (Tensor, Tensor, Tensor)",
          "aten::batch_norm(Tensor input, Tensor? weight, Tensor? bias, Tensor? running_mean, Tensor? running_var, bool training, float momentum, float eps, bool cudnn_enabled) -> Tensor"};
      for (auto signature : BatchNormFwd) {
        auto ptr_op = getOperatorForLiteral(signature);
        REGISTER_PARSE_RULE(
            ptr_op,
            {
              MemoryFormat format = MemoryFormat::Contiguous;
              Val* operand = nullptr;
              std::tie(format, operand) =
                  value_map[node->input(0)->unique()].getEntry();
              auto input = operand->as<TensorView>();

              TensorView* weight = nullptr;
              if (!node->input(1)->type()->isSubtypeOf(
                      static_cast<c10::TypePtr>(NoneType::get()))) {
                weight = value_map[node->input(1)->unique()]->as<TensorView>();
              }

              TensorView* bias = nullptr;
              if (!node->input(2)->type()->isSubtypeOf(
                      static_cast<c10::TypePtr>(NoneType::get()))) {
                bias = value_map[node->input(2)->unique()]->as<TensorView>();
              }

              // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
              auto training = constant_as<bool>(node->input(5));
              TORCH_INTERNAL_ASSERT(
                  training.has_value(),
                  "The training (bool) parameter is required.");
              const bool kTraining = training.value();

              TensorView* running_mean = nullptr;
              if (!node->input(3)->type()->isSubtypeOf(
                      static_cast<c10::TypePtr>(NoneType::get()))) {
                running_mean =
                    value_map[node->input(3)->unique()]->as<TensorView>();
              }

              TensorView* running_var = nullptr;
              if (!node->input(4)->type()->isSubtypeOf(
                      static_cast<c10::TypePtr>(NoneType::get()))) {
                running_var =
                    value_map[node->input(4)->unique()]->as<TensorView>();
              }

              Val* momentum_ptr = nullptr;
              // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
              if (auto momentum = constant_as<float>(node->input(6))) {
                momentum_ptr = new Double(momentum.value());
              } else {
                // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
                momentum_ptr = value_map[node->input(6)->unique()];
              }

              Val* eps_ptr = nullptr;
              // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
              if (auto eps = constant_as<float>(node->input(7))) {
                eps_ptr = new Double(eps.value());
              } else {
                // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
                eps_ptr = value_map[node->input(7)->unique()];
              }

              auto result = batch_norm(
                  input,
                  weight,
                  bias,
                  running_mean,
                  running_var,
                  kTraining,
                  momentum_ptr,
                  eps_ptr,
                  format == MemoryFormat::ChannelsLast);

              if (node->kind() ==
                      c10::Symbol::fromQualString("aten::native_batch_norm") ||
                  node->kind() ==
                      c10::Symbol::fromQualString(
                          "aten::_batch_norm_impl_index")) {
                // TODO: output 3 & 4 are not created
                //       we are not creating these outputs because codegen
                //       currently lacks the support.
                value_map.emplace(
                    node->output(0)->unique(),
                    ValueHolder(result.output, format));
                value_map.emplace(node->output(1)->unique(), result.mean);
                value_map.emplace(node->output(2)->unique(), result.invstd);
              } else if (
                  node->kind() ==
                  c10::Symbol::fromQualString("aten::batch_norm")) {
                value_map.emplace(
                    node->output()->unique(),
                    ValueHolder(result.output, format));
              }
            },
            [](const Node* node) -> bool { return true; },
            [](const Node* node) -> OperatorType {
              return OperatorType::Normalization;
            });
      }
    }

    {
      auto ptr_op = getOperatorForLiteral(
          "aten::_batch_norm_impl_index_backward(int impl_index, Tensor input, Tensor grad_output, Tensor? weight, Tensor? running_mean, Tensor? running_var, Tensor? save_mean, Tensor? save_var_transform, bool train, float eps, bool[3] output_mask, Tensor reservedSpace) -> (Tensor, Tensor, Tensor)");
      REGISTER_PARSE_RULE(
          ptr_op,
          {
            // discard impl_index and reservedSpace since we don't use them
            MemoryFormat format;
            std::list<Val*> list_val;
            std::tie(format, list_val) = getConsistentValues(
                c10::nullopt,
                value_map[node->inputs()[1]->unique()],
                value_map[node->inputs()[2]->unique()]);
            auto operand0 = list_val.front();
            list_val.pop_front();
            auto operand1 = list_val.front();
            list_val.pop_front();
            auto input = operand0->as<TensorView>();
            auto grad_out = operand1->as<TensorView>();

            TensorView* weight = nullptr;
            if (!node->input(3)->type()->isSubtypeOf(
                    static_cast<c10::TypePtr>(NoneType::get()))) {
              weight = value_map[node->input(3)->unique()]->as<TensorView>();
            }

            TensorView* running_mean = nullptr;
            if (!node->input(4)->type()->isSubtypeOf(
                    static_cast<c10::TypePtr>(NoneType::get()))) {
              running_mean =
                  value_map[node->input(4)->unique()]->as<TensorView>();
            }

            TensorView* running_var = nullptr;
            if (!node->input(5)->type()->isSubtypeOf(
                    static_cast<c10::TypePtr>(NoneType::get()))) {
              running_var =
                  value_map[node->input(5)->unique()]->as<TensorView>();
            }

            TensorView* save_mean = nullptr;
            // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
            if (!node->input(6)->type()->isSubtypeOf(
                    static_cast<c10::TypePtr>(NoneType::get()))) {
              // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
              save_mean = value_map[node->input(6)->unique()]->as<TensorView>();
            }

            TensorView* save_invstd = nullptr;
            // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
            if (!node->input(7)->type()->isSubtypeOf(
                    static_cast<c10::TypePtr>(NoneType::get()))) {
              save_invstd =
                  // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
                  value_map[node->input(7)->unique()]->as<TensorView>();
            }

            // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
            auto training = constant_as<bool>(node->input(8));
            TORCH_INTERNAL_ASSERT(
                training.has_value(),
                "The training (bool) parameter is required.");
            const bool kTraining = training.value();

            // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
            Val* eps_ptr = nullptr;
            // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
            if (auto eps = constant_as<float>(node->input(9))) {
              eps_ptr = new Double(eps.value());
            } else {
              // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
              eps_ptr = value_map[node->input(7)->unique()];
            }

            // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
            auto out_mask_list = constant_as<c10::List<bool>>(node->input(10));
            TORCH_INTERNAL_ASSERT(
                out_mask_list.has_value(),
                "output mask for batch_norm_backward");
            std::vector<bool> output_mask;
            for (const auto value : out_mask_list->vec()) {
              output_mask.emplace_back(static_cast<bool>(value));
            }

            // TODO: merge this loop below.
            if (kTraining) {
              TORCH_INTERNAL_ASSERT(
                  save_mean != nullptr && save_invstd != nullptr,
                  "When training=True, save_mean and save_invstd are required.");
            } else {
              // TODO: this is not a legit assumption? Can't we run with
              // track_running_stats == false && training == false
              // which should just run through the case above.
              TORCH_INTERNAL_ASSERT(
                  running_mean != nullptr && running_var != nullptr,
                  "When training=False, running_mean and running_invstd are required.");
            }

            auto grads = batch_norm_backward(
                input,
                grad_out,
                weight,
                running_mean,
                running_var,
                save_mean,
                save_invstd,
                kTraining,
                eps_ptr,
                output_mask,
                format == MemoryFormat::ChannelsLast);

            if (output_mask[0]) {
              TORCH_INTERNAL_ASSERT(grads.grad_input != nullptr);
              value_map.emplace(
                  node->output(0)->unique(),
                  ValueHolder(grads.grad_input, format));
            } else {
              TORCH_INTERNAL_ASSERT(grads.grad_input == nullptr);
              value_map.emplace(
                  node->output(0)->unique(),
                  ValueHolder(TensorViewBuilder().build(), format));
            }

            if (output_mask[1]) {
              TORCH_INTERNAL_ASSERT(grads.grad_weight != nullptr);
              value_map.emplace(node->output(1)->unique(), grads.grad_weight);
            } else {
              TORCH_INTERNAL_ASSERT(grads.grad_weight == nullptr);
              value_map.emplace(
                  node->output(1)->unique(), TensorViewBuilder().build());
            }

            if (output_mask[2]) {
              TORCH_INTERNAL_ASSERT(grads.grad_bias != nullptr);
              value_map.emplace(node->output(2)->unique(), grads.grad_bias);
            } else {
              TORCH_INTERNAL_ASSERT(grads.grad_bias == nullptr);
              value_map.emplace(
                  node->output(2)->unique(), TensorViewBuilder().build());
            }
          },
          [](const Node* node) -> bool { return true; },
          [](const Node* node) -> OperatorType {
            return OperatorType::Normalization;
          });
    }

    {
      std::array<const char*, kNumLayernormFwd> LayerNormFwd = {
          "aten::native_layer_norm(Tensor input, int[] normalized_shape, Tensor? weight, Tensor? bias, float eps) -> (Tensor, Tensor, Tensor)",
          "aten::layer_norm(Tensor input, int[] normalized_shape, Tensor? weight=None, Tensor? bias=None, float eps=1e-05, bool cudnn_enable=True) -> Tensor"};
      for (auto signature : LayerNormFwd) {
        auto ptr_op = getOperatorForLiteral(signature);
        REGISTER_PARSE_RULE(
            ptr_op,
            {
              MemoryFormat format;
              std::list<Val*> list_val;
              std::tie(format, list_val) = getConsistentValues(
                  MemoryFormat::Contiguous,
                  value_map[node->inputs()[0]->unique()]);
              auto input_t = list_val.front();
              list_val.pop_front();
              auto input = input_t->as<TensorView>();

              auto norm_shape_optional =
                  constant_as<c10::List<int64_t>>(node->input(1));
              TORCH_INTERNAL_ASSERT(
                  norm_shape_optional.has_value(),
                  "The Normalized_Shape list is required.");
              auto norm_shape = norm_shape_optional->vec();

              TensorView* weight = nullptr;
              if (!node->input(2)->type()->isSubtypeOf(
                      static_cast<c10::TypePtr>(NoneType::get()))) {
                weight = value_map[node->input(2)->unique()]->as<TensorView>();
              }

              TensorView* bias = nullptr;
              if (!node->input(3)->type()->isSubtypeOf(
                      static_cast<c10::TypePtr>(NoneType::get()))) {
                bias = value_map[node->input(3)->unique()]->as<TensorView>();
              }

              Val* eps_ptr = nullptr;
              if (auto eps = constant_as<float>(node->input(4))) {
                eps_ptr = new Double(eps.value());
              } else {
                eps_ptr = value_map[node->input(4)->unique()];
              }

              auto result =
                  layer_norm(input, norm_shape, weight, bias, eps_ptr);

              if (node->kind() ==
                  c10::Symbol::fromQualString("aten::native_layer_norm")) {
                value_map.emplace(node->output(0)->unique(), result.output);
                value_map.emplace(node->output(1)->unique(), result.mean);
                value_map.emplace(node->output(2)->unique(), result.invstd);
              } else if (
                  node->kind() ==
                  c10::Symbol::fromQualString("aten::layer_norm")) {
                value_map.emplace(node->output()->unique(), result.output);
              }
            },
            // TODO: #ProfileIValue List should update this
            [](const Node* node) -> bool { return true; },
            [](const Node* node) -> OperatorType {
              return OperatorType::Normalization;
            });
      }
    }

    {
      auto ptr_op = getOperatorForLiteral(
          "aten::native_layer_norm_backward(Tensor grad_out, Tensor input, int[] normalized_shape, Tensor mean, Tensor rstd, Tensor? weight, Tensor? bias, bool[3] output_mask) -> (Tensor, Tensor, Tensor)");
      REGISTER_PARSE_RULE(
          ptr_op,
          {
            MemoryFormat format;
            std::list<Val*> list_val;
            std::tie(format, list_val) = getConsistentValues(
                MemoryFormat::Contiguous,
                value_map[node->inputs()[0]->unique()],
                value_map[node->inputs()[1]->unique()]);
            auto grad_out_t = list_val.front();
            list_val.pop_front();
            auto input_t = list_val.front();
            list_val.pop_front();
            auto grad_out = grad_out_t->as<TensorView>();
            auto input = input_t->as<TensorView>();

            auto norm_shape_optional =
                constant_as<c10::List<int64_t>>(node->input(2));
            TORCH_INTERNAL_ASSERT(
                norm_shape_optional.has_value(),
                "The Normalized_Shape list is required.");
            auto norm_shape = norm_shape_optional->vec();

            auto mean = value_map[node->input(3)->unique()]->as<TensorView>();
            auto rstd = value_map[node->input(4)->unique()]->as<TensorView>();

            TensorView* weight = nullptr;
            // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
            if (!node->input(5)->type()->isSubtypeOf(
                    static_cast<c10::TypePtr>(NoneType::get()))) {
              // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
              weight = value_map[node->input(5)->unique()]->as<TensorView>();
            }

            TensorView* bias = nullptr;
            // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
            if (!node->input(6)->type()->isSubtypeOf(
                    static_cast<c10::TypePtr>(NoneType::get()))) {
              // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
              bias = value_map[node->input(6)->unique()]->as<TensorView>();
            }

            // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
            auto output_mask_optional =
                constant_as<c10::List<bool>>(node->input(7));
            TORCH_INTERNAL_ASSERT(
                output_mask_optional.has_value(),
                "output mask for layer_norm_backward");
            std::vector<bool> output_mask = output_mask_optional->vec();

            auto grad = layer_norm_backward(
                grad_out,
                input,
                norm_shape,
                mean,
                rstd,
                weight,
                bias,
                output_mask);

            if (output_mask[0]) {
              TORCH_INTERNAL_ASSERT(grad.grad_input != nullptr);
              value_map.emplace(node->output(0)->unique(), grad.grad_input);
            } else {
              TORCH_INTERNAL_ASSERT(grad.grad_input == nullptr);
              value_map.emplace(
                  node->output(0)->unique(), TensorViewBuilder().build());
            }

            if (output_mask[1] && weight != nullptr) {
              TORCH_INTERNAL_ASSERT(grad.grad_weight != nullptr);
              value_map.emplace(node->output(1)->unique(), grad.grad_weight);
            } else {
              TORCH_INTERNAL_ASSERT(grad.grad_weight == nullptr);
              value_map.emplace(
                  node->output(1)->unique(), TensorViewBuilder().build());
            }

            if (output_mask[2] && bias != nullptr) {
              TORCH_INTERNAL_ASSERT(grad.grad_bias != nullptr);
              value_map.emplace(node->output(2)->unique(), grad.grad_bias);
            } else {
              TORCH_INTERNAL_ASSERT(grad.grad_bias == nullptr);
              value_map.emplace(
                  node->output(2)->unique(), TensorViewBuilder().build());
            }
          },
          // TODO: #ProfileIValue List should update this
          [](const Node* node) -> bool { return true; },
          [](const Node* node) -> OperatorType {
            return OperatorType::Normalization;
          });
    }

    {
      auto ptr_op = getOperatorForLiteral(
          "aten::softmax.int(Tensor self, int dim, int? dtype) -> Tensor");
      REGISTER_PARSE_RULE(
          ptr_op,
          {
            MemoryFormat format;
            std::list<Val*> list_val;
            std::tie(format, list_val) = getConsistentValues(
                MemoryFormat::Contiguous,
                value_map[node->inputs()[0]->unique()]);
            auto input_t = list_val.front();
            list_val.pop_front();
            auto input = input_t->as<TensorView>();

            auto dim_value = constant_as<int>(node->input(1));
            TORCH_INTERNAL_ASSERT(
                dim_value.has_value(), "dim in softmax is not valid");

            auto output = softmax(input, dim_value.value());
            value_map.emplace(node->output()->unique(), output);
          },
          [](const Node* node) -> bool {
            if (node->inputs()[1]->node()->kind() != prim::Constant) {
              return false;
            }
            // TODO: support dynamic input by profiling it
            if (!node->inputs()[2]->type()->isSubtypeOf(
                    static_cast<c10::TypePtr>(NoneType::get())) &&
                node->inputs()[2]->node()->kind() != prim::Constant) {
              return false;
            }
            return true;
          },
          [](const Node* node) -> OperatorType {
            return OperatorType::Normalization;
          });
    }

    {
      auto ptr_op = getOperatorForLiteral(
          "aten::_softmax_backward_data(Tensor grad_output, Tensor output, int dim, ScalarType input_dtype) -> Tensor");
      REGISTER_PARSE_RULE(
          ptr_op,
          {
            auto grad_output =
                value_map[node->input(0)->unique()]->as<TensorView>();

            auto output = value_map[node->input(1)->unique()]->as<TensorView>();

            auto dim_value = constant_as<int>(node->input(2));
            TORCH_INTERNAL_ASSERT(
                dim_value.has_value(), "dim in softmax is not valid");

            // input_dtype here is ignored! type_inference handles it
            auto grad_input =
                softmax_backward(grad_output, output, dim_value.value());

            value_map.emplace(node->output()->unique(), grad_input);
          },
          [](const Node* node) -> bool {
            if (node->inputs()[2]->node()->kind() != prim::Constant) {
              return false;
            }
            return true;
          },
          [](const Node* node) -> OperatorType {
            return OperatorType::Normalization;
          });
    }

    {
      auto ptr_op = getOperatorForLiteral(
          "aten::sum.dim_IntList(Tensor self, int[1] dim, bool keepdim=False, *, int? dtype=None) -> (Tensor)");
      REGISTER_PARSE_RULE(
          ptr_op,
          {
            // TODO: support channels last in sum
            MemoryFormat format;
            std::list<Val*> list_val;
            std::tie(format, list_val) = getConsistentValues(
                MemoryFormat::Contiguous,
                value_map[node->inputs()[0]->unique()]);
            auto self = list_val.front();
            list_val.pop_front();
            auto dims_list = constant_as<c10::List<int64_t>>(node->input(1));
            TORCH_INTERNAL_ASSERT(
                dims_list.has_value(),
                "aten::sum cannot be fused with dynamic axes");
            std::vector<int> dims;
            for (const auto dim : dims_list->vec()) {
              dims.emplace_back(static_cast<int>(dim));
            }
            auto keepdim = constant_as<bool>(node->input(2));
            TORCH_INTERNAL_ASSERT(
                keepdim.has_value(),
                "aten::sum cannot be fused with dynamic keepdim");
            auto out = sum(self->as<TensorView>(), dims, keepdim.value());
            value_map.emplace(node->output()->unique(), out);
          },
          [](const Node* node) -> bool {
            // TODO: support cast of output types
            if (!node->inputs()[3]->type()->isSubtypeOf(
                    static_cast<c10::TypePtr>(NoneType::get()))) {
              // We can only handle output as half, float, and double;
              if (const auto opt_ivalue = toIValue(node->input(3))) {
                const auto scalar_type = opt_ivalue->toScalarType();
                if (scalar_type == at::ScalarType::Double ||
                    scalar_type == at::ScalarType::Float ||
                    scalar_type == at::ScalarType::BFloat16 ||
                    scalar_type == at::ScalarType::Half) {
                  return true;
                }
              }
              return false;
            }
            // we don't support dynamic reduction axes;
            if (node->inputs()[1]->node()->kind() != prim::Constant) {
              return false;
            }
            // we don't support dynamic keepdim yet;
            if (node->inputs()[2]->node()->kind() != prim::Constant) {
              return false;
            }
            return true;
          },
          [](const Node* node) -> OperatorType {
            return OperatorType::Reduction;
          });
    }

    {
      auto ptr_op = getOperatorForLiteral(
          "aten::mean.dim(Tensor self, int[1] dim, bool keepdim=False, *, ScalarType? dtype=None) -> Tensor");
      REGISTER_PARSE_RULE(
          ptr_op,
          {
            MemoryFormat format;
            std::list<Val*> list_val;
            std::tie(format, list_val) = getConsistentValues(
                MemoryFormat::Contiguous,
                value_map[node->inputs()[0]->unique()]);
            auto operand = list_val.front();
            list_val.pop_front();
            auto self = operand->as<TensorView>();
            auto dims_list = constant_as<c10::List<int64_t>>(node->input(1));
            TORCH_INTERNAL_ASSERT(
                dims_list.has_value(),
                "aten::mean cannot be fused with dynamic axes");
            std::vector<int> dims;
            for (const auto dim : dims_list->vec()) {
              dims.emplace_back(static_cast<int>(dim));
            }
            auto keepdim = constant_as<bool>(node->input(2));
            TORCH_INTERNAL_ASSERT(
                keepdim.has_value(),
                "aten::mean cannot be fused with dynamic keepdim");
            auto o_sum = sum(self, dims, keepdim.value());
            Val* num_features = new Double(1);
            for (auto axis : dims) {
              if (axis < 0) {
                axis += int(self->nDims());
              }
              num_features =
                  mul(num_features, self->domain()->domain()[axis]->extent());
            }
            auto out = div(o_sum, num_features);
            value_map.emplace(node->output()->unique(), out);
          },
          [](const Node* node) -> bool {
            // TODO: support cast of output types
            if (!node->inputs()[3]->type()->isSubtypeOf(
                    static_cast<c10::TypePtr>(NoneType::get()))) {
              // We can only handle output as half, float, and double;
              if (const auto opt_ivalue = toIValue(node->input(3))) {
                const auto scalar_type = opt_ivalue->toScalarType();
                if (scalar_type == at::ScalarType::Double ||
                    scalar_type == at::ScalarType::Float ||
                    scalar_type == at::ScalarType::BFloat16 ||
                    scalar_type == at::ScalarType::Half) {
                  return true;
                }
              }
              return false;
            }
            // we don't support dynamic reduction axes;
            if (node->inputs()[1]->node()->kind() != prim::Constant) {
              return false;
            }
            // we don't support dynamic keepdim yet;
            if (node->inputs()[2]->node()->kind() != prim::Constant) {
              return false;
            }
            return true;
          },
          [](const Node* node) -> OperatorType {
            return OperatorType::Reduction;
          });
    }
    {
      std::array<const char*, kNumSumToSize> SumToSize = {
          "aten::_grad_sum_to_size(Tensor(a) self, int[]? size) -> Tensor(a)",
          "aten::sum_to_size(Tensor self, int[] size) -> Tensor"};
      for (auto signature : SumToSize) {
        auto ptr_op = getOperatorForLiteral(signature);
        REGISTER_PARSE_RULE(
            ptr_op,
            {
              MemoryFormat format;
              std::list<Val*> list_val;
              std::tie(format, list_val) = getConsistentValues(
                  MemoryFormat::Contiguous,
                  value_map[node->inputs()[0]->unique()]);
              auto self = list_val.front();
              list_val.pop_front();
              auto size_to = constant_as<c10::List<int64_t>>(node->input(1));
              TORCH_INTERNAL_ASSERT(
                  size_to.has_value(),
                  "aten::sum cannot be fused with dynamic axes");
              if (!size_to->empty()) {
                auto out = sum_to(self->as<TensorView>(), size_to->vec());
                value_map.emplace(node->output()->unique(), out);
              } else {
                // We are introducing alias here!
                value_map.emplace(node->output()->unique(), self);
              }
            },
            [](const Node* node) -> bool {
              // we don't support dynamic reduction axes;
              if (node->inputs()[1]->node()->kind() != prim::Constant) {
                return false;
              }
              return true;
              // auto size_to = constant_as<c10::List<int64_t>>(node->input(1));
              // return size_to.has_value() && !size_to->empty();
            },
            [](const Node* node) -> OperatorType {
              auto size_to = constant_as<c10::List<int64_t>>(node->input(1));
              // technically size_to->empty() should never occur, as specialized
              // _grad_sum_to_size should have been removed by optimization pass
              if (size_to->empty()) {
                return OperatorType::ElementWise;
              } else {
                return OperatorType::ReductionToSize;
              }
            });
      }
    }

    {
      std::array<const char*, kNumAutocastOps> AutocastOps = {
          "aten::_autocast_to_reduced_precision(Tensor(a) self, bool cuda_enabled, bool cpu_enabled, ScalarType cuda_dtype, ScalarType cpu_dtype) -> Tensor(a)",
          "aten::_autocast_to_full_precision(Tensor(a) self, bool cuda_enabled, bool cpu_enabled) -> Tensor(a)"};
      for (auto signature : AutocastOps) {
        auto ptr_op = getOperatorForLiteral(signature);
        REGISTER_PARSE_RULE(
            ptr_op,
            {
              MemoryFormat format;
              std::list<Val*> list_val;
              std::tie(format, list_val) = getConsistentValues(
                  c10::nullopt, value_map[node->inputs()[0]->unique()]);
              auto self = list_val.front();
              list_val.pop_front();

              auto out = set(self);
              value_map.emplace(
                  node->output()->unique(), ValueHolder(out, format));
            },
            nullptr,
            nullptr);
      }
    }

    // Limiting aten::to implementation to only change the dtype of a tensor
    {
      auto ptr_op = getOperatorForLiteral(
          "aten::to.dtype(Tensor self, ScalarType dtype, bool non_blocking=False, bool copy=False, MemoryFormat? memory_format=None) -> Tensor");
      REGISTER_PARSE_RULE(
          ptr_op,
          {
            MemoryFormat format;
            std::list<Val*> list_val;
            std::tie(format, list_val) = getConsistentValues(
                c10::nullopt, value_map[node->inputs()[0]->unique()]);
            auto self = list_val.front();
            list_val.pop_front();

            // we need static type for cast
            TORCH_INTERNAL_ASSERT(
                node->input(1)->node()->kind() == prim::Constant);
            auto dtype = toIValue(node->input(1))->toScalarType();

            // We want to keep our internal fusion math in FP32
            // Shape Inference will continue to propagate the right
            // type to outputs unchanged.
            if (dtype == at::ScalarType::Half) {
              dtype = at::ScalarType::Float;
            }
            if (dtype == at::ScalarType::BFloat16) {
              dtype = at::ScalarType::Float;
            }

            auto out = castOp(aten_to_data_type(dtype), self);
            value_map.emplace(
                node->output()->unique(), ValueHolder(out, format));
          },
          nullptr,
          nullptr);
    }

    {
      auto ptr_op = getOperatorForLiteral(
          "aten::type_as(Tensor self, Tensor other) -> Tensor");
      REGISTER_PARSE_RULE(
          ptr_op,
          {
            MemoryFormat format;
            std::list<Val*> list_val;
            std::tie(format, list_val) = getConsistentValues(
                c10::nullopt, value_map[node->inputs()[0]->unique()]);
            auto self = list_val.front();
            list_val.pop_front();

            // TODO: switch to PyTorch dtype as it's closer to truth.
            // For now, reality is that PyTorch IR profiling information could
            // be missing even with profiling executor, due to upstream
            // transformations between profiling runs to fusion pass.
            auto opt_dtype =
                value_map[node->inputs()[1]->unique()]->getDataType();
            TORCH_INTERNAL_ASSERT(opt_dtype.has_value());

            auto out = castOp(opt_dtype.value(), self);
            value_map.emplace(
                node->output()->unique(), ValueHolder(out, format));
          },
          nullptr,
          nullptr);
    }

    {
      // We are not fusing `linear` yet, because we can't codegen efficient gemm
      // However, we still need this here, so PE would insert profile node for
      // this node.
      // During fusion pass, We decompose linear into gemm + elementwise.
      auto ptr_op = getOperatorForLiteral(
          "aten::linear(Tensor input, Tensor weight, Tensor? bias=None) -> Tensor");
      REGISTER_PARSE_RULE(
          ptr_op,
          {
            // this entry is created so we do profile input tensors;
            TORCH_INTERNAL_ASSERT(false, "not implemented yet");
          },
          [](const Node* node) -> bool {
            // We only profile `linear` layer with bias.
            if (node->input(2)->type()->isSubtypeOf(
                    static_cast<c10::TypePtr>(NoneType::get()))) {
              return false;
            }
            return true;
          });
    }

    {
      auto ptr_op = getOperatorForLiteral(
          "prim::add_optional(Tensor(a) input, Tensor? bias) -> Tensor(a)");
      REGISTER_PARSE_RULE(
          ptr_op,
          {
            // this entry is created so we do profile input tensors;
            if (node->input(1)->type()->isSubtypeOf(
                    static_cast<c10::TypePtr>(NoneType::get()))) {
              // forwarding the value;
              value_map.emplace(
                  node->output()->unique(),
                  value_map[node->inputs()[0]->unique()]);
            } else {
              MemoryFormat format;
              std::list<Val*> list_val;
              std::tie(format, list_val) = getConsistentValues(
                  c10::nullopt,
                  value_map[node->inputs()[0]->unique()],
                  value_map[node->inputs()[1]->unique()]);
              auto lhs = list_val.front();
              list_val.pop_front();
              auto rhs = list_val.front();
              list_val.pop_front();

              auto out = binaryOp(
                  BinaryOpType::Add,
                  lhs,
                  rhs,
                  TypePromotion::default_op_config);
              value_map.emplace(
                  node->output()->unique(), ValueHolder(out, format));
            }
          },
          nullptr,
          nullptr);
    }

    {
      auto ptr_op = getOperatorForLiteral("aten::gelu(Tensor self) -> Tensor");
      REGISTER_PARSE_RULE(
          ptr_op,
          {
            MemoryFormat format;
            std::list<Val*> list_val;
            std::tie(format, list_val) = getConsistentValues(
                c10::nullopt, value_map[node->inputs()[0]->unique()]);
            auto self = list_val.front();
            list_val.pop_front();
            auto out = gelu(self);
            value_map.emplace(
                node->output()->unique(), ValueHolder(out, format));
          },
          nullptr,
          nullptr);
    }

    {
      auto ptr_op = getOperatorForLiteral(
          "aten::gelu_backward(Tensor grad, Tensor self) -> Tensor");
      REGISTER_PARSE_RULE(
          ptr_op,
          {
            MemoryFormat format;
            std::list<Val*> list_val;
            std::tie(format, list_val) = getConsistentValues(
                c10::nullopt,
                value_map[node->inputs()[0]->unique()],
                value_map[node->inputs()[1]->unique()]);
            auto grad_out = list_val.front();
            list_val.pop_front();
            auto self = list_val.front();
            list_val.pop_front();

            auto grad_in = gelu_backward(grad_out, self);
            value_map.emplace(
                node->output()->unique(), ValueHolder(grad_in, format));
          },
          nullptr,
          nullptr);
    }

    {
      auto ptr_op = getOperatorForLiteral(
          "aten::amax(Tensor self, int[1] dim=[], bool keepdim=False) -> Tensor");
      REGISTER_PARSE_RULE(
          ptr_op,
          {
            MemoryFormat format;
            std::list<Val*> list_val;
            std::tie(format, list_val) = getConsistentValues(
                MemoryFormat::Contiguous,
                value_map[node->inputs()[0]->unique()]);
            auto self = list_val.front();
            list_val.pop_front();
            auto dims_list = constant_as<c10::List<int64_t>>(node->input(1));
            TORCH_INTERNAL_ASSERT(
                dims_list.has_value(),
                "aten::amax cannot be fused with dynamic axes");
            std::vector<int> dims;
            for (const auto dim : dims_list->vec()) {
              dims.emplace_back(static_cast<int>(dim));
            }
            auto keepdim = constant_as<bool>(node->input(2));
            TORCH_INTERNAL_ASSERT(
                keepdim.has_value(),
                "aten::amax cannot be fused with dynamic keepdim");

            auto out = max(self->as<TensorView>(), dims, keepdim.value());
            value_map.emplace(node->output()->unique(), out);
          },
          [](const Node* node) -> bool {
            // we don't support dynamic reduction axes;
            if (node->inputs()[1]->node()->kind() != prim::Constant) {
              return false;
            }
            // we don't support dynamic keepdim yet;
            if (node->inputs()[2]->node()->kind() != prim::Constant) {
              return false;
            }
            return true;
          },
          [](const Node* node) -> OperatorType {
            return OperatorType::Reduction;
          });
    }
  }

  void processJitNode(const JitOp* node) {
    if (node->kind() == prim::Constant) {
      // partition doesn't take constant node explicitly, but it does and copy
      // constant into subgraph. So we need to register constants in codegen IR;
      for (auto output : node->outputs()) {
        TORCH_INTERNAL_ASSERT(
            registerScalar(output),
            "registration of output failed at index ",
            output->offset(),
            " for node ",
            *node);
      }
    } else {
      auto reg_entry = lookupInRegistry(node);
      TORCH_INTERNAL_ASSERT(
          reg_entry != nullptr,
          "CudaFusionGroup Parser doesn't handle node: ",
          canonicalSchemaString(node->schema()));
      reg_entry->parse(node, value_map_);
    }
  }

  bool registerValue(const JitValue* val) {
    return registerInputTensor(val) || registerScalar(val);
  }

  bool registerScalar(const JitValue* val) {
    if (val->type()->isSubtypeOf(static_cast<c10::TypePtr>(FloatType::get()))) {
      // NOLINTNEXTLINE(cppcoreguidelines-init-variables)
      CgValue cg_val;
      if (auto ival = constant_as<double>(val)) {
        cg_val = new Double(ival.value());
      } else {
        cg_val = new Double();
      }
      value_map_.emplace(val->unique(), cg_val);
      return true;
    } else if (val->type()->isSubtypeOf(
                   static_cast<c10::TypePtr>(IntType::get()))) {
      // NOLINTNEXTLINE(cppcoreguidelines-init-variables)
      CgValue cg_val;
      if (auto ival = constant_as<int64_t>(val)) {
        cg_val = new Int(ival.value());
      } else {
        cg_val = new Int();
      }
      value_map_.emplace(val->unique(), cg_val);
      return true;
    } else if (val->type()->isSubtypeOf(
                   static_cast<c10::TypePtr>(BoolType::get()))) {
      // NOLINTNEXTLINE(cppcoreguidelines-init-variables)
      CgValue cg_val;
      if (auto ival = constant_as<bool>(val)) {
        cg_val = new Bool(ival.value());
      } else {
        cg_val = new Bool();
      }
      value_map_.emplace(val->unique(), cg_val);
      return true;
    } else if (val->type()->isSubtypeOf(
                   static_cast<c10::TypePtr>(NoneType::get()))) {
      // TODO: should we consider adding support for NoneType;
      return true;
    } else if (val->type()->cast<ListType>()) {
      // TODO: we don't support list type in codegen yet;
      // This is a WAR to allow axes of reduction to be passed as constant list;
      // We simply ignore conversion if the scalar value is a constant;
      return toIValue(val).has_value();
    }
    return false;
  }

  bool registerInputTensor(const JitValue* val) {
    // NOLINTNEXTLINE(cppcoreguidelines-init-variables)
    CgValue cg_val;
    // Don't register if we don't support the type
    if (auto tensor_type = val->type()->cast<c10::TensorType>()) {
      if (!tensor_type->scalarType().has_value()) {
        return false;
      }

      if (aten_to_data_type(tensor_type->scalarType().value()) ==
          DataType::Null) {
        return false;
      }

      // check for NHWC contiguous tensor
      TORCH_CHECK(tensor_type->dim().has_value(), "rank missing");
      const auto n_dim = tensor_type->dim().value();
      bool channels_last_contiguous = false;

      if (n_dim > 2) {
        channels_last_contiguous = true;

        for (const auto i : c10::irange(n_dim)) {
          const auto& stride_property_i = tensor_type->stride_properties()[i];
          // check for channels last stride index, stride_index_[i] indicates
          // the axis that's the i-th fastest:
          //   1. fastest dimension should be axis 1;
          //   2. slowest dimension should be axis 0;
          //   3. every other dimension should follow accordingly;
          if (stride_property_i->stride_index_.has_value() &&
              ((i == 0 && stride_property_i->stride_index_.value() == 1) ||
               (i == n_dim - 1 &&
                stride_property_i->stride_index_.value() == 0) ||
               (stride_property_i->stride_index_.value() == n_dim - i))) {
            continue;
          }

          channels_last_contiguous = false;
          break;
        }

        // construct permuted tensor_type
        if (channels_last_contiguous) {
          auto opt_s_vec = tensor_type->symbolic_sizes().sizes();
          TORCH_CHECK(opt_s_vec.has_value(), "missing rank of symbolic sizes");
          std::vector<c10::ShapeSymbol> nhwc_s_vec = opt_s_vec.value();
          // changing N_C_S0_S1_... -> N_S0_S1_..._C
          nhwc_s_vec.push_back(nhwc_s_vec[1]);
          nhwc_s_vec.erase(++(nhwc_s_vec.begin()));

          // copying stride properties because we need to permute it
          auto opt_stride_vec = tensor_type->stride_properties().sizes();
          TORCH_CHECK(opt_stride_vec.has_value(), "missing stride properties");
          auto nhwc_stride_vec = opt_stride_vec.value();
          // // changing N_C_S0_S1_... -> N_S0_S1_..._C
          // nhwc_stride_vec.push_back(nhwc_stride_vec[1]);
          // nhwc_stride_vec.erase(++(nhwc_stride_vec.begin()));
          // Note that we are only updating stride_properties.stride_index
          for (const auto i : c10::irange(n_dim)) {
            nhwc_stride_vec[i]->stride_index_ = n_dim - i - 1;
          }

          // auto updated_tensor_type = c10::TensorType::create(
          tensor_type = c10::TensorType::create(
              tensor_type->scalarType(),
              tensor_type->device(),
              nhwc_s_vec,
              nhwc_stride_vec,
              tensor_type->requires_grad(),
              tensor_type->undefined());
        }
      }

      cg_val = new TensorView(tensor_type);
      value_map_.emplace(
          val->unique(),
          ValueHolder(
              cg_val,
              /*c_last*/
              channels_last_contiguous ? MemoryFormat::ChannelsLast
                                       : MemoryFormat::Contiguous));
      return true;
    }
    return false;
  }

  std::shared_ptr<Graph> graph_;

  // maps from JitValue::unique() to fusion Val;
  std::unordered_map<size_t, ValueHolder> value_map_;

  static std::unordered_set<Symbol> parser_symbol_set_;

  // parsing rule registry.
  static std::unordered_map<std::string, RegistrationEntry>
      jit_operator_registry_; // NOLINT

  // pointing cached entry stored in `jit_operator_registry_`
  static std::unordered_map<const FunctionSchema*, const RegistrationEntry*>
      cached_registry_lookup_; // NOLINT

  // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
  static bool init_registry_;
};
std::unordered_set<Symbol> IrParser::parser_symbol_set_; // NOLINT
std::unordered_map<std::string, IrParser::RegistrationEntry>
    IrParser::jit_operator_registry_; // NOLINT
std::unordered_map<const FunctionSchema*, const IrParser::RegistrationEntry*>
    IrParser::cached_registry_lookup_; // NOLINT

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
bool IrParser::init_registry_ = true;

ProfileIValueOp* insertProfileIValueOp(
    Node* node,
    size_t offset,
    ProfilingRecord* pr) {
  auto in_val = node->input(offset);
  auto pn = pr->createProfileIValueNode(in_val);
  pn->insertBefore(node);
  node->replaceInput(offset, pn->output());
  return pn;
}

void profileSize(ProfilingRecord* pr, Node* node, size_t offset) {
  auto pn = insertProfileIValueOp(node, offset, pr);

  const auto ivalue_profiler = [pr, pn](Stack& stack) {
    std::lock_guard<std::mutex> lock(pr->mutex_);

    // TODO: we don't care about merging multiple profiling runs as we don't
    // support it at all;
    int64_t frame_id = 0;
    pop(stack, frame_id);
    IValue value;
    pop(stack, value);

    std::vector<int64_t> size_vec;
    if (value.isIntList()) {
      size_vec = value.toIntVector();
    } else if (value.isNone()) {
      size_vec.clear();
    } else {
      TORCH_INTERNAL_ASSERT(
          false, "profileSize does not support data type: ", value.tagKind());
    }
    if (!pn->hasAttribute(sizeAttr)) {
      pn->is_(sizeAttr, size_vec);
    } else {
      auto profiled_ints = pn->is(sizeAttr);
      TORCH_INTERNAL_ASSERT(
          profiled_ints.size() == size_vec.size() &&
              std::equal(
                  profiled_ints.begin(), profiled_ints.end(), size_vec.begin()),
          "profiling ivalue doesn't support merge");
    }
    push(stack, value);
  };
  pn->setCallback(ivalue_profiler);
}

void profileIntList(ProfilingRecord* pr, Node* node, size_t offset) {
  auto pn = insertProfileIValueOp(node, offset, pr);

  const auto ivalue_profiler = [pr, pn](Stack& stack) {
    std::lock_guard<std::mutex> lock(pr->mutex_);

    // TODO: we don't care about merging multiple profiling runs as we don't
    // support it at all;
    int64_t frame_id = 0;
    pop(stack, frame_id);
    IValue value;
    pop(stack, value);
    TORCH_INTERNAL_ASSERT(
        value.isIntList(), "profiling seeing the wrong data type");
    if (!pn->hasAttribute(intListAttr)) {
      pn->is_(intListAttr, value.toIntVector());
    } else {
      auto profiled_ints = pn->is(intListAttr);
      auto input_ints = value.toIntList();
      TORCH_INTERNAL_ASSERT(
          profiled_ints.size() == input_ints.size() &&
              std::equal(
                  profiled_ints.begin(),
                  profiled_ints.end(),
                  input_ints.begin()),
          "profiling ivalue doesn't support merge");
    }
    push(stack, value);
  };

  pn->setCallback(ivalue_profiler);
}

void profileBool(ProfilingRecord* pr, Node* node, size_t offset) {
  auto pn = insertProfileIValueOp(node, offset, pr);

  const auto ivalue_profiler = [pr, pn](Stack& stack) {
    std::lock_guard<std::mutex> lock(pr->mutex_);

    // TODO: we don't care about merging multiple profiling runs as we don't
    // support it at all;
    int64_t frame_id = 0;
    pop(stack, frame_id);
    IValue value;
    pop(stack, value);
    TORCH_INTERNAL_ASSERT(
        value.isBool(), "profiling seeing the wrong data type");
    if (!pn->hasAttribute(boolAttr)) {
      pn->i_(boolAttr, value.toBool());
    } else {
      auto profiled_bool = pn->i(boolAttr);
      auto input_bool = value.toBool();
      TORCH_INTERNAL_ASSERT(
          input_bool == profiled_bool,
          "profiling ivalue doesn't support merge");
    }
    push(stack, value);
  };

  pn->setCallback(ivalue_profiler);
}

void profileInt(ProfilingRecord* pr, Node* node, size_t offset) {
  auto pn = insertProfileIValueOp(node, offset, pr);

  const auto ivalue_profiler = [pr, pn](Stack& stack) {
    std::lock_guard<std::mutex> lock(pr->mutex_);

    // TODO: we don't care about merging multiple profiling runs as we don't
    // support it at all;
    int64_t frame_id = 0;
    pop(stack, frame_id);
    IValue value;
    pop(stack, value);
    TORCH_INTERNAL_ASSERT(
        value.isInt(), "profiling seeing the wrong data type");
    if (!pn->hasAttribute(intAttr)) {
      pn->i_(intAttr, value.toInt());
    } else {
      auto profiled_int = pn->i(intAttr);
      auto input_int = value.toInt();
      TORCH_INTERNAL_ASSERT(
          input_int == profiled_int, "profiling ivalue doesn't support merge");
    }
    push(stack, value);
  };

  pn->setCallback(ivalue_profiler);
}

void profileBoolList(ProfilingRecord* pr, Node* node, size_t offset) {
  auto pn = insertProfileIValueOp(node, offset, pr);

  const auto ivalue_profiler = [pr, pn](Stack& stack) {
    std::lock_guard<std::mutex> lock(pr->mutex_);

    // TODO: we don't care about merging multiple profiling runs as we don't
    // support it at all;
    int64_t frame_id = 0;
    pop(stack, frame_id);
    IValue value;
    pop(stack, value);
    TORCH_INTERNAL_ASSERT(
        value.isBoolList(), "profiling seeing the wrong data type");
    if (!pn->hasAttribute(boolListAttr)) {
      auto list = value.toBoolList();
      std::vector<int64_t> val(list.begin(), list.end());
      pn->is_(boolListAttr, val);
    } else {
      auto profiled_ints = pn->is(boolListAttr);
      auto input_bools = value.toBoolList();
      TORCH_INTERNAL_ASSERT(
          profiled_ints.size() == input_bools.size() &&
              std::equal(
                  input_bools.begin(),
                  input_bools.end(),
                  profiled_ints.begin()),
          "profiling ivalue doesn't support merge");
    }
    push(stack, value);
  };

  pn->setCallback(ivalue_profiler);
}

bool anyInBlock(
    const Block* block,
    const std::function<bool(const Node*)>& fn) {
  for (auto node : block->nodes()) {
    if (fn(node)) {
      return true;
    }
    for (auto block : node->blocks()) {
      if (anyInBlock(block, fn)) {
        return true;
      }
    }
  }
  return false;
}

} // namespace

bool hasReductionNode(const Block* block) {
  return anyInBlock(block, isReductionNode);
}

bool isReductionNode(const Node* node) {
  return IrParser::isReductionNode(node);
}

bool isReductionToSizeNode(const Node* node) {
  return IrParser::isReductionToSizeNode(node);
}

bool hasNormalizationNode(const Block* block) {
  return anyInBlock(block, isNormalizationNode);
}

bool isNormalizationNode(const Node* node) {
  return IrParser::isNormalizationNode(node);
}

bool isElementWiseNode(const Node* node) {
  return IrParser::isElementWiseNode(node);
}

bool isNodeParsible(const Node* node) {
  return IrParser::canParseNode(node);
}

bool shouldProfileNode(const Node* node) {
  return IrParser::lookupInSymbolSet(node);
}

bool insertProfileIValue(ProfilingRecord* pr, Node* node, size_t offset) {
  // is skip constant necessary?
  if (node->input(offset)->node()->kind() == prim::Constant) {
    return false;
  }

  static auto dropout_schema =
      getOperatorForLiteral(
          "aten::dropout(Tensor input, float p, bool train) -> Tensor")
          ->schema();
  if (node->matches(dropout_schema)) {
    switch (offset) {
      // argument 2: Is training?
      case 2:
        profileBool(pr, node, offset);
        break;
      default:
        return false;
    }
    return true;
  }

#if false // temporarily disable this for dropout changes
  static auto native_dropout_schema =
      getOperatorForLiteral(
          "aten::native_dropout(Tensor input, float p, float scale, bool train) -> (Tensor, Tensor)")
          ->schema();
  if (node->matches(native_dropout_schema)) {
    switch (offset) {
      // argument 3: Is training?
      case 3:
        profileBool(pr, node, offset);
        break;
      default:
        return false;
    }
    return true;
  }
#endif

  static auto amax_schema =
      getOperatorForLiteral(
          "aten::amax(Tensor self, int[1] dim=[], bool keepdim=False) -> Tensor")
          ->schema();
  if (node->matches(amax_schema)) {
    switch (offset) {
      // argument 1: reduction axes;
      case 1:
        profileIntList(pr, node, offset);
        break;
      // argument 2: keepdim;
      case 2:
        profileBool(pr, node, offset);
        break;
      default:
        return false;
    }
    return true;
  }

  static auto reduction_operator_schema =
      getOperatorForLiteral(
          "aten::sum.dim_IntList(Tensor self, int[1] dim, bool keepdim=False, *, int? dtype=None) -> (Tensor)")
          ->schema();
  if (node->matches(reduction_operator_schema)) {
    switch (offset) {
      // argument 1: reduction axes;
      case 1:
        profileIntList(pr, node, offset);
        break;
      // argument 2: keepdim;
      case 2:
        profileBool(pr, node, offset);
        break;
      default:
        return false;
    }
    return true;
  }

  static auto sum_to_size_schema =
      getOperatorForLiteral(
          "aten::sum_to_size(Tensor self, int[] size) -> Tensor")
          ->schema();
  static auto grad_sum_to_size_schema =
      getOperatorForLiteral(
          "aten::_grad_sum_to_size(Tensor(a) self, int[]? size) -> Tensor(a)")
          ->schema();
  if (node->matches(sum_to_size_schema) ||
      node->matches(grad_sum_to_size_schema)) {
    switch (offset) {
      // argument 1: reduction sizes;
      case 1:
        // TODO(profile_size): double check optional[size]?
        profileSize(pr, node, offset);
        break;
      default:
        return false;
    }
    return true;
  }

  static auto batch_norm_impl_index_schema =
      getOperatorForLiteral(
          "aten::_batch_norm_impl_index(Tensor input, Tensor? weight, Tensor? bias, Tensor? running_mean, Tensor? running_var, bool training, float momentum, float eps, bool cudnn_enabled) -> (Tensor, Tensor, Tensor, Tensor, int)")
          ->schema();
  static auto native_batch_norm_schema =
      getOperatorForLiteral(
          "aten::native_batch_norm(Tensor input, Tensor? weight, Tensor? bias, Tensor? running_mean, Tensor? running_var, bool training, float momentum, float eps) -> (Tensor, Tensor, Tensor)")
          ->schema();
  static auto batch_norm_schema =
      getOperatorForLiteral(
          "aten::batch_norm(Tensor input, Tensor? weight, Tensor? bias, Tensor? running_mean, Tensor? running_var, bool training, float momentum, float eps, bool cudnn_enabled) -> Tensor")
          ->schema();
  static auto instance_norm_schema =
      getOperatorForLiteral(
          "aten::instance_norm(Tensor input, Tensor? weight, Tensor? bias, Tensor? running_mean, Tensor? running_var, bool use_input_stats, float momentum, float eps, bool cudnn_enabled) -> Tensor")
          ->schema();
  if (node->matches(native_batch_norm_schema) ||
      node->matches(batch_norm_impl_index_schema) ||
      node->matches(batch_norm_schema) || node->matches(instance_norm_schema)) {
    switch (offset) {
      // argument 5: training;
      case 5:
        profileBool(pr, node, offset);
        break;
      default:
        return false;
    }
    return true;
  }

  static auto native_layer_norm_schema =
      getOperatorForLiteral(
          "aten::native_layer_norm(Tensor input, int[] normalized_shape, Tensor? weight, Tensor? bias, float eps) -> (Tensor, Tensor, Tensor)")
          ->schema();
  static auto layer_norm_schema =
      getOperatorForLiteral(
          "aten::layer_norm(Tensor input, int[] normalized_shape, Tensor? weight=None, Tensor? bias=None, float eps=1e-05, bool cudnn_enable=True) -> Tensor")
          ->schema();
  if (node->matches(native_layer_norm_schema) ||
      node->matches(layer_norm_schema)) {
    switch (offset) {
      case 1:
        profileIntList(pr, node, offset);
        break;
      default:
        return false;
    }
    return true;
  }

  static auto batch_norm_impl_index_backward_schema =
      getOperatorForLiteral(
          "aten::_batch_norm_impl_index_backward(int impl_index, Tensor input, Tensor grad_output, Tensor? weight, Tensor? running_mean, Tensor? running_var, Tensor? save_mean, Tensor? save_var_transform, bool train, float eps, bool[3] output_mask, Tensor reservedSpace) -> (Tensor, Tensor, Tensor)")
          ->schema();
  if (node->matches(batch_norm_impl_index_backward_schema)) {
    switch (offset) {
      // TODO: guard impl_index, but I think that's not needed;
      // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
      case 8: // argument 8: training;
        profileBool(pr, node, offset);
        break;
      // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
      case 10:
        profileBoolList(pr, node, offset);
        break;
      default:
        return false;
    }
    return true;
  }

  static auto native_layer_norm_backward_schema =
      getOperatorForLiteral(
          "aten::native_layer_norm_backward(Tensor grad_out, Tensor input, int[] normalized_shape, Tensor mean, Tensor rstd, Tensor? weight, Tensor? bias, bool[3] output_mask) -> (Tensor, Tensor, Tensor)")
          ->schema();
  if (node->matches(native_layer_norm_backward_schema)) {
    switch (offset) {
      case 2:
        profileIntList(pr, node, offset);
        break;
      // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
      case 7:
        profileBoolList(pr, node, offset);
        break;
      default:
        return false;
    }
    return true;
  }

  static auto to_dtype_schema =
      getOperatorForLiteral(
          "aten::to.dtype(Tensor self, ScalarType dtype, bool non_blocking=False, bool copy=False, MemoryFormat? memory_format=None) -> Tensor")
          ->schema();
  if (node->matches(to_dtype_schema)) {
    switch (offset) {
      case 1:
        profileInt(pr, node, offset);
        return true;
      default:
        return false;
    }
  }

  static auto softmax_backward_data_schema =
      getOperatorForLiteral(
          "aten::_softmax_backward_data(Tensor grad_output, Tensor output, int dim, ScalarType input_dtype) -> Tensor")
          ->schema();
  if (node->matches(softmax_backward_data_schema)) {
    switch (offset) {
      case 3:
        profileInt(pr, node, offset);
        return true;
      default:
        return false;
    }
  }

  return false;
}

void insertProfileNodesForCUDAFuser_(Block* block, ProfilingRecord* pr) {
  for (const auto& n : block->nodes()) {
    for (const auto offset : c10::irange(n->inputs().size())) {
      insertProfileIValue(pr, n, offset);
    }

    for (auto ib : n->blocks()) {
      insertProfileNodesForCUDAFuser_(ib, pr);
    }
  }
}

void InsertProfileNodes(ProfilingRecord* pr) {
  insertProfileNodesForCUDAFuser_(pr->profiled_graph_->block(), pr);
}

std::unique_ptr<Fusion> parseJitIR(const std::shared_ptr<Graph>& graph) {
  FUSER_PERF_SCOPE("parseJitIR");

  IrParser parser(graph);
  return parser.parse();
}

} // namespace cuda
} // namespace fuser
} // namespace jit
} // namespace torch
