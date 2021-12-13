
#include <torch/csrc/jit/codegen/cuda/instrumentation.h>
#include <torch/csrc/jit/codegen/cuda/kernel_expr_evaluator.h>
#include <torch/csrc/jit/codegen/cuda/kernel_ir_printer.h>

#include <iostream>

namespace torch {
namespace jit {
namespace fuser {
namespace cuda {
namespace kir {

void ExpressionEvaluator::bind(
    const Val* value,
    Int::ScalarType concrete_value) {
  TORCH_CHECK(value->isScalar());
  TORCH_CHECK(value->dtype() == DataType::Int);
  TORCH_CHECK(!value->isConst(), "Tried to bind to a constant value");
  TORCH_CHECK(
      value->definition() == nullptr,
      "Tried to bind to a value that is computed in the kernel IR: ",
      toString(value),
      " with ",
      concrete_value);
  known_values_[value] = concrete_value;
}

void ExpressionEvaluator::bind(
    ParallelType pt,
    Int::ScalarType concrete_value) {
  TORCH_INTERNAL_ASSERT(isParallelTypeThread(pt));
  if (precomputed_integers_) {
    // Need to bind the thread value to integer machine
    //  in pre-computed mode.
    precomputed_integers_->bindConcreteParallelTypeValue(pt, concrete_value);
  } else {
    known_parallel_dimensions_[pt] = concrete_value;
  }
}

c10::optional<Int::ScalarType> ExpressionEvaluator::evaluate(const Val* value) {
  if (precomputed_integers_ && precomputed_integers_->ready()) {
    return precomputed_integers_->getMaybeValueFor(value);
  } else if (value->isScalar() && value->isConst()) {
    return value->as<Int>()->value();
  } else {
    FUSER_PERF_SCOPE("kir::ExpressionEvaluator::evaluate");

    TORCH_CHECK(value->isScalar());
    TORCH_CHECK(value->dtype() == DataType::Int);

    // Is the value known (either explicit binding or memoized)?
    const auto pre_eval_it = known_values_.find(value);
    if (pre_eval_it != known_values_.end()) {
      return pre_eval_it->second;
    }

    value->accept(this);

    const auto post_eval_it = known_values_.find(value);
    return post_eval_it != known_values_.end()
        ? c10::optional<Int::ScalarType>(post_eval_it->second)
        : c10::nullopt;
  }
  return c10::nullopt;
}

bool ExpressionEvaluator::isConst(const Val* value) {
  return ExpressionEvaluator().evaluate(value).has_value();
}

void ExpressionEvaluator::print() const {
  std::cout << "\nEvaluation context\n";
  std::cout << "--------------------\n";
  for (const auto& kv : known_values_) {
    std::cout << toString(kv.first) << " = " << kv.second << "\n";
  }
  std::cout << "--------------------\n\n";
}

void ExpressionEvaluator::unhandled(const void*) {
  TORCH_INTERNAL_ASSERT(
      false, "Kernel IR expression evaluation reached an unsupported node");
}

void ExpressionEvaluator::visit(const Int* value) {
  TORCH_INTERNAL_ASSERT(!value->isConst());
  if (auto def = value->definition()) {
    def->accept(this);
  }
}

void ExpressionEvaluator::visit(const NamedScalar* named_scalar) {
  const auto& name = named_scalar->name();
  for (auto pt : kParallelTypeThreads) {
    auto pt_val_it = known_parallel_dimensions_.find(pt);
    if (pt_val_it == known_parallel_dimensions_.end()) {
      continue;
    }
    if (name == stringifyThreadSize(pt)) {
      known_values_[named_scalar] = pt_val_it->second;
      return;
    }
  }
}

void ExpressionEvaluator::visit(const UnaryOp* unary_op) {
  const auto in = evaluate(unary_op->in());
  if (in.has_value()) {
    switch (unary_op->operation()) {
      case UnaryOpType::Neg:
        known_values_[unary_op->out()] = -*in;
        break;
      case UnaryOpType::Cast:
        known_values_[unary_op->out()] = *in;
        break;
      default:
        TORCH_CHECK(!"Unexpected operator type");
    }
  }
}

void ExpressionEvaluator::visit(const BinaryOp* binary_op) {
  const auto lhs = evaluate(binary_op->lhs());
  const auto rhs = evaluate(binary_op->rhs());
  if (lhs.has_value() && rhs.has_value()) {
    switch (binary_op->operation()) {
      case BinaryOpType::Add:
        known_values_[binary_op->out()] = *lhs + *rhs;
        break;
      case BinaryOpType::Sub:
        known_values_[binary_op->out()] = *lhs - *rhs;
        break;
      case BinaryOpType::Mul:
        known_values_[binary_op->out()] = *lhs * *rhs;
        break;
      case BinaryOpType::Div:
        TORCH_CHECK(*rhs != 0);
        known_values_[binary_op->out()] = *lhs / *rhs;
        break;
      case BinaryOpType::Mod:
        TORCH_CHECK(*rhs != 0);
        known_values_[binary_op->out()] = *lhs % *rhs;
        break;
      case BinaryOpType::CeilDiv:
        TORCH_CHECK(*rhs != 0);
        known_values_[binary_op->out()] = (*lhs + *rhs - 1) / *rhs;
        break;
      case BinaryOpType::And:
        known_values_[binary_op->out()] = Int::ScalarType(*lhs && *rhs);
        break;
      default:
        TORCH_CHECK(!"Unexpected operator type");
    }
  }
}

} // namespace kir
} // namespace cuda
} // namespace fuser
} // namespace jit
} // namespace torch
