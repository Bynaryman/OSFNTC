#ifdef USE_VULKAN_API

#include <gtest/gtest.h>
#include <ATen/ATen.h>

// TODO: These functions should move to a common place.

namespace {

bool checkRtol(const at::Tensor& diff, const std::vector<at::Tensor>& inputs) {
  float maxValue = 0.0f;

  for (const auto& tensor : inputs) {
    maxValue = fmax(tensor.abs().max().item<float>(), maxValue);
  }

#ifdef USE_VULKAN_FP16_INFERENCE
  constexpr float tolerance = 1e-2;
#else
  constexpr float tolerance = 1e-5;
#endif

  return diff.abs().max().item<float>() <= (tolerance * maxValue);
}

bool almostEqual(const at::Tensor& a, const at::Tensor& b) {
  return checkRtol(a - b, {a, b});
}

bool exactlyEqual(const at::Tensor& a, const at::Tensor& b) {
  return (a - b).abs().max().item<float>() == 0.0f;
}

void showRtol(const at::Tensor& a, const at::Tensor& b) {
  const auto diff = (a - b).abs();

  float maxValue = a.abs().max().item<float>();
  maxValue = fmax(b.abs().max().item<float>(), maxValue);

#ifdef USE_VULKAN_FP16_INFERENCE
  constexpr float tolerance = 1e-2;
#else
  constexpr float tolerance = 1e-5;
#endif

  const float maxDiff = maxValue * tolerance;
  std::cout << "Max Diff allowed: " << maxDiff << std::endl;
  if (diff.sizes().size() == 2) {
    for (int y = 0; y < diff.sizes()[0]; y++) {
      std::cout << y << ":";
      for (int x = 0; x < diff.sizes()[1]; x++) {
        float diff_xy = diff[y][x].item<float>();
        if (diff_xy > maxDiff) {
          std::cout << std::setw(5) << x;
        }
        else {
          std::cout << std::setw(5) << " ";
        }
      }
      std::cout << std::endl;
    }
  }
}


static void gen_allpermutations(std::vector<std::vector<int64_t>>& out, std::vector<int64_t> in, int i) {
  // generate all permutations of a given dims
  if (i == in.size()) {
    out.push_back(in);
  }
  else {
    for (int j = i; j < in.size(); ++j) {
      std::swap(in[i], in[j]);
      gen_allpermutations(out, in, i + 1);
    }
  }
}

static void slice_test(const std::vector<int64_t>& size, int64_t dim, c10::optional<int64_t> start, c10::optional<int64_t> end, int64_t step) {
  // Guard
  if (!at::is_vulkan_available()) {
    return;
  }

  // Arrange
  const auto in_cpu = at::rand(size, at::device(at::kCPU).dtype(at::kFloat));
  const auto in_vulkan = in_cpu.vulkan();

  // Act
  const auto out_cpu = at::slice(in_cpu, dim, start, end, step);
  const auto out_vulkan = at::slice(in_vulkan, dim, start, end, step);

  // Assert
  const auto check = almostEqual(out_cpu, out_vulkan.cpu());
  if (!check) {
    showRtol(out_cpu, out_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

static void slice_tests(const std::unordered_map<int64_t, std::vector<int64_t>>& dim2sizes) {
  for (const auto& dim2size : dim2sizes) {
    slice_test(dim2size.second, dim2size.first, 10, 30, 1);         // i.e., 4D tensor's equivalent indexing = [:,:,:,10:30:1]
    slice_test(dim2size.second, dim2size.first, 10, 30, 7);         // i.e., 4D tensor's equivalent indexing = [:,:,:,10:30:7]
    slice_test(dim2size.second, dim2size.first, 10, 50, 2);         // i.e., 4D tensor's equivalent indexing = [:,:,:,10:50:2] with end=out of range
    slice_test(dim2size.second, dim2size.first, -60, 60, 2);        // i.e., 4D tensor's equivalent indexing = [:,:,:,-60:60:2] with start/end=out of range
    slice_test(dim2size.second, dim2size.first, -30, -10, 1);       // i.e., 4D tensor's equivalent indexing = [:,:,:,-30:-10:1] with negative start/end
    slice_test(dim2size.second, dim2size.first, 0, INT64_MAX, 1);   // i.e., 4D 's equivalent indexing = [:,:,:,0:9223372036854775807:1] with end=INT64_MAX
    slice_test(dim2size.second, dim2size.first, -10, INT64_MAX, 1); // i.e., 4D 's equivalent indexing = [:,:,:,-10:9223372036854775807:1] with negative start and end=INT64_MAX
    slice_test(dim2size.second, dim2size.first, INT64_MIN, INT64_MAX, 1); // i.e., 4D 's equivalent indexing = [:,:,:,-9223372036854775808:9223372036854775807:1] with start=INT64_MIN and end=INT64_MAX
    slice_test(dim2size.second, dim2size.first, {}, {}, 1);         // i.e., 4D 's equivalent indexing = [:,:,:,::1] with empty start/end
  }
}

} // namespace

namespace {

TEST(VulkanAPITest, adaptive_avg_pool2d) {
  if (!at::is_vulkan_available()) {
    return;
  }
  c10::InferenceMode mode;

  const auto in_cpu = at::rand({5, 7, 47, 31}, at::TensorOptions(at::kCPU).dtype(at::kFloat));
  const auto out_cpu = at::adaptive_avg_pool2d(in_cpu, {3, 3});
  const auto out_vulkan = at::adaptive_avg_pool2d(in_cpu.vulkan(), {3, 3});

  const auto check = almostEqual(out_cpu, out_vulkan.cpu());
  if (!check) {
    showRtol(out_cpu, out_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, add) {
  if (!at::is_vulkan_available()) {
    return;
  }

  const auto a_cpu = at::rand({11, 7, 139, 109}, at::device(at::kCPU).dtype(at::kFloat));
  const auto a_vulkan = a_cpu.vulkan();

  const auto b_cpu = at::rand({11, 7, 139, 109}, at::device(at::kCPU).dtype(at::kFloat));
  const auto b_vulkan = b_cpu.vulkan();

  const auto c_cpu = at::add(a_cpu, b_cpu, 2.1f);
  const auto c_vulkan = at::add(a_vulkan, b_vulkan, 2.1f);

  const auto check = almostEqual(c_cpu, c_vulkan.cpu());
  if (!check) {
    showRtol(c_cpu, c_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, add_broadcast0) {
  if (!at::is_vulkan_available()) {
    return;
  }

  const auto a_cpu = at::rand({3, 5, 179, 221}, at::device(at::kCPU).dtype(at::kFloat));
  const auto a_vulkan = a_cpu.vulkan();

  const auto b_cpu = at::rand({3, 5, 1, 1}, at::device(at::kCPU).dtype(at::kFloat));
  const auto b_vulkan = b_cpu.vulkan();

  const auto c_cpu = at::add(a_cpu, b_cpu, 1.8f);
  const auto c_vulkan = at::add(a_vulkan, b_vulkan, 1.8f);

  const auto check = almostEqual(c_cpu, c_vulkan.cpu());
  if (!check) {
    showRtol(c_cpu, c_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, add_broadcast1) {
  if (!at::is_vulkan_available()) {
    return;
  }

  const auto a_cpu = at::rand({3, 5, 179, 221}, at::device(at::kCPU).dtype(at::kFloat));
  const auto a_vulkan = a_cpu.vulkan();

  const auto b_cpu = at::rand({3, 5, 1, 221}, at::device(at::kCPU).dtype(at::kFloat));
  const auto b_vulkan = b_cpu.vulkan();

  const auto c_cpu = at::add(a_cpu, b_cpu, 1.8f);
  const auto c_vulkan = at::add(a_vulkan, b_vulkan, 1.8f);

  const auto check = almostEqual(c_cpu, c_vulkan.cpu());
  if (!check) {
    showRtol(c_cpu, c_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, add_broadcast2) {
  if (!at::is_vulkan_available()) {
    return;
  }

  const auto a_cpu = at::rand({3, 4, 179, 221}, at::device(at::kCPU).dtype(at::kFloat));
  const auto a_vulkan = a_cpu.vulkan();

  const auto b_cpu = at::rand({4, 1, 1}, at::device(at::kCPU).dtype(at::kFloat));
  const auto b_vulkan = b_cpu.vulkan();

  const auto c_cpu = at::add(a_cpu, b_cpu, 2.5f);
  const auto c_vulkan = at::add(a_vulkan, b_vulkan, 2.5f);

  const auto check = almostEqual(c_cpu, c_vulkan.cpu());
  if (!check) {
    showRtol(c_cpu, c_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, add_) {
  if (!at::is_vulkan_available()) {
    return;
  }

  auto a_cpu = at::rand({61, 17, 29, 83}, at::device(at::kCPU).dtype(at::kFloat));
  auto a_vulkan = a_cpu.vulkan();

  const auto b_cpu = at::rand({61, 17, 29, 83}, at::device(at::kCPU).dtype(at::kFloat));
  const auto b_vulkan = b_cpu.vulkan();

  a_cpu.add_(b_cpu, 2.1f);
  a_vulkan.add_(b_vulkan, 2.1f);

  const auto check = almostEqual(a_cpu, a_vulkan.cpu());
  if (!check) {
    showRtol(b_cpu, b_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, add_broadcast0_) {
  if (!at::is_vulkan_available()) {
    return;
  }

  auto a_cpu = at::rand({16, 17, 29, 83}, at::device(at::kCPU).dtype(at::kFloat));
  auto a_vulkan = a_cpu.vulkan();

  const auto b_cpu = at::rand({16, 17, 29, 1}, at::device(at::kCPU).dtype(at::kFloat));
  const auto b_vulkan = b_cpu.vulkan();

  a_cpu.add_(b_cpu, 2.1f);
  a_vulkan.add_(b_vulkan, 2.1f);

  const auto check = almostEqual(a_cpu, a_vulkan.cpu());
  if (!check) {
    showRtol(b_cpu, b_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, add_broadcast1_) {
  if (!at::is_vulkan_available()) {
    return;
  }

  auto a_cpu = at::rand({3, 8, 29, 83}, at::device(at::kCPU).dtype(at::kFloat));
  auto a_vulkan = a_cpu.vulkan();

  const auto b_cpu = at::rand({3, 8, 1, 1}, at::device(at::kCPU).dtype(at::kFloat));
  const auto b_vulkan = b_cpu.vulkan();

  a_cpu.add_(b_cpu, 2.1f);
  a_vulkan.add_(b_vulkan, 2.1f);

  const auto check = almostEqual(a_cpu, a_vulkan.cpu());
  if (!check) {
    showRtol(b_cpu, b_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, add_scalar) {
  if (!at::is_vulkan_available()) {
    return;
  }

  const auto a_cpu = at::rand({13, 23, 59, 73}, at::device(at::kCPU).dtype(at::kFloat));
  const auto a_vulkan = a_cpu.vulkan();

  const float b_scalar = 3.1415f;

  const auto c_cpu = at::add(a_cpu, b_scalar, 2.1f);
  const auto c_vulkan = at::add(a_vulkan, b_scalar, 2.1f);

  const auto check = almostEqual(c_cpu, c_vulkan.cpu());
  if (!check) {
    showRtol(c_cpu, c_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, add_scalar_) {
  if (!at::is_vulkan_available()) {
    return;
  }

  auto a_cpu = at::rand({47, 2, 23, 97}, at::device(at::kCPU).dtype(at::kFloat));
  auto a_vulkan = a_cpu.vulkan();

  const float b_scalar = 3.1415f;

  a_cpu.add_(b_scalar, 2.1f);
  a_vulkan.add_(b_scalar, 2.1f);

  const auto check = almostEqual(a_cpu, a_vulkan.cpu());
  if (!check) {
    showRtol(a_cpu, a_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, addmm) {
  if (!at::is_vulkan_available()) {
    return;
  }

  constexpr float alpha = 2.1f;
  constexpr float beta = 103.24;

  const auto bias_cpu = at::rand({179, 163}, at::device(at::kCPU).dtype(at::kFloat));
  const auto m1_cpu = at::rand({179, 67}, at::device(at::kCPU).dtype(at::kFloat));
  const auto m2_cpu = at::rand({67, 163}, at::device(at::kCPU).dtype(at::kFloat));
  const auto out_cpu = at::addmm(bias_cpu, m1_cpu, m2_cpu, beta, alpha);

  const auto m1_vulkan = m1_cpu.vulkan();
  const auto out_vulkan = at::addmm(bias_cpu, m1_vulkan, m2_cpu, beta, alpha);

  const auto check = almostEqual(out_cpu, out_vulkan.cpu());
  if (!check) {
    showRtol(out_cpu, out_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, addmm_expand) {
  if (!at::is_vulkan_available()) {
    return;
  }

  constexpr float alpha = 2.1f;
  constexpr float beta = 103.24;

  const auto bias_cpu = at::rand({1000}, at::device(at::kCPU).dtype(at::kFloat));
  const auto m1_cpu = at::rand({1, 1280}, at::device(at::kCPU).dtype(at::kFloat));
  const auto m2_cpu = at::rand({1280, 1000}, at::device(at::kCPU).dtype(at::kFloat));
  const auto out_cpu = at::addmm(bias_cpu, m1_cpu, m2_cpu, beta, alpha);

  const auto m1_vulkan = m1_cpu.vulkan();
  const auto out_vulkan = at::addmm(bias_cpu, m1_vulkan, m2_cpu, beta, alpha);

  const auto check = almostEqual(out_cpu, out_vulkan.cpu());
  if (!check) {
    showRtol(out_cpu, out_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, avg_pool2d) {
  if (!at::is_vulkan_available()) {
    return;
  }

  const auto in_cpu = at::rand({3, 19, 43, 79}, at::TensorOptions(at::kCPU).dtype(at::kFloat));
  const auto out_cpu = at::avg_pool2d(in_cpu, {5, 3}, {1, 2}, {2, 0}, true);
  const auto out_vulkan = at::avg_pool2d(in_cpu.vulkan(), {5, 3}, {1, 2}, {2, 0}, true);

  const auto check = almostEqual(out_cpu, out_vulkan.cpu());
  if (!check) {
    showRtol(out_cpu, out_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, clamp) {
  if (!at::is_vulkan_available()) {
    return;
  }

  const auto in_cpu = at::rand({17, 197, 302, 5}, at::device(at::kCPU).dtype(at::kFloat));
  const auto in_vulkan = in_cpu.vulkan();

  const float min_value = 0.2f;
  const float max_value = 0.8f;

  const auto out_cpu = at::clamp(in_cpu, min_value, max_value);
  const auto out_vulkan = at::clamp(in_vulkan, min_value, max_value);

  const auto check = almostEqual(out_cpu, out_vulkan.cpu());
  if (!check) {
    showRtol(out_cpu, out_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, clamp_) {
  if (!at::is_vulkan_available()) {
    return;
  }

  const auto cpu = at::rand({17, 197, 302, 5}, at::device(at::kCPU).dtype(at::kFloat));
  const auto vulkan = cpu.vulkan();

  const float min_value = 0.2f;
  const float max_value = 0.8f;

  cpu.clamp_(min_value, max_value);
  vulkan.clamp_(min_value, max_value);

  const auto check = almostEqual(cpu, vulkan.cpu());
  if (!check) {
    showRtol(cpu, vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, conv2d) {
  if (!at::is_vulkan_available()) {
    return;
  }

  constexpr int64_t groups = 1;
  constexpr std::array<int64_t, 2u> stride{2, 2};
  constexpr std::array<int64_t, 2u> padding{1, 1};
  //TODO: Support conv2d with dilation != 1
  constexpr std::array<int64_t, 2u> dilation{1, 1};

  constexpr struct {
    uint32_t batches;
    uint32_t channels;
    uint32_t width;
    uint32_t height;

    std::array<int64_t, 4u> size() const {
      return {
        batches,
        channels,
        width,
        height,
      };
    }
  } input {1, 3, 8, 8};

  constexpr struct {
    uint32_t output_channels;
    uint32_t input_channels;
    uint32_t width;
    uint32_t height;

    std::array<int64_t, 4u> size() const {
      return {
        output_channels,
        input_channels,
        width,
        height,
      };
    }
  } weights {1, input.channels, 3, 3};

  const auto input_cpu = at::randn(input.size(), at::device(at::kCPU).dtype(at::kFloat));
  const auto weights_cpu = at::randn(weights.size(), at::device(at::kCPU).dtype(at::kFloat));
  const auto bias_cpu = at::randn({weights.output_channels}, at::device(at::kCPU).dtype(at::kFloat));

  const auto output_cpu = at::conv2d(
      input_cpu,
      weights_cpu,
      bias_cpu,
      stride,
      padding,
      dilation,
      groups);

  const auto output_vulkan = at::conv2d(
      input_cpu.vulkan(),
      weights_cpu,
      bias_cpu,
      stride,
      padding,
      dilation,
      groups).cpu();

  const bool check = almostEqual(output_cpu, output_vulkan);
  if (!check) {
    showRtol(output_cpu, output_vulkan);
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, conv2d_dw) {
  if (!at::is_vulkan_available()) {
    return;
  }

  constexpr int64_t groups = 7;
  constexpr std::array<int64_t, 2u> stride{2, 3};
  constexpr std::array<int64_t, 2u> padding{0, 4};
  constexpr std::array<int64_t, 2u> dilation{3, 1};

  constexpr struct {
    uint32_t batches;
    uint32_t channels;
    uint32_t width;
    uint32_t height;

    std::array<int64_t, 4u> size() const {
      return {
        batches,
        channels,
        width,
        height,
      };
    }
  } input {1, groups, 137, 199};

  constexpr struct {
    uint32_t output_channels;
    uint32_t input_channels;
    uint32_t width;
    uint32_t height;

    std::array<int64_t, 4u> size() const {
      return {
        output_channels,
        input_channels,
        width,
        height,
      };
    }
  } weights {groups, 1, 17, 7};

  const auto input_cpu = at::rand(input.size(), at::device(at::kCPU).dtype(at::kFloat));
  const auto weights_cpu = at::rand(weights.size(), at::device(at::kCPU).dtype(at::kFloat));
  const auto bias_cpu = at::rand({weights.output_channels}, at::device(at::kCPU).dtype(at::kFloat));

  const auto output_cpu = at::conv2d(
      input_cpu,
      weights_cpu,
      bias_cpu,
      stride,
      padding,
      dilation,
      groups);

  const auto output_vulkan = at::conv2d(
      input_cpu.vulkan(),
      weights_cpu,
      bias_cpu,
      stride,
      padding,
      dilation,
      groups);

  const bool check = almostEqual(output_cpu, output_vulkan.cpu());
  if (!check) {
    showRtol(output_cpu, output_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, conv2d_pw) {
  if (!at::is_vulkan_available()) {
    return;
  }

  constexpr int64_t groups = 1;
  constexpr std::array<int64_t, 2u> stride{1, 1};
  constexpr std::array<int64_t, 2u> padding{0, 0};
  constexpr std::array<int64_t, 2u> dilation{1, 1};

  constexpr struct {
    uint32_t batches;
    uint32_t channels;
    uint32_t width;
    uint32_t height;

    std::array<int64_t, 4u> size() const {
      return {
        batches,
        channels,
        width,
        height,
      };
    }
  } input {1, 17, 127, 397};

  constexpr struct {
    uint32_t output_channels;
    uint32_t input_channels;
    uint32_t width;
    uint32_t height;

    std::array<int64_t, 4u> size() const {
      return {
        output_channels,
        input_channels,
        width,
        height,
      };
    }
  } weights {29, input.channels, 1, 1};

  const auto input_cpu = at::randn(input.size(), at::device(at::kCPU).dtype(at::kFloat));
  const auto weights_cpu = at::randn(weights.size(), at::device(at::kCPU).dtype(at::kFloat));
  const auto bias_cpu = at::randn({weights.output_channels}, at::device(at::kCPU).dtype(at::kFloat));

  const auto output_cpu = at::conv2d(
      input_cpu,
      weights_cpu,
      bias_cpu,
      stride,
      padding,
      dilation,
      groups);

  const auto output_vulkan = at::conv2d(
      input_cpu.vulkan(),
      weights_cpu,
      bias_cpu,
      stride,
      padding,
      dilation,
      groups);

  const bool check = almostEqual(output_cpu, output_vulkan.cpu());
  if (!check) {
    showRtol(output_cpu, output_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, conv2d_winograd) {
  if (!at::is_vulkan_available()) {
    return;
  }

  constexpr int64_t groups = 1;
  constexpr std::array<int64_t, 2u> stride{1, 1};
  constexpr std::array<int64_t, 2u> padding{2, 2};
  constexpr std::array<int64_t, 2u> dilation{1, 1};

  constexpr struct {
    uint32_t batches;
    uint32_t channels;
    uint32_t width;
    uint32_t height;

    std::array<int64_t, 4u> size() const {
      return {
        batches,
        channels,
        width,
        height,
      };
    }
  } input {1, 10, 177, 232};

  constexpr struct {
    uint32_t output_channels;
    uint32_t input_channels;
    uint32_t width;
    uint32_t height;

    std::array<int64_t, 4u> size() const {
      return {
        output_channels,
        input_channels,
        width,
        height,
      };
    }
  } weights {13, input.channels, 3, 3};

  const auto input_cpu = at::rand(input.size(), at::device(at::kCPU).dtype(at::kFloat));
  const auto weights_cpu = at::rand(weights.size(), at::device(at::kCPU).dtype(at::kFloat));
  const auto bias_cpu = at::rand({weights.output_channels}, at::device(at::kCPU).dtype(at::kFloat));

  const auto output_cpu = at::conv2d(
      input_cpu,
      weights_cpu,
      bias_cpu,
      stride,
      padding,
      dilation,
      groups);

  const auto output_vulkan = at::conv2d(
      input_cpu.vulkan(),
      weights_cpu,
      bias_cpu,
      stride,
      padding,
      dilation,
      groups).cpu();

  const bool check = almostEqual(output_cpu, output_vulkan);
  if (!check) {
    showRtol(output_cpu, output_vulkan);
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, copy) {
  if (!at::is_vulkan_available()) {
    return;
  }

  const auto cpu = at::rand({13, 17, 37, 19}, at::device(at::kCPU).dtype(at::kFloat));
  const auto vulkan = cpu.vulkan();

  const auto check = exactlyEqual(cpu, vulkan.cpu());
  if (!check) {
    showRtol(cpu, vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, div) {
  if (!at::is_vulkan_available()) {
    return;
  }

  const auto a_cpu = at::rand({11, 7, 139, 109}, at::device(at::kCPU).dtype(at::kFloat))+0.01;
  const auto a_vulkan = a_cpu.vulkan();

  const auto b_cpu = at::rand({11, 7, 139, 109}, at::device(at::kCPU).dtype(at::kFloat))+0.01;
  const auto b_vulkan = b_cpu.vulkan();

  const auto c_cpu = at::div(a_cpu, b_cpu);
  const auto c_vulkan = at::div(a_vulkan, b_vulkan);

  const auto check = almostEqual(c_cpu, c_vulkan.cpu());
  if (!check) {
    showRtol(c_cpu, c_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, div_broadcast0) {
  if (!at::is_vulkan_available()) {
    return;
  }

  const auto a_cpu = at::rand({3, 5, 1, 1}, at::device(at::kCPU).dtype(at::kFloat))+0.01;
  const auto a_vulkan = a_cpu.vulkan();

  const auto b_cpu = at::rand({3, 5, 179, 221}, at::device(at::kCPU).dtype(at::kFloat))+0.01;
  const auto b_vulkan = b_cpu.vulkan();

  const auto c_cpu = at::div(a_cpu, b_cpu);
  const auto c_vulkan = at::div(a_vulkan, b_vulkan);

  const auto check = almostEqual(c_cpu, c_vulkan.cpu());
  if (!check) {
    showRtol(c_cpu, c_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, div_broadcast1) {
  if (!at::is_vulkan_available()) {
    return;
  }

  const auto a_cpu = at::rand({3, 5, 179, 221}, at::device(at::kCPU).dtype(at::kFloat))+0.01;
  const auto a_vulkan = a_cpu.vulkan();

  const auto b_cpu = at::rand({3, 5, 1, 221}, at::device(at::kCPU).dtype(at::kFloat))+0.01;
  const auto b_vulkan = b_cpu.vulkan();

  const auto c_cpu = at::div(a_cpu, b_cpu);
  const auto c_vulkan = at::div(a_vulkan, b_vulkan);

  const auto check = almostEqual(c_cpu, c_vulkan.cpu());
  if (!check) {
    showRtol(c_cpu, c_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, div_broadcast2) {
  if (!at::is_vulkan_available()) {
    return;
  }

  const auto a_cpu = at::rand({3, 4, 179, 221}, at::device(at::kCPU).dtype(at::kFloat))+0.01;
  const auto a_vulkan = a_cpu.vulkan();

  const auto b_cpu = at::rand({4, 1, 1}, at::device(at::kCPU).dtype(at::kFloat))+0.01;
  const auto b_vulkan = b_cpu.vulkan();

  const auto c_cpu = at::div(a_cpu, b_cpu);
  const auto c_vulkan = at::div(a_vulkan, b_vulkan);

  const auto check = almostEqual(c_cpu, c_vulkan.cpu());
  if (!check) {
    showRtol(c_cpu, c_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, div_) {
  if (!at::is_vulkan_available()) {
    return;
  }

  auto a_cpu = at::rand({61, 17, 29, 83}, at::device(at::kCPU).dtype(at::kFloat))+0.01;
  auto a_vulkan = a_cpu.vulkan();

  const auto b_cpu = at::rand({61, 17, 29, 83}, at::device(at::kCPU).dtype(at::kFloat))+0.01;
  const auto b_vulkan = b_cpu.vulkan();

  a_cpu.div_(b_cpu);
  a_vulkan.div_(b_vulkan);

  const auto check = almostEqual(a_cpu, a_vulkan.cpu());
  if (!check) {
    showRtol(b_cpu, b_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, div_broadcast0_) {
  if (!at::is_vulkan_available()) {
    return;
  }

  auto a_cpu = at::rand({12, 17, 29, 83}, at::device(at::kCPU).dtype(at::kFloat))+0.01;
  auto a_vulkan = a_cpu.vulkan();

  const auto b_cpu = at::rand({12, 17, 29, 1}, at::device(at::kCPU).dtype(at::kFloat))+0.01;
  const auto b_vulkan = b_cpu.vulkan();

  a_cpu.div_(b_cpu);
  a_vulkan.div_(b_vulkan);

  const auto check = almostEqual(a_cpu, a_vulkan.cpu());
  if (!check) {
    showRtol(b_cpu, b_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, div_broadcast1_) {
  if (!at::is_vulkan_available()) {
    return;
  }

  auto a_cpu = at::rand({3, 8, 29, 83}, at::device(at::kCPU).dtype(at::kFloat))+0.01;
  auto a_vulkan = a_cpu.vulkan();

  const auto b_cpu = at::rand({8, 1, 1}, at::device(at::kCPU).dtype(at::kFloat))+0.01;
  const auto b_vulkan = b_cpu.vulkan();

  a_cpu.div_(b_cpu);
  a_vulkan.div_(b_vulkan);

  const auto check = almostEqual(a_cpu, a_vulkan.cpu());
  if (!check) {
    showRtol(b_cpu, b_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, div_scalar) {
  if (!at::is_vulkan_available()) {
    return;
  }

  const auto a_cpu = at::rand({17, 213, 213, 7}, at::device(at::kCPU).dtype(at::kFloat));
  const auto a_vulkan = a_cpu.vulkan();

  const float b_scalar = 3.1415f;

  const auto c_cpu = at::div(a_cpu, b_scalar);
  const auto c_vulkan = at::div(a_vulkan, b_scalar);

  const auto check = almostEqual(c_cpu, c_vulkan.cpu());
  if (!check) {
    showRtol(c_cpu, c_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, div_scalar_) {
  if (!at::is_vulkan_available()) {
    return;
  }

  auto a_cpu = at::rand({11, 7, 139, 109}, at::device(at::kCPU).dtype(at::kFloat));
  auto a_vulkan = a_cpu.vulkan();

  const float b_scalar = 3.1415f;

  a_cpu.div_(b_scalar);
  a_vulkan.div_(b_scalar);

  const auto check = almostEqual(a_cpu, a_vulkan.cpu());
  if (!check) {
    showRtol(a_cpu, a_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, empty) {
  if (!at::is_vulkan_available()) {
    return;
  }

  ASSERT_NO_THROW(at::empty({1, 17, 41, 53}, at::device(at::kVulkan).dtype(at::kFloat)));
}

TEST(VulkanAPITest, hardsigmoid) {
  if (!at::is_vulkan_available()) {
    return;
  }

  const auto in_cpu = at::rand({17, 197, 302, 5}, at::device(at::kCPU).dtype(at::kFloat))*12 - 6;
  const auto in_vulkan = in_cpu.vulkan();

  const auto out_cpu = at::hardsigmoid(in_cpu);
  const auto out_vulkan = at::hardsigmoid(in_vulkan);

  const auto check = almostEqual(out_cpu, out_vulkan.cpu());
  if (!check) {
    showRtol(out_cpu, out_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, hardsigmoid_) {
  if (!at::is_vulkan_available()) {
    return;
  }

  auto cpu = at::rand({17, 197, 302, 5}, at::device(at::kCPU).dtype(at::kFloat))*12 - 6;
  auto vulkan = cpu.vulkan();

  at::hardsigmoid_(cpu);
  at::hardsigmoid_(vulkan);

  const auto check = almostEqual(cpu, vulkan.cpu());
  if (!check) {
    showRtol(cpu, vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, hardshrink) {
  if (!at::is_vulkan_available()) {
    return;
  }

  for (const auto lambd_value : {-4.2, -1.0, -0.42, 0.0, 0.42, 1.0, 4.2, 42.42}) {
    const auto in_cpu = (at::rand({17, 197, 302, 5}, at::device(at::kCPU).dtype(at::kFloat)) - 0.5) * 20;  // between -10 and +10
    const auto in_vulkan = in_cpu.vulkan();

    const auto out_cpu = at::hardshrink(in_cpu, lambd_value);
    const auto out_vulkan = at::hardshrink(in_vulkan, lambd_value);

    const auto check = almostEqual(out_cpu, out_vulkan.cpu());

    if (!check) {
      showRtol(out_cpu, out_vulkan.cpu());
    }

    ASSERT_TRUE(check);
  }
}

TEST(VulkanAPITest, hardshrink_) {
  if (!at::is_vulkan_available()) {
    return;
  }

  for (const auto lambd_value : {-4.2, -1.0, -0.42, 0.0, 0.42, 1.0, 4.2, 42.42}) {
    const auto cpu = (at::rand({17, 197, 302, 5}, at::device(at::kCPU).dtype(at::kFloat)) - 0.5) * 20;  // between -10 and +10
    const auto vulkan = cpu.vulkan();

    cpu.hardshrink(lambd_value);
    vulkan.hardshrink(lambd_value);

    const auto check = almostEqual(cpu, vulkan.cpu());
    if (!check) {
      showRtol(cpu, vulkan.cpu());
    }

    ASSERT_TRUE(check);
  }
}

TEST(VulkanAPITest, leaky_relu) {
  if (!at::is_vulkan_available()) {
    return;
  }

  for (const auto negative_slope : {0.01, 0.001, 1.0, -0.001}) {
    const auto in_cpu = at::rand({17, 197, 302, 5}, at::device(at::kCPU).dtype(at::kFloat));
    const auto in_vulkan = in_cpu.vulkan();

    const auto out_cpu = at::leaky_relu(in_cpu, negative_slope);
    const auto out_vulkan = at::leaky_relu(in_vulkan, negative_slope);

    const auto check = almostEqual(out_cpu, out_vulkan.cpu());

    if (!check) {
      showRtol(out_cpu, out_vulkan.cpu());
    }

    ASSERT_TRUE(check);
  }
}

TEST(VulkanAPITest, leaky_relu_) {
  if (!at::is_vulkan_available()) {
    return;
  }

  for (const auto negative_slope : {0.01, 0.001, 1.0, -0.001}) {
    auto cpu = at::rand({17, 197, 302, 5}, at::device(at::kCPU).dtype(at::kFloat));
    auto vulkan = cpu.vulkan();

    at::leaky_relu_(cpu, negative_slope);
    at::leaky_relu_(vulkan, negative_slope);

    const auto check = almostEqual(cpu, vulkan.cpu());
    if (!check) {
      showRtol(cpu, vulkan.cpu());
    }

    ASSERT_TRUE(check);
  }
}

TEST(VulkanAPITest, hardswish) {
  if (!at::is_vulkan_available()) {
    return;
  }

  const auto in_cpu = at::rand({17, 197, 302, 5}, at::device(at::kCPU).dtype(at::kFloat))*12 - 6;
  const auto in_vulkan = in_cpu.vulkan();

  const auto out_cpu = at::hardswish(in_cpu);
  const auto out_vulkan = at::hardswish(in_vulkan);

  const auto check = almostEqual(out_cpu, out_vulkan.cpu());
  if (!check) {
    showRtol(out_cpu, out_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, hardswish_) {
  if (!at::is_vulkan_available()) {
    return;
  }

  auto cpu = at::rand({17, 197, 302, 5}, at::device(at::kCPU).dtype(at::kFloat))*12 - 6;
  auto vulkan = cpu.vulkan();

  at::native::hardswish_(cpu);
  at::hardswish_(vulkan);

  const auto check = almostEqual(cpu, vulkan.cpu());
  if (!check) {
    showRtol(cpu, vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, max_pool2d) {
  if (!at::is_vulkan_available()) {
    return;
  }
  c10::InferenceMode mode;

  const auto in_cpu = at::rand({5, 13, 55, 68}, at::TensorOptions(at::kCPU).dtype(at::kFloat));
  const auto out_cpu = at::max_pool2d(in_cpu, {3, 4}, {2, 1}, {1, 1}, {1, 1}, false);
  const auto out_vulkan = at::max_pool2d(in_cpu.vulkan(), {3, 4}, {2, 1}, {1, 1}, {1,1}, false);

  const auto check = almostEqual(out_cpu, out_vulkan.cpu());
  if (!check) {
    showRtol(out_cpu, out_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, mean) {
  const auto in_cpu = at::rand({17, 3, 79, 53}, at::TensorOptions(at::kCPU).dtype(at::kFloat));
  const auto out_cpu = at::mean(in_cpu, {-1, -2}, true);

  const auto in_vulkan = in_cpu.vulkan();
  const auto out_vulkan = at::mean(in_vulkan, {-1, -2}, true);

  const auto check = almostEqual(out_cpu, out_vulkan.cpu());
  if (!check) {
    showRtol(out_cpu, out_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, mean2d) {
  const auto in_cpu = at::rand({11, 7, 173, 37}, at::TensorOptions(at::kCPU).dtype(at::kFloat));
  const auto out_cpu = at::mean(in_cpu, {-1, -2}, false);

  const auto in_vulkan = in_cpu.vulkan();
  const auto out_vulkan = at::mean(in_vulkan, {-1, -2}, false);

  const auto check = almostEqual(out_cpu, out_vulkan.cpu());
  if (!check) {
    showRtol(out_cpu, out_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, mm) {
  if (!at::is_vulkan_available()) {
    return;
  }

  const auto m1_cpu = at::rand({179, 67}, at::device(at::kCPU).dtype(at::kFloat));
  const auto m2_cpu = at::rand({67, 163}, at::device(at::kCPU).dtype(at::kFloat));
  const auto out_cpu = m1_cpu.mm(m2_cpu);

  const auto m1_vulkan = m1_cpu.vulkan();
  const auto out_vulkan = m1_vulkan.mm(m2_cpu);

  const auto check = almostEqual(out_cpu, out_vulkan.cpu());
  if (!check) {
    showRtol(out_cpu, out_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, mul) {
  if (!at::is_vulkan_available()) {
    return;
  }

  const auto a_cpu = at::rand({11, 7, 139, 109}, at::device(at::kCPU).dtype(at::kFloat));
  const auto a_vulkan = a_cpu.vulkan();

  const auto b_cpu = at::rand({11, 7, 139, 109}, at::device(at::kCPU).dtype(at::kFloat));
  const auto b_vulkan = b_cpu.vulkan();

  const auto c_cpu = at::mul(a_cpu, b_cpu);
  const auto c_vulkan = at::mul(a_vulkan, b_vulkan);

  const auto check = almostEqual(c_cpu, c_vulkan.cpu());
  if (!check) {
    showRtol(c_cpu, c_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, mul_broadcast0) {
  if (!at::is_vulkan_available()) {
    return;
  }

  const auto a_cpu = at::rand({3, 5, 1, 1}, at::device(at::kCPU).dtype(at::kFloat));
  const auto a_vulkan = a_cpu.vulkan();

  const auto b_cpu = at::rand({3, 5, 179, 221}, at::device(at::kCPU).dtype(at::kFloat));
  const auto b_vulkan = b_cpu.vulkan();

  const auto c_cpu = at::mul(a_cpu, b_cpu);
  const auto c_vulkan = at::mul(a_vulkan, b_vulkan);

  const auto check = almostEqual(c_cpu, c_vulkan.cpu());
  if (!check) {
    showRtol(c_cpu, c_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, mul_broadcast1) {
  if (!at::is_vulkan_available()) {
    return;
  }

  const auto a_cpu = at::rand({3, 5, 179, 221}, at::device(at::kCPU).dtype(at::kFloat));
  const auto a_vulkan = a_cpu.vulkan();

  const auto b_cpu = at::rand({3, 5, 1, 221}, at::device(at::kCPU).dtype(at::kFloat));
  const auto b_vulkan = b_cpu.vulkan();

  const auto c_cpu = at::mul(a_cpu, b_cpu);
  const auto c_vulkan = at::mul(a_vulkan, b_vulkan);

  const auto check = almostEqual(c_cpu, c_vulkan.cpu());
  if (!check) {
    showRtol(c_cpu, c_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, mul_broadcast2) {
  if (!at::is_vulkan_available()) {
    return;
  }

  const auto a_cpu = at::rand({3, 4, 179, 221}, at::device(at::kCPU).dtype(at::kFloat));
  const auto a_vulkan = a_cpu.vulkan();

  const auto b_cpu = at::rand({4, 1, 1}, at::device(at::kCPU).dtype(at::kFloat));
  const auto b_vulkan = b_cpu.vulkan();

  const auto c_cpu = at::mul(a_cpu, b_cpu);
  const auto c_vulkan = at::mul(a_vulkan, b_vulkan);

  const auto check = almostEqual(c_cpu, c_vulkan.cpu());
  if (!check) {
    showRtol(c_cpu, c_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, mul_) {
  if (!at::is_vulkan_available()) {
    return;
  }

  auto a_cpu = at::rand({61, 17, 29, 83}, at::device(at::kCPU).dtype(at::kFloat));
  auto a_vulkan = a_cpu.vulkan();

  const auto b_cpu = at::rand({61, 17, 29, 83}, at::device(at::kCPU).dtype(at::kFloat));
  const auto b_vulkan = b_cpu.vulkan();

  a_cpu.mul_(b_cpu);
  a_vulkan.mul_(b_vulkan);

  const auto check = almostEqual(a_cpu, a_vulkan.cpu());
  if (!check) {
    showRtol(b_cpu, b_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, mul_broadcast0_) {
  if (!at::is_vulkan_available()) {
    return;
  }

  auto a_cpu = at::rand({12, 17, 29, 83}, at::device(at::kCPU).dtype(at::kFloat));
  auto a_vulkan = a_cpu.vulkan();

  const auto b_cpu = at::rand({12, 17, 29, 1}, at::device(at::kCPU).dtype(at::kFloat));
  const auto b_vulkan = b_cpu.vulkan();

  a_cpu.mul_(b_cpu);
  a_vulkan.mul_(b_vulkan);

  const auto check = almostEqual(a_cpu, a_vulkan.cpu());
  if (!check) {
    showRtol(b_cpu, b_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, mul_broadcast1_) {
  if (!at::is_vulkan_available()) {
    return;
  }

  auto a_cpu = at::rand({3, 8, 29, 83}, at::device(at::kCPU).dtype(at::kFloat));
  auto a_vulkan = a_cpu.vulkan();

  const auto b_cpu = at::rand({8, 1, 1}, at::device(at::kCPU).dtype(at::kFloat));
  const auto b_vulkan = b_cpu.vulkan();

  a_cpu.mul_(b_cpu);
  a_vulkan.mul_(b_vulkan);

  const auto check = almostEqual(a_cpu, a_vulkan.cpu());
  if (!check) {
    showRtol(b_cpu, b_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, mul_scalar) {
  if (!at::is_vulkan_available()) {
    return;
  }

  const auto a_cpu = at::rand({17, 213, 213, 7}, at::device(at::kCPU).dtype(at::kFloat));
  const auto a_vulkan = a_cpu.vulkan();

  const float b_scalar = 3.1415f;

  const auto c_cpu = at::mul(a_cpu, b_scalar);
  const auto c_vulkan = at::mul(a_vulkan, b_scalar);

  const auto check = almostEqual(c_cpu, c_vulkan.cpu());
  if (!check) {
    showRtol(c_cpu, c_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, mul_scalar_) {
  if (!at::is_vulkan_available()) {
    return;
  }

  auto a_cpu = at::rand({11, 7, 139, 109}, at::device(at::kCPU).dtype(at::kFloat));
  auto a_vulkan = a_cpu.vulkan();

  const float b_scalar = 3.1415f;

  a_cpu.mul_(b_scalar);
  a_vulkan.mul_(b_scalar);

  const auto check = almostEqual(a_cpu, a_vulkan.cpu());
  if (!check) {
    showRtol(a_cpu, a_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, reflection_pad2d) {
  if (!at::is_vulkan_available()) {
    return;
  }

  const auto a_cpu = at::rand({2, 3, 47, 63}, at::device(at::kCPU).dtype(at::kFloat));
  const auto a_vulkan = a_cpu.vulkan();

  const auto out_cpu = at::reflection_pad2d(a_cpu, {9,8,5,12});
  const auto out_vulkan = at::reflection_pad2d(a_vulkan, {9,8,5,12}).cpu();

  const auto check = almostEqual(out_cpu, out_vulkan);
  if (!check) {
    showRtol(out_cpu, out_vulkan);
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, reshape) {
  if (!at::is_vulkan_available()) {
    return;
  }
  c10::InferenceMode mode;

  const auto in_cpu = at::rand({47, 11, 83, 97}, at::device(at::kCPU).dtype(at::kFloat));
  const auto in_vulkan = in_cpu.vulkan();

  const std::array<int64_t, 2> shape{47 * 83, 11 * 97};

  const auto out_cpu = at::reshape(in_cpu, shape);
  const auto out_vulkan = at::reshape(in_vulkan, shape);

  const auto check = almostEqual(out_cpu, out_vulkan.cpu());
  if (!check) {
    showRtol(out_cpu, out_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, reshape_) {
  if (!at::is_vulkan_available()) {
    return;
  }
  c10::InferenceMode mode;

  const auto cpu = at::rand({59, 41, 19, 67}, at::device(at::kCPU).dtype(at::kFloat));
  const auto vulkan = cpu.vulkan();

  const std::array<int64_t, 3> shape{59, 41 * 67, 19};

  cpu.reshape(shape);
  vulkan.reshape(shape);

  const auto check = almostEqual(cpu, vulkan.cpu());
  if (!check) {
    showRtol(cpu, vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, sigmoid) {
  if (!at::is_vulkan_available()) {
    return;
  }

  const auto in_cpu = at::rand({17, 197, 302, 5}, at::device(at::kCPU).dtype(at::kFloat));
  const auto in_vulkan = in_cpu.vulkan();

  const auto out_cpu = at::sigmoid(in_cpu);
  const auto out_vulkan = at::sigmoid(in_vulkan);

  const auto check = almostEqual(out_cpu, out_vulkan.cpu());
  if (!check) {
    showRtol(out_cpu, out_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, sigmoid_) {
  if (!at::is_vulkan_available()) {
    return;
  }

  auto cpu = at::rand({17, 197, 302, 5}, at::device(at::kCPU).dtype(at::kFloat));
  auto vulkan = cpu.vulkan();

  at::sigmoid_(cpu);
  at::sigmoid_(vulkan);

  const auto check = almostEqual(cpu, vulkan.cpu());
  if (!check) {
    showRtol(cpu, vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, softmax) {
  at::Tensor test_in[] = {
    at::rand({1, 196, 302, 5}, at::TensorOptions(at::kCPU).dtype(at::kFloat)),
    at::rand({1, 197, 302, 5}, at::TensorOptions(at::kCPU).dtype(at::kFloat)),
    at::rand({1, 198, 302, 5}, at::TensorOptions(at::kCPU).dtype(at::kFloat)),
    at::rand({1, 199, 302, 5}, at::TensorOptions(at::kCPU).dtype(at::kFloat)),
  };

  for (auto in_cpu : test_in) {
    const auto out_cpu = at::softmax(in_cpu, 1);

    const auto in_vulkan = in_cpu.vulkan();
    const auto out_vulkan = at::softmax(in_vulkan, 1);

    const auto check = almostEqual(out_cpu, out_vulkan.cpu());
    if (!check) {
      showRtol(out_cpu, out_vulkan.cpu());
    }

    ASSERT_TRUE(check);
  }
}

TEST(VulkanAPITest, log_softmax) {
  at::Tensor test_in[] = {
    at::rand({1, 196, 302, 5}, at::TensorOptions(at::kCPU).dtype(at::kFloat)),
    at::rand({1, 197, 302, 5}, at::TensorOptions(at::kCPU).dtype(at::kFloat)),
    at::rand({1, 198, 302, 5}, at::TensorOptions(at::kCPU).dtype(at::kFloat)),
    at::rand({1, 199, 302, 5}, at::TensorOptions(at::kCPU).dtype(at::kFloat)),
  };

  for (auto in_cpu : test_in) {
    const auto out_cpu = at::softmax(in_cpu, 1);

    const auto in_vulkan = in_cpu.vulkan();
    const auto out_vulkan = at::log_softmax(in_vulkan, 1);

    const auto check = almostEqual(out_cpu, out_vulkan.cpu());
    if (!check) {
      showRtol(out_cpu, out_vulkan.cpu());
    }

    ASSERT_TRUE(check);
  }
}

TEST(VulkanAPITest, tanh) {
  if (!at::is_vulkan_available()) {
    return;
  }

  const auto in_cpu = at::rand({17, 197, 302, 5}, at::device(at::kCPU).dtype(at::kFloat));
  const auto in_vulkan = in_cpu.vulkan();

  const auto out_cpu = at::tanh(in_cpu);
  const auto out_vulkan = at::tanh(in_vulkan);

  const auto check = almostEqual(out_cpu, out_vulkan.cpu());
  if (!check) {
    showRtol(out_cpu, out_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, tanh_) {
  if (!at::is_vulkan_available()) {
    return;
  }

  auto cpu = at::rand({17, 197, 302, 5}, at::device(at::kCPU).dtype(at::kFloat));
  auto vulkan = cpu.vulkan();

  at::tanh_(cpu);
  at::tanh_(vulkan);

  const auto check = almostEqual(cpu, vulkan.cpu());
  if (!check) {
    showRtol(cpu, vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, sub) {
  if (!at::is_vulkan_available()) {
    return;
  }

  const auto a_cpu = at::rand({11, 7, 139, 109}, at::device(at::kCPU).dtype(at::kFloat));
  const auto a_vulkan = a_cpu.vulkan();

  const auto b_cpu = at::rand({11, 7, 139, 109}, at::device(at::kCPU).dtype(at::kFloat));
  const auto b_vulkan = b_cpu.vulkan();

  const auto c_cpu = at::sub(a_cpu, b_cpu, 2.1f);
  const auto c_vulkan = at::sub(a_vulkan, b_vulkan, 2.1f);

  const auto check = almostEqual(c_cpu, c_vulkan.cpu());
  if (!check) {
    showRtol(c_cpu, c_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, sub_broadcast0) {
  if (!at::is_vulkan_available()) {
    return;
  }

  const auto a_cpu = at::rand({3, 5, 179, 221}, at::device(at::kCPU).dtype(at::kFloat));
  const auto a_vulkan = a_cpu.vulkan();

  const auto b_cpu = at::rand({3, 5, 1, 1}, at::device(at::kCPU).dtype(at::kFloat));
  const auto b_vulkan = b_cpu.vulkan();

  const auto c_cpu = at::sub(a_cpu, b_cpu, 1.8f);
  const auto c_vulkan = at::sub(a_vulkan, b_vulkan, 1.8f);

  const auto check = almostEqual(c_cpu, c_vulkan.cpu());
  if (!check) {
    showRtol(c_cpu, c_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, sub_broadcast1) {
  if (!at::is_vulkan_available()) {
    return;
  }

  const auto a_cpu = at::rand({3, 5, 179, 221}, at::device(at::kCPU).dtype(at::kFloat));
  const auto a_vulkan = a_cpu.vulkan();

  const auto b_cpu = at::rand({3, 5, 1, 221}, at::device(at::kCPU).dtype(at::kFloat));
  const auto b_vulkan = b_cpu.vulkan();

  const auto c_cpu = at::sub(a_cpu, b_cpu, 1.8f);
  const auto c_vulkan = at::sub(a_vulkan, b_vulkan, 1.8f);

  const auto check = almostEqual(c_cpu, c_vulkan.cpu());
  if (!check) {
    showRtol(c_cpu, c_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, sub_broadcast2) {
  if (!at::is_vulkan_available()) {
    return;
  }

  const auto a_cpu = at::rand({3, 4, 179, 221}, at::device(at::kCPU).dtype(at::kFloat));
  const auto a_vulkan = a_cpu.vulkan();

  const auto b_cpu = at::rand({4, 1, 1}, at::device(at::kCPU).dtype(at::kFloat));
  const auto b_vulkan = b_cpu.vulkan();

  const auto c_cpu = at::sub(a_cpu, b_cpu, 2.5f);
  const auto c_vulkan = at::sub(a_vulkan, b_vulkan, 2.5f);

  const auto check = almostEqual(c_cpu, c_vulkan.cpu());
  if (!check) {
    showRtol(c_cpu, c_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, sub_) {
  if (!at::is_vulkan_available()) {
    return;
  }

  auto a_cpu = at::rand({61, 17, 29, 83}, at::device(at::kCPU).dtype(at::kFloat));
  auto a_vulkan = a_cpu.vulkan();

  const auto b_cpu = at::rand({61, 17, 29, 83}, at::device(at::kCPU).dtype(at::kFloat));
  const auto b_vulkan = b_cpu.vulkan();

  a_cpu.sub_(b_cpu, 2.1f);
  a_vulkan.sub_(b_vulkan, 2.1f);

  const auto check = almostEqual(a_cpu, a_vulkan.cpu());
  if (!check) {
    showRtol(b_cpu, b_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, sub_broadcast0_) {
  if (!at::is_vulkan_available()) {
    return;
  }

  auto a_cpu = at::rand({16, 17, 29, 83}, at::device(at::kCPU).dtype(at::kFloat));
  auto a_vulkan = a_cpu.vulkan();

  const auto b_cpu = at::rand({16, 17, 29, 1}, at::device(at::kCPU).dtype(at::kFloat));
  const auto b_vulkan = b_cpu.vulkan();

  a_cpu.sub_(b_cpu, 2.1f);
  a_vulkan.sub_(b_vulkan, 2.1f);

  const auto check = almostEqual(a_cpu, a_vulkan.cpu());
  if (!check) {
    showRtol(b_cpu, b_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, sub_broadcast1_) {
  if (!at::is_vulkan_available()) {
    return;
  }

  auto a_cpu = at::rand({3, 8, 29, 83}, at::device(at::kCPU).dtype(at::kFloat));
  auto a_vulkan = a_cpu.vulkan();

  const auto b_cpu = at::rand({3, 8, 1, 1}, at::device(at::kCPU).dtype(at::kFloat));
  const auto b_vulkan = b_cpu.vulkan();

  a_cpu.sub_(b_cpu, 2.1f);
  a_vulkan.sub_(b_vulkan, 2.1f);

  const auto check = almostEqual(a_cpu, a_vulkan.cpu());
  if (!check) {
    showRtol(b_cpu, b_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, transposed_conv2d) {
  // Guard
  if (!at::is_vulkan_available()) {
    return;
  }

  // Arrange
  constexpr int64_t groups = 1;
  constexpr std::array<int64_t, 2u> stride{1, 2};
  constexpr std::array<int64_t, 2u> padding{1, 0};
  constexpr std::array<int64_t, 2u> output_padding{0, 1};
  //TODO: Support conv_transpose2d with dilation != 1
  constexpr std::array<int64_t, 2u> dilation{1, 1};

  constexpr struct {
    uint32_t batches;
    uint32_t channels;
    uint32_t height;
    uint32_t width;

    std::array<int64_t, 4u> size() const {
      return {
        batches,
        channels,
        height,
        width,
      };
    }
  } input {1, 55, 7, 19};

  constexpr struct {
    uint32_t input_channels;
    uint32_t output_channels;
    uint32_t height;
    uint32_t width;

    std::array<int64_t, 4u> size() const {
      return {
        input_channels,
        output_channels,
        height,
        width,
      };
    }
  } weights {input.channels, 47, 2, 3};

  const auto input_cpu = at::randn(input.size(), at::device(at::kCPU).dtype(at::kFloat));
  const auto weights_cpu = at::randn(weights.size(), at::device(at::kCPU).dtype(at::kFloat));
  const auto bias_cpu = at::zeros({weights.output_channels}, at::device(at::kCPU).dtype(at::kFloat));

  // Act
  const auto output_cpu = at::conv_transpose2d(
      input_cpu,
      weights_cpu,
      bias_cpu,
      stride,
      padding,
      output_padding,
      groups,
      dilation);

  const auto output_vk = at::conv_transpose2d(
      input_cpu.vulkan(),
      weights_cpu,
      bias_cpu,
      stride,
      padding,
      output_padding,
      groups,
      dilation).cpu();

  // Assert
  const bool check = almostEqual(output_cpu, output_vk);
  if (!check) {
    showRtol(output_cpu, output_vk);
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, upsample_nearest2d) {
  if (!at::is_vulkan_available()) {
    return;
  }

  const auto in_cpu = at::rand({1, 2, 2, 3}, at::TensorOptions(at::kCPU).dtype(at::kFloat));
  const auto out_cpu = at::upsample_nearest2d(in_cpu, {4, 6});

  const auto in_vulkan = in_cpu.vulkan();
  const auto out_vulkan = at::upsample_nearest2d(in_vulkan, {4, 6});

  const auto check = almostEqual(out_cpu, out_vulkan.cpu());
  if (!check) {
    showRtol(out_cpu, out_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

#if !defined(__APPLE__)
TEST(VulkanAPITest, cat_dim1_samefeature_success) {
  // Guard
  if (!at::is_vulkan_available()) {
    return;
  }

  // Arrange
  const auto in_cpu1 = at::rand({3, 9, 221, 193}, at::device(at::kCPU).dtype(at::kFloat));
  const auto in_cpu2 = at::rand({3, 9, 221, 193}, at::device(at::kCPU).dtype(at::kFloat));
  const auto in_cpu3 = at::rand({3, 9, 221, 193}, at::device(at::kCPU).dtype(at::kFloat));

  // Act
  const auto out_cpu = at::cat({in_cpu1, in_cpu2, in_cpu3}, 1);
  const auto out_vulkan = at::cat({in_cpu1.vulkan(), in_cpu2.vulkan(), in_cpu3.vulkan()}, 1); // dim=feature(channel)

  // Assert
  const auto check = almostEqual(out_cpu, out_vulkan.cpu());
  if (!check) {
    showRtol(out_cpu, out_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, cat_dim1_difffeature_success) {
  // Guard
  if (!at::is_vulkan_available()) {
    return;
  }

  // Arrange
  const auto in_cpu1 = at::rand({3, 3, 221, 193}, at::device(at::kCPU).dtype(at::kFloat));
  const auto in_cpu2 = at::rand({3, 8, 221, 193}, at::device(at::kCPU).dtype(at::kFloat));
  const auto in_cpu3 = at::rand({3, 11, 221, 193}, at::device(at::kCPU).dtype(at::kFloat));

  // Act
  const auto out_cpu = at::cat({in_cpu1, in_cpu2, in_cpu3}, 1);
  const auto out_vulkan = at::cat({in_cpu1.vulkan(), in_cpu2.vulkan(), in_cpu3.vulkan()}, 1); // dim=feature(channel)

  // Assert
  const auto check = almostEqual(out_cpu, out_vulkan.cpu());
  if (!check) {
    showRtol(out_cpu, out_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, cat_dim1_texture2d_success) {
  // Guard
  if (!at::is_vulkan_available()) {
    return;
  }

  // Arrange: 2D Texture (VK_IMAGE_VIEW_TYPE_2D)
  const auto in_cpu1 = at::rand({2, 3, 2, 2}, at::device(at::kCPU).dtype(at::kFloat));
  const auto in_cpu2 = at::rand({2, 3, 2, 2}, at::device(at::kCPU).dtype(at::kFloat));
  const auto in_cpu3 = at::rand({2, 3, 2, 2}, at::device(at::kCPU).dtype(at::kFloat));

  // Act
  const auto out_cpu = at::cat({in_cpu1, in_cpu2, in_cpu3}, 1);
  const auto out_vulkan = at::cat({in_cpu1.vulkan(), in_cpu2.vulkan(), in_cpu3.vulkan()}, 1); // dim=feature(channel)

  // Assert
  const auto check = almostEqual(out_cpu, out_vulkan.cpu());
  if (!check) {
    showRtol(out_cpu, out_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}
#endif /* !defined(__APPLE__) */

TEST(VulkanAPITest, cat_dim1_singledepth_success) {
  // Guard
  if (!at::is_vulkan_available()) {
    return;
  }

  // Arrange: batch x channel (1x1) = single depth texture
  const auto in_cpu1 = at::rand({1, 1, 221, 193}, at::device(at::kCPU).dtype(at::kFloat));
  const auto in_cpu2 = at::rand({1, 1, 221, 193}, at::device(at::kCPU).dtype(at::kFloat));
  const auto in_cpu3 = at::rand({1, 1, 221, 193}, at::device(at::kCPU).dtype(at::kFloat));

  // Act
  const auto out_cpu = at::cat({in_cpu1, in_cpu2, in_cpu3}, 1);
  const auto out_vulkan = at::cat({in_cpu1.vulkan(), in_cpu2.vulkan(), in_cpu3.vulkan()}, 1); // dim=feature(channel)

  // Assert
  const auto check = almostEqual(out_cpu, out_vulkan.cpu());
  if (!check) {
    showRtol(out_cpu, out_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, cat_dim1_singletensor_success) {
  // Guard
  if (!at::is_vulkan_available()) {
    return;
  }

  // Arrange: single input tensor
  const auto in_cpu1 = at::rand({3, 7, 221, 193}, at::device(at::kCPU).dtype(at::kFloat));

  // Act
  const auto out_cpu = at::cat({in_cpu1}, 1);
  const auto out_vulkan = at::cat({in_cpu1}, 1); // dim=feature(channel)

  // Assert
  const auto check = almostEqual(out_cpu, out_vulkan.cpu());
  if (!check) {
    showRtol(out_cpu, out_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, cat_dim1_twotensors_success) {
  // Guard
  if (!at::is_vulkan_available()) {
    return;
  }

  // Arrange: two input tensors
  const auto in_cpu1 = at::rand({3, 7, 221, 193}, at::device(at::kCPU).dtype(at::kFloat));
  const auto in_cpu2 = at::rand({3, 7, 221, 193}, at::device(at::kCPU).dtype(at::kFloat));

  // Act
  const auto out_cpu = at::cat({in_cpu1, in_cpu2}, 1);
  const auto out_vulkan = at::cat({in_cpu1.vulkan(), in_cpu2.vulkan()}, 1); // dim=feature(channel)

  // Assert
  const auto check = almostEqual(out_cpu, out_vulkan.cpu());
  if (!check) {
    showRtol(out_cpu, out_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, cat_dim1_bat1_mult4ch_success) {
  // Guard
  if (!at::is_vulkan_available()) {
    return;
  }

  // Arrange: batch=1 and channel (a multiple of 4 <-> channel %4 == 0)
  const auto in_cpu1 = at::rand({1, 4, 221, 193}, at::device(at::kCPU).dtype(at::kFloat));
  const auto in_cpu2 = at::rand({1, 4, 221, 193}, at::device(at::kCPU).dtype(at::kFloat));
  const auto in_cpu3 = at::rand({1, 4, 221, 193}, at::device(at::kCPU).dtype(at::kFloat));

  // Act
  const auto out_cpu = at::cat({in_cpu1, in_cpu2, in_cpu3}, 1);
  const auto out_vulkan = at::cat({in_cpu1.vulkan(), in_cpu2.vulkan(), in_cpu3.vulkan()}, 1); // dim=feature(channel)

  // Assert
  const auto check = almostEqual(out_cpu, out_vulkan.cpu());
  if (!check) {
    showRtol(out_cpu, out_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, cat_dim1_bat2_mult4ch_success) {
  // Guard
  if (!at::is_vulkan_available()) {
    return;
  }

  // Arrange: batch=2 and channel (a multiple of 4 <-> channel %4 == 0)
  const auto in_cpu1 = at::rand({2, 4, 221, 193}, at::device(at::kCPU).dtype(at::kFloat));
  const auto in_cpu2 = at::rand({2, 4, 221, 193}, at::device(at::kCPU).dtype(at::kFloat));
  const auto in_cpu3 = at::rand({2, 4, 221, 193}, at::device(at::kCPU).dtype(at::kFloat));

  // Act
  const auto out_cpu = at::cat({in_cpu1, in_cpu2, in_cpu3}, 1);
  const auto out_vulkan = at::cat({in_cpu1.vulkan(), in_cpu2.vulkan(), in_cpu3.vulkan()}, 1); // dim=feature(channel)

  // Assert
  const auto check = almostEqual(out_cpu, out_vulkan.cpu());
  if (!check) {
    showRtol(out_cpu, out_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, cat_dim1_mult4ch_mixed_success) {
  // Guard
  if (!at::is_vulkan_available()) {
    return;
  }

  // Arrange: batch=1 and channel (different multiples of 4 <-> channel %4 == 0)
  const auto in_cpu1 = at::rand({3, 4, 221, 193}, at::device(at::kCPU).dtype(at::kFloat));
  const auto in_cpu2 = at::rand({3, 8, 221, 193}, at::device(at::kCPU).dtype(at::kFloat));
  const auto in_cpu3 = at::rand({3, 12, 221, 193}, at::device(at::kCPU).dtype(at::kFloat));

  // Act
  const auto out_cpu = at::cat({in_cpu1, in_cpu2, in_cpu3}, 1);
  const auto out_vulkan = at::cat({in_cpu1.vulkan(), in_cpu2.vulkan(), in_cpu3.vulkan()}, 1); // dim=feature(channel)

  // Assert
  const auto check = almostEqual(out_cpu, out_vulkan.cpu());
  if (!check) {
    showRtol(out_cpu, out_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, cat_dim1_mult4ch_nonmult4ch_success) {
  // Guard
  if (!at::is_vulkan_available()) {
    return;
  }

  // Arrange: batch=1 and channel (a mixed set of multiples and non-multiples of 4)
  const auto in_cpu1 = at::rand({3, 3, 221, 193}, at::device(at::kCPU).dtype(at::kFloat));
  const auto in_cpu2 = at::rand({3, 4, 221, 193}, at::device(at::kCPU).dtype(at::kFloat));
  const auto in_cpu3 = at::rand({3, 7, 221, 193}, at::device(at::kCPU).dtype(at::kFloat));
  const auto in_cpu4 = at::rand({3, 8, 221, 193}, at::device(at::kCPU).dtype(at::kFloat));

  // Act
  const auto out_cpu = at::cat({in_cpu1, in_cpu2, in_cpu3, in_cpu4}, 1);
  const auto out_vulkan = at::cat({in_cpu1.vulkan(), in_cpu2.vulkan(), in_cpu3.vulkan(), in_cpu4.vulkan()}, 1); // dim=feature(channel)

  // Assert
  const auto check = almostEqual(out_cpu, out_vulkan.cpu());
  if (!check) {
    showRtol(out_cpu, out_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, cat_dim2_sameheight_success) {
  // Guard
  if (!at::is_vulkan_available()) {
    return;
  }

  // Arrange
  const auto in_cpu1 = at::rand({3, 9, 221, 193}, at::device(at::kCPU).dtype(at::kFloat));
  const auto in_cpu2 = at::rand({3, 9, 221, 193}, at::device(at::kCPU).dtype(at::kFloat));
  const auto in_cpu3 = at::rand({3, 9, 221, 193}, at::device(at::kCPU).dtype(at::kFloat));

  // Act
  const auto out_cpu = at::cat({in_cpu1, in_cpu2, in_cpu3}, 2);
  const auto out_vulkan = at::cat({in_cpu1.vulkan(), in_cpu2.vulkan(), in_cpu3.vulkan()}, 2);

  // Assert
  const auto check = almostEqual(out_cpu, out_vulkan.cpu());
  if (!check) {
    showRtol(out_cpu, out_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, cat_dim2_diffheight_success) {
  // Guard
  if (!at::is_vulkan_available()) {
    return;
  }

  // Arrange
  const auto in_cpu1 = at::rand({3, 9, 221, 193}, at::device(at::kCPU).dtype(at::kFloat));
  const auto in_cpu2 = at::rand({3, 9, 112, 193}, at::device(at::kCPU).dtype(at::kFloat));
  const auto in_cpu3 = at::rand({3, 9, 331, 193}, at::device(at::kCPU).dtype(at::kFloat));

  // Act
  const auto out_cpu = at::cat({in_cpu1, in_cpu2, in_cpu3}, 2);
  const auto out_vulkan = at::cat({in_cpu1.vulkan(), in_cpu2.vulkan(), in_cpu3.vulkan()}, 2);

  // Assert
  const auto check = almostEqual(out_cpu, out_vulkan.cpu());
  if (!check) {
    showRtol(out_cpu, out_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, cat_dim2_singledepth_success) {
  // Guard
  if (!at::is_vulkan_available()) {
    return;
  }

  // Arrange: batch x channel (1x1) = single depth texture
  const auto in_cpu1 = at::rand({1, 1, 221, 193}, at::device(at::kCPU).dtype(at::kFloat));
  const auto in_cpu2 = at::rand({1, 1, 221, 193}, at::device(at::kCPU).dtype(at::kFloat));
  const auto in_cpu3 = at::rand({1, 1, 221, 193}, at::device(at::kCPU).dtype(at::kFloat));

  // Act
  const auto out_cpu = at::cat({in_cpu1, in_cpu2, in_cpu3}, 2);
  const auto out_vulkan = at::cat({in_cpu1.vulkan(), in_cpu2.vulkan(), in_cpu3.vulkan()}, 2);

  // Assert
  const auto check = almostEqual(out_cpu, out_vulkan.cpu());
  if (!check) {
    showRtol(out_cpu, out_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, cat_dim2_invalidinputs_exceptions) {
  // Guard
  if (!at::is_vulkan_available()) {
    return;
  }

  // Arrange: Vulkan cat inputs must have matching sizes except concatenated dimension
  {
    const auto in_cpu1 = at::rand({3, 5, 221, 193}, at::device(at::kCPU).dtype(at::kFloat));
    const auto in_cpu2 = at::rand({3, 9, 112, 193}, at::device(at::kCPU).dtype(at::kFloat));
    const auto in_cpu3 = at::rand({3, 9, 331, 193}, at::device(at::kCPU).dtype(at::kFloat));

    // Act
    EXPECT_THROW({
      const auto out_vulkan = at::cat({in_cpu1.vulkan(), in_cpu2.vulkan(), in_cpu3.vulkan()}, 2);
    }, ::c10::Error);
  }

  // Arrange: Vulkan cat expects 4 dimensional inputs
  {
    const auto in_cpu1 = at::rand({3, 9, 221, 193}, at::device(at::kCPU).dtype(at::kFloat));
    const auto in_cpu2 = at::rand({9, 112, 193}, at::device(at::kCPU).dtype(at::kFloat));
    const auto in_cpu3 = at::rand({3, 9, 331, 193}, at::device(at::kCPU).dtype(at::kFloat));

    // Act
    EXPECT_THROW({
      const auto out_vulkan = at::cat({in_cpu1.vulkan(), in_cpu2.vulkan(), in_cpu3.vulkan()}, 2);
    }, ::c10::Error);
  }

  // Arrange: Vulkan cat is implemented only for height dimension
  {
    const auto in_cpu1 = at::rand({3, 9, 221, 193}, at::device(at::kCPU).dtype(at::kFloat));
    const auto in_cpu2 = at::rand({3, 9, 112, 193}, at::device(at::kCPU).dtype(at::kFloat));
    const auto in_cpu3 = at::rand({3, 9, 331, 193}, at::device(at::kCPU).dtype(at::kFloat));

    // Act
    EXPECT_THROW({
      const auto out_vulkan = at::cat({in_cpu1.vulkan(), in_cpu2.vulkan(), in_cpu3.vulkan()}, 0);
    }, ::c10::Error);
    EXPECT_THROW({
      const auto out_vulkan = at::cat({in_cpu1.vulkan(), in_cpu2.vulkan(), in_cpu3.vulkan()}, 1);
    }, ::c10::Error);
    EXPECT_THROW({
      const auto out_vulkan = at::cat({in_cpu1.vulkan(), in_cpu2.vulkan(), in_cpu3.vulkan()}, 3);
    }, ::c10::Error);
  }
}

TEST(VulkanAPITest, permute_2d_success) {
  // Guard
  if (!at::is_vulkan_available()) {
    return;
  }

  // Arrange
  const auto in_cpu = at::rand({2, 3}, at::device(at::kCPU).dtype(at::kFloat));

  // Act
  const auto out_cpu = at::permute(in_cpu, {1, 0});
  const auto out_vulkan = at::permute(in_cpu.vulkan(), {1, 0});

  // Assert
  const auto check = almostEqual(out_cpu, out_vulkan.cpu());
  if (!check) {
    showRtol(out_cpu, out_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, permute_3d_success) {
  // Guard
  if (!at::is_vulkan_available()) {
    return;
  }

  // Arrange
  const auto in_cpu = at::rand({2, 3, 2}, at::device(at::kCPU).dtype(at::kFloat));
  std::vector<std::vector<int64_t>> all_dims;
  std::vector<int64_t> in{0, 1, 2};
  gen_allpermutations(all_dims, in, 0);

  for (const auto& dims : all_dims) {
    // Act
    const auto out_cpu = at::permute(in_cpu, dims);
    const auto out_vulkan = at::permute(in_cpu.vulkan(), dims);

    // Assert
    const auto check = almostEqual(out_cpu, out_vulkan.cpu());
    if (!check) {
      showRtol(out_cpu, out_vulkan.cpu());
    }

    ASSERT_TRUE(check);
  }
}

TEST(VulkanAPITest, permute_4d_success) {
  // Guard
  if (!at::is_vulkan_available()) {
    return;
  }

  // Arrange
  const auto in_cpu = at::rand({2, 3, 4, 5}, at::device(at::kCPU).dtype(at::kFloat));
  std::vector<std::vector<int64_t>> all_dims;
  std::vector<int64_t> in{0, 1, 2, 3};
  gen_allpermutations(all_dims, in, 0);

  for (const auto& dims : all_dims) {
    // Act
    const auto out_cpu = at::permute(in_cpu, dims);
    const auto out_vulkan = at::permute(in_cpu.vulkan(), dims);

    // Assert
    const auto check = almostEqual(out_cpu, out_vulkan.cpu());
    if (!check) {
      showRtol(out_cpu, out_vulkan.cpu());
    }

    ASSERT_TRUE(check);
  }
}

TEST(VulkanAPITest, permute_4dmclaren_success) {
  // Guard
  if (!at::is_vulkan_available()) {
    return;
  }

  // Arrange: McLaren Model usage
  const auto in_cpu = at::rand({1, 2, 1, 161}, at::device(at::kCPU).dtype(at::kFloat));

  // Act
  const auto out_cpu = at::permute(in_cpu, {0, 2, 1, 3});
  const auto out_vulkan = at::permute(in_cpu.vulkan(), {0, 2, 1, 3});

  // Assert
  const auto check = almostEqual(out_cpu, out_vulkan.cpu());
  if (!check) {
    showRtol(out_cpu, out_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, permute_4dbig_success) {
  // Guard
  if (!at::is_vulkan_available()) {
    return;
  }

  // Arrange
  const auto in_cpu = at::rand({3, 9, 89, 91}, at::device(at::kCPU).dtype(at::kFloat));
  std::vector<std::vector<int64_t>> all_dims;
  std::vector<int64_t> in{0, 1, 2, 3};
  gen_allpermutations(all_dims, in, 0);

  for (const auto& dims : all_dims) {
    // Act
    const auto out_cpu = at::permute(in_cpu, dims);
    const auto out_vulkan = at::permute(in_cpu.vulkan(), dims);

    // Assert
    const auto check = almostEqual(out_cpu, out_vulkan.cpu());
    if (!check) {
      showRtol(out_cpu, out_vulkan.cpu());
    }

    ASSERT_TRUE(check);
  }
}

TEST(VulkanAPITest, permute_negativedims_success) {
  // Guard
  if (!at::is_vulkan_available()) {
    return;
  }

  // Arrange
  const auto in_cpu = at::rand({5, 4, 3, 2}, at::device(at::kCPU).dtype(at::kFloat));

  // Act: {-1,-2,-3,0} is equivalent to {3,2,1,0}
  const auto out_cpu = at::permute(in_cpu, {-1, -2, -3, 0});
  const auto out_vulkan = at::permute(in_cpu.vulkan(), {-1, -2, -3, 0});

  // Assert
  const auto check = almostEqual(out_cpu, out_vulkan.cpu());
  if (!check) {
    showRtol(out_cpu, out_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, permute_1d_nochange) {
  // Guard
  if (!at::is_vulkan_available()) {
    return;
  }

  // Arrange
  const auto in_cpu = at::rand({161}, at::device(at::kCPU).dtype(at::kFloat));

  // Act
  const auto out_cpu = at::permute(in_cpu, {0});
  const auto out_vulkan = at::permute(in_cpu.vulkan(), {0});

  // Assert
  const auto check = almostEqual(out_cpu, out_vulkan.cpu());
  if (!check) {
    showRtol(out_cpu, out_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, permute_sameDims_nochange) {
  // Guard
  if (!at::is_vulkan_available()) {
    return;
  }

  // Arrange
  const auto in_cpu = at::rand({1, 2, 1, 161}, at::device(at::kCPU).dtype(at::kFloat));

  // Act
  const auto out_cpu = at::permute(in_cpu, {0, 1, 2, 3});
  const auto out_vulkan = at::permute(in_cpu.vulkan(), {0, 1, 2, 3});

  // Assert
  const auto check = almostEqual(out_cpu, out_vulkan.cpu());
  if (!check) {
    showRtol(out_cpu, out_vulkan.cpu());
  }

  ASSERT_TRUE(check);
}

TEST(VulkanAPITest, permute_invalidinputs_exceptions) {
  // Guard
  if (!at::is_vulkan_available()) {
    return;
  }

  // Arrange
  const auto in_cpu = at::rand({1, 2, 1, 161}, at::device(at::kCPU).dtype(at::kFloat));

  // Act: Repeated dim
  EXPECT_THROW({
    const auto out_vulkan = at::permute(in_cpu.vulkan(), {2, 2, 1, 0});
  }, ::c10::Error);

  EXPECT_THROW({
    const auto out_vulkan = in_cpu.vulkan();
    out_vulkan.permute({2, 2, 1, 0});
  }, ::c10::Error);

  // Act: Number of dims don't match
  EXPECT_THROW({
    const auto out_vulkan = at::permute(in_cpu.vulkan(), {4, 3, 2, 1, 0});
  }, ::c10::Error);

  EXPECT_THROW({
    const auto out_vulkan = at::permute(in_cpu.vulkan(), {2, 1, 0});
  }, ::c10::Error);

  EXPECT_THROW({
    const auto out_vulkan = in_cpu.vulkan();
    out_vulkan.permute({4, 3, 2, 1, 0});
  }, ::c10::Error);

  EXPECT_THROW({
    const auto out_vulkan = in_cpu.vulkan();
    out_vulkan.permute({2, 1, 0});
  }, ::c10::Error);

  // Act: Dim out of range
  EXPECT_THROW({
    const auto out_vulkan = at::permute(in_cpu.vulkan(), {5, 2, 1, 0});
  }, ::c10::Error);

  EXPECT_THROW({
    const auto out_vulkan = in_cpu.vulkan();
    out_vulkan.permute({5, 2, 1, 0});
  }, ::c10::Error);

  // Act: Input tensor size > 4D
  const auto in_cpu_5d = at::rand({1, 2, 1, 2, 161}, at::device(at::kCPU).dtype(at::kFloat));
  EXPECT_THROW({
    const auto out_vulkan_5d = at::permute(in_cpu_5d.vulkan(), {4, 3, 2, 1, 0});
  }, ::c10::Error);

  EXPECT_THROW({
    const auto out_vulkan_5d = in_cpu_5d.vulkan();
    out_vulkan_5d.permute({4, 3, 2, 1, 0});
  }, ::c10::Error);
}

TEST(VulkanAPITest, slice_width_success) {
  // Arrange
  std::unordered_map<int64_t, std::vector<int64_t>> dim2sizes {
    {3, {2, 3, 40, 50}},  // 4D tensors with dim=width
    {2, {3, 40, 50}},     // 3D tensors with dim=width
    {1, {40, 50}},        // 2D tensors with dim=width
    {0, {50}},            // 1D tensors with dim=width
  };

  // Act/Assert
  slice_tests(dim2sizes);
}

TEST(VulkanAPITest, slice_height_success) {
  // Arrange
  std::unordered_map<int64_t, std::vector<int64_t>> dim2sizes {
    {2, {2, 3, 40, 50}},  // 4D tensors with dim=height
    {1, {3, 40, 50}},     // 3D tensors with dim=height
    {0, {40, 50}},        // 2D tensors with dim=height
                          // 1D tesnors don't have height dim for test
  };

  // Act/Assert
  slice_tests(dim2sizes);
}

TEST(VulkanAPITest, slice_feature_success) {
  // Arrange
  std::unordered_map<int64_t, std::vector<int64_t>> dim2sizes {
    {1, {2, 40, 13, 14}}, // 4D tensors with dim=feature(channel)
    {0, {40, 13, 14}},    // 3D tensors with dim=feature(channel)
                          // 1D and 2D tesnors don't have feature(channel) dim for test
  };

  // Act/Assert
  slice_tests(dim2sizes);
}

TEST(VulkanAPITest, slice_batch_success) {
  // Arrange
  std::unordered_map<int64_t, std::vector<int64_t>> dim2sizes {
    {0, {40, 3, 13, 14}}, // 4D tensors with dim=batch
                          // 1D, 2D and 3D tesnors don't have batch dim for test
  };

  // Act/Assert
  slice_tests(dim2sizes);
}

TEST(VulkanAPITest, slice_invalidinputs_exceptions) {
  // Act: slice step must be positive
  EXPECT_THROW({
    slice_test({2, 3, 4, 5}, 3, 0, 3, 0);
  }, ::c10::Error);

  // Act: Vulkan doesn't support zero-sized slice (when start=end)
  EXPECT_THROW({
    slice_test({2, 3, 4, 5}, 3, 0, 0, 1);
  }, ::c10::Error);

  // Act: Vulkan doesn't support zero-sized slice (when start > end)
  EXPECT_THROW({
    slice_test({2, 3, 4, 5}, 3, 3, 2, 1);
  }, ::c10::Error);
}

enum class OpType {
  addmm,
  conv2d,
  hardtanh_,
  mean,
 };

class BaseOp {
 public:
  explicit BaseOp(const OpType) {}
  virtual ~BaseOp() = default;

  virtual at::Tensor run(at::Tensor&) const = 0;
  virtual std::string toString() const = 0;

};

class Addmm final : public BaseOp {
 public:
  Addmm(
      const int64_t m1H,
      const int64_t m1W,
      const int64_t m2W,
      const float beta,
      const float alpha)
    : BaseOp(OpType::addmm),
      m2_(at::rand(c10::IntArrayRef({m1W, m2W}), at::device(at::kCPU).dtype(at::kFloat))),
      b_(at::rand(c10::IntArrayRef({m1H, m2W}), at::device(at::kCPU).dtype(at::kFloat))),
      beta_(beta),
      alpha_(alpha) {
  }

  at::Tensor run(at::Tensor& t) const override {
    if (t.is_vulkan()) {
      return at::addmm(b_, t, m2_, beta_, alpha_);
    }

    return at::addmm(b_, t, m2_, beta_, alpha_);
  }

  std::string toString() const override {
    return "addmm";
  }

 private:
  at::Tensor m2_;
  at::Tensor b_;
  float beta_;
  float alpha_;
};

class Conv2d final : public BaseOp {
 public:
  Conv2d(
      const c10::IntArrayRef wsizes,
      const int64_t groups,
      const int64_t stride,
      const int64_t padding)
      : BaseOp(OpType::conv2d),
        groups_(groups),
        stride_(stride),
        padding_(padding),
        w_(at::rand(wsizes, at::device(at::kCPU).dtype(at::kFloat))),
        b_(at::rand(wsizes[0], at::device(at::kCPU).dtype(at::kFloat))){
  }

  at::Tensor run(at::Tensor& t) const override {
    return at::conv2d(t, w_, b_, {stride_}, {padding_}, {1}, groups_);
  }

  std::string toString() const override {
    return "conv2d";
  }

 private:
  int64_t groups_;
  int64_t stride_;
  int64_t padding_;
  at::Tensor w_;
  at::Tensor b_;
};

class Hardtanh_ final : public BaseOp {
 public:
  Hardtanh_() : BaseOp(OpType::hardtanh_) {}

  at::Tensor run(at::Tensor& input) const override {
    return at::hardtanh_(input, 0, 6);
  }

  std::string toString() const override {
    return "hardtanh_";
  }
};

class Mean final : public BaseOp {
 public:
  Mean() : BaseOp(OpType::mean) {}

  at::Tensor run(at::Tensor& input) const override {
    return at::mean(input, {2, 3}, false);
  }

  std::string toString() const override {
    return "mean";
  }
};

class OpsList {
 public:
  OpsList() {}
  explicit OpsList(std::vector<std::unique_ptr<BaseOp>> ops)
    : ops_(std::move(ops)) {
  }

  auto run(const at::Tensor& input) {
    at::Tensor output = input;

    for (const auto& op : ops_) {
      output = op->run(output);
    }

    return output;
  }

  auto run(const at::Tensor& input, const at::Tensor& v_input) {
    at::Tensor output = input;
    at::Tensor v_output = v_input;

    for (const auto& op : ops_) {
      output = op->run(output);
      v_output = op->run(v_output);
    }

    return std::make_pair(output, v_output);
  }

 protected:
  std::vector<std::unique_ptr<BaseOp>> ops_;
};

class MobileNetV2 final : public OpsList {
 public:
  MobileNetV2() {
    ops_.emplace_back(new Conv2d({32, 3, 3, 3}, 1, 2, 1));
    ops_.emplace_back(new Hardtanh_());
    ops_.emplace_back(new Conv2d({32, 1, 3, 3}, 32, 1, 1));
    ops_.emplace_back(new Hardtanh_());
    ops_.emplace_back(new Conv2d({16, 32, 1, 1}, 1, 1, 0));
    ops_.emplace_back(new Conv2d({96, 16, 1, 1}, 1, 1, 0));
    ops_.emplace_back(new Hardtanh_());
    ops_.emplace_back(new Conv2d({96, 1, 3, 3}, 96, 2, 1));
    ops_.emplace_back(new Hardtanh_());
    ops_.emplace_back(new Conv2d({24, 96, 1, 1}, 1, 1, 0));
    ops_.emplace_back(new Conv2d({144, 24, 1, 1}, 1, 1, 0));
    ops_.emplace_back(new Hardtanh_());
    ops_.emplace_back(new Conv2d({144, 1, 3, 3}, 144, 1, 1));
    ops_.emplace_back(new Hardtanh_());
    ops_.emplace_back(new Conv2d({24, 144, 1, 1}, 1, 1, 0));
    ops_.emplace_back(new Conv2d({144, 24, 1, 1}, 1, 1, 0));
    ops_.emplace_back(new Hardtanh_());
    ops_.emplace_back(new Conv2d({144, 1, 3, 3}, 144, 2, 1));
    ops_.emplace_back(new Hardtanh_());
    ops_.emplace_back(new Conv2d({32, 144, 1, 1}, 1, 1, 0));
    ops_.emplace_back(new Conv2d({192, 32, 1, 1}, 1, 1, 0));
    ops_.emplace_back(new Hardtanh_());
    ops_.emplace_back(new Conv2d({192, 1, 3, 3}, 192, 1, 1));
    ops_.emplace_back(new Hardtanh_());
    ops_.emplace_back(new Conv2d({32, 192, 1, 1}, 1, 1, 0));
    ops_.emplace_back(new Conv2d({192, 32, 1, 1}, 1, 1, 0));
    ops_.emplace_back(new Hardtanh_());
    ops_.emplace_back(new Conv2d({192, 1, 3, 3}, 192, 1, 1));
    ops_.emplace_back(new Hardtanh_());
    ops_.emplace_back(new Conv2d({32, 192, 1, 1}, 1, 1, 0));
    ops_.emplace_back(new Conv2d({192, 32, 1, 1}, 1, 1, 0));
    ops_.emplace_back(new Hardtanh_());
    ops_.emplace_back(new Conv2d({192, 1, 3, 3}, 192, 2, 1));
    ops_.emplace_back(new Hardtanh_());
    ops_.emplace_back(new Conv2d({64, 192, 1, 1}, 1, 1, 0));
    ops_.emplace_back(new Conv2d({384, 64, 1, 1}, 1, 1, 0));
    ops_.emplace_back(new Hardtanh_());
    ops_.emplace_back(new Conv2d({384, 1, 3, 3}, 384, 1, 1));
    ops_.emplace_back(new Hardtanh_());
    ops_.emplace_back(new Conv2d({64, 384, 1, 1}, 1, 1, 0));
    ops_.emplace_back(new Conv2d({384, 64, 1, 1}, 1, 1, 0));
    ops_.emplace_back(new Hardtanh_());
    ops_.emplace_back(new Conv2d({384, 1, 3, 3}, 384, 1, 1));
    ops_.emplace_back(new Hardtanh_());
    ops_.emplace_back(new Conv2d({64, 384, 1, 1}, 1, 1, 0));
    ops_.emplace_back(new Conv2d({384, 64, 1, 1}, 1, 1, 0));
    ops_.emplace_back(new Hardtanh_());
    ops_.emplace_back(new Conv2d({384, 1, 3, 3}, 384, 1, 1));
    ops_.emplace_back(new Hardtanh_());
    ops_.emplace_back(new Conv2d({64, 384, 1, 1}, 1, 1, 0));
    ops_.emplace_back(new Conv2d({384, 64, 1, 1}, 1, 1, 0));
    ops_.emplace_back(new Hardtanh_());
    ops_.emplace_back(new Conv2d({384, 1, 3, 3}, 384, 1, 1));
    ops_.emplace_back(new Hardtanh_());
    ops_.emplace_back(new Conv2d({96, 384, 1, 1}, 1, 1, 0));
    ops_.emplace_back(new Conv2d({576, 96, 1, 1}, 1, 1, 0));
    ops_.emplace_back(new Hardtanh_());
    ops_.emplace_back(new Conv2d({576, 1, 3, 3}, 576, 1, 1));
    ops_.emplace_back(new Hardtanh_());
    ops_.emplace_back(new Conv2d({96, 576, 1, 1}, 1, 1, 0));
    ops_.emplace_back(new Conv2d({576, 96, 1, 1}, 1, 1, 0));
    ops_.emplace_back(new Hardtanh_());
    ops_.emplace_back(new Conv2d({576, 1, 3, 3}, 576, 1, 1));
    ops_.emplace_back(new Hardtanh_());
    ops_.emplace_back(new Conv2d({96, 576, 1, 1}, 1, 1, 0));
    ops_.emplace_back(new Conv2d({576, 96, 1, 1}, 1, 1, 0));
    ops_.emplace_back(new Hardtanh_());
    ops_.emplace_back(new Conv2d({576, 1, 3, 3}, 576, 2, 1));
    ops_.emplace_back(new Hardtanh_());
    ops_.emplace_back(new Conv2d({160, 576, 1, 1}, 1, 1, 0));
    ops_.emplace_back(new Conv2d({960, 160, 1, 1}, 1, 1, 0));
    ops_.emplace_back(new Hardtanh_());
    ops_.emplace_back(new Conv2d({960, 1, 3, 3}, 960, 1, 1));
    ops_.emplace_back(new Hardtanh_());
    ops_.emplace_back(new Conv2d({160, 960, 1, 1}, 1, 1, 0));
    ops_.emplace_back(new Conv2d({960, 160, 1, 1}, 1, 1, 0));
    ops_.emplace_back(new Hardtanh_());
    ops_.emplace_back(new Conv2d({960, 1, 3, 3}, 960, 1, 1));
    ops_.emplace_back(new Hardtanh_());
    ops_.emplace_back(new Conv2d({160, 960, 1, 1}, 1, 1, 0));
    ops_.emplace_back(new Conv2d({960, 160, 1, 1}, 1, 1, 0));
    ops_.emplace_back(new Hardtanh_());
    ops_.emplace_back(new Conv2d({960, 1, 3, 3}, 960, 1, 1));
    ops_.emplace_back(new Hardtanh_());
    ops_.emplace_back(new Conv2d({320, 960, 1, 1}, 1, 1, 0));
    ops_.emplace_back(new Conv2d({1280, 320, 1, 1}, 1, 1, 0));
    ops_.emplace_back(new Hardtanh_());
    ops_.emplace_back(new Mean());
    ops_.emplace_back(new Addmm(1, 1280, 1000, 0, 1));
  }
};

TEST(VulkanAPITest, mobilenetv2) {
  if (!at::is_vulkan_available()) {
    return;
  }
  c10::InferenceMode mode;

  MobileNetV2 mn2;

  const auto input = at::rand({1, 3, 224, 224}, at::device(at::kCPU).dtype(at::kFloat));
  const auto output = mn2.run(input, input.vulkan());

  const auto check = almostEqual(output.first, output.second.cpu());
  if (!check) {
    showRtol(output.first, output.second.cpu());
  }

  ASSERT_TRUE(check);
}

} // namespace

#endif /* USE_VULKAN_API */
