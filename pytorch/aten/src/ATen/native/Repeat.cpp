#include <ATen/ATen.h>
#include <ATen/Parallel.h>
#include <ATen/native/Repeat.h>
#include <c10/util/irange.h>

template <typename index_t>
static void compute_cpu(
    index_t* repeat_ptr,
    int64_t* cumsum_ptr,
    index_t* result_ptr,
    int64_t size,
    int64_t result_size) {
  TORCH_CHECK(
      (result_size == cumsum_ptr[size - 1]),
      "allocated size does not match required size");
  at::parallel_for(0, size, 1, [&](int64_t i_begin, int64_t i_end) {
    for (const auto i : c10::irange(i_begin, i_end)) {
      int64_t end = cumsum_ptr[i];
      index_t size = repeat_ptr[i];
      TORCH_CHECK((size >= 0), "repeats can not be negative");
      int64_t start = end - size;
      for (const auto j : c10::irange(start, end)) {
        result_ptr[j] = i;
      }
    }
  });
}

namespace at {
namespace native {

Tensor repeat_interleave_cpu(
    const Tensor& repeat,
    c10::optional<int64_t> output_size) {
  Tensor output;
  AT_DISPATCH_INDEX_TYPES(repeat.scalar_type(), "repeat_interleave_cpu", [&]() {
    output = repeat_interleave_common<index_t, compute_cpu<index_t>>(
        repeat, output_size);
  });

  return output;
}

Tensor repeat_interleave(
    const Tensor& self,
    const Tensor& repeats,
    c10::optional<int64_t> dim,
    c10::optional<int64_t> output_size) {
  Tensor input = self;
  if (!dim) {
    input = self.flatten();
    dim = 0;
  }

  Tensor repeats_ = repeats;
  if (repeats.dim() == 0 || (repeats.dim() == 1 && repeats.size(0) == 1)) {
    repeats_ = repeats.reshape({1}).expand({input.size(dim.value())});
  } else if (repeats.dim() == 1) {
    TORCH_CHECK(
        repeats.size(0) == input.size(dim.value()),
        "repeats must have the same size as input along dim")
  } else {
    AT_ERROR("repeats must be 0-dim or 1-dim tensor");
  }

  return input.index_select(
      dim.value(), at::repeat_interleave(repeats_, output_size));
}

Tensor repeat_interleave(
    const Tensor& self,
    int64_t repeats,
    c10::optional<int64_t> dim,
    c10::optional<int64_t> output_size) {
  at::Tensor repeats_ =
      at::empty(1, self.options().dtype(at::kLong)).fill_(repeats);
  return at::native::repeat_interleave(self, repeats_, dim, output_size);
}

} // namespace native
} // namespace at
