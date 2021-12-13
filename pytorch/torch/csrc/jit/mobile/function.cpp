#include <caffe2/serialize/inline_container.h>
#include <torch/csrc/jit/mobile/function.h>
#include <torch/csrc/jit/mobile/interpreter.h>
#include <torch/csrc/jit/mobile/prim_ops_registery.h>
#include <torch/csrc/jit/runtime/instruction.h>
#include <torch/csrc/jit/runtime/operator.h>

namespace torch {
namespace jit {

char const* toString(OpCode op);
namespace mobile {
Function::Function(c10::QualifiedName name)
    : name_(std::move(name)), code_(std::make_shared<Code>()) {}

Function::Function(
    c10::QualifiedName name,
    std::shared_ptr<Code> code,
    at::optional<c10::FunctionSchema> schema)
    : name_(std::move(name)),
      code_(std::move(code)),
      schema_(std::move(schema)) {}

const c10::QualifiedName& Function::qualname() const {
  return name_;
}

void Function::append_instruction(OpCode op, int X, int N, int64_t dbg_handle) {
  TORCH_CHECK(
      isOpSupportedInMobile(op),
      toString(op),
      " is not supported in mobile module.");
  code_->instructions_.emplace_back(op, X, N);
  code_->debug_handles_.emplace_back(dbg_handle);
}

void Function::append_instruction(OpCode op, int X, int N) {
  TORCH_CHECK(
      isOpSupportedInMobile(op),
      toString(op),
      " is not supported in mobile module.");
  code_->instructions_.emplace_back(op, X, N);
}

bool Function::append_operator(
    const std::string& name,
    const std::string& overload_name,
    const c10::optional<int>& num_specified_args,
    int64_t model_version) { /* TODO: T90339189 deprecate all v3 when v3 models
                                are removed */
  // Keep the original opname in code_
  code_->op_names_.emplace_back(name, overload_name);
  const auto& opname = code_->op_names_.back();
  code_->operator_input_sizes_.emplace_back(num_specified_args.value_or(-1));
  auto func = makeOperatorFunction(opname, num_specified_args, model_version);
  if (!func.has_value()) {
    return false;
  }
  code_->operators_.emplace_back(*func);
  return true;
}

void Function::append_constant(const c10::IValue& constant) {
  code_->constants_.push_back(constant);
}

void Function::append_type(const at::TypePtr& type) {
  code_->types_.push_back(type);
}

void Function::append_function(mobile::Function& function) {
  code_->functions_.push_back(&function);
}

void Function::set_register_size(size_t size) {
  code_->register_size_ = size;
}

int64_t Function::get_debug_handle(size_t pc) const {
  TORCH_CHECK(code_, "Valid code must exist.");
  TORCH_CHECK(
      pc < code_->debug_handles_.size(),
      "Module debug info index out of boundary.");
  return code_->debug_handles_[pc];
}

torch::jit::Function& Function::setSchema(c10::FunctionSchema schema) {
  schema_ = std::move(schema);
  return *this;
}

bool Function::hasSchema() const {
  return schema_.has_value();
}

const c10::FunctionSchema& Function::getSchema() const {
  return *schema_;
}

void Function::run(Stack& stack) {
  if (hasSchema()) { // if we have a schema then resolve optional args if any
    getSchema().checkAndNormalizeInputs(
        stack, std::unordered_map<std::string, IValue>{} /*kwargs*/);
  }
  InterpreterState interp_state(*code_);
  interp_state.run(stack);
}

at::IValue Function::operator()(Stack& stack) {
  run(stack);
  return stack.front();
}

size_t Function::num_inputs() const {
  return schema_->arguments().size();
}

bool Function::call(Stack&, c10::function_ref<void(const mobile::Code&)> f) {
  f(*code_);
  return true;
}

const std::shared_ptr<Code> Function::get_code() const {
  return code_;
}

const std::vector<int64_t>& Function::getExceptionDebugHandles() const {
  return getInterpretersExceptionDebugHandles();
}

c10::optional<std::function<void(Stack&)>> makeOperatorFunction(
    c10::OperatorName opname,
    c10::optional<int> num_specified_args,
    int64_t model_version) {
  std::function<void(Stack&)> fn;
  const auto full_name = c10::toString(opname);
  const std::vector<c10::Argument>* pArgs = nullptr;
  bool promoted_op = mobile::hasPrimOpsFn(full_name);
  if (promoted_op) {
    fn = mobile::getPrimOpsFn(full_name);
  } else {
    std::shared_ptr<Operator> jit_op = findOperatorFor(opname);
    if (jit_op) {
      fn = [jit_op](Stack& stack) { jit_op->getOperation()(stack); };
      pArgs = &jit_op->schema().arguments();
    } else {
      auto op = c10::Dispatcher::singleton().findSchema(opname);
      if (op.has_value()) {
        fn = [op](Stack& stack) { op->callBoxed(&stack); };
        if (op->hasSchema()) {
          pArgs = &op->schema().arguments();
        } else {
          TORCH_CHECK(false, "arguments are missing for operator ", opname);
        }
      } else {
        return c10::nullopt;
      }
    }
  }

  if (!promoted_op) {
    TORCH_INTERNAL_ASSERT_DEBUG_ONLY(pArgs);
    const auto& args = *pArgs;
    if (model_version == 0x3LL && opname.name == "aten::_convolution" &&
        opname.overload_name.empty()) {
      // Since byte-code versions 0x4L, convolution has an additional
      // default-value argument (allow_tf32=True, see
      // https://github.com/pytorch/pytorch/pull/40737). This wrapper handles
      // backward compatibility with models of byte-code version <= 0x3L, where
      // this bool argument does not yet exist.
      fn = [fn](Stack& stack) {
        stack.push_back(true);
        fn(stack);
      };
    } else {
      // num_specified_args >= 0 indicates number of arguments are available
      // from model. We can use it to handle backward compatibility.
      if (num_specified_args &&
          num_specified_args.value() < static_cast<int64_t>(args.size())) {
        fn = [fn, num_specified_args, &args](Stack& stack) {
          std::vector<IValue> out_args;
          // The following logic pops and temporarily stores all out arguments
          // from the stack (which can be 0 or more, and always appended to the
          // schema), in order to push the necessary default values. Finally,
          // the out arguments are pushed back into the stack.
          for (size_t i = args.size() - 1; i > 0 && args.at(i).is_out(); i--) {
            out_args.push_back(stack.back());
            stack.pop_back();
          }
          size_t start_index = num_specified_args.value() - out_args.size();
          TORCH_CHECK(
              start_index >= 0,
              "The number of output arguments is: ",
              out_args.size(),
              ", which is more then the number of specified arguments: ",
              num_specified_args.value());
          for (size_t i = start_index; i < (args.size() - out_args.size());
               ++i) {
            TORCH_CHECK(
                args[i].default_value().has_value(),
                "Error happened at preparing for default values for the argument. The ",
                i,
                "th argument ",
                args[i].name(),
                " does not have a specified value or default value. ");

            stack.push_back(args[i].default_value());
          }
          stack.insert(stack.end(), out_args.rbegin(), out_args.rend());
          fn(stack);
        };
      }
    }
  }
  return fn;
}

} // namespace mobile
} // namespace jit
} // namespace torch
