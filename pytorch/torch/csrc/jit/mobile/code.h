#pragma once

#include <vector>

#include <ATen/core/ivalue.h>
#include <ATen/core/operator_name.h>
#include <torch/csrc/jit/runtime/instruction.h>

namespace torch {
namespace jit {
namespace mobile {

using Stack = std::vector<c10::IValue>;
using DebugHandle = int64_t;

class Function;

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
struct Code {
  std::vector<Instruction> instructions_;
  std::vector<DebugHandle> debug_handles_;
  std::vector<c10::OperatorName> op_names_;
  std::vector<int> operator_input_sizes_;
  std::vector<std::function<void(Stack&)>> operators_;
  std::vector<c10::IValue> constants_;
  std::vector<c10::TypePtr> types_;
  // TODO After we actually export CALL instructions we can remove this.
  // We may need a two-stage importing scheme, where we firstly construct all
  // function objects, and then append referenced function pointers. This could
  // be done in parseMethods().
  std::vector<mobile::Function*> functions_;
  size_t register_size_; // Aggregated output size.
};

} // namespace mobile
} // namespace jit
} // namespace torch
