#define TORCH_ASSERT_NO_OPERATORS
#include <ATen/AccumulateType.h>
#include <ATen/Context.h>
#include <ATen/Dispatch.h>
#include <ATen/native/cuda/Loops.cuh>
#include <ATen/native/DispatchStub.h>
#include <ATen/native/TensorIterator.h>
#include <ATen/native/PointwiseOps.h>
#include <c10/core/Scalar.h>

namespace at { namespace native {

void addcmul_cuda_kernel(TensorIteratorBase& iter, const Scalar& value) {
  AT_DISPATCH_ALL_TYPES_AND_COMPLEX_AND2(kHalf, kBFloat16, iter.dtype(), "addcmul_cuda", [&]() {
    // note(mkozuki): If scalar_t is fp16 or bfloat16, cast scalar to float
    // and do math in fp32 for better accuracy.
    using accscalar_t = at::acc_type<scalar_t, true>;
    auto alpha = value.to<accscalar_t>();
    gpu_kernel(iter, [alpha]GPU_LAMBDA(scalar_t a, scalar_t b, scalar_t c) -> scalar_t {
      return a + alpha * (static_cast<accscalar_t>(b) * static_cast<accscalar_t>(c));
    });
  });
}

void addcdiv_cuda_kernel(TensorIteratorBase& iter, const Scalar& value) {
  AT_DISPATCH_ALL_TYPES_AND_COMPLEX_AND2(kHalf, kBFloat16, iter.dtype(), "addcdiv_cuda", [&]() {
    // note(mkozuki): If scalar_t is fp16 or bfloat16, cast scalar to float
    // and do math in fp32 for better accuracy.
    using accscalar_t = at::acc_type<scalar_t, true>;
    auto alpha = value.to<accscalar_t>();
    gpu_kernel(iter, [alpha]GPU_LAMBDA(scalar_t a, scalar_t b, scalar_t c) -> scalar_t {
      return a + alpha * (b / static_cast<accscalar_t>(c));
    });
  });
}

void smooth_l1_backward_cuda_kernel(TensorIterator& iter, const Scalar& norm, double beta) {
  AT_DISPATCH_ALL_TYPES_AND(kHalf, iter.dtype(), "smooth_l1_backward_cuda", [&iter, &norm, beta] {
      auto norm_val = norm.to<scalar_t>();
      scalar_t beta_val(beta);
      gpu_kernel(iter, [norm_val, beta_val]GPU_LAMBDA(scalar_t input, scalar_t target, scalar_t grad_output) -> scalar_t {
        const auto x = input - target;
        if (x < -beta_val)
          return -norm_val * grad_output;
        else if (x > beta_val)
          return norm_val * grad_output;
        else
          return norm_val * x * grad_output / beta_val;
    });
  });
}

void huber_backward_cuda_kernel(TensorIterator& iter, const Scalar& norm, double delta) {
  AT_DISPATCH_FLOATING_TYPES_AND2(kBFloat16, kHalf, iter.dtype(), "huber_backward_cuda", [&iter, &norm, delta] {
    auto norm_val = norm.to<scalar_t>();
    scalar_t delta_val(delta);
    gpu_kernel(iter, [norm_val, delta_val]GPU_LAMBDA(scalar_t input, scalar_t target, scalar_t grad_output) -> scalar_t {
      const auto x = input - target;
      if (x < -delta_val) {
        return -norm_val * grad_output * delta_val;
      } else if (x > delta_val) {
        return norm_val * grad_output * delta_val;
      } else {
        return norm_val * x * grad_output;
      }
    });
  });
}

void mse_backward_cuda_kernel(TensorIterator& iter, const Scalar& value) {
  AT_DISPATCH_FLOATING_TYPES_AND2(at::ScalarType::Half, at::ScalarType::BFloat16, iter.dtype(), "mse_backward_cuda", [&]() {
    auto alpha = value.to<scalar_t>();
    gpu_kernel(iter, [alpha]GPU_LAMBDA(scalar_t a, scalar_t b, scalar_t c) -> scalar_t {
      return alpha * (a - b) * c;
    });
  });
}

REGISTER_DISPATCH(addcdiv_stub, &addcdiv_cuda_kernel);
REGISTER_DISPATCH(addcmul_stub, &addcmul_cuda_kernel);
REGISTER_DISPATCH(smooth_l1_backward_stub, &smooth_l1_backward_cuda_kernel);
REGISTER_DISPATCH(huber_backward_stub, &huber_backward_cuda_kernel);
REGISTER_DISPATCH(mse_backward_stub, &mse_backward_cuda_kernel);
}} // namespace at::native
