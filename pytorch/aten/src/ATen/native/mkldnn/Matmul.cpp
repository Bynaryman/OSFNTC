#include <ATen/ATen.h>
#include <ATen/Config.h>
#include <ATen/NativeFunctions.h>
#include <ATen/native/mkldnn/Matmul.h>
#if !AT_MKLDNN_ENABLED()

namespace at {
namespace native {

void mkldnn_matmul(
    const Tensor &mat1,
    const Tensor &mat2,
    const Tensor &result,
    float beta,
    float alpha) {
  TORCH_CHECK(false, "mkldnn_matmul: ATen not compiled with MKLDNN support");
}

bool use_mkldnn_bf16_matmul(
    const Tensor& mat1,
    const Tensor& mat2,
    const c10::optional<Tensor>& result_opt){
  return false;
}

} // namespace native
} // namespace at

#else // AT_MKLDNN_EBABLED

#include <ATen/native/mkldnn/MKLDNNCommon.h>
#include <ATen/native/mkldnn/Utils.h>

namespace at {
namespace native {

void mkldnn_matmul(
    const Tensor &mat1,
    const Tensor &mat2,
    const Tensor &result,
    float beta,
    float alpha) {
  TORCH_CHECK((mat1.dim() == 2 && mat2.dim() == 2) || // aten::addmm
              (mat1.dim() == 3 && mat2.dim() == 3) || // aten::bmm, aten::baddbmm
              (mat1.dim() == 2 && mat2.dim() == 1) || // aten::mv
              (mat1.dim() == 1 && mat2.dim() == 1),  // aten::dot
              "mkldnn_matmul:  unsupported dims for mat and mat2");
  TORCH_CHECK(mat1.scalar_type() == at::kBFloat16 &&
   mat2.scalar_type() == at::kBFloat16 &&
   result.scalar_type() == at::kBFloat16, "mkldnn_matmul:  only enabled for bf16 path");
  TORCH_CHECK(mkldnn_bf16_device_check(),
    "mkldnn_matmul: mkldnn_matmul bf16 path needs the cpu support avx512bw, avx512vl and avx512dq");

  auto mat1_unsqueezed = mat1.dim() == 1 ? mat1.unsqueeze(0) : mat1;
  auto mat2_unsqueezed = mat2.dim() == 1 ? mat2.unsqueeze(1) : mat2;
  auto result_unsqueezed = result.dim() == 1 ? result.unsqueeze(1) : result;

  ideep::attr_t op_attr;
  // "addmm", "addbmm" "baddbmm" in pytorch allow bias to be 2-D or 3-D tensor
  // but mkldnn matmul primitive only support bias be 1-D tensors
  // to address their differences, we use mkldnn post ops to perform a fused "add" after matrix multiplication is over
  if (beta != 0.0f) op_attr = ideep::attr_t::fuse_sum();
  // If alpha = 0, dose not need actually do gemm computation
  if (alpha == 0)
    return;

  auto is_mkldnn_optimized_format = [&](const Tensor& t) {
    if (t.is_contiguous()) return true;
    const auto sizes = t.sizes();
    const auto strides = t.strides();
    if (t.dim() == 2){
      return strides[0] == 1 && strides[1] == sizes[0];
    } else {
      // dim = 3
      return strides[0] == sizes[1] * sizes[2] && strides[1] == 1 && strides[2] == sizes[1];
    }
  };

  // Mkldnn only optimized for contiguous or transposed (transpose last 2 dim if 3-D tensor) format now
  // Will remove this "contiguous" after mkldnn have fully supported
  Tensor mat1_ = is_mkldnn_optimized_format(mat1_unsqueezed) ? mat1_unsqueezed : mat1_unsqueezed.contiguous();
  Tensor mat2_ = is_mkldnn_optimized_format(mat2_unsqueezed) ? mat2_unsqueezed : mat2_unsqueezed.contiguous();

  // mkldnn_matmul only proceed CPU tensor
  const ideep::tensor x = itensor_view_from_dense(mat1_);
  const ideep::tensor w = itensor_view_from_dense(mat2_);
  ideep::tensor y = itensor_view_from_dense(result_unsqueezed);
  ideep::matmul_forward::compute(x, w, y, alpha, beta,
      ideep::scale_t(), ideep::scale_t(), ideep::scale_t(), op_attr);
  if (y.get_data_handle() != result.data_ptr()){
    // ideep will query onednn expect format of output
    // if given output format is not expected, ideep will re-init an output buffer
    // under this case, we need copy the re-inited buffer back to given buffer
    ideep::tensor public_y = itensor_view_from_dense(result);
    y.reorder_to(public_y);
  }

  if (mat1.dim() == 1 && mat2.dim() == 1){
    // aten::dot
    result.squeeze_();
  }

}

inline bool checksize(const Tensor& mat1, const Tensor& mat2){
  // if dim = 2, mat1's size = (m * n), mat2's size = (n * k)
  // else if dim = 3, mat1's size = (b * m * n), mat2's size = (b * n * k)
  // else called from aten::mv, mat1.size = (m * n), mat2.size = (n)
  // only m * n * b * k(if exist) are large enough we can get benefit from mkldnn optimized gemm kernel
  static const int64_t mkldnn_gemm_min_size = 16 * 16 * 16;
  if (mat1.dim() == 1 && mat2.dim() == 1) {
    // aten::dot
    return mat1.size(0) > mkldnn_gemm_min_size;
  } else if (mat1.dim() == 2 && mat2.dim() == 1) {
    // aten::mv
    return mat1.size(0) * mat1.size(1) > mkldnn_gemm_min_size;
  } else if (mat2.dim() == 2 && mat2.dim() == 2) {
    // aten::addmm
    return mat1.size(0) * mat1.size(1) * mat2.size(1) > mkldnn_gemm_min_size;
  } else {
    // aten::bmm, aten::baddbmm
    return mat1.size(0) * mat1.size(1) * mat1.size(2) * mat2.size(2) > mkldnn_gemm_min_size;
  }
}

bool use_mkldnn_bf16_matmul(
    const Tensor& mat1,
    const Tensor& mat2,
    const c10::optional<Tensor>& result_opt) {
  c10::MaybeOwned<Tensor> result_maybe_owned = at::borrow_from_optional_tensor(result_opt);
  const Tensor& result = *result_maybe_owned;
  return (
    at::globalContext().userEnabledMkldnn() &&
    mat1.scalar_type() == kBFloat16 &&
    mat2.scalar_type() == kBFloat16 &&
    (!result.defined() || result.scalar_type() == kBFloat16) &&
    mat1.numel() != 0 &&
    mat2.numel() != 0 &&
    mkldnn_bf16_device_check() &&
    checksize(mat1, mat2));
}

} // namespace native
} // namespace at

#endif // AT_MKLDNN_EBABLED
