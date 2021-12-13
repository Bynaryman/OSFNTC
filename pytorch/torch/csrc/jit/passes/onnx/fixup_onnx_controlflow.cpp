#include <torch/csrc/jit/passes/onnx/fixup_onnx_controlflow.h>

#include <aten/src/ATen/InitialTensorOptions.h>
#include <c10/util/irange.h>
#include <torch/csrc/jit/jit_log.h>
#include <torch/csrc/jit/passes/dead_code_elimination.h>
#include <torch/csrc/jit/passes/onnx/helper.h>
#include <torch/csrc/jit/passes/onnx/peephole.h>
#include <torch/csrc/jit/passes/onnx/shape_type_inference.h>

namespace torch {
namespace jit {

namespace onnx {
using namespace ::c10::onnx;
}

namespace {
const int ONNX_OPSET_13 = 13;
const int ONNX_TYPE_BOOL = 9;

Node* CreateCastToBoolNode(Value* val, Graph* graph) {
  Node* cast_node = graph->create(onnx::Cast);
  cast_node->addInput(val);
  cast_node->i_(attr::to, ONNX_TYPE_BOOL);
  cast_node->output()->setType(BoolType::get());
  return cast_node;
}

Node* InsertCastForCond(Value* cond_val, Graph* graph, Node* consumer_node) {
  // prev:  cond_val -> consumer_node
  // after: cond_val -> cast -> consumer_node
  // NOTE: The cast is required because operators like PyTorch Greater/Less
  //       return tensor in type torch.uint8. However the type for condition
  //       input in ONNX Loop must be bool.
  Node* cast_node = CreateCastToBoolNode(cond_val, graph);
  cast_node->insertBefore(consumer_node);

  consumer_node->replaceInputWith(cond_val, cast_node->output());
  return cast_node;
}

bool IsCondCastRequired(Value* cond_val) {
  const auto& type = cond_val->type();
  if (auto tt = type->cast<TensorType>()) {
    if (auto scalar_type = tt->scalarType()) {
      return *scalar_type != c10::kBool;
    }
  }
  return !type->isSubtypeOf(*BoolType::get());
}

bool IsErasableSequence(const Node* loop_node, size_t i) {
  TORCH_INTERNAL_ASSERT(loop_node->blocks().size() == 1);
  auto* sub_block = loop_node->blocks()[0];
  auto* seq_node = sub_block->outputs()[i - 1]->node();
  auto* in_val = sub_block->inputs()[i];

  if (seq_node->kind() != ::c10::onnx::SequenceInsert) {
    return false;
  }

  if (seq_node->inputs().size() == 3) {
    // Non-default insert position is not supported.
    return false;
  }

  if (seq_node->input(0) != in_val) {
    // Only SequenceInsert that applies on loop-carried sequence is supported.
    return false;
  }

  const auto* init_seq_node = loop_node->inputs()[i]->node();
  const auto init_seq_node_kind = init_seq_node->kind();
  if ((init_seq_node_kind != ::c10::onnx::SequenceEmpty) &&
      (init_seq_node_kind != ::c10::prim::ListConstruct ||
       init_seq_node->inputs().size() != 0)) {
    // Initial sequence must be empty.
    return false;
  }

  if (seq_node->output()->uses().size() != 1) {
    // The sequence is not supported to be used elsewhere inside the sub-block.
    return false;
  }

  return true;
}

// ONNX::Loop does not support Sequence type as loop-carried dependencies. Only
// tensors are supported. This pass converts Sequence loop-carried dependencies
// to scan_outputs. In opset 11, only the below pattern is supported.
//
// PTIR graph:
//  ...
//  %res.1 : Tensor[] = prim::ListConstruct()
//  %res : Tensor[] = prim::Loop(%11, %22, %res.1)
//    block0(%i.1 : Tensor, %res.6 : Tensor[]):
//      ...
//      %res.3 : Tensor[] = aten::append(%res.6, %17)
//      -> (%22, %res.3)
//  return (%res.3)
//
// ONNX graph:
//  ...
//  %res : Tensor = onnx::Loop(%11, %22)
//    block0(%i.1 : Tensor):
//      ...
//      -> (%22, %17)
//  %res_seq : Tensor[] = onnx::SplitToSequence[keepdims=0](%res)
//  return (%res_seq)
std::vector<Value*> ConvertSequenceDependencies(Node* node, int opset_version) {
  if (node->kind() != ::c10::onnx::Loop) {
    return node->outputs().vec();
  }

  if (opset_version >= ONNX_OPSET_13) {
    // Sequence type as loop-carried dependencies should be supported by ONNX
    // ospet 13.
    return node->outputs().vec();
  }

  auto* loop_node = node;

  TORCH_INTERNAL_ASSERT(loop_node->blocks().size() == 1);
  auto* sub_block = loop_node->blocks()[0];

  std::vector<size_t> idx_to_remove;
  std::vector<Value*> new_outputs;
  // ONNX Loop node:
  // sub-block inputs are  (iter, cond, loop-carried dependencies)
  // sub-block outputs are (      cond, loop-carried dependencies, scan outputs)
  // inputs are            (iter, cond, loop-carried dependencies)
  // outputs are           (            loop-carried dependencies, scan outputs)
  for (size_t i = 2; i < sub_block->inputs().size(); ++i) {
    if (IsErasableSequence(loop_node, i)) {
      auto* seq_node = sub_block->outputs()[i - 1]->node();
      // Replace sequence output with the inserted element.
      auto inserted_value = seq_node->input(1);
      sub_block->return_node()->replaceInputWith(
          seq_node->output(), inserted_value);

      // Split the added scan_output back to expected tensor sequence.
      auto loop_output = loop_node->output(i - 2);
      Node* split_node =
          loop_node->owningGraph()->create(onnx::SplitToSequence);
      loop_output->replaceAllUsesWith(split_node->output());
      split_node->i_(attr::keepdims, 0);
      split_node->addInput(loop_output);
      split_node->insertAfter(loop_node);
      split_node->output()->setType(loop_output->type());
      split_node->copyMetadata(loop_node);

      // Update loop output type.
      loop_output->setType(c10::unshapedType(inserted_value->type()));

      // The node that produces sequence should be safe to remove now.
      seq_node->destroy();

      idx_to_remove.push_back(i);
      new_outputs.push_back(split_node->output());
    } else {
      new_outputs.push_back(loop_node->output(i - 2));
    }
  }

  // Remove sequence outputs, and replace with scan outputs.
  for (const auto i : c10::irange(idx_to_remove.size())) {
    size_t idx = idx_to_remove[i] - i;

    sub_block->eraseInput(idx);
    loop_node->removeInput(idx);

    // Swap output order. Move all scan outputs to the back.
    sub_block->return_node()->addInput(
        sub_block->return_node()->inputs().at(idx - 1));
    sub_block->return_node()->removeInput(idx - 1);

    auto loop_out = loop_node->addOutput();
    loop_out->copyMetadata(loop_node->outputs().at(idx - 2));
    loop_node->outputs().at(idx - 2)->replaceAllUsesWith(loop_out);
    loop_node->eraseOutput(idx - 2);
  }

  return new_outputs;
}

// Resolving limitation from ONNX that the block output can not be
// a value from outside the block. Inserting an Identity node inside
// the block, linking with the value outside as workaround.
void FixupONNXSubblockOutputs(Node* n) {
  for (Block* block : n->blocks()) {
    for (Value* output : block->outputs()) {
      if (output->node()->owningBlock() != block) {
        Node* id_node = block->owningGraph()->create(onnx::Identity);
        id_node->insertBefore(block->return_node());
        id_node->addInput(output);
        id_node->output()->copyMetadata(output);
        id_node->copyMetadata(n);
        block->return_node()->replaceInputWith(output, id_node->output());
      }
    }
  }
}

} // anonymous namespace

void FixupONNXLoopNodeInputs(Node* node) {
  if (node->kind() != ::c10::onnx::Loop) {
    return;
  }

  auto* graph = node->owningGraph();

  // add cast to condition input outside the loop.
  Value* cond_val = node->input(1);
  if (IsCondCastRequired(cond_val)) {
    auto* cast_node = InsertCastForCond(cond_val, graph, node);
    cast_node->copyMetadata(node);
  }

  // Setup Loop input cond and i.
  TORCH_INTERNAL_ASSERT(node->blocks().size() == 1);
  auto* sub_block = node->blocks().at(0);
  Value* cond = sub_block->insertInput(1, "cond");
  cond->setType(BoolType::get());

  Value* i = sub_block->inputs().at(0);
  i->setType(TensorType::fromNumberType(*IntType::get()));

  // add cast to condition input inside the loop.
  Value* next_cond_val = sub_block->outputs().at(0);
  if (IsCondCastRequired(next_cond_val)) {
    auto* cast_node =
        InsertCastForCond(next_cond_val, graph, sub_block->return_node());
    cast_node->copyMetadata(node);
  }
}

std::vector<Value*> FixupONNXLoopNode(Node* node, int opset_version) {
  auto output_size = node->outputs().size();
  FixupONNXLoopNodeInputs(node);
  FixupONNXSubblockOutputs(node);
  // NOTE: the output order is deliberately changed to match expected order
  //       since onnx loop requires scan outputs to be the last outputs.
  auto new_outputs = ConvertSequenceDependencies(node, opset_version);

  // Copy type of block output to node output.
  FixupONNXControlflowNodeOutputs(node);
  TORCH_INTERNAL_ASSERT(output_size == new_outputs.size());
  return new_outputs;
}

// Check if node is prim::Uninitialized,
// or output of prim::Uninitialized->onnx::Identity
bool IsUninitializedNode(Node* n) {
  if (n->kind() == ::c10::onnx::Identity &&
      n->inputs()[0]->node()->kind() == prim::Uninitialized)
    return true;
  if (n->kind() == prim::Uninitialized)
    return true;
  return false;
}

// Infer shape and type of the uninitialized_output from the corresponding
// output of the other subblock. prim::Uninitialized node is proven to be
// unused. So replace this node with a Constant (TensorType) or
// Sequence (ListType) of the inferred shape and type.
void InferShapeTypeForUninitializedOutput(
    Graph* graph,
    Block* block,
    Value* uninitialized_output,
    Value* other_output,
    int opset_version) {
  Node* const_node = nullptr;
  if (auto output_type = other_output->type()->cast<TensorType>()) {
    auto elem_type =
        at::initialTensorOptions().dtype(output_type->scalarType());
    const_node = graph->create(::c10::onnx::Constant, 1);

    if (output_type->sizes().concrete_sizes().has_value()) {
      auto size = output_type->sizes().concrete_sizes().value();
      const_node->t_(attr::value, at::zeros(size, elem_type));
      const_node->output()->setType(other_output->type());
    } else {
      const_node->t_(attr::value, at::zeros({}, elem_type));
      const_node->output()->setType(
          TensorType::create(*(output_type->scalarType()), at::kCPU, {}, {}));
    }
  } else if (auto output_type = other_output->type()->cast<ListType>()) {
    TypePtr elem = output_type->getElementType();
    const_node = graph->create(::c10::onnx::SequenceEmpty, 1);
    if (elem->cast<TensorType>() &&
        elem->cast<TensorType>()->scalarType().has_value()) {
      auto scalar_type = elem->cast<TensorType>()->scalarType().value();
      auto onnx_type = ATenTypeToOnnxType(scalar_type);
      const_node->i_(attr::dtype, onnx_type);
      const_node->output()->setType(other_output->type());
    } else {
      std::cerr
          << "Warning: UninitializedOutput - Invalid elem Type of ListTensor found."
          << std::endl;
      const_node->output()->setType(other_output->type());
    }
  }

  const ParamMap empty_params_dict = {};
  ONNXShapeTypeInference(const_node, empty_params_dict, opset_version);
  const_node->insertBefore(block->return_node());
  const_node->copyMetadata(block->return_node());
  uninitialized_output->replaceAllUsesWith(const_node->output());
  uninitialized_output->node()->destroy();
}

// Corresponding outputs for ONNX If then and else subblocks should have
// same shape and type. This pass detects if prim::Uninitialized node
// appears as part of outputs of either of the subblocks, and infers
// shape and type from the corresponding output of the other subblock
// In the example graph below, shape and type of the subblock output %7
// for subblock 1 is inferred from %y.1. Shape and type of Subblock
// output %7 is inferred from %y.5.
//
// graph(%y.1 : Int(3:4, 4:1, requires_grad=0, device=cpu)):
//   ...
//   %7 : Tensor = prim::Uninitialized()
//   %16 : bool, %17 : Tensor, %y.14 : Tensor = prim::If(%15) #
//   test/onnx/test_pytorch_onnx_onnxruntime.py:614:20
//     block0():
//       %y.5 : Tensor = aten::add(%y.1, %3, %6) #
//       test/onnx/test_pytorch_onnx_onnxruntime.py:615:28
//       -> (%2, %7, %y.5)
//     block1():
//       -> (%1, %y.1, %7)
//   ...

void ONNXFixupUninitializedOutput(Node* node, int opset_version) {
  if (node->kind() != ::c10::onnx::If) {
    return;
  }

  GRAPH_DUMP("Graph before fixing If shape type: ", node->owningGraph());
  auto* if_node = node;
  auto* graph = if_node->owningGraph();

  // Check if the input to ONNX If node is node Bool, and insert
  // cast to Bool if needed.
  if (!if_node->input()->type()->isSubtypeOf(*BoolType::get())) {
    Node* cast_node = CreateCastToBoolNode(if_node->input(), graph);
    cast_node->insertBefore(if_node);
    cast_node->copyMetadata(if_node);
    if_node->replaceInputWith(if_node->input(), cast_node->output());
  }

  Block* then_block = if_node->blocks()[0];
  Block* else_block = if_node->blocks()[1];

  // Infer shape and type for subblock outputs
  TORCH_INTERNAL_ASSERT(
      then_block->outputs().size() == else_block->outputs().size())
  for (const auto i : c10::irange(else_block->outputs().size())) {
    Value* then_block_output = then_block->outputs()[i];
    Value* else_block_output = else_block->outputs()[i];

    // If both subblocks have an uninitialized output, shape and type cannot
    // be inferred.
    TORCH_CHECK(
        !(IsUninitializedNode(then_block_output->node()) &&
          IsUninitializedNode(else_block_output->node())),
        "Cannot infer shape and type for ONNX If with uninitialized output in both subblocks. Please check the model graph.");

    if (IsUninitializedNode(then_block_output->node())) {
      InferShapeTypeForUninitializedOutput(
          graph,
          then_block,
          then_block_output,
          else_block_output,
          opset_version);
      if_node->outputs()[i]->setType(then_block->outputs()[i]->type());
    } else if (IsUninitializedNode(else_block_output->node())) {
      InferShapeTypeForUninitializedOutput(
          graph,
          else_block,
          else_block_output,
          then_block_output,
          opset_version);
      if_node->outputs()[i]->setType(else_block->outputs()[i]->type());
    }
  }
}

void ONNXMergeIfBlockOutputShapes(Node* node) {
  TORCH_INTERNAL_ASSERT(node->kind() == ::c10::onnx::If);
  Block* then_block = node->blocks().at(0);
  Block* else_block = node->blocks().at(1);

  TORCH_INTERNAL_ASSERT(
      then_block->outputs().size() == else_block->outputs().size())

  auto findCommonShape =
      [](const ::c10::SymbolicShape& a,
         const ::c10::SymbolicShape& b) -> ::c10::SymbolicShape {
    std::vector<::c10::ShapeSymbol> dims;
    if (a.rank() && b.rank() && a.rank() == b.rank()) {
      for (const auto j : c10::irange(a.rank().value())) {
        if (a[j] == b[j]) {
          dims.emplace_back(a[j]);
        } else {
          dims.emplace_back(::c10::ShapeSymbol::newSymbol());
        }
      }
      return ::c10::SymbolicShape(dims);
    }
    if (a.rank() && a.rank().value() > 0) {
      return a;
    }
    if (b.rank() && b.rank().value() > 0) {
      return b;
    }

    return ::c10::SymbolicShape();
  };

  auto mergeTensorType =
      [&findCommonShape](TensorTypePtr a, TensorTypePtr b) -> TensorTypePtr {
    if (a && b) {
      const auto& a_shape = a->symbolic_sizes();
      const auto& b_shape = b->symbolic_sizes();
      auto commonShape = findCommonShape(a_shape, b_shape);
      return a->withSymbolicShapes(commonShape);
    } else if (a) {
      return a;
    } else if (b) {
      return b;
    }
    return nullptr;
  };

  auto mergeListType = [&mergeTensorType](
                           ListTypePtr a, ListTypePtr b) -> ListTypePtr {
    if (a && b) {
      auto a_tensor_type = a->getElementType()->cast<TensorType>();
      auto b_tensor_type = b->getElementType()->cast<TensorType>();
      auto tensor_type = mergeTensorType(a_tensor_type, b_tensor_type);
      if (tensor_type) {
        return a->withContained({tensor_type})->cast<ListType>();
      }
      // Both branches produce ListType without tensor shape.
      return a;
    } else if (a) {
      return a;
    } else if (b) {
      return b;
    }
    return nullptr;
  };

  for (const auto i : c10::irange(else_block->outputs().size())) {
    auto then_type = then_block->outputs().at(i)->type();
    auto else_type = else_block->outputs().at(i)->type();
    auto then_tensor_type = then_type->cast<TensorType>();
    auto else_tensor_type = else_type->cast<TensorType>();
    auto then_list_type = then_type->cast<ListType>();
    auto else_list_type = else_type->cast<ListType>();
    if (then_tensor_type || else_tensor_type) {
      if (auto tensor_type =
              mergeTensorType(then_tensor_type, else_tensor_type)) {
        node->output(i)->setType(tensor_type);
      }
    } else if (then_list_type || else_list_type) {
      if (auto list_type = mergeListType(then_list_type, else_list_type)) {
        node->output(i)->setType(list_type);
      }
    }
  }
}

std::vector<Value*> FixupONNXIfNode(Node* node, int opset_version) {
  if (node->kind() != ::c10::onnx::If) {
    return node->outputs().vec();
  }
  GRAPH_DUMP("Graph before fixing controlflow: ", node->owningGraph());
  FixupONNXSubblockOutputs(node);
  ONNXFixupUninitializedOutput(node, opset_version);
  // Copy type of block output to node output.
  ONNXMergeIfBlockOutputShapes(node);

  GRAPH_DUMP("Graph after fixing controlflow: ", node->owningGraph());
  return node->outputs().vec();
}

std::vector<Value*> FixupONNXControlflowNode(Node* n, int opset_version) {
  switch (n->kind()) {
    case ::c10::onnx::Loop: {
      return FixupONNXLoopNode(n, opset_version);
    }
    case ::c10::onnx::If: {
      return FixupONNXIfNode(n, opset_version);
    }
    default:
      return n->outputs().vec();
  }
}

void FixupONNXControlflowNodeOutputs(Node* n) {
  switch (n->kind()) {
    case ::c10::onnx::Loop: {
      auto loop_carried_output_size = n->blocks().at(0)->inputs().size() - 2;
      for (auto i : c10::irange(n->outputs().size())) {
        auto type = n->blocks().at(0)->outputs().at(i + 1)->type();
        if (i < loop_carried_output_size) {
          n->output(i)->setType(type);
        } else {
          if (auto t_type = type->cast<TensorType>()) {
            auto sizes = t_type->symbolic_sizes().sizes();
            if (sizes.has_value()) {
              sizes.value().emplace(
                  sizes.value().begin(), c10::ShapeSymbol::newSymbol());
              type = t_type->withSymbolicShapes(sizes.value());
            }
          }
          n->output(i)->setType(type);
        }
      }
      break;
    }
    case ::c10::onnx::If: {
      ONNXMergeIfBlockOutputShapes(n);
      break;
    }
    default:
      break;
  }
}

} // namespace jit
} // namespace torch
