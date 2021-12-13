#include <c10/util/Exception.h>
#include <torch/csrc/jit/mobile/profiler_edge.h>
#include <string>
#include <vector>

namespace torch {
namespace jit {
namespace mobile {

thread_local KinetoEdgeCPUProfiler* tls_edge_profiler{nullptr};

KinetoEdgeCPUProfiler::KinetoEdgeCPUProfiler(
    const torch::jit::mobile::Module& m,
    const std::string& fname,
    const bool report_input_shapes,
    const bool profile_memory,
    const bool with_stack,
    const bool with_flops,
    const bool with_modules)
    : m_(m), trace_file_name_(fname) {
  profiler::ProfilerConfig config(
      profiler::ProfilerState::KINETO,
      report_input_shapes,
      profile_memory,
      with_stack,
      with_flops,
      with_modules);
  profiler::prepareProfiler(config, {profiler::ActivityType::CPU});
  if (with_modules || with_stack) {
    auto post_processing = [this, with_stack, with_modules](
                               std::vector<profiler::KinetoEvent>& events) {
      std::string no_debug_info("Model was not saved with debug information");
      for (auto& e : events) {
        if (with_modules) {
          // Since KinetoEvents's module hierarchy takes vector of strings we
          // just construct a temporary vector using one string element
          if (this->m_.hasDebugHandles()) {
            e.moduleHierarchy(std::vector<std::string>(
                {this->m_.getModuleHierarchy(e.debugHandle())}));
          } else {
            e.moduleHierarchy(std::vector<std::string>({no_debug_info}));
          }
        } else if (with_stack) {
          // Since KinetoEvents's stack trace takes vector of strings we just
          // construct a temporary vector using one string element
          if (this->m_.hasDebugHandles()) {
            e.stack(std::vector<std::string>(
                {this->m_.getCallStack(e.debugHandle())}));
          } else {
            e.stack(std::vector<std::string>({no_debug_info}));
          }
        }
      }
    };
    profiler::enableProfilerWithEventPostProcess(
        config,
        {profiler::ActivityType::CPU},
        post_processing,
        {at::RecordScope::LITE_INTERPRETER});
  } else {
    profiler::enableProfiler(
        config,
        {profiler::ActivityType::CPU},
        {at::RecordScope::LITE_INTERPRETER});
  }
  trace_file_name_ = fname;
  TORCH_CHECK(
      tls_edge_profiler == nullptr, "Edge profiler is already profiling.")
  tls_edge_profiler = this;
}

void KinetoEdgeCPUProfiler::recordBackendEvent(
    const int64_t start_time_us,
    const int64_t end_time_us,
    const int64_t debug_handle,
    const std::string& event_name,
    const std::string& backend_name) {
  profiler::reportBackendEventToActiveKinetoProfiler(
      start_time_us,
      end_time_us,
      debug_handle,
      at::RecordScope::LITE_INTERPRETER,
      event_name,
      backend_name);
}

const std::unique_ptr<profiler::ProfilerResult>& KinetoEdgeCPUProfiler::
    disableProfiler() {
  TORCH_CHECK(
      !profiler_result_,
      "KinetoEdgeCPUProfiler already disabled. "
      "To get list of events use getProfilerResults()");
  profiler_result_ = profiler::disableProfiler();
  return profiler_result_;
}

const std::unique_ptr<profiler::ProfilerResult>& KinetoEdgeCPUProfiler::
    getProfilerResult() {
  TORCH_CHECK(
      profiler_result_,
      "KinetoEdgeCPUProfiler has not been disabled. "
      "use disableProfiler() API first, which returns the ProfilerResult.");
  return profiler_result_;
}

KinetoEdgeCPUProfiler::~KinetoEdgeCPUProfiler() {
  if (!trace_file_name_.empty()) {
    if (profiler_result_) {
      profiler_result_->save(trace_file_name_);
    } else {
      profiler::disableProfiler()->save(trace_file_name_);
    }
  }
  tls_edge_profiler = nullptr;
}

KinetoEdgeCPUProfiler* getCurrentEdgeProfiler() {
  return tls_edge_profiler;
}

} // namespace mobile
} // namespace jit
} // namespace torch
