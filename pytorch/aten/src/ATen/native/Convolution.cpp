#include <ATen/ATen.h>
#include <ATen/Parallel.h>
#include <ATen/native/ConvUtils.h>
#include <ATen/native/Pool.h>
#include <ATen/native/cpu/DepthwiseConvKernel.h>
#include <ATen/native/utils/ParamUtils.h>
#include <ATen/native/xnnpack/Engine.h>
#include <ATen/NativeFunctions.h>
#include <c10/util/accumulate.h>
#include <c10/util/irange.h>

#include <ATen/Config.h>
#include <c10/macros/Macros.h>

#include <limits>

#if AT_NNPACK_ENABLED()
#include <nnpack.h>
#endif


constexpr int MIOPEN_DIM_MAX = 5;

namespace at { namespace native {

DEFINE_DISPATCH(convolution_depthwise3x3_winograd_stub);

std::ostream& operator<<(std::ostream & out, const ConvParams& params) {
  out << "ConvParams {"
      << "  stride = " << IntArrayRef{params.stride}
      << "  padding = " << IntArrayRef{params.padding}
      << "  dilation = " << IntArrayRef{params.dilation}
      << "  transposed = " << params.transposed
      << "  output_padding = " << IntArrayRef{params.output_padding}
      << "  groups = " << params.groups
      << "  benchmark = " << params.benchmark
      << "  deterministic = " << params.deterministic
      << "  cudnn_enabled = " << params.cudnn_enabled
      << "  allow_tf32 = " << params.allow_tf32
      << "}";
  return out;
}

auto ConvParams::is_strided() const -> bool {
  bool is_strided = false;
  for (auto s : stride) {
    is_strided |= (s != 1);
  }
  return is_strided;
}

auto ConvParams::is_dilated() const -> bool {
  bool is_dilated = false;
  for (auto d : dilation) {
    is_dilated |= (d != 1);
  }
  return is_dilated;
}

auto ConvParams::is_padded() const -> bool {
  bool is_padded = false;
  for (auto p : padding) {
    is_padded |= (p != 0);
  }
  return is_padded;
}

auto ConvParams::is_output_padding_neg() const -> bool {
  bool is_non_neg = false;
  for (auto p : output_padding) {
    is_non_neg |= (p < 0);
  }
  return is_non_neg;
}

auto ConvParams::is_output_padding_big() const -> bool {
  bool is_big = false;
  for (auto i: c10::irange(output_padding.size())) {
    is_big |= (output_padding[i] >= stride[i]);
  }
  return is_big;
}

auto ConvParams::is_padding_neg() const -> bool {
  bool is_non_neg = false;
  for (auto p : padding) {
    is_non_neg |= (p < 0);
  }
  return is_non_neg;
}

auto ConvParams::is_stride_nonpos() const -> bool {
  bool is_nonpos = false;
  for (auto s : stride) {
    is_nonpos |= (s <= 0);
  }
  return is_nonpos;
}

auto ConvParams::view1d_as_2d() -> void {
  if (stride.size() == 1) {
    stride.insert(stride.begin(), 1);
    padding.insert(padding.begin(), 0);
    dilation.insert(dilation.begin(), 1);
    output_padding.insert(output_padding.begin(), 0);
  }
}

auto ConvParams::use_cpu_depthwise3x3_winograd(
    const at::Tensor& input,
    const at::Tensor& weight,
    const at::Tensor& bias) const -> bool {
#if defined(__ARM_NEON__)
  // Currently only 3x3 depthwise convolutions on tensors of float are supported.
  return (input.ndimension() == 4) &&
         (input.size(1) == groups) &&
         (weight.ndimension() == 4 ) &&
         (weight.size(0) % input.size(1) == 0) &&
         (weight.size(1) == 1) &&
         (weight.size(2) == 3) &&
         (weight.size(3) == 3) &&
         (input.device().is_cpu()) &&
         (input.scalar_type() == at::kFloat) &&
         input.is_contiguous() &&
         (weight.device().is_cpu()) &&
         (weight.scalar_type() == at::kFloat) &&
         weight.is_contiguous() &&
         (!bias.defined() ||
            ((bias.device().is_cpu()) &&
             (bias.scalar_type() == at::kFloat))) &&
         !is_strided() &&
         !is_dilated() &&
         // 3x3 depthwith convolutions implementation is inference only
         !(GradMode::is_enabled() &&
                 (input.requires_grad() ||
                  weight.requires_grad() ||
                 (bias.defined() && bias.requires_grad()))) &&
         !transposed;
#else
  return false;
#endif
}

auto ConvParams::needs_64bit_indexing_no_split(const at::Tensor& input, const at::Tensor& weight) const -> bool {
  constexpr int64_t int_max = std::numeric_limits<int>::max();
  int64_t numel_input = input.numel();
  // empty input
  if (numel_input == 0) {
    return false;
  }
  // input size can not be reduced to the range of int by splitting the batch dim
  int64_t n = input.size(0);
  if (numel_input / n > int_max) {
    return true;
  }
  // output size can not be reduced to the range of int by splitting the batch dim
  int64_t outsize = 1;
  if (transposed) {
    std::vector<int64_t> o = conv_input_size(input.sizes(), weight.sizes(), padding, output_padding, stride, dilation, groups);
    outsize = c10::multiply_integers(o.begin() + 1, o.end());
  } else {
    std::vector<int64_t> o = conv_output_size(input.sizes(), weight.sizes(), padding, stride, dilation);
    outsize = c10::multiply_integers(o.begin() + 1, o.end());
  }
  return outsize > int_max;
}

auto ConvParams::use_cudnn(const at::Tensor& input, const at::Tensor& weight) const -> bool {
  if (needs_64bit_indexing_no_split(input, weight)) {
    return false;
  }
  if (!detail::getCUDAHooks().compiledWithCuDNN()) {
    return false;
  }
  if (!input.is_cuda() || !cudnn_enabled) {
    return false;
  }
  if (input.scalar_type() == at::kBFloat16 || weight.scalar_type() == at::kBFloat16) {
    return false;
  }
  if (cudnn_conv_suggest_memory_format(input, weight) == at::MemoryFormat::Contiguous) {
    // bypass dilation checks for channels_last convolution
    if (deterministic && is_dilated()) {
      // cudnn doesn't support deterministic dilated convolution fully yet
      return false;
    }
    if (is_dilated()) {
      return detail::getCUDAHooks().supportsDilatedConvolutionWithCuDNN() && !is_output_padding_big();
    }
  }
  return !is_output_padding_big();
}

auto ConvParams::use_miopen(const at::Tensor& input, const at::Tensor& weight, bool bias_defined) const -> bool {
  if (needs_64bit_indexing_no_split(input, weight)) {
    return false;
  }
  return ((input.scalar_type() == at::kFloat) || (input.scalar_type() == at::kHalf) || (input.scalar_type() == at::kBFloat16))
         && detail::getCUDAHooks().compiledWithMIOpen()
         && input.is_cuda()
         && input.dim() <= MIOPEN_DIM_MAX
         && !(groups > 1 && is_dilated()) // MIOpen currently does not support dilation with groups of size > 1
         && !(input.scalar_type() == at::kBFloat16 && bias_defined) // MIOpen currently doesn't support bias with bfloat16
         && cudnn_enabled
         ;
}

auto ConvParams::use_mkldnn(const at::Tensor& input, const at::Tensor& weight) const -> bool {
#if AT_MKLDNN_ENABLED()
  if (!at::globalContext().userEnabledMkldnn()) {
    return false;
  }
  return (input.is_mkldnn()) || // input is mkldnn Tensor
    (input.device().is_cpu() &&
     input.scalar_type() == kFloat && // only on CPU Float Tensors
     !transposed && // or transposed tensors
     // For 1x1 filters, MKLDNN is faster than THNN when multi-threaded,
     // but THNN is faster when single-threaded.
     (is_strided() || is_dilated() || input.size(0) >= 16 ||
      weight.size(-1) != 1 || weight.size(-2) != 1 || at::get_num_threads() > 1) &&
     (groups > 1
      || (weight.size(-1) > 3 && weight.size(-2) > 3)
      || input.size(0) > 1
      || input.size(0)*input.size(1)*input.size(2)*input.size(3) > 20480) // for some case, native is faster
      );

#endif
  return false;
}

auto ConvParams::use_nnpack(const at::Tensor& input, const at::Tensor& weight) const -> bool {
#if AT_NNPACK_ENABLED()
  return at::_nnpack_available() &&
         input.device().is_cpu() &&
         input.scalar_type() == kFloat && // only on CPU Float Tensors
         !is_dilated() && // or dilation
         !transposed &&   // or transposed tensors
         input.ndimension() == 4 && // must be in NCHW format
         weight.ndimension() == 4 &&
         (weight.size(2) < 17) && (weight.size(3) < 17) // NNPACK only supports kernels up to 16x16
#if !defined(C10_MOBILE)
         && input.size(0) >= 16 // ensure large enough batch size to ensure perf, tuneable
#endif
     ;
#endif
  return false;
}

auto ConvParams::use_xnnpack(
    const at::Tensor& input,
    const at::Tensor& weight,
    const at::Tensor& bias) const -> bool {
#if defined(C10_MOBILE)
  if (!transposed) {
    return (input.size(1) == groups) &&
            xnnpack::use_convolution2d(
                input,
                weight,
                bias,
                padding,
                stride,
                dilation,
                groups,
                transposed);
  }
#endif
  return false;
}

// We currently only have depthwise support for the case where groups ==
// nInputPlane and nInputPlane == nOutputPlane (the latter due to the lack of
// a depthwise multiplier)
auto ConvParams::is_depthwise(
        const at::Tensor& input, const at::Tensor& weight) const -> bool {
  return input.is_cuda() &&
         !transposed &&
         (input.ndimension() == 4 || input.ndimension() == 5) &&
         input.size(1) == groups &&
         groups > 1 && // no point if there is only a single group
         weight.size(0) % input.size(1) == 0; // output channels must be a multiple of input channels
}

// Check workload to activate fast depthwise FP16 cudnn conv kernels
bool check_cudnn_depthwise_workload(const at::Tensor& input, int stride) {
  int w = input.size(3);  // same as h
  int ch = input.size(1);
  int bs = input.size(0);
  if (stride==1) {
    if (w >= 7) {
      // All batch sizes and nb_channels
      if (w >= 112) {
        return true;
      }

      // large nb_channels
      if (ch >= 1024) {
        // NOLINTNEXTLINE(bugprone-branch-clone,cppcoreguidelines-avoid-magic-numbers)
        if (w >= 56) {
          return true;
        } else if (bs >= 32) {
          return true;
        }
      }

      // batch_size specific
      if (bs >= 128) {
        // NOLINTNEXTLINE(bugprone-branch-clone,cppcoreguidelines-avoid-magic-numbers)
        if (ch >= 512) {
          return true;
        } else if (ch >= 64) {
          if (w >= 14) {
            return true;
          }
        } else if ((ch >= 32) && (w >=28)) {
          return true;
        }
      } else if (bs >= 64) {
        // NOLINTNEXTLINE(bugprone-branch-clone,cppcoreguidelines-avoid-magic-numbers)
        if ((ch >= 256) && (w >= 14)) {
          return true;
        } else if ((ch >= 32) && (w >= 28)) {
          return true;
        }
      } else if (bs >= 32) {
        // NOLINTNEXTLINE(bugprone-branch-clone,cppcoreguidelines-avoid-magic-numbers)
        if ((ch >= 256) && (w >= 14)) {
          return true;
        } else if ((ch >= 128) && (w >= 28)) {
          return true;
        } else if ((ch >= 32) && (w >= 56)) {
          return true;
        }
      } else if (bs >= 16) {
        if ((ch >= 1024) && (w >= 14)) {
          return true;
        }
        // NOLINTNEXTLINE(bugprone-branch-clone,cppcoreguidelines-avoid-magic-numbers)
        if ((ch >= 256) && (w >= 28)) {
          return true;
        } else if ((ch >= 32) && (w >= 56)) {
          return true;
        }
      } else if (bs >= 8) {
        // NOLINTNEXTLINE(bugprone-branch-clone,cppcoreguidelines-avoid-magic-numbers)
        if ((ch >= 512) && (w >= 28)) {
          return true;
        } else if ((ch >= 64) && (w >= 56)) {
          return true;
        }
      }
    }
  } else if (stride==2) {
    if (ch < 256) {
      return false;
    }

    if (w >= 7) {
      if (bs >= 128) {
        // NOLINTNEXTLINE(bugprone-branch-clone,cppcoreguidelines-avoid-magic-numbers)
        if (ch >= 1024) {
          return true;
        } else if ((ch >= 512) && (w >= 14)) {
          return true;
        } else if (w >= 28) {
          return true;
        }
      } else if (bs >= 64) {
        // NOLINTNEXTLINE(bugprone-branch-clone,cppcoreguidelines-avoid-magic-numbers)
        if ((ch >= 512) && (w >= 14)) {
          return true;
        } else if (w >= 28) {
          return true;
        }
      } else if (bs >= 32) {
        // NOLINTNEXTLINE(bugprone-branch-clone,cppcoreguidelines-avoid-magic-numbers)
        if ((ch >= 1024) && (w >= 14)) {
          return true;
        } else if (w >= 28) {
          return true;
        }
      } else if (bs >= 16) {
        // NOLINTNEXTLINE(bugprone-branch-clone,cppcoreguidelines-avoid-magic-numbers)
        if ((ch >= 512) && (w >= 28)) {
          return true;
        } else if (w >= 56) {
          return true;
        }
      } else if (bs >= 8) {
        // NOLINTNEXTLINE(bugprone-branch-clone,cppcoreguidelines-avoid-magic-numbers)
        if ((ch >= 1024) && (w >= 28)) {
          return true;
        } else if (w >= 56) {
          return true;
        }
      } else if (bs >= 1) {
        if ((ch >= 512) && (w >=112)) {
          return true;
        }
      }
    }
  }
  return false;
}

// simplified version for cudnn 8.2 and above
bool check_cudnn_depthwise_workload_with_filter(const at::Tensor& input, int stride, const at::Tensor& weight) {
  // 1D conv
  if(input.size(2) == 1 && stride == 1){
    return true;
  }

  // 2d conv
  // only square filters
  if (weight.size(2) != weight.size(3)) return false;
  int filter = weight.size(3);
  // only 1/3/5 filter
  if (filter != 1 && filter != 3 && filter != 5) return false;
  // we don't enforce square input but only check width to reduce heuristic space
  if (input.size(3) < 7) return false; // min width 7
  int w = input.size(3);
  // only 1/2 stride, use cudnn for all stride 1
  if (stride == 1) return true;
  if (stride != 2) return false;

  int ch = input.size(1);
  int bs = input.size(0);
  // special case since bs1 show good perf in lots of cases
  if (bs == 1) {
    if (filter == 1 && w <= 28) return true;
    if (filter == 3 || filter == 5) return true;
  } else {
    if (filter == 1 && bs <= 16 && ch >= 128 && w <= 7) return true;
    if (filter == 3 || filter == 5) {
      if ((ch >= 512) || (ch >= 256 && w >= 28)) return true;
    }
  }
  return false;
}

// Use cudnn for FP16 depthwise convolutions
auto ConvParams::use_cudnn_depthwise(
        const at::Tensor& input, const at::Tensor& weight) const -> bool {
  if (cudnn_conv_suggest_memory_format(input, weight) != at::MemoryFormat::Contiguous && use_cudnn(input, weight)) {
    // always use cudnn_depthwise for channels_last format
    return true;
  }
  if (detail::getCUDAHooks().supportsDepthwiseConvolutionWithCuDNN()) {
    long cudnn_version = detail::getCUDAHooks().versionCuDNN();
    if (cudnn_version >= 8200) {
      bool kernel_cond =  (use_cudnn(input, weight) &&
                           input.scalar_type() == kHalf && // only for FP16
                           weight.scalar_type() == kHalf &&
                           is_depthwise(input, weight) &&
                           input.ndimension() == 4 &&   // TODO: 5-D contiguous depthwise is not supported yet, need benchmarks
                           !is_dilated() && // no dilation supported
                           (stride[0] == stride[1] || input.size(2) == 1) && // square or 1d
                           input.size(1) >= 32); // min 32 channels supported)
      if (kernel_cond) {
        return check_cudnn_depthwise_workload_with_filter(input, stride[1], weight);
      }
    }
    // keep (7600 <= cudnn < 8200) code unchanged
    bool kernel_cond =  (cudnn_version >= 7600 &&
                         use_cudnn(input, weight) &&
                         input.scalar_type() == kHalf && // only for FP16
                         weight.scalar_type() == kHalf &&
                         is_depthwise(input, weight) &&
                         input.ndimension() == 4 &&   // TODO: 5-D contiguous depthwise is not supported yet, need benchmarks
                         weight.size(2) == weight.size(3) && // only square kernels
                         input.size(2) >= 7 && // min width/height 7
                         !is_dilated() && // no dilation supported
                         stride[0] == stride[1] && // equal strides
                         ((weight.size(3) == 3) || (weight.size(3) == 1)) &&
                         input.size(1) >= 32); // min 32 channels supported)
    if (kernel_cond) {
      return check_cudnn_depthwise_workload(input, stride[0]);
    } else {
      return false;
    }
  } else {
    return false;
  }
}

static void check_shape_forward(const at::Tensor& input,
                                const c10::IntArrayRef& weight_sizes, const at::Tensor& bias,
                                const ConvParams& params) {
  int64_t k = input.ndimension();
  int64_t weight_dim = weight_sizes.size();
  int64_t groups = params.groups;
  auto padding = params.padding;
  auto dilation = params.dilation;
  bool transposed = params.transposed;

  TORCH_CHECK(!params.is_padding_neg(), "negative padding is not supported");
  TORCH_CHECK(!params.is_output_padding_neg(), "negative output_padding is not supported");
  TORCH_CHECK(!params.is_stride_nonpos(), "non-positive stride is not supported");

  TORCH_CHECK(weight_dim == k,
           "Expected ", weight_dim, "-dimensional input for ", weight_dim,
           "-dimensional weight ", weight_sizes, ", but got ", k, "-dimensional input of size ",
           input.sizes(), " instead");
  TORCH_CHECK(weight_sizes[0] >= groups,
           "Given groups=", groups, ", expected weight to be at least ", groups,
           " at dimension 0, but got weight of size ", weight_sizes, " instead");
  TORCH_CHECK(weight_sizes[0] % groups == 0,
           "Given groups=", groups, ", expected weight to be divisible by ",
           groups, " at dimension 0, but got weight of size [", weight_sizes,
           "] instead");

  if (!transposed) {
    std::vector<int64_t> input_shape;
    std::vector<int64_t> kernel_shape;
    bool kernel_size_correct = true;

    TORCH_CHECK(input.size(1) == (weight_sizes[1] * groups),
                "Given groups=", groups, ", weight of size ", weight_sizes,
                ", expected input", input.sizes(), " to have ",
                (weight_sizes[1] * groups), " channels, but got ", input.size(1),
                " channels instead");

    TORCH_CHECK(!bias.defined() || (bias.ndimension() == 1 && bias.size(0) == weight_sizes[0]),
             "Given weight of size ", weight_sizes,
             ", expected bias to be 1-dimensional with ", weight_sizes[0], " elements",
             ", but got bias of size ", bias.sizes(), " instead");

    for (const auto i : c10::irange(2, k)) {
      input_shape.push_back(input.size(i) + 2 * padding[i-2]);
      // log new kernel size considering dilation
      kernel_shape.push_back(dilation[i-2] * (weight_sizes[i]-1) + 1);
      if (input_shape.back() < kernel_shape.back()) {
        kernel_size_correct = false;
      }
    }

    TORCH_CHECK(input_shape.size() == kernel_shape.size(), "Inconsistent shape between Input and Kernel");

    if (!kernel_size_correct) {
      // If kernel size is incorrect
      std::ostringstream input_ss;
      std::ostringstream kernel_ss;
      std::string separator = "";

      for (int i = 0, len = input_shape.size(); i < len; ++i) {
        input_ss << separator << input_shape[i];
        kernel_ss << separator << kernel_shape[i];
        separator = " x ";
      }

      AT_ERROR("Calculated padded input size per channel: (", input_ss.str(), "). "
               "Kernel size: (", kernel_ss.str(), "). Kernel size can't be greater than actual input size");
    }
  } else { // transposed
    TORCH_CHECK(input.size(1) == weight_sizes[0],
             "Given transposed=", transposed, ", weight of size ", weight_sizes,
             ", expected input", input.sizes(), " to have ", weight_sizes[0],
             " channels, but got ", input.size(1), " channels instead");
    TORCH_CHECK(!bias.defined() || (bias.ndimension() == 1 && bias.size(0) == weight_sizes[1] * groups),
             "Given transposed=", transposed, ", weight of size ", weight_sizes,
             ", expected bias to be 1-dimensional with ", weight_sizes[1] * groups, " elements",
             ", but got bias of size ", bias.sizes(), " instead");
  }
}

static void check_input_same_type_as_parameters(
    const Tensor& input,
    const Tensor& weight,
    const Tensor& bias) {
  TORCH_CHECK(input.options().type_equal(weight.options()),
      "Input type (", input.toString(), ") and weight type (", weight.toString(),
      ") should be the same");
  TORCH_CHECK(!bias.defined() || (input.options().type_equal(bias.options())),
      "Input type (", input.toString(), ") and bias type (", bias.toString(),
      ") should be the same");
}

static auto view4d(const at::Tensor& tensor) -> at::Tensor {
  TORCH_CHECK(tensor.ndimension() == 3,
           "expected 3D tensor, got tensor with ", tensor.ndimension(),
           " dimensions instead");
  return tensor.unsqueeze(2);
}

static auto view3d(const at::Tensor& tensor) -> at::Tensor {
  TORCH_CHECK(tensor.ndimension() == 4,
           "expected 4D tensor, got tensor with ", tensor.ndimension(),
           " dimensions instead");
  return tensor.squeeze(2);
}


static at::Tensor subtensor(at::Tensor& tensor, int dim, int groups, int g) {
  if (!tensor.defined()) {
    return at::Tensor();
  }
  int64_t n = tensor.sizes()[dim] / groups;
  return tensor.narrow(dim, n * g, n).contiguous();
}


at::Tensor conv1d(
    const Tensor& input, const Tensor& weight, const c10::optional<Tensor>& bias_opt,
    IntArrayRef stride, IntArrayRef padding, IntArrayRef dilation, int64_t groups) {
  // See [Note: hacky wrapper removal for optional tensor]
  c10::MaybeOwned<Tensor> bias_maybe_owned = at::borrow_from_optional_tensor(bias_opt);
  const Tensor& bias = *bias_maybe_owned;

  return at::convolution(input, weight, bias, stride, padding, dilation,
                         false, {0}, groups);
}

at::Tensor conv2d(
    const Tensor& input, const Tensor& weight, const c10::optional<Tensor>& bias_opt,
    IntArrayRef stride, IntArrayRef padding, IntArrayRef dilation, int64_t groups) {
  // See [Note: hacky wrapper removal for optional tensor]
  c10::MaybeOwned<Tensor> bias_maybe_owned = at::borrow_from_optional_tensor(bias_opt);
  const Tensor& bias = *bias_maybe_owned;

  return at::convolution(input, weight, bias, stride, padding, dilation,
                         false, {{0, 0}}, groups);
}

at::Tensor conv3d(
    const Tensor& input, const Tensor& weight, const c10::optional<Tensor>& bias_opt,
    IntArrayRef stride, IntArrayRef padding, IntArrayRef dilation, int64_t groups) {
  // See [Note: hacky wrapper removal for optional tensor]
  c10::MaybeOwned<Tensor> bias_maybe_owned = at::borrow_from_optional_tensor(bias_opt);
  const Tensor& bias = *bias_maybe_owned;

  return at::convolution(input, weight, bias, stride, padding, dilation,
                         false, {{0, 0, 0}}, groups);
}


static Tensor convolution_same(
    const Tensor &input, const Tensor &weight, const Tensor &bias,
    IntArrayRef stride, IntArrayRef dilation, int64_t groups) {

  auto k = weight.dim();
  TORCH_CHECK(k > 2, "weight should have at least three dimensions");
  auto dim = static_cast<size_t>(k - 2);
  auto weight_sizes = weight.sizes();
  auto input_sizes = input.sizes();
  TORCH_CHECK(k == input.dim(),
              "Expected ", k, "-dimensional input for ",
              k, "-dimensional weight", weight_sizes, ", but got ",
              input.dim(), "-dimensional input of size ",
              input.sizes(), " instead");
  TORCH_CHECK(stride.size() == dim || stride.size() == 1U,
              "stride cannot broadcast to ", dim, " dimensions");
  TORCH_CHECK(dilation.size() == dim || dilation.size() == 1U,
              "dilation cannot broadcast to ", dim, " dimensions");
  for (auto i: c10::irange(stride.size())) {
    TORCH_CHECK(stride[i] == 1, "padding='same' is not supported for strided convolutions");
  }

  // Calculate the correct padding
  DimVector padding_l, padding_r;
  bool symmetric_padding = true;
  for (auto i: c10::irange(dim)) {
    auto s = stride.size() == 1 ? stride[0] : stride[i];
    auto d = dilation.size() == 1 ? dilation[0] : dilation[i];
    auto pad = pooling_same_mode_padding_lr(
        input_sizes[i + 2], weight_sizes[i + 2], s, d);
    padding_l.push_back(pad.first);
    padding_r.push_back(pad.second);
    if (pad.first != pad.second) {
      symmetric_padding = false;
    }
  }

  if (symmetric_padding) {
    // All backends handle symmetric padding natively
    DimVector output_padding(static_cast<size_t>(dim));
    return native::convolution(input, weight, bias, stride, padding_l, dilation,
                               false, output_padding, groups);
  }

  TORCH_WARN_ONCE("Using padding='same' with even kernel lengths and odd dilation may"
                  " require a zero-padded copy of the input be created");
  SmallVector<int64_t, kDimVectorStaticSize * 2> pad_nd(static_cast<size_t>(2 * dim));
  for (auto i: c10::irange(dim)) {
    // Apply padding by the difference, leaving only a symmetric padding
    auto delta_pad = padding_r[i] - padding_l[i];
    auto pad_idx = 2 * (dim - 1 - i);  // F.pad goes from last dim to first
    if (delta_pad > 0) {
      pad_nd[pad_idx + 1] = delta_pad;
    } else {
      pad_nd[pad_idx] = delta_pad;
      padding_l[i] = padding_r[i];
    }
  }
  auto padded_input = at::constant_pad_nd(input, pad_nd, 0);
  DimVector output_padding(static_cast<size_t>(dim));
  return at::convolution(padded_input, weight, bias, stride, padding_l,
                         dilation, false, output_padding, groups);
}

Tensor _convolution_mode(
    const Tensor& input, const Tensor& weight, const c10::optional<Tensor>& bias_opt,
    IntArrayRef stride, c10::string_view padding, IntArrayRef dilation,
    int64_t groups) {
  // See [Note: hacky wrapper removal for optional tensor]
  c10::MaybeOwned<Tensor> bias_maybe_owned = at::borrow_from_optional_tensor(bias_opt);
  const Tensor& bias = *bias_maybe_owned;

  if (padding == "same") {
    return at::native::convolution_same(
        input, weight, bias, stride, dilation, groups);
  } else if (padding == "valid") {
    // NOLINTNEXTLINE(modernize-avoid-c-arrays,cppcoreguidelines-avoid-c-arrays)
    const int64_t padding_[] = {0};
    return at::native::convolution(
        input, weight, bias, stride, padding_, dilation, false, padding_, groups);
  }
  TORCH_CHECK(false, "Invalid padding string: '", padding, "'");
}

at::Tensor conv1d(
    const Tensor& input, const Tensor& weight, const c10::optional<Tensor>& bias,
    IntArrayRef stride, c10::string_view padding, IntArrayRef dilation,
    int64_t groups) {
  return at::_convolution_mode(
      input, weight, bias, stride, std::move(padding), dilation, groups);
}

at::Tensor conv2d(
    const Tensor& input, const Tensor& weight, const c10::optional<Tensor>& bias,
    IntArrayRef stride, c10::string_view padding, IntArrayRef dilation,
    int64_t groups) {
  return at::_convolution_mode(
      input, weight, bias, stride, std::move(padding), dilation, groups);
}

at::Tensor conv3d(
    const Tensor& input, const Tensor& weight, const c10::optional<Tensor>& bias,
    IntArrayRef stride, c10::string_view padding, IntArrayRef dilation,
    int64_t groups) {
  return at::_convolution_mode(
      input, weight, bias, stride, std::move(padding), dilation, groups);
}

at::Tensor conv_transpose1d(
    const Tensor& input, const Tensor& weight, const c10::optional<Tensor>& bias_opt,
    IntArrayRef stride, IntArrayRef padding, IntArrayRef output_padding, int64_t groups, IntArrayRef dilation) {
  // See [Note: hacky wrapper removal for optional tensor]
  c10::MaybeOwned<Tensor> bias_maybe_owned = at::borrow_from_optional_tensor(bias_opt);
  const Tensor& bias = *bias_maybe_owned;

  return at::convolution(input, weight, bias, stride, padding, dilation,
                         true, output_padding, groups);
}

at::Tensor conv_transpose2d(
    const Tensor& input, const Tensor& weight, const c10::optional<Tensor>& bias_opt,
    IntArrayRef stride, IntArrayRef padding, IntArrayRef output_padding, int64_t groups, IntArrayRef dilation) {
  // See [Note: hacky wrapper removal for optional tensor]
  c10::MaybeOwned<Tensor> bias_maybe_owned = at::borrow_from_optional_tensor(bias_opt);
  const Tensor& bias = *bias_maybe_owned;

  return at::convolution(input, weight, bias, stride, padding, dilation,
                         true, output_padding, groups);
}

at::Tensor conv_transpose3d(
    const Tensor& input, const Tensor& weight, const c10::optional<Tensor>& bias_opt,
    IntArrayRef stride, IntArrayRef padding, IntArrayRef output_padding, int64_t groups, IntArrayRef dilation) {
  // See [Note: hacky wrapper removal for optional tensor]
  c10::MaybeOwned<Tensor> bias_maybe_owned = at::borrow_from_optional_tensor(bias_opt);
  const Tensor& bias = *bias_maybe_owned;

  return at::convolution(input, weight, bias, stride, padding, dilation,
                         true, output_padding, groups);
}

at::Tensor convolution(
    const Tensor& input, const Tensor& weight, const c10::optional<Tensor>& bias_opt,
    IntArrayRef stride, IntArrayRef padding, IntArrayRef dilation,
    bool transposed, IntArrayRef output_padding, int64_t groups) {
  // See [Note: hacky wrapper removal for optional tensor]
  c10::MaybeOwned<Tensor> bias_maybe_owned = at::borrow_from_optional_tensor(bias_opt);
  const Tensor& bias = *bias_maybe_owned;

  auto& ctx = at::globalContext();
  // See Note [Enabling Deterministic Operations]
  bool deterministic = ctx.deterministicCuDNN() || ctx.deterministicAlgorithms();
  return at::_convolution(input, weight, bias, stride, padding, dilation,
                          transposed, output_padding, groups,
                          ctx.benchmarkCuDNN(), deterministic, ctx.userEnabledCuDNN(), ctx.allowTF32CuDNN());
}

at::Tensor convolution_overrideable(
    const Tensor& input, const Tensor& weight, const c10::optional<Tensor>& bias_opt,
    IntArrayRef stride, IntArrayRef padding, IntArrayRef dilation,
    bool transposed, IntArrayRef output_padding, int64_t groups) {
  // See [Note: hacky wrapper removal for optional tensor]
  c10::MaybeOwned<Tensor> bias_maybe_owned = at::borrow_from_optional_tensor(bias_opt);

  TORCH_CHECK_NOT_IMPLEMENTED(false, "convolution_overrideable not implemented. You are likely triggering this with tensor backend other than CPU/CUDA/MKLDNN, if this is intended, please use TORCH_LIBRARY_IMPL to override this function ");
}

// Selects a backend for convolution based on the inputs and params.
ConvBackend select_conv_backend(
    const Tensor& input_r, const Tensor& weight_r, const c10::optional<Tensor>& bias_opt,
    IntArrayRef stride_, IntArrayRef padding_, IntArrayRef dilation_,
    bool transposed_, IntArrayRef output_padding_, int64_t groups_) {
  c10::MaybeOwned<Tensor> bias_maybe_owned = at::borrow_from_optional_tensor(bias_opt);
  const Tensor& bias = *bias_maybe_owned;

  auto& ctx = at::globalContext();
  auto k = weight_r.ndimension();
  int64_t dim = k - 2;
  ConvParams params;
  params.stride = expand_param_if_needed(stride_, "stride", dim);
  params.padding = expand_param_if_needed(padding_, "padding", dim);
  params.dilation = expand_param_if_needed(dilation_, "dilation", dim);
  params.transposed = transposed_;
  params.output_padding = expand_param_if_needed(output_padding_, "output_padding", dim);
  params.groups = groups_;
  params.benchmark = ctx.benchmarkCuDNN();
  params.deterministic = ctx.deterministicCuDNN() || ctx.deterministicAlgorithms();
  params.cudnn_enabled = ctx.userEnabledCuDNN();
  params.allow_tf32 = ctx.allowTF32CuDNN();

  auto input = input_r;
  auto weight = weight_r;
  check_shape_forward(input, weight.sizes(), bias, params);

  // Expand 1d -> 2d.
  // This is only done for backends that don't natively support 1d spatial input.
  if (k == 3 && !input.is_mkldnn()) {
    // avoid accidentally going through NHWC for permuted 3d input.
    input = input.contiguous();
    params.view1d_as_2d();
    input = view4d(input);
    weight = view4d(weight);
  }

  return select_conv_backend(input, weight, bias, params);
}

ConvBackend select_conv_backend(
    const Tensor& input,
    const Tensor& weight,
    const Tensor& bias,
    const ConvParams& params) {

  // don't send empty inputs through backends
  if (input.size(0) == 0 || input.size(1) == 0) {
    if (input.is_mkldnn() && weight.is_mkldnn()) {
      return ConvBackend::MkldnnEmpty;
    } else {
      return ConvBackend::Empty;
    }
  }

  if (params.is_depthwise(input, weight)) {
    if (params.use_cudnn_depthwise(input, weight)) {
      return ConvBackend::Cudnn;
    } else if (params.use_miopen(input, weight, bias.defined())){
      return ConvBackend::MiopenDepthwise;
    } else {
      if (input.ndimension() == 4) {
        return ConvBackend::CudaDepthwise2d;
      } else if (input.ndimension() == 5) {
        return ConvBackend::CudaDepthwise3d;
      } else {
        // unsupported
      }
    }
  } else if (params.use_cudnn(input, weight)) {
    if (params.transposed) {
      return ConvBackend::CudnnTranspose;
    } else {
      return ConvBackend::Cudnn;
    }
  } else if (params.use_miopen(input, weight, bias.defined())) {
    if (params.transposed) {
      return ConvBackend::MiopenTranspose;
    } else {
      return ConvBackend::Miopen;
    }
  } else if (params.use_mkldnn(input, weight)) {
    return ConvBackend::Mkldnn;
  } else if (params.use_xnnpack(input, weight, bias)) {
    // Using prepacked conv is preferred, but XNNPACK is still the fastest
    // option for NHWC.
    return ConvBackend::Xnnpack2d;
  } else if (params.use_cpu_depthwise3x3_winograd(input, weight, bias)) {
    return ConvBackend::Winograd3x3Depthwise;
  } else if (
      !params.transposed && (input.ndimension() == 5) &&
      (input.device().is_cpu()) &&
      !params.is_dilated()) {
    // fast path for grouped conv3d
    return ConvBackend::Slow3d;
  } else if (input.device().is_cpu() || input.is_cuda()) {
    // backends without support for groups
    if (params.transposed) {
      if (input.ndimension() == 4) {
        return ConvBackend::SlowTranspose2d;
      } else if (input.ndimension() == 5) {
        return ConvBackend::SlowTranspose3d;
      } else {
        // unsupported
      }
    } else {  /* Not transposed */
      if (input.ndimension() == 4) {
        if (params.is_dilated()) {
          return ConvBackend::SlowDilated2d;
        } else {  /* dim == 4, non-dilated */
          if (params.use_nnpack(input, weight)) {
            return ConvBackend::NnpackSpatial;
          } else {
            /* CPU implementation has specialized MM kernels
               for non-dilated case here */
            return ConvBackend::Slow2d;
          }
        }
      } else if (input.ndimension() == 5 && (input.is_cuda() || params.is_dilated())) {
        return ConvBackend::SlowDilated3d;
      } else if (input.ndimension() == 5) { /* dim == 5, CPU, non-dilated */
        /* CPU implementation has specialized MM kernels
           for non-dilated case here */
        return ConvBackend::Slow3d;
      } else {
        // unsupported
      }
    }
  } else {
    // Only reach here when input is backend with out-of-source implementation.
    return ConvBackend::Overrideable;
  }

  // Error out if no suitable backend was found.
  AT_ERROR("unsupported ConvNd parameters");
}

at::Tensor _convolution_nogroup_backend(
    const Tensor& input,
    const Tensor& weight,
    const Tensor& bias,
    const ConvBackend backend,
    const ConvParams& params) {
  auto kernel_size = weight.sizes().slice(2);
  switch(backend) {
    case ConvBackend::NnpackSpatial:
#if AT_NNPACK_ENABLED()
      return at::_nnpack_spatial_convolution(input, weight, bias, params.padding, params.stride);
#else
      TORCH_INTERNAL_ASSERT(false, "NnpackSpatial backend was selected in PyTorch compiled without nnpack support");
#endif
    case ConvBackend::Slow2d:
      return at::thnn_conv2d(input, weight, kernel_size, bias, params.stride, params.padding);
    case ConvBackend::SlowDilated2d:
      return at::slow_conv_dilated2d(
          input, weight, kernel_size, bias, params.stride, params.padding, params.dilation);
    case ConvBackend::SlowDilated3d:
      return at::slow_conv_dilated3d(
          input, weight, kernel_size, bias, params.stride, params.padding, params.dilation);
    case ConvBackend::SlowTranspose2d:
      return at::slow_conv_transpose2d(
          input, weight, kernel_size, bias, params.stride, params.padding, params.output_padding, params.dilation);
    case ConvBackend::SlowTranspose3d:
      return at::slow_conv_transpose3d(
          input, weight, kernel_size, bias, params.stride, params.padding, params.output_padding, params.dilation);
    default:
      TORCH_CHECK(false, "Unsupported conv nogroup backend encountered");
  }
}

static inline std::vector<int64_t> calc_output_size(
    const Tensor& input,
    const Tensor& weight,
    const ConvParams& params) {
  std::vector<int64_t> output_size = params.transposed ?
    conv_input_size(input.sizes(), weight.sizes(), params.padding, params.output_padding,
        params.stride, params.dilation, params.groups) :
    conv_output_size(input.sizes(), weight.sizes(), params.padding, params.stride, params.dilation);

  // Handle empty # of channels.
  if (input.size(1) == 0) {
    output_size[input_channels_dim] = 0;
  }
  return output_size;
}

at::Tensor _convolution(
    const Tensor& input_r, const Tensor& weight_r, const c10::optional<Tensor>& bias_r_opt,
    IntArrayRef stride_, IntArrayRef padding_, IntArrayRef dilation_,
    bool transposed_, IntArrayRef output_padding_, int64_t groups_,
    bool benchmark, bool deterministic, bool cudnn_enabled, bool allow_tf32) {
  // See [Note: hacky wrapper removal for optional tensor]
  c10::MaybeOwned<Tensor> bias_r_maybe_owned = at::borrow_from_optional_tensor(bias_r_opt);
  const Tensor& bias_r = *bias_r_maybe_owned;

  auto input = input_r;
  auto weight = weight_r;
  auto bias = bias_r;
  auto k = weight.ndimension();
  c10::IntArrayRef weight_sizes = weight.sizes();
  int64_t dim = k - 2;

  TORCH_CHECK(dim > 0, "weight should have at least three dimensions");

  ConvParams params;
  params.stride = expand_param_if_needed(stride_, "stride", dim);
  params.padding = expand_param_if_needed(padding_, "padding", dim);
  params.dilation = expand_param_if_needed(dilation_, "dilation", dim);
  params.transposed = transposed_;
  params.output_padding = expand_param_if_needed(output_padding_, "output_padding", dim);
  params.groups = groups_;
  params.benchmark = benchmark;
  params.deterministic = deterministic;
  params.cudnn_enabled = cudnn_enabled;
  params.allow_tf32 = allow_tf32;

  check_shape_forward(input, weight_sizes, bias, params);

  // Expand 1d -> 2d.
  // This is only done for backends that don't natively support 1d spatial input.
  if (k == 3 && !input.is_mkldnn()) {
    // avoid accidentally going through NHWC for permuted 3d input.
    input = input.contiguous();
    params.view1d_as_2d();
    input = view4d(input);
    weight = view4d(weight);
  }

  // Select appropriate backend to use.
  ConvBackend backend = select_conv_backend(input, weight, bias, params);

  at::MemoryFormat backend_memory_format = at::MemoryFormat::Contiguous;
#if !defined(C10_MOBILE)
  // cudnn and miopen are guaranteed not to be on mobile, and T102591915
  // suggests that maybe the cudnn condition sometimes segfaults (though
  // I can't imagine how)
  if (detail::getCUDAHooks().compiledWithCuDNN()) {
    backend_memory_format = cudnn_conv_suggest_memory_format(input, weight);
  }
  if (detail::getCUDAHooks().compiledWithMIOpen() && miopen_conv_use_channels_last(input, weight)) {
    backend_memory_format = (k == 5) ? at::MemoryFormat::Contiguous /*at::MemoryFormat::ChannelsLast3d*/ : at::MemoryFormat::ChannelsLast;
  }
#endif

  // Call the backend.
  Tensor output;
  auto kernel_size = weight.sizes().slice(2);
  switch (backend) {
    case ConvBackend::CudaDepthwise2d:
      output = at::_conv_depthwise2d(input.contiguous(), weight, kernel_size, bias,
          params.stride, params.padding, params.dilation);
      break;
    case ConvBackend::CudaDepthwise3d:
      output = at::conv_depthwise3d(input.contiguous(), weight, kernel_size, bias,
          params.stride, params.padding, params.dilation);
      break;
    case ConvBackend::Cudnn:
      check_input_same_type_as_parameters(input, weight, bias);
      output = at::cudnn_convolution(
          input.contiguous(backend_memory_format), weight, params.padding, params.stride,
          params.dilation, params.groups, params.benchmark, params.deterministic, params.allow_tf32);
      if (bias.defined()) {
        output.add_(reshape_bias(input.dim(), bias));
      }
      break;
    case ConvBackend::CudnnTranspose:
      check_input_same_type_as_parameters(input, weight, bias);
      output = at::cudnn_convolution_transpose(
          input.contiguous(backend_memory_format), weight, params.padding, params.output_padding,
          params.stride, params.dilation, params.groups, params.benchmark, params.deterministic, params.allow_tf32);
      if (bias.defined()) {
        output.add_(reshape_bias(input.dim(), bias));
      }
      break;
    case ConvBackend::Empty:
    {
      auto weight_view = at::_unsafe_view(weight, -1);
      output = (input.size(1) == 0) ? (input.view(-1) * weight_view) : (input * weight_view[0]);
      if (bias.defined()) {
        output.add_(bias[0]);
      }
      output = output.view(calc_output_size(input, weight, params));
      break;
    }
    case ConvBackend::Miopen:
      check_input_same_type_as_parameters(input, weight, bias);
      output = at::miopen_convolution(
          input.contiguous(backend_memory_format), weight, bias, params.padding, params.stride,
          params.dilation, params.groups, params.benchmark, params.deterministic);
      break;
    case ConvBackend::MiopenDepthwise:
      output = at::miopen_depthwise_convolution(
          input.contiguous(backend_memory_format), weight, bias, params.padding, params.stride,
          params.dilation, params.groups, params.benchmark, params.deterministic);
      break;
    case ConvBackend::MiopenTranspose:
      check_input_same_type_as_parameters(input, weight, bias);
      output = at::miopen_convolution_transpose(
          input.contiguous(backend_memory_format), weight, bias, params.padding, params.output_padding,
          params.stride, params.dilation, params.groups, params.benchmark, params.deterministic);
      break;
    case ConvBackend::Mkldnn:
#if AT_MKLDNN_ENABLED()
      TORCH_CHECK(input.options().type_equal(weight.options())
          || (input.is_mkldnn() && weight.device().is_cpu() && weight.scalar_type() == kFloat),
          "Input type (", input.toString(), ") and weight type (", weight.toString(),
          ") should be the same or input should be a MKLDNN tensor and weight is a dense tensor");
      TORCH_CHECK(!bias.defined() || (input.options().type_equal(bias.options()))
          || (input.is_mkldnn() && bias.device().is_cpu() && bias.scalar_type() == kFloat),
          "Input type (", input.toString(), ") and bias type (", bias.toString(),
          ") should be the same or input should be a MKLDNN tensor and bias is a dense tensor");
      if (!input.is_mkldnn()) {
        // need to ensure contiguous for non-mkldnn tensors
        input = input.contiguous();
        weight = weight.contiguous();
        bias = bias.defined() ? bias.contiguous() : bias;
      }
      output = at::mkldnn_convolution(
          input, weight, bias, params.padding, params.stride, params.dilation, params.groups);
#else
      TORCH_INTERNAL_ASSERT(false, "Mkldnn backend was selected in PyTorch compiled without mkldnn support");
#endif
      break;
    case ConvBackend::MkldnnEmpty:
#if AT_MKLDNN_ENABLED()
      output = empty_mkldnn(
          calc_output_size(input, weight, params), optTypeMetaToScalarType(input.options().dtype_opt()),
          input.options().layout_opt(), input.options().device_opt(), input.options().pinned_memory_opt());
#else
      TORCH_INTERNAL_ASSERT(false, "Mkldnn backend was selected in PyTorch compiled without mkldnn support");
#endif
      break;
    case ConvBackend::Overrideable:
      output = at::convolution_overrideable(
          input, weight, bias, params.stride, params.padding, params.dilation, params.transposed,
          params.output_padding, params.groups);
      break;
    case ConvBackend::Slow3d:
      output = at::slow_conv3d(input, weight, kernel_size, bias, params.stride, params.padding);
      break;
    case ConvBackend::Winograd3x3Depthwise:
      output = convolution_depthwise3x3_winograd_stub(
          input.device().type(), input, weight, bias, params.stride, params.padding, params.groups);
      break;
    case ConvBackend::Xnnpack2d:
      output = xnnpack::convolution2d(
          input, weight, bias, params.padding, params.stride, params.dilation, params.groups);
      break;
    // Handle backends that don't natively support groups > 1.
    case ConvBackend::NnpackSpatial:
    case ConvBackend::Slow2d:
    case ConvBackend::SlowDilated2d:
    case ConvBackend::SlowDilated3d:
    case ConvBackend::SlowTranspose2d:
    case ConvBackend::SlowTranspose3d:
      if (params.groups == 1) {
        output = _convolution_nogroup_backend(input.contiguous(), weight, bias, backend, params);
      } else {
        std::vector<Tensor> outputs(params.groups);
        input = input.contiguous();
        for (const auto g : c10::irange(params.groups)) {
          auto input_g = subtensor(input, 1, params.groups, g);
          auto weight_g = subtensor(weight, 0, params.groups, g);
          auto bias_g = subtensor(bias, 0, params.groups, g);
          outputs[g] = _convolution_nogroup_backend(input_g, weight_g, bias_g, backend, params);
        }
        output = at::cat(outputs, 1);
      }
      break;
  }

  if (k == 3 && !input.is_mkldnn()) {
    output = view3d(output);
  }

  return output;
}

at::Tensor _convolution(
    const Tensor& input_r, const Tensor& weight_r, const c10::optional<Tensor>& bias_r_opt,
    IntArrayRef stride_, IntArrayRef padding_, IntArrayRef dilation_,
    bool transposed_, IntArrayRef output_padding_, int64_t groups_,
    bool benchmark, bool deterministic, bool cudnn_enabled)
{
  // See [Note: hacky wrapper removal for optional tensor]
  c10::MaybeOwned<Tensor> bias_r_maybe_owned = at::borrow_from_optional_tensor(bias_r_opt);
  const Tensor& bias_r = *bias_r_maybe_owned;

  return at::_convolution(input_r, weight_r, bias_r, stride_, padding_, dilation_, transposed_, output_padding_, groups_, benchmark, deterministic, cudnn_enabled, at::globalContext().allowTF32CuDNN());
}

std::tuple<Tensor, Tensor, Tensor> convolution_backward_overrideable(
        const Tensor& grad_output, const Tensor& input, const Tensor& weight,
        IntArrayRef stride, IntArrayRef padding, IntArrayRef dilation,
        bool transposed, IntArrayRef output_padding, int64_t groups, std::array<bool, 3> output_mask) {
  AT_ERROR("You are likely triggering this with tensor backend other than CPU/CUDA/MKLDNN, if this is intended, please use TORCH_LIBRARY_IMPL to override this function ");
  return std::tuple<Tensor, Tensor, Tensor>(
          at::empty_like(input, LEGACY_CONTIGUOUS_MEMORY_FORMAT),
          at::empty_like(weight, LEGACY_CONTIGUOUS_MEMORY_FORMAT),
          at::empty({}));
}

static Tensor subvariable(const Tensor& var, int dim, int groups, int g) {
  int64_t n = var.sizes()[dim] / groups;
  auto result = var.narrow(dim, n * g, n);
  return result;
}

std::tuple<Tensor,Tensor,Tensor> _convolution_double_backward( const c10::optional<Tensor>& ggI_opt, const c10::optional<Tensor>& ggW_r_opt, const c10::optional<Tensor>& ggb_opt,
    const Tensor& gO_r, const Tensor& weight_r, const Tensor& input,
    IntArrayRef stride_, IntArrayRef padding_, IntArrayRef dilation_,
    bool transposed_, IntArrayRef output_padding_, int64_t groups_,
    bool benchmark, bool deterministic, bool cudnn_enabled, bool allow_tf32,
    std::array<bool, 3> output_mask) {
  // See [Note: hacky wrapper removal for optional tensor]
  c10::MaybeOwned<Tensor> ggI_maybe_owned = at::borrow_from_optional_tensor(ggI_opt);
  const Tensor& ggI = *ggI_maybe_owned;
  const Tensor& ggW_r = c10::value_or_else(ggW_r_opt, [] {return Tensor();});
  const Tensor& ggb = c10::value_or_else(ggb_opt, [] {return Tensor();});


  auto ggW = ggW_r;
  auto gO = gO_r;
  auto weight = weight_r;

  ConvParams params;
  params.stride = stride_.vec();
  params.padding = padding_.vec();
  params.dilation = dilation_.vec();
  params.transposed = transposed_;
  params.output_padding = output_padding_.vec();
  // TODO: hacky way of inferring the groups number for grouped Conv3D
  // See: https://github.com/pytorch/pytorch/pull/36355
  if (!params.transposed && input.dim() > 4) {
    // NOLINTNEXTLINE(cppcoreguidelines-narrowing-conversions,bugprone-narrowing-conversions)
    params.groups = input.size(1) / weight.size(1);
  } else {
    params.groups = groups_;
  }
  params.benchmark = benchmark;
  params.deterministic = deterministic;
  params.cudnn_enabled = cudnn_enabled;
  params.allow_tf32 = allow_tf32;

  // Compute ggO = conv(ggI, w) + conv(i, ggW) + ggb
  Tensor ggO;
  if (input.numel() != 0) {
    if (ggI.defined()) {
      if (weight.is_cuda()) {
        weight = weight.contiguous();
      }
      ggO = at::_convolution(ggI, weight, Tensor(), params.stride, params.padding, params.dilation, params.transposed, params.output_padding, params.groups, params.benchmark, params.deterministic, params.cudnn_enabled, params.allow_tf32);
    }

    if (ggW.defined()) {
      if (ggW.is_cuda()) {
        ggW = ggW.contiguous();
      }
      auto ggW_term = at::_convolution(input, ggW, Tensor(), params.stride, params.padding, params.dilation, params.transposed, params.output_padding, params.groups, params.benchmark, params.deterministic, params.cudnn_enabled, params.allow_tf32);
      if (ggO.defined()) {
        ggO = ggO + ggW_term;
      } else {
        ggO = ggW_term;
      }
    }
  }

  if (ggb.defined()) {
    // View as (1, ggb.size(0), 1, 1...)

    // Expand
    std::vector<int64_t> new_size(gO.ndimension(), 1);
    new_size[1] = ggb.sizes()[0];
    auto ggb_contiguous = ggb.contiguous();
    auto ggb_view = ggb_contiguous.view(new_size);

    // Expand
    auto ggb_expanded = ggb_view.expand(gO.sizes());

    if (ggO.defined()) {
      ggO = ggO + ggb_expanded;
    } else {
      ggO = ggb_expanded;
    }
  }

  // Compute gW = conv(ggI, gO)
  Tensor gW;
  if (ggI.defined()) {

    // Modified params with correct padding
    ConvParams gw_conv_params(params);

    // Disable groups as they are handled separately
    auto groups = gw_conv_params.groups;
    gw_conv_params.groups = 1;
    std::swap(gw_conv_params.dilation, gw_conv_params.stride);

    // Transpose gO and ggI to accumulate over batch
    auto gOt = gO.transpose(0, 1);
    auto ggIt = ggI.transpose(0, 1);

    Tensor gWt;
    // Compute conv
    if (input.numel() != 0) {
      if (groups == 1) {

        if (gOt.is_cuda()) {
          gOt = gOt.contiguous();
        }
        // Compute conv
        if (params.transposed) {
          gw_conv_params.transposed = false;
          gWt = at::_convolution(gOt, ggIt, Tensor(), gw_conv_params.stride, gw_conv_params.padding, gw_conv_params.dilation, gw_conv_params.transposed, gw_conv_params.output_padding, gw_conv_params.groups, gw_conv_params.benchmark, gw_conv_params.deterministic, gw_conv_params.cudnn_enabled, params.allow_tf32);
        } else {
          gWt = at::_convolution(ggIt, gOt, Tensor(), gw_conv_params.stride, gw_conv_params.padding, gw_conv_params.dilation, gw_conv_params.transposed, gw_conv_params.output_padding, gw_conv_params.groups, gw_conv_params.benchmark, gw_conv_params.deterministic, gw_conv_params.cudnn_enabled, params.allow_tf32);
        }
      } else {
        std::vector<Tensor> gWt_list(groups);
        for (const auto g : c10::irange(groups)) {
          auto ggIt_g = subvariable(ggIt, 0, groups, g);
          auto gOt_g = subvariable(gOt, 0, groups, g);
          if (gOt_g.is_cuda()) {
            gOt_g = gOt_g.contiguous();
          }

          // Compute conv
          if (params.transposed) {
            gw_conv_params.transposed = false;
            gWt_list[g] = at::_convolution(gOt_g, ggIt_g, Tensor(), gw_conv_params.stride, gw_conv_params.padding, gw_conv_params.dilation, gw_conv_params.transposed, gw_conv_params.output_padding, gw_conv_params.groups, gw_conv_params.benchmark, gw_conv_params.deterministic, gw_conv_params.cudnn_enabled, params.allow_tf32);
          } else {
            gWt_list[g] = at::_convolution(ggIt_g, gOt_g, Tensor(), gw_conv_params.stride, gw_conv_params.padding, gw_conv_params.dilation, gw_conv_params.transposed, gw_conv_params.output_padding, gw_conv_params.groups, gw_conv_params.benchmark, gw_conv_params.deterministic, gw_conv_params.cudnn_enabled, params.allow_tf32);
          }
        }

        gWt = at::cat(gWt_list, 1);
      }

      // Transpose gW to match chan_in and chan_out
      gW = gWt.transpose(0, 1);

      // narrow gW to only relevant portion
      // we do it this way instead of narrowing the input itself because
      // the ConvForward kernels don't support asymmetric padding.
      auto gW_size = gW.sizes();
      auto w_size = weight.sizes();
      for (const auto i : c10::irange(2, gW_size.size())) {
        if (gW_size[i] > w_size[i]) {
            gW = gW.narrow(i, 0, w_size[i]);
            gW_size = gW.sizes();
        }
      }
    }
  }

  // Compute gI = convT(gO, ggW) if !transposed
  //         gI = conv(gO, ggw)  if transposed
  Tensor gI;
  if (input.numel() != 0) {
    if (ggW.defined()) {
      ConvParams gi_conv_params(params);
      gi_conv_params.transposed = !params.transposed;

      if (params.transposed) {
        if (gO.is_cuda()) {
          gO = gO.contiguous();
        }
        gI = at::_convolution(gO, ggW, Tensor(), gi_conv_params.stride, gi_conv_params.padding, gi_conv_params.dilation, gi_conv_params.transposed, gi_conv_params.output_padding, gi_conv_params.groups, gi_conv_params.benchmark, gi_conv_params.deterministic, gi_conv_params.cudnn_enabled, params.allow_tf32);

        // narrow gI to only relevant portion
        // we do it this way because negative output_padding is not supported
        // TODO: figure out if we can narrow gO and save some compute,
        // rather than narrowing the computed gI
        auto gI_size = gI.sizes();
        auto i_size = input.sizes();
        for (const auto i : c10::irange(2, gI_size.size())) {
          if (gI_size[i] > i_size[i]) {
            gI = gI.narrow(i, 0, i_size[i]);
            gI_size = gI.sizes();
          }
        }
      } else {
        // calculate output_padding
        // TODO: figure out why this needs to be computed...
        auto kernel_size = weight.sizes().slice(2);
        auto input_shape = input.sizes().slice(2);
        auto grad_output_shape = gO.sizes().slice(2);

        for (const auto i : c10::irange(kernel_size.size())) {
          // Check if whole input has been used or not
          auto expected_input_shape = (kernel_size[i] - 1) * gi_conv_params.dilation[i]
            - 2 * gi_conv_params.padding[i]
            + (gi_conv_params.stride[i] * (grad_output_shape[i] - 1) + 1);
          if (expected_input_shape != input_shape[i]) {
            gi_conv_params.output_padding[i] = input_shape[i] - expected_input_shape;
          }
        }

        if (gO.is_cuda()) {
          gO = gO.contiguous();
        }
        gI = at::_convolution(gO, ggW, Tensor(), gi_conv_params.stride, gi_conv_params.padding, gi_conv_params.dilation, gi_conv_params.transposed, gi_conv_params.output_padding, gi_conv_params.groups, gi_conv_params.benchmark, gi_conv_params.deterministic, gi_conv_params.cudnn_enabled, params.allow_tf32);
      }
    }
  }

  return std::tuple<Tensor,Tensor,Tensor>{ggO, gI, gW};
}

}} // at::native
