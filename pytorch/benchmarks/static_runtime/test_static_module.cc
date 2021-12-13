#include <gtest/gtest.h>
#include <torch/csrc/jit/ir/alias_analysis.h>
#include <torch/csrc/jit/ir/irparser.h>
#include <torch/csrc/jit/runtime/static/ProcessedNodeInputs.h>
#include <torch/csrc/jit/runtime/static/fusion.h>
#include <torch/csrc/jit/runtime/static/impl.h>
#include <torch/csrc/jit/runtime/static/memory_planner.h>
#include <torch/csrc/jit/runtime/static/ops.h>
#include <torch/csrc/jit/runtime/static/passes.h>

#include "deep_wide_pt.h"
#include "test_utils.h"

using namespace torch;
using namespace torch::jit;
using namespace torch::jit::test;

C10_DECLARE_bool(static_runtime_disable_debug_memory_overlap_check);

namespace {

StaticModule makeStaticModuleFromScript(const std::string& script) {
  Module m("module");
  m.define(script);
  return StaticModule(m);
}

bool testCanEnableStaticRuntime(const std::string& jit_script) {
  script::Module module("module");
  module.define(jit_script);

  Method method = module.get_method("forward");
  auto graph = module.get_method("forward").graph();

  // here we do not freeze graph
  return canEnableStaticRuntime(graph);
}

bool testHasInplaceOp(const std::string& jit_script) {
  script::Module module("module");
  module.define(jit_script);

  Method method = module.get_method("forward");
  auto graph = module.get_method("forward").graph();

  AliasDb alias_db(graph);
  return HasInplaceOp(graph, alias_db);
}

bool testModuleHasOp(const std::string& jit_script, const char* op_name) {
  script::Module module("module");
  module.define(jit_script);

  return forwardHasOp(module, op_name);
}

const auto reshape_inplace_script = R"JIT(
  def forward(self, inp: Tensor, shape: List[int]):
      a = inp + inp
      b = a.reshape(shape)
      c = b.sigmoid_()
      d = c + c
      e = a + a
      f = b + b
      return (d, e, f)
)JIT";

const auto reshape_inplace_script_1 = R"JIT(
  def forward(self, inp: Tensor, shape: List[int], flag: bool):
    if flag:
      a = inp + inp
      b = a.reshape(shape)
      c = b.sigmoid()
    else:
      a = inp * inp
      b = a.sigmoid_()
      c = b.reshape(shape)
    d = c + c
    e = a + a
    f = b + b
    return (d, e, f)
)JIT";

const auto sigmoid_inplace_script = R"JIT(
  def forward(self, inp: Tensor):
      a = torch.sigmoid(inp, out=inp).clone()
      return (a)
)JIT";

const auto sigmoid_out_script = R"JIT(
  def forward(self, inp: Tensor):
      a = inp + inp
      b = torch.sigmoid(inp, out=a).clone()
      return (b)
)JIT";

} // namespace

// Test that StaticModule::value_group groups values of the graph into
// 1) Inputs/Constants and their aliases 2) Outputs and their aliases.
TEST(StaticModule, ValueGroup) {
  const std::string src = R"IR(
    graph(%input0 : Tensor, %input1 : Tensor):
      # Constants.
      %0 : int = prim::Constant[value=1]()
      # Internal values.
      %1 : Tensor = aten::add(%input0, %input1, %0)
      # This includes aliases of output.
      %2 : Tensor = aten::add(%input0, %1, %0)
      # This includes output.
      %3 : (Tensor) = prim::TupleConstruct(%2)
      return (%3)
    )IR";
  auto input_graph = std::make_shared<torch::jit::Graph>();
  torch::jit::parseIR(src, input_graph.get());
  torch::jit::StaticModule sm(input_graph);
  const Graph& graph = sm.graph();
  std::vector<const Node*> nodes(graph.nodes().begin(), graph.nodes().end());
  const auto& value_group = sm.value_group();

  std::vector<const Value*> expected_input_aliases{
      graph.inputs()[0], graph.inputs()[1], nodes[0]->output()};
  for (auto* value : expected_input_aliases) {
    EXPECT_TRUE(value_group.isExternalAlias(value));
  }

  std::vector<const Value*> expected_output_aliases{
      graph.outputs()[0], nodes[2]->output()};
  for (auto* value : expected_output_aliases) {
    EXPECT_TRUE(value_group.isOutputAlias(value));
  }
  EXPECT_FALSE(value_group.isAlwaysAlive(nodes[1]->output()));
  EXPECT_TRUE(value_group.isAlwaysAlive(graph.inputs()[0]));
  EXPECT_TRUE(value_group.isAlwaysAlive(graph.inputs()[1]));
  EXPECT_TRUE(value_group.isAlwaysAlive(graph.outputs()[0]));
}

TEST(StaticModule, IsOptimizableContainerType_NonOptimizableInputs) {
  // Cannot use out variants for list/tuple construction here because
  // inputs are not produced by nodes with out variants.
  const std::string src = R"JIT(
        def forward(self, a, b):
            a_alias = a.view(a.size())
            non_optimizable_list = [a_alias]
            non_optimizable_tuple = (b, )
            return non_optimizable_list, non_optimizable_tuple
    )JIT";

  auto sm = makeStaticModuleFromScript(src);
  const auto& graph = sm.graph();

  for (const Node* n : graph.nodes()) {
    EXPECT_FALSE(sm.is_optimizable_container_type(n));
  }
}

TEST(StaticModule, IsOptimizableContainerType_WrongType) {
  // Cannot use out variants for list/tuple construction here because
  // types are not Tensors
  const std::string src = R"JIT(
        def forward(self, x: int, y: int):
            a = 1 + x
            b = 2 + y
            non_optimizable_list = [a]
            non_optimizable_tuple = (b, )
            return non_optimizable_list, non_optimizable_tuple
    )JIT";

  auto sm = makeStaticModuleFromScript(src);
  const auto& graph = sm.graph();

  for (const Node* n : graph.nodes()) {
    EXPECT_FALSE(sm.is_optimizable_container_type(n));
  }
}

TEST(StaticModule, IsOptimizableContainerType_CanUseOutVariant) {
  // This container should be optimizable since aten::add has an
  // out variant the container contains Tensors.
  const std::string src = R"JIT(
        def forward(self, x):
            a = torch.relu(x)
            optimizable_list = [a]
            return optimizable_list
    )JIT";
  auto sm = makeStaticModuleFromScript(src);
  const auto& graph = sm.graph();

  for (const Node* n : graph.nodes()) {
    if (n->kind() == c10::prim::ListConstruct) {
      EXPECT_TRUE(sm.is_optimizable_container_type(n));
    } else {
      EXPECT_FALSE(sm.is_optimizable_container_type(n));
    }
  }
}

// Test operator() with rvalue inputs
TEST(StaticModule, RValueInputs) {
  const std::string src = R"JIT(
    def forward(self, x):
        y = torch.relu(x)
        return y.clone()
  )JIT";
  auto sm = makeStaticModuleFromScript(src);

  std::vector<IValue> input{at::randn({1})};

  auto expected = sm(input, {});
  auto actual = sm(std::move(input), {});

  EXPECT_TRUE(expected.isTensor());
  EXPECT_TRUE(actual.isTensor());
  EXPECT_TRUE(expected.toTensor().equal(actual.toTensor()));
}

TEST(StaticRuntime, InPlace) {
  EXPECT_TRUE(testHasInplaceOp(reshape_inplace_script));
  EXPECT_TRUE(testHasInplaceOp(reshape_inplace_script_1));
  EXPECT_TRUE(testHasInplaceOp(sigmoid_inplace_script));
  EXPECT_FALSE(testHasInplaceOp(sigmoid_out_script));
}

TEST(StaticRuntime, ModuleHasOp) {
  EXPECT_TRUE(testModuleHasOp(reshape_inplace_script, "aten::sigmoid_"));
  EXPECT_TRUE(testModuleHasOp(reshape_inplace_script_1, "aten::reshape"));
  EXPECT_TRUE(testModuleHasOp(sigmoid_inplace_script, "aten::clone"));
  EXPECT_FALSE(testModuleHasOp(reshape_inplace_script_1, "aten::add_"));
}

TEST(StaticRuntime, CanEnableStaticRuntime) {
  const auto while_script = R"JIT(
    def forward(self, a: Tensor, x: int):
        c = 0
        while c < x:
            a = a * a
            c += 2
        return a
  )JIT";

  const auto for_script = R"JIT(
    def forward(self, a: Tensor, x: int):
        for c in range(x):
            a = a * a
        return a
  )JIT";

  const auto if_script = R"JIT(
    def forward(self, a: Tensor, b: bool):
        if b:
            return a
        else:
            return a * a
  )JIT";

  const auto is_script = R"JIT(
    def forward(self, a: Tensor, b: Tensor):
        return a is b
  )JIT";

  const auto is_not_script = R"JIT(
    def forward(self, a: Tensor, b: Tensor):
        return a is not b
  )JIT";

  EXPECT_TRUE(testCanEnableStaticRuntime(reshape_inplace_script));
  EXPECT_FALSE(testCanEnableStaticRuntime(for_script));
  EXPECT_FALSE(testCanEnableStaticRuntime(while_script));
  EXPECT_FALSE(testCanEnableStaticRuntime(if_script));
  EXPECT_FALSE(testCanEnableStaticRuntime(is_script));
  EXPECT_FALSE(testCanEnableStaticRuntime(is_not_script));
}

TEST(StaticRuntime, NestedOutput) {
  // dict of tuple of list
  const auto nested_output_script_0 = R"JIT(
    def forward(self, a, b):
      c = (a + b).relu().nan_to_num().float()
      d = a.flatten().nan_to_num() * b.flatten().nan_to_num()
      e = d.float().relu()
      f = ([c], [d])
      g = ([e], [f])
      return ({"prediction":(f, g)})
  )JIT";

  // tuple of lists
  const auto nested_output_script_1 = R"JIT(
    def forward(self, a, b):
      c = (a + b).relu().nan_to_num().float()
      d = a.flatten().nan_to_num() * b.flatten().nan_to_num()
      e = d.float().relu()
      f = [c]
      g = [e]
      return (f, g)
  )JIT";

  // list of tuple of dict
  const auto nested_output_script_2 = R"JIT(
    def forward(self, a, b):
      c = (a + b).relu().nan_to_num().float()
      d = b * c
      e = a.flatten().nan_to_num() * b.flatten().nan_to_num()
      f = e.float().relu()
      g = ({"d": d}, {"b": b})
      h = ({"e": e}, {"f": f})
      return [g, h]
  )JIT";

  // lit of dict
  const auto nested_output_script_3 = R"JIT(
    def forward(self, a, b):
      c = (a + b).relu().nan_to_num().float()
      d = b * c
      e = a.flatten().nan_to_num() * b.flatten().nan_to_num()
      f = e.float().relu()
      g = {"d": d, "b": b}
      h = {"e": e, "f": f}
      return [g, h]
  )JIT";

  auto run_test = [&](std::vector<int64_t> shapes) {
    auto a = at::randn(shapes);
    auto b = at::randn(shapes);

    std::vector<IValue> args{a, b};
    testStaticRuntime(nested_output_script_0, args);
    testStaticRuntime(nested_output_script_1, args);
    testStaticRuntime(nested_output_script_2, args);
    testStaticRuntime(nested_output_script_3, args);

    if (shapes.size() > 0 && shapes[0] != 0) {
      shapes[0] *= 3;
      testStaticRuntime(
          nested_output_script_0, args, {at::randn(shapes), at::randn(shapes)});
      testStaticRuntime(
          nested_output_script_1, args, {at::randn(shapes), at::randn(shapes)});
    }
  };
  run_test({2, 3, 1, 2});
  run_test({2, 6});
}

// test memory reuse
TEST(StaticRuntime, LongModel) {
  torch::jit::Module mod = getLongScriptModel();
  auto a = torch::randn({2, 2});
  auto b = torch::randn({2, 2});
  auto c = torch::randn({2, 2});

  // run jit graph executor
  std::vector<at::IValue> input_ivalues({a, b, c});
  at::Tensor output_1 = mod.forward(input_ivalues).toTensor();

  // run static runtime
  std::vector<c10::IValue> input_tensors({a, b, c});
  torch::jit::StaticModule smod(mod);
  at::Tensor output_2 = smod(input_tensors, {}).toTensor();
  smod.runtime().check_for_memory_leak();
  EXPECT_TRUE(torch::allclose(output_1, output_2, 1e-6));
}

TEST(StaticRuntime, TrivialModel) {
  torch::jit::Module mod = getTrivialScriptModel();
  auto a = torch::randn({2, 2});
  auto b = torch::randn({2, 2});
  auto c = torch::randn({2, 2});

  // run jit graph executor
  std::vector<at::IValue> input_ivalues({a, b, c});
  at::Tensor output_1 = mod.forward(input_ivalues).toTensor();

  // run static runtime
  std::vector<c10::IValue> input_tensors({a, b, c});
  torch::jit::StaticModule smod(mod);
  at::Tensor output_2 = smod(input_tensors, {}).toTensor();
  smod.runtime().check_for_memory_leak();
  EXPECT_TRUE(torch::allclose(output_1, output_2, 1e-6));
}

TEST(StaticRuntime, DeepWide) {
  const int embedding_size = 32;
  const int num_features = 50;
  torch::jit::Module mod = getDeepAndWideSciptModel();
  torch::jit::StaticModule smod(mod);

  for (int batch_size : {1, 8, 32}) {
    for (int i = 0; i < 2; ++i) {
      auto ad_emb_packed = torch::randn({batch_size, 1, embedding_size});
      auto user_emb = torch::randn({batch_size, 1, embedding_size});
      auto wide = torch::randn({batch_size, num_features});

      // run jit graph executor
      std::vector<at::IValue> inputs({ad_emb_packed, user_emb, wide});
      auto output_1 = getTensor(mod.forward(inputs));

      // run static runtime
      std::vector<c10::IValue> input_tensors({ad_emb_packed, user_emb, wide});
      auto outputs = smod(input_tensors, {}).toTupleRef().elements();
      ASSERT_TRUE(outputs.size() > 0);
      at::Tensor output_2 = outputs[0].toTensor();
      smod.runtime().check_for_memory_leak();
      EXPECT_TRUE(torch::allclose(output_1, output_2, 1e-6));
    }
  }
}

TEST(StaticRuntime, KWargsAPI_1) {
  const int embedding_size = 32;
  const int num_features = 50;
  auto module = getDeepAndWideSciptModel();
  torch::jit::StaticModule smod(module);

  for (int batch_size : {1, 8, 32}) {
    for (int i = 0; i < 2; ++i) {
      auto ad_emb_packed = torch::randn({batch_size, 1, embedding_size});
      auto user_emb = torch::randn({batch_size, 1, embedding_size});
      auto wide = torch::randn({batch_size, num_features});
      {
        std::vector<at::IValue> inputs({ad_emb_packed, user_emb, wide});

        // run jit graph executor
        at::Tensor output_1 = getTensor(module.forward(inputs));

        // run static runtime
        c10::IValue output_ivalue = smod(inputs, {});
        smod.runtime().check_for_memory_leak();

        at::Tensor output_2 = getTensor(output_ivalue);
        EXPECT_TRUE(torch::allclose(output_1, output_2, 1e-6));

        // check for output aliasing
        EXPECT_EQ(output_ivalue.use_count(), 1);
        output_ivalue = IValue();

        EXPECT_EQ(output_2.getIntrusivePtr().use_count(), 1);
      }

      // check for input aliasing (deep & wide does not have ops
      // that create aliases of input tensors)
      EXPECT_EQ(ad_emb_packed.getIntrusivePtr().use_count(), 1);
      EXPECT_EQ(user_emb.getIntrusivePtr().use_count(), 1);
      EXPECT_EQ(wide.getIntrusivePtr().use_count(), 1);
    }
  }
}

TEST(StaticRuntime, KWargsAPI_2) {
  const int embedding_size = 32;
  const int num_features = 50;
  auto module = getDeepAndWideSciptModel();
  torch::jit::StaticModule smod(module);

  for (int batch_size : {1, 8, 32}) {
    for (int i = 0; i < 2; ++i) {
      auto ad_emb_packed = torch::randn({batch_size, 1, embedding_size});
      auto user_emb = torch::randn({batch_size, 1, embedding_size});
      auto wide = torch::randn({batch_size, num_features});
      {
        // run jit graph executor
        std::vector<at::IValue> args({ad_emb_packed, user_emb, wide});
        at::Tensor output_1 = getTensor(module.forward(args));

        std::unordered_map<std::string, c10::IValue> kwargs(
            {{"ad_emb_packed", ad_emb_packed},
             {"user_emb", user_emb},
             {"wide", wide}});

        // run static runtime
        c10::IValue output_ivalue = smod(std::vector<IValue>{}, kwargs);
        smod.runtime().check_for_memory_leak();

        at::Tensor output_2 = getTensor(output_ivalue);
        EXPECT_TRUE(torch::allclose(output_1, output_2, 1e-6));

        // check for output aliasing
        EXPECT_EQ(output_ivalue.use_count(), 1);
        output_ivalue = IValue();

        EXPECT_EQ(output_2.getIntrusivePtr().use_count(), 1);
      }

      EXPECT_EQ(ad_emb_packed.getIntrusivePtr().use_count(), 1);
      EXPECT_EQ(user_emb.getIntrusivePtr().use_count(), 1);
      EXPECT_EQ(wide.getIntrusivePtr().use_count(), 1);
    }
  }
}

TEST(StaticRuntime, CleanUpMemory) {
  const int embedding_size = 32;
  const int num_features = 50;
  torch::jit::Module mod = getDeepAndWideSciptModel();

  for (auto cleanup_activations : {true, false}) {
    for (auto enable_out_variant : {true, false}) {
      for (auto optimize_memory : {true, false}) {
        for (auto manage_output_tensors : {true, false}) {
          if (manage_output_tensors && !enable_out_variant) {
            // when manage_output_tensors is enabled, enable_out_variant
            // must be enabled too
            continue;
          }
          if (optimize_memory && !enable_out_variant) {
            // when optimize_memory is enabled, enable_out_variant must be
            // enabled too
            continue;
          }
          VLOG(1) << "cleanup_activations: " << cleanup_activations
                  << ", enable_out_variant: " << enable_out_variant
                  << ", optimize_memory: " << optimize_memory
                  << ", manage_output_tensors: " << manage_output_tensors;
          torch::jit::StaticModuleOptions opts{
              cleanup_activations,
              enable_out_variant,
              optimize_memory,
              manage_output_tensors};
          torch::jit::StaticModule smod(mod, false, opts);
          torch::jit::StaticRuntime runtime(smod);

          for (int batch_size : {1, 8, 32}) {
            for (int i = 0; i < 2; ++i) {
              auto ad_emb_packed =
                  torch::randn({batch_size, 1, embedding_size});
              auto user_emb = torch::randn({batch_size, 1, embedding_size});
              auto wide = torch::randn({batch_size, num_features});

              // run jit graph executor
              std::vector<at::IValue> inputs({ad_emb_packed, user_emb, wide});
              auto output_1 = getTensor(mod.forward(inputs));

              // run static runtime
              std::vector<c10::IValue> input_tensors(
                  {ad_emb_packed, user_emb, wide});
              auto outputs = runtime(input_tensors, {}).toTupleRef().elements();
              ASSERT_TRUE(outputs.size() > 0);
              auto output_2 = outputs[0].toTensor();
              runtime.check_for_memory_leak();
              EXPECT_TRUE(torch::allclose(output_1, output_2, 1e-6));
              if (manage_output_tensors) {
                runtime.deallocateOutputTensors();
                runtime.checkOutputTensorMemoryLeaks();
              }
            }
          }
        }
      }
    }
  }
}

TEST(StaticRuntime, ManageOutputTensors) {
  const std::string test_graph = R"IR(
    graph(%0 : Tensor):
      # With manage_output_tensor enabled, this tensor is managed.
      %1 : Tensor = aten::abs(%0)
      # The output container object is never managed.
      %2 : (Tensor) = prim::TupleConstruct(%1)
      return (%2)
  )IR";
  auto a = at::randn({2, 2});
  auto b = at::randn({3, 6});
  std::vector<at::IValue> args{a};
  std::vector<at::IValue> args2{b};
  testStaticRuntime(test_graph, args);
  testStaticRuntime(test_graph, args, args2);
}

TEST(
    StaticRuntime,
    ManageOutputTensorsReturnsOutputContainingManagedOutputTensor) {
  const std::string test_graph = R"IR(
    graph(%0 : Tensor):
      # With manage_output_tensor enabled, this tensor is managed.
      %1 : Tensor = aten::abs(%0)
      # The output container object is never managed.
      %2 : (Tensor) = prim::TupleConstruct(%1)
      return (%2)
  )IR";
  auto g = std::make_shared<torch::jit::Graph>();
  torch::jit::parseIR(test_graph, g.get());
  torch::jit::StaticModuleOptions opts{
      /*cleanup_activations=*/true,
      /*enable_out_variant=*/true,
      /*optimize_memory=*/true,
      /*manage_output_tensors=*/true};
  auto a = at::randn({2, 2});
  std::vector<at::IValue> args{a};
  torch::jit::StaticModule smod(g, opts);
  torch::jit::StaticRuntime runtime(smod);
  // Profile run.
  {
    IValue tuple = runtime(args, {});
    ASSERT_TRUE(tuple.isTuple());
    ASSERT_EQ(tuple.toTupleRef().elements().size(), 1);
    // Do not manage intput value.
    EXPECT_FALSE(runtime.isManagedOutputTensor(args[0]));
    // Do not manage direct output value.
    EXPECT_FALSE(runtime.isManagedOutputTensor(tuple));
    IValue element = tuple.toTupleRef().elements()[0];
    // Tensor to be managed, but not yet from the profile run.
    EXPECT_FALSE(runtime.isManagedOutputTensor(element));
    tuple = IValue();
    runtime.deallocateOutputTensors();
    runtime.checkOutputTensorMemoryLeaks();
  }
  // Second run that manages output tensors.
  {
    IValue tuple = runtime(args, {});
    ASSERT_TRUE(tuple.isTuple());
    ASSERT_EQ(tuple.toTupleRef().elements().size(), 1);
    // Do not manage intput value.
    EXPECT_FALSE(runtime.isManagedOutputTensor(args[0]));
    // Do not manage direct output value.
    EXPECT_FALSE(runtime.isManagedOutputTensor(tuple));
    IValue element = tuple.toTupleRef().elements()[0];
    // Tensor to be managed, but not yet from the profile run.
    EXPECT_TRUE(runtime.isManagedOutputTensor(element));
    tuple = IValue();
    runtime.deallocateOutputTensors();
    runtime.checkOutputTensorMemoryLeaks();
  }
}

TEST(StaticRuntime, ManageOutputTensorsWithDeallocateOutputTensors) {
  const int embedding_size = 32;
  const int num_features = 50;
  torch::jit::Module mod = getDeepAndWideSciptModel();

  torch::jit::StaticModuleOptions opts{
      /*cleanup_activations=*/true,
      /*enable_out_variant=*/true,
      /*optimize_memory=*/true,
      /*manage_output_tensors=*/true};
  torch::jit::StaticModule smod(mod, false, opts);
  torch::jit::StaticRuntime runtime(smod);
  // Reenter the runtime with the input with the same shape/different shapes.
  for (int batch_size : {8, 8, 24, 8}) {
    auto ad_emb_packed = torch::randn({batch_size, 1, embedding_size});
    auto user_emb = torch::randn({batch_size, 1, embedding_size});
    auto wide = torch::randn({batch_size, num_features});
    std::vector<c10::IValue> input_tensors({ad_emb_packed, user_emb, wide});
    runtime(input_tensors, {});
    runtime.check_for_memory_leak();
    runtime.deallocateOutputTensors();
    runtime.checkOutputTensorMemoryLeaks();
  }
}

TEST(StaticRuntime, ManageOutputTensorsWithoutDeallocateOutputTensors) {
  const int embedding_size = 32;
  const int num_features = 50;
  torch::jit::Module mod = getDeepAndWideSciptModel();

  torch::jit::StaticModuleOptions opts{
      /*cleanup_activations=*/true,
      /*enable_out_variant=*/true,
      /*optimize_memory=*/true,
      /*manage_output_tensors=*/true};
  torch::jit::StaticModule smod(mod, false, opts);
  torch::jit::StaticRuntime runtime(smod);
  int batch_size = 8;
  auto ad_emb_packed = torch::randn({batch_size, 1, embedding_size});
  auto user_emb = torch::randn({batch_size, 1, embedding_size});
  auto wide = torch::randn({batch_size, num_features});
  std::vector<c10::IValue> input_tensors({ad_emb_packed, user_emb, wide});
  // Profile run.
  runtime(input_tensors, {});
  runtime.deallocateOutputTensors();
  // Run again to allocate output Tensors without deallocating them.
  runtime(input_tensors, {});
  // Memory leak checking fails.
  EXPECT_THROW(runtime.checkOutputTensorMemoryLeaks(), std::exception);
  // Calling the runtime without deallocation fails too.
  EXPECT_THROW(runtime(input_tensors, {}), std::exception);
  // After deallocation, everything works fine.
  runtime.deallocateOutputTensors();
  runtime.checkOutputTensorMemoryLeaks();
  runtime(input_tensors, {});
}

TEST(StaticRuntime, DisableManageOutputTensors) {
  const std::string test_graph = R"IR(
    graph(%0 : Tensor):
      # With manage_output_tensor enabled, this tensor is managed.
      %1 : Tensor = aten::abs(%0)
      # The output container object is never managed.
      %2 : (Tensor) = prim::TupleConstruct(%1)
      return (%2)
  )IR";
  auto g = std::make_shared<torch::jit::Graph>();
  torch::jit::parseIR(test_graph, g.get());
  torch::jit::StaticModuleOptions opts{
      /*cleanup_activations=*/true,
      /*enable_out_variant=*/true,
      /*optimize_memory=*/true,
      /*manage_output_tensors=*/true};
  auto a = at::randn({2, 2});
  std::vector<at::IValue> args{a};
  torch::jit::StaticModule smod(g, opts);
  torch::jit::StaticRuntime runtime(smod);
  // Profile run.
  {
    IValue tuple = runtime(args, {});
    IValue element = tuple.toTupleRef().elements()[0];
    EXPECT_FALSE(runtime.isManagedOutputTensor(element));
    tuple = IValue();
    runtime.deallocateOutputTensors();
    runtime.checkOutputTensorMemoryLeaks();
  }
  // Second run that manages output tensors.
  {
    IValue tuple = runtime(args, {});
    IValue element = tuple.toTupleRef().elements()[0];
    EXPECT_TRUE(runtime.isManagedOutputTensor(element));
    tuple = IValue();
    runtime.deallocateOutputTensors();
    runtime.checkOutputTensorMemoryLeaks();
  }

  // Reset the runtime and start profiling again.
  runtime.disableManageOutputTensors();

  IValue copied_output_tensor;
  IValue original_output_tensor;
  // New profile run.
  {
    IValue tuple = runtime(args, {});
    IValue element = tuple.toTupleRef().elements()[0];
    EXPECT_FALSE(runtime.isManagedOutputTensor(element));
    copied_output_tensor = element.deepcopy();
    original_output_tensor = element;
    tuple = IValue();
    // No-op since manage_output_tensor is disabled now.
    runtime.deallocateOutputTensors();
    runtime.checkOutputTensorMemoryLeaks();
  }
  // Ensure that `original_output_tensor` is no longer managed: even after
  // calling `runtime.deallocateOutputTensors();` `original_output_tensor` still
  // contains a valid value.
  EXPECT_TRUE(
      original_output_tensor.toTensor().equal(copied_output_tensor.toTensor()));

  // Ensure that the second optimized run does not manage the output tensor
  // either.
  {
    IValue tuple = runtime(args, {});
    IValue element = tuple.toTupleRef().elements()[0];
    EXPECT_FALSE(runtime.isManagedOutputTensor(element));
    copied_output_tensor = element.deepcopy();
    original_output_tensor = element;
    tuple = IValue();
    // No-op since manage_output_tensor is disabled now.
    runtime.deallocateOutputTensors();
    runtime.checkOutputTensorMemoryLeaks();
  }
  // Ensure that `original_output_tensor` is no longer managed: even after
  // calling `runtime.deallocateOutputTensors();` `original_output_tensor` still
  // contains a valid value.
  EXPECT_TRUE(
      original_output_tensor.toTensor().equal(copied_output_tensor.toTensor()));
}

TEST(StaticRuntime, FusionPass) {
  const int embedding_size = 32;
  const int num_features = 50;
  for (int batch_size : {1, 8, 32}) {
    for (int i = 0; i < 2; ++i) {
      torch::jit::Module module = getDeepAndWideSciptModel();
      auto ad_emb_packed = torch::randn({batch_size, 1, embedding_size});
      auto user_emb = torch::randn({batch_size, 1, embedding_size});
      auto wide = torch::randn({batch_size, num_features});

      // run jit graph executor
      std::vector<at::IValue> inputs({ad_emb_packed, user_emb, wide});
      auto output_1 = getTensor(module.forward(inputs));

      Method method = module.get_method("forward");
      auto graph = method.graph();
      fuseStaticSubgraphs(graph, 2);
      bool hit = false;
      for (const auto& n : module.get_method("forward").graph()->nodes()) {
        if (n->kind() == torch::jit::prim::StaticSubgraph) {
          hit = true;
        }
      }
      EXPECT_TRUE(hit);
      auto output_2 = getTensor(module.forward(inputs));
      EXPECT_TRUE(torch::allclose(output_1, output_2, 1e-6));
    }
  }
}

static ProcessedNodeInputs createProcessedNodeInputs(
    c10::ArrayRef<uint16_t> inputs) {
  ProcessedNodeInputs result(inputs.size());
  for (const auto idx : c10::irange(inputs.size())) {
    result[idx] = inputs[idx];
  }
  return result;
}

TEST(
    ProcessedNode,
    VerifyNoMemoryOverlapWithImmutableInputsWithImmutableArguments) {
  const auto sigmoid_script = R"JIT(
    def forward(self, inp: Tensor):
        b = torch.sigmoid(inp).clone()
        return (b)
  )JIT";
  script::Module module("module");
  // Not using out= variant.
  module.define(sigmoid_script);
  torch::jit::StaticModule smodule(module);
  Node* sigmoid_node = getNodeWithKind(smodule, "aten::sigmoid");
  std::array<IValue, 2> values = {torch::randn({2, 3}), torch::randn({3, 1})};
  ProcessedFunction fn(
      sigmoid_node,
      /*enable_out_variant=*/true,
      /*check_memory_overlap=*/false);
  ProcessedNode pnode(sigmoid_node, &fn, createProcessedNodeInputs({0}), 1);
  pnode.set_values(values.data());
  EXPECT_TRUE(pnode.verify_no_memory_overlap(/* force_check*/ true));

  pnode.Output(0) = values[0];
  EXPECT_FALSE(pnode.verify_no_memory_overlap(/* force_check*/ true));
}

TEST(ProcessedNode, VerifyNoMemoryOverlapWithImmutableInputsWithInplaceOps) {
  script::Module module("module");
  // Using out= variant.
  module.define(sigmoid_inplace_script);
  torch::jit::StaticModule smodule(module);
  Node* sigmoid_node = getNodeWithKind(smodule, "aten::sigmoid");
  std::array<IValue, 2> values = {torch::randn({2, 3}), torch::randn({3, 1})};
  ProcessedFunction fn(
      sigmoid_node,
      /*enable_out_variant=*/true,
      /*check_memory_overlap=*/false);
  ProcessedNode pnode(sigmoid_node, &fn, createProcessedNodeInputs({0}), 1);
  pnode.set_values(values.data());

  ASSERT_EQ(&pnode.Output(0), &values[1]);
  EXPECT_TRUE(pnode.verify_no_memory_overlap());

  pnode.Output(0) = values[0];
  EXPECT_TRUE(pnode.verify_no_memory_overlap());
}

TEST(ProcessedNode, VerifyNoMemoryOverlapWithOverlappingOutputs) {
  auto g = std::make_shared<torch::jit::Graph>();
  torch::jit::parseIR(
      R"IR(
    graph(%0):
      %1 : Tensor, %2 : Tensor = prim::ListUnpack(%0)
      return (%1, %2))IR",
      g.get());
  torch::jit::StaticModule smodule(g);
  Node* list_unpack_node = getNodeWithKind(smodule, "prim::ListUnpack");
  {
    std::array<IValue, 3> values = {
        at::randn({2, 3}), at::empty({1, 3}), at::empty({4, 5})};
    ProcessedFunction fn(
        list_unpack_node,
        /*enable_out_variant=*/true,
        /*check_memory_overlap */ false);
    ProcessedNode list_unpack_pnode(
        list_unpack_node, &fn, createProcessedNodeInputs({0}), 1);
    list_unpack_pnode.set_values(values.data());
    ASSERT_EQ(list_unpack_pnode.outputs().size(), 2);
    EXPECT_TRUE(
        list_unpack_pnode.verify_no_memory_overlap(/* force_check*/ true));
  }
  {
    std::array<IValue, 3> values = {
        at::randn({2, 3}), at::empty({1, 3}), at::empty({4, 5})};
    ProcessedFunction fn(
        list_unpack_node,
        /*enable_out_variant=*/true,
        /*check_memory_overlap */ false);
    ProcessedNode list_unpack_pnode(
        list_unpack_node, &fn, createProcessedNodeInputs({0}), 1);
    list_unpack_pnode.set_values(values.data());
    auto b = at::randn({2, 3});
    list_unpack_pnode.Output(0) = b;
    list_unpack_pnode.Output(1) = b;
    EXPECT_FALSE(
        list_unpack_pnode.verify_no_memory_overlap(/* force_check*/ true));
  }
}

namespace test {
at::Tensor bad_add(const at::Tensor& self, int64_t b) {
  if (b == 0) {
    return self;
  }
  return at::native::add(self, b);
}

at::Tensor good_add(const at::Tensor& self, int64_t b) {
  if (b == 0) {
    return self;
  }
  return at::native::add(self, b);
}
} // namespace test

// test::bad_add has the schema with incorrect alias annotation.
// test::good_add has the correct alias annotation.
TORCH_LIBRARY_FRAGMENT(test, m) {
  m.def("bad_add(Tensor self, int b=0) -> Tensor");
  m.def("good_add(Tensor(a) self, int b=0) -> Tensor(a)");
}
TORCH_LIBRARY_IMPL(test, CPU, m) {
  m.impl("bad_add", ::test::bad_add);
  m.impl("good_add", ::test::good_add);
}

TEST(StaticRuntime, BadSchemaAliasInfo) {
  FLAGS_static_runtime_disable_debug_memory_overlap_check = true;
  const std::string src = R"IR(
      graph(%x: Tensor, %s: int):
          %c0 : int = prim::Constant[value=0]()
          %c1 : int = prim::Constant[value=1]()
          %a = aten::add(%x, %x, %c1)
          %b1 = test::bad_add(%a, %s) # b1 aliases a
          %t : (Tensor) = prim::TupleConstruct(%b1)
          return (%t)
  )IR";

  const auto x1 = at::randn({2, 2});
  // big enough to trigger resize of the internal buffer
  const auto x2 = at::randn({3, 6});
  testStaticRuntime(src, {x1, 0}, {x2, 10});
  // This test doesn't pass yet. This is the corner case mentioned in Step 2 of
  // [Check and correct bad schema alias info at runtime]
  // testStaticRuntime(src, {x1, 10}, {x2, 0});
  FLAGS_static_runtime_disable_debug_memory_overlap_check = false;
}

// This test repeats the last test, but with the correct schema alias
// annotations
TEST(StaticRuntime, GoodSchemaAliasInfo) {
  // comment out the prim::TupleConstruct repro the failure of
  // DCHECK(!isManagedOutputTensor(*outputs_[0]));
  const std::string src = R"IR(
      graph(%x: Tensor, %s: int):
          %c0 : int = prim::Constant[value=0]()
          %c1 : int = prim::Constant[value=1]()
          %a = aten::add(%x, %x, %c1)
          %b1 = test::good_add(%a, %s) # b1 aliases a
          # return (%b1)
          %t : (Tensor) = prim::TupleConstruct(%b1)
          return (%t)
  )IR";

  const auto x1 = at::randn({2, 2});
  // big enough to trigger resize of the internal buffer
  const auto x2 = at::randn({3, 6});
  testStaticRuntime(src, {x1, 0}, {x2, 10});
  testStaticRuntime(src, {x1, 10}, {x2, 0});
}

TEST(ProcessedFunction, ProcessedFunction) {
  const auto script = R"JIT(
    def forward(self, inp: Tensor):
        b = torch.sigmoid(inp).clone()
        c = torch.transpose(b, 0, 1)
        return (c)
  )JIT";
  script::Module module("module");
  module.define(script);
  torch::jit::StaticModule smodule(module);

  Node* sigmoid_node = getNodeWithKind(smodule, "aten::sigmoid");
  ProcessedFunction sigmoid_fn(
      sigmoid_node,
      /*enable_out_variant=*/true,
      /*check_memory_overlap=*/false);
  EXPECT_TRUE(sigmoid_fn.f());
  EXPECT_EQ(sigmoid_fn.kind(), ProcessedFunction::Kind::kOutVariant);
  EXPECT_FALSE(sigmoid_fn.checkMemoryOverlap());

  Node* transpose_node = getNodeWithKind(smodule, "aten::transpose");
  ProcessedFunction transpose_fn(
      transpose_node,
      /*enable_out_variant=*/true,
      /*check_memory_overlap=*/false);
  EXPECT_TRUE(transpose_fn.f());
  EXPECT_EQ(transpose_fn.kind(), ProcessedFunction::Kind::kNativeFunction);
  EXPECT_FALSE(transpose_fn.checkMemoryOverlap());
}

TEST(ManagedTensorRanges, NoAliases) {
  const std::string src = R"IR(
    graph(%x : Tensor):
        %y : Tensor = aten::mul(%x, %x)
        %z : Tensor = aten::mul(%y, %x)
        %output : Tensor = aten::mul(%z, %z)
        return (%output)
  )IR";
  auto graph = std::make_shared<Graph>();
  std::unordered_map<std::string, Value*> vmap;
  parseIR(src, graph.get(), vmap);

  auto* y = vmap["y"];
  auto* z = vmap["z"];

  FastSet<const Value*> managed_tensors = {y, z};
  ManagedTensorRanges ranges(graph, managed_tensors);

  std::vector<Node*> nodes(
      graph->block()->nodes().begin(), graph->block()->nodes().end());
  ASSERT_EQ(nodes.size(), 3);

  EXPECT_FALSE(ranges.nodeFreesManagedTensors(nodes[0]));

  EXPECT_TRUE(ranges.nodeFreesManagedTensors(nodes[1]));
  EXPECT_EQ(
      ranges.availableTensorValuesAfterNode(nodes[1]),
      std::vector<const Value*>{y});

  EXPECT_TRUE(ranges.nodeFreesManagedTensors(nodes[2]));
  EXPECT_EQ(
      ranges.availableTensorValuesAfterNode(nodes[2]),
      std::vector<const Value*>{z});
}

TEST(ManagedTensorRanges, AliasExtendingLifetimes) {
  const std::string src = R"IR(
    graph(%x : Tensor):
        %y : Tensor = aten::mul(%x, %x)
        %y_size : int[] = aten::size(%y)
        %z1 : Tensor = aten::mul(%y, %y)
        %y_alias : Tensor = aten::view(%y, %y_size)
        %z2 : Tensor = aten::mul(%y_alias, %y_alias)
        %output : Tensor = aten::mul(%z1, %z2)
        return (%output)
  )IR";
  auto graph = std::make_shared<Graph>();
  std::unordered_map<std::string, Value*> vmap;
  parseIR(src, graph.get(), vmap);

  auto* y = vmap["y"];
  auto* z1 = vmap["z1"];
  auto* z2 = vmap["z2"];

  FastSet<const Value*> managed_tensors = {y, z1, z2};
  ManagedTensorRanges ranges(graph, managed_tensors);

  std::vector<Node*> nodes(
      graph->block()->nodes().begin(), graph->block()->nodes().end());
  ASSERT_EQ(nodes.size(), 6);

  for (const auto i : c10::irange(4)) {
    EXPECT_FALSE(ranges.nodeFreesManagedTensors(nodes[i]));
  }

  EXPECT_TRUE(ranges.nodeFreesManagedTensors(nodes[4]));
  EXPECT_EQ(
      ranges.availableTensorValuesAfterNode(nodes[4]),
      std::vector<const Value*>{y});

  EXPECT_TRUE(ranges.nodeFreesManagedTensors(nodes[5]));
  const auto& available_after_5 =
      ranges.availableTensorValuesAfterNode(nodes[5]);
  // We don't care about the order, so convert to set. But make sure
  // there are no duplicates.
  FastSet<const Value*> available_after_5_set(
      available_after_5.begin(), available_after_5.end());
  EXPECT_EQ(available_after_5_set.size(), available_after_5.size());
  EXPECT_EQ(available_after_5_set, FastSet<const Value*>({z1, z2}));
}

TEST(ManagedTensorRanges, LifetimeOverlap) {
  const std::string src = R"IR(
    graph(%a : Tensor):
        %b : Tensor = aten::mul(%a, %a)
        %c : Tensor = aten::mul(%b, %b)
        %c_size : int[] = aten::size(%c)
        %c_alias : Tensor = aten::view(%c, %c_size)
        %d : Tensor = aten::mul(%a, %a)
        %e : Tensor = aten::mul(%c_alias, %c_alias)
        %output : Tensor = aten::mul(%e, %e)
        return (%output)
  )IR";
  auto graph = std::make_shared<Graph>();
  std::unordered_map<std::string, Value*> vmap;
  parseIR(src, graph.get(), vmap);
  auto* b = vmap["b"];
  auto* c = vmap["c"];
  auto* d = vmap["d"];
  auto* e = vmap["e"];

  ManagedTensorRanges ranges(graph, {b, c, d, e});
  const std::vector<std::pair<Value*, Value*>> overlapping_values{
      {b, c}, {c, d}, {c, e}};

  const std::vector<std::pair<Value*, Value*>> disjoint_values{{b, d}, {b, e}};

  for (const auto& values : overlapping_values) {
    EXPECT_TRUE(ranges.lifetimesOverlap(values.first, values.second));
    EXPECT_TRUE(ranges.lifetimesOverlap(values.second, values.first));
  }
  for (const auto& values : disjoint_values) {
    EXPECT_FALSE(ranges.lifetimesOverlap(values.first, values.second));
    EXPECT_FALSE(ranges.lifetimesOverlap(values.second, values.first));
  }
}

TEST(ManagedTensorRanges, OverlappingLifetimesContainers) {
  const std::string src = R"IR(
    graph(%a : Tensor):
        %b : Tensor = aten::mul(%a, %a)
        %c : Tensor = aten::mul(%b, %b)
        %tuple : (Tensor, Tensor) = prim::TupleConstruct(%b, %c)
        %b_alias : Tensor, %c_alias : Tensor = prim::TupleUnpack(%tuple)
        %d : Tensor = aten::mul(%b_alias, %c_alias)
        %output : Tensor = aten::mul(%d, %d)
        return (%output)
  )IR";
  auto graph = std::make_shared<Graph>();
  std::unordered_map<std::string, Value*> vmap;
  parseIR(src, graph.get(), vmap);
  auto* b = vmap["b"];
  auto* c = vmap["c"];
  auto* d = vmap["d"];

  ManagedTensorRanges ranges(graph, {b, c, d});

  EXPECT_TRUE(ranges.lifetimesOverlap(b, c));
  EXPECT_TRUE(ranges.lifetimesOverlap(b, d));
  EXPECT_TRUE(ranges.lifetimesOverlap(c, d));
}

TEST(ManagedTensorRanges, OverlappingLifetimesOutputs) {
  const std::string src = R"IR(
    graph(%a : Tensor):
        %output : Tensor = aten::mul(%a, %a)
        %b : Tensor = aten::mul(%a, %a)
        return (%output)
  )IR";
  auto graph = std::make_shared<Graph>();
  std::unordered_map<std::string, Value*> vmap;
  parseIR(src, graph.get(), vmap);
  auto* b = vmap["b"];
  auto* output = vmap["output"];

  ManagedTensorRanges ranges(graph, {b, output});

  EXPECT_TRUE(ranges.lifetimesOverlap(b, output));
}

namespace {

// For checking the correctness of assignStorageToManageTensors, the following
// conditions must hold
// 1. All managed tensors are assigned to some storage group, and a tensor
//    may not be assigned to more than 1 storage group.
// 2. Managed tensors with overlapping lifetimes should not be in the same
//    storage group.
// 3. The number of reused tensors is >= min_reused_tensors.
void checkStorageGroups(
    const std::vector<StorageGroup>& storage_groups,
    const ManagedTensorRanges& ranges,
    const FastMap<const Value*, at::Tensor*>& tensor_value_to_tensor,
    size_t min_reused_tensors) {
  // Some extra bookkeeping; construct the set of managed Tensor* and
  // invert the tensor_value_to_tensor map. StorageGroup stores
  // Tensor*, so this will make everything a little easier.
  FastMap<at::Tensor*, const Value*> tensor_to_tensor_value;
  FastSet<at::Tensor*> managed_tensors;
  for (auto& key_value : tensor_value_to_tensor) {
    ASSERT_EQ(
        tensor_to_tensor_value.find(key_value.second),
        tensor_to_tensor_value.end());
    tensor_to_tensor_value.emplace(key_value.second, key_value.first);
    managed_tensors.insert(key_value.second);
  }

  // Condition (1)
  FastSet<at::Tensor*> actual_assigned_tensors;
  for (const auto& storage_group : storage_groups) {
    for (auto* tensor : storage_group.group()) {
      ASSERT_EQ(
          actual_assigned_tensors.find(tensor), actual_assigned_tensors.end());
      actual_assigned_tensors.insert(tensor);
    }
  }
  ASSERT_EQ(actual_assigned_tensors, managed_tensors);

  // Condition (2)
  size_t num_reused = 0;
  for (const auto& storage_group : storage_groups) {
    const auto& group = storage_group.group();
    num_reused += group.size() - 1;
    for (const auto i : c10::irange(group.size() - 1)) {
      for (const auto j : c10::irange(i + 1, group.size())) {
        const auto* v1 = tensor_to_tensor_value.at(group[i]);
        const auto* v2 = tensor_to_tensor_value.at(group[j]);
        EXPECT_FALSE(ranges.lifetimesOverlap(v1, v2));
      }
    }
  }

  // Condition (3)
  EXPECT_GE(num_reused, min_reused_tensors);
}

// A convenience function for testing assignStorageToManagedTensors. It
// takes in an IR graph as well as a map from managed tensor name to tensor
// value. It constructs all of the necessary data structures, invokes
// assignStorageToManageTensors, and verifies correctness with
// checkStorageGroups.
void testAssignStorageToManagedTensors(
    const std::string& src,
    FastMap<std::string, at::Tensor> managed_tensor_name_to_tensor,
    size_t min_reused_tensors) {
  auto graph = std::make_shared<Graph>();
  std::unordered_map<std::string, Value*> vmap;
  parseIR(src, graph.get(), vmap);

  FastSet<const Value*> managed_tensor_values;
  FastMap<const Value*, at::Tensor*> tensor_value_to_tensor;

  for (auto& key_value : managed_tensor_name_to_tensor) {
    const auto& tensor_name = key_value.first;
    auto vmap_it = vmap.find(tensor_name);
    ASSERT_TRUE(vmap_it != vmap.end());
    managed_tensor_values.insert(vmap_it->second);
    tensor_value_to_tensor.emplace(vmap_it->second, &key_value.second);
  }
  ASSERT_EQ(managed_tensor_values.size(), tensor_value_to_tensor.size());

  auto ranges = ManagedTensorRanges(graph, managed_tensor_values);
  std::vector<StorageGroup> groups;
  assignStorageToManagedTensors(
      graph->block()->nodes(), ranges, tensor_value_to_tensor, groups);

  checkStorageGroups(
      groups, ranges, tensor_value_to_tensor, min_reused_tensors);
}

} // namespace

TEST(AssignStorageToManagedTensors, NoAliases) {
  const auto src = R"IR(
    graph(%a : Tensor):
      %b : Tensor = aten::mul(%a, %a)
      %c : Tensor = aten::mul(%b, %b)
      %d : Tensor = aten::mul(%c, %c)
      %e : Tensor = aten::mul(%b, %d)
      %output : Tensor = aten::mul(%e, %e)
      return (%output)
  )IR";
  FastMap<std::string, at::Tensor> managed_tensor_name_to_tensor{
      {"b", at::randn({1})},
      {"c", at::randn({1})},
      {"d", at::randn({1})},
      {"e", at::randn({1})}};
  const size_t min_reused_tensors = 1;
  testAssignStorageToManagedTensors(
      src, std::move(managed_tensor_name_to_tensor), min_reused_tensors);
}

TEST(AssignStorageToManagedTensors, Aliases) {
  const auto src = R"IR(
    graph(%a : Tensor):
      %b : Tensor = aten::mul(%a, %a)
      %c : Tensor = aten::mul(%b, %b)
      %d : Tensor = aten::mul(%c, %c)
      %c_size : int[] = aten::size(%c)
      %c_alias : Tensor = aten::view(%c, %c_size)
      %e : Tensor = aten::mul(%b, %d)
      %f : Tensor = aten::mul(%c_alias, %c_alias)
      %output : Tensor = aten::mul(%e, %f)
      return (%output)
  )IR";
  FastMap<std::string, at::Tensor> managed_tensor_name_to_tensor{
      {"b", at::randn({1})},
      {"c", at::randn({1})},
      {"d", at::randn({1})},
      {"e", at::randn({1})},
      {"f", at::randn({1})}};
  const size_t min_reused_tensors = 1;
  testAssignStorageToManagedTensors(
      src, std::move(managed_tensor_name_to_tensor), min_reused_tensors);
}

namespace {
TORCH_LIBRARY_FRAGMENT(static_runtime_tests, m) {
  m.def(torch::schema(
      "static_runtime_tests::variadic_outputs(Tensor a) -> ...",
      at::AliasAnalysisKind::PURE_FUNCTION));
}
} // namespace

TEST(AssignStorageToManagedTensors, MultipleUnused) {
  const auto src = R"IR(
    graph(%a : Tensor):
        %z : Tensor = aten::mul(%a, %a)
        %out: Tensor = aten::mul(%z, %z)
        %x : Tensor, %y : Tensor = static_runtime_tests::variadic_outputs(%a)
        return (%out)
  )IR";
  FastMap<std::string, at::Tensor> managed_tensor_name_to_tensor{
      {"z", at::randn({1})}, {"x", at::randn({1})}, {"y", at::randn({1})}};
  const size_t min_reused_tensors = 1;
  testAssignStorageToManagedTensors(
      src, std::move(managed_tensor_name_to_tensor), min_reused_tensors);
}
