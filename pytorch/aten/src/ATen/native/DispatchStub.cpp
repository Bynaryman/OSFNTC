#include <ATen/native/DispatchStub.h>

#include <c10/util/Exception.h>

#include <cpuinfo.h>
#include <cstdlib>
#include <cstring>

namespace at { namespace native {

static CPUCapability compute_cpu_capability() {
  auto envar = std::getenv("ATEN_CPU_CAPABILITY");
  if (envar) {
#ifdef HAVE_VSX_CPU_DEFINITION
    if (strcmp(envar, "vsx") == 0) {
      return CPUCapability::VSX;
    }
#else
#ifdef HAVE_AVX512_CPU_DEFINITION
    if (strcmp(envar, "avx512") == 0) {
      return CPUCapability::AVX512;
    }
#endif
#ifdef HAVE_AVX2_CPU_DEFINITION
    if (strcmp(envar, "avx2") == 0) {
      return CPUCapability::AVX2;
    }
#endif
#endif
    if (strcmp(envar, "default") == 0) {
      return CPUCapability::DEFAULT;
    }
    TORCH_WARN("ignoring invalid value for ATEN_CPU_CAPABILITY: ", envar);
  }

#if !defined(__powerpc__) && !defined(__s390x__)
  if (cpuinfo_initialize()) {
#ifdef HAVE_AVX512_CPU_DEFINITION
    // GCC supports some AVX512 intrinsics such as _mm512_set_epi16 only in
    // versions 9 & beyond. So, we want to ensure that only releases built with
    // supported compilers on supported hardware return CPU Capability AVX512,
    // if it's supported on the hardware PyTorch is running on.
    if (cpuinfo_has_x86_avx512vl() && cpuinfo_has_x86_avx512bw() &&  \
        cpuinfo_has_x86_avx512dq() && cpuinfo_has_x86_fma3()) {
      return CPUCapability::AVX512;
    }
#endif
#ifdef HAVE_AVX2_CPU_DEFINITION
    if (cpuinfo_has_x86_avx2() && cpuinfo_has_x86_fma3()) {
      return CPUCapability::AVX2;
    }
#endif
  }
#endif
#ifdef HAVE_VSX_CPU_DEFINITION
  return CPUCapability::VSX;
#else
  return CPUCapability::DEFAULT;
#endif
}

CPUCapability get_cpu_capability() {
  static CPUCapability capability = compute_cpu_capability();
  return capability;
}

void* DispatchStubImpl::get_call_ptr(
  DeviceType device_type
  , void *DEFAULT
#ifdef HAVE_AVX512_CPU_DEFINITION
  , void *AVX512
#endif
#ifdef HAVE_AVX2_CPU_DEFINITION
  , void *AVX2
#endif
#ifdef HAVE_VSX_CPU_DEFINITION
  , void *VSX
#endif
) {
  switch (device_type) {
    case DeviceType::CPU: {
      // Use memory_order_relaxed here since even if two threads race,
      // they will still compute the same value for cpu_dispatch_ptr.
      auto fptr = cpu_dispatch_ptr.load(std::memory_order_relaxed);
      if (!fptr) {
        fptr = choose_cpu_impl(
          DEFAULT
#ifdef HAVE_AVX512_CPU_DEFINITION
          , AVX512
#endif
#ifdef HAVE_AVX2_CPU_DEFINITION
          , AVX2
#endif
#ifdef HAVE_VSX_CPU_DEFINITION
          , VSX
#endif
        );
        cpu_dispatch_ptr.store(fptr, std::memory_order_relaxed);
      }
      return fptr;
    }

    case DeviceType::CUDA:
      TORCH_INTERNAL_ASSERT(cuda_dispatch_ptr, "DispatchStub: missing CUDA kernel");
      return cuda_dispatch_ptr;

    case DeviceType::HIP:
      TORCH_INTERNAL_ASSERT(hip_dispatch_ptr, "DispatchStub: missing HIP kernel");
      return hip_dispatch_ptr;

    default:
      AT_ERROR("DispatchStub: unsupported device type", device_type);
  }
}

void* DispatchStubImpl::choose_cpu_impl(
  void *DEFAULT
#ifdef HAVE_AVX512_CPU_DEFINITION
  , void *AVX512
#endif
#ifdef HAVE_AVX2_CPU_DEFINITION
  , void *AVX2
#endif
#ifdef HAVE_VSX_CPU_DEFINITION
  , void *VSX
#endif
) {
  auto capability = static_cast<int>(get_cpu_capability());
  (void)capability;
#ifdef HAVE_AVX512_CPU_DEFINITION
  if (capability >= static_cast<int>(CPUCapability::AVX512)) {
    // Quantization kernels have also been disabled on Windows
    // for AVX512 because some of their tests are flaky on Windows.
    // Ideally, we should have AVX512 kernels for all kernels.
    if (C10_UNLIKELY(!AVX512)) {
      // dispatch to AVX2, since the AVX512 kernel is missing
      TORCH_INTERNAL_ASSERT(AVX2, "DispatchStub: missing AVX2 kernel");
      return AVX2;
    } else {
      return AVX512;
    }
  }
#endif
#ifdef HAVE_AVX2_CPU_DEFINITION
  if (capability >= static_cast<int>(CPUCapability::AVX2)) {
    TORCH_INTERNAL_ASSERT(AVX2, "DispatchStub: missing AVX2 kernel");
    return AVX2;
  }
#endif
#ifdef HAVE_VSX_CPU_DEFINITION
  if (capability >= static_cast<int>(CPUCapability::VSX)) {
    TORCH_INTERNAL_ASSERT(VSX, "DispatchStub: missing VSX kernel");
    return VSX;
  }
#endif
  TORCH_INTERNAL_ASSERT(DEFAULT, "DispatchStub: missing default kernel");
  return DEFAULT;
}

}}  // namespace at::native
