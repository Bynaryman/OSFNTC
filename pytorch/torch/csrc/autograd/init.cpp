#include <torch/csrc/python_headers.h>

#include <c10/core/DeviceType.h>
#include <c10/core/InferenceMode.h>
#include <torch/csrc/Exceptions.h>
#include <torch/csrc/utils/pybind.h>
#include <torch/csrc/autograd/autograd.h>
#include <torch/csrc/autograd/grad_mode.h>
#include <torch/csrc/jit/python/pybind_utils.h>
#include <ATen/autocast_mode.h>
#include <ATen/cpp_custom_type_hack.h>
#include <ATen/record_function.h>
#include <torch/csrc/autograd/profiler.h>
#include <torch/csrc/autograd/profiler_python.h>
#include <torch/csrc/autograd/python_function.h>
#include <torch/csrc/autograd/function.h>
#include <torch/csrc/autograd/saved_variable.h>
#include <torch/csrc/autograd/python_saved_variable_hooks.h>
#include <torch/csrc/autograd/utils/wrap_outputs.h>
#include <torch/csrc/autograd/utils/python_arg_parsing.h>
#include <torch/csrc/autograd/python_mode.h>
#include <torch/csrc/autograd/python_variable.h>
#include <torch/csrc/utils/pycfunction_helpers.h>
#include <c10/core/ScalarType.h>

#include <set>
#include <unordered_set>

struct DisableTorchDispatch {
  DisableTorchDispatch() : guard_(c10::DispatchKey::Python) {
  }
  c10::impl::ExcludeDispatchKeyGuard guard_;
};

PyObject* THPAutograd_initExtension(PyObject* _unused, PyObject *unused) {
  using namespace torch::autograd::profiler;
  auto tensor_module = THPObjectPtr(PyImport_ImportModule("torch._tensor"));
  if (!tensor_module)
    return nullptr;

  // NOTE: "leaks" THPVariableClass
  THPVariableClass = PyObject_GetAttrString(tensor_module, "Tensor");
  if (!THPVariableClass)
    return nullptr;

  auto autograd_module = THPObjectPtr(PyImport_ImportModule("torch.autograd"));
  if (!autograd_module)
    return nullptr;

  // NOTE: "leaks" Function
  THPFunctionClass = PyObject_GetAttrString(autograd_module, "Function");
  if (!THPFunctionClass)
    return nullptr;

  auto torch_C_module = THPObjectPtr(PyImport_ImportModule("torch._C"));
  if (!torch_C_module)
    return nullptr;
  auto _C_m = py::handle(torch_C_module).cast<py::module>();
  auto m = _C_m.def_submodule("_autograd", "autograd bindings");

  auto parameter_module = THPObjectPtr(PyImport_ImportModule("torch.nn.parameter"));
  if (!parameter_module)
    return nullptr;

  // NOTE: "leaks" ParameterClass
  ParameterClass = PyObject_GetAttrString(parameter_module, "Parameter");
  if (!ParameterClass)
    return nullptr;

  py::enum_<ProfilerState>(m, "ProfilerState")
      .value("Disabled", ProfilerState::Disabled)
      .value("CPU", ProfilerState::CPU)
      .value("CUDA", ProfilerState::CUDA)
      .value("NVTX", ProfilerState::NVTX)
      .value("KINETO", ProfilerState::KINETO)
      .value("KINETO_GPU_FALLBACK", ProfilerState::KINETO_GPU_FALLBACK);

  py::enum_<ActivityType>(m, "ProfilerActivity")
      .value("CPU", ActivityType::CPU)
      .value("CUDA", ActivityType::CUDA);

  py::class_<ProfilerConfig>(m, "ProfilerConfig")
      .def(py::init<ProfilerState,
          bool, /* record_input_shapes */
          bool, /* profile_memory */
          bool, /* with_stack */
          bool, /* with_flops */
          bool  /* with_modules */
          >());

  py::class_<LegacyEvent>(m, "ProfilerEvent")
      .def("kind", &LegacyEvent::kindStr)
      .def("name", [](const LegacyEvent& e) { return e.name(); })
      .def("thread_id", &LegacyEvent::threadId)
      .def("fwd_thread_id", &LegacyEvent::fwdThreadId)
      .def("device", &LegacyEvent::device)
      .def("cpu_elapsed_us", &LegacyEvent::cpuElapsedUs)
      .def("cuda_elapsed_us", &LegacyEvent::cudaElapsedUs)
      .def("has_cuda", &LegacyEvent::hasCuda)
      .def("shapes", &LegacyEvent::shapes)
      .def("cpu_memory_usage", &LegacyEvent::cpuMemoryUsage)
      .def("cuda_memory_usage", &LegacyEvent::cudaMemoryUsage)
      .def("handle", &LegacyEvent::handle)
      .def("node_id", &LegacyEvent::nodeId)
      .def("is_remote", &LegacyEvent::isRemote)
      .def("sequence_nr", &LegacyEvent::sequenceNr)
      .def("stack", &LegacyEvent::stack)
      .def("scope", &LegacyEvent::scope)
      .def("correlation_id", &LegacyEvent::correlationId)
      .def("start_us", &LegacyEvent::cpuUs)
      .def("flops", &LegacyEvent::flops)
      .def("is_async", &LegacyEvent::isAsync);

  py::enum_<c10::DeviceType>(m, "DeviceType")
      .value("CPU", c10::DeviceType::CPU)
      .value("CUDA", c10::DeviceType::CUDA)
      .value("MKLDNN", c10::DeviceType::MKLDNN)
      .value("OPENGL", c10::DeviceType::OPENGL)
      .value("OPENCL", c10::DeviceType::OPENCL)
      .value("IDEEP", c10::DeviceType::IDEEP)
      .value("HIP", c10::DeviceType::HIP)
      .value("FPGA", c10::DeviceType::FPGA)
      .value("ORT", c10::DeviceType::ORT)
      .value("XLA", c10::DeviceType::XLA)
      .value("Lazy", c10::DeviceType::Lazy)
      .value("MLC", c10::DeviceType::MLC)
      .value("HPU", c10::DeviceType::HPU)
      .value("Meta", c10::DeviceType::Meta)
      .value("Vulkan", c10::DeviceType::Vulkan)
      .value("Metal", c10::DeviceType::Metal);

  py::class_<KinetoEvent>(m, "_KinetoEvent")
      // name of the event
      .def("name", [](const KinetoEvent& e) {
        return e.name();
      })
      // PyTorch thread id of the start callback
      .def("start_thread_id", [](const KinetoEvent& e) {
        return e.startThreadId();
      })
      // PyTorch thread id of the end callback
      .def("end_thread_id", [](const KinetoEvent& e) {
        return e.endThreadId();
      })
      // for events of scope BACKWARD_FUNCTION - PyTorch thread id
      // of the corresponding forward op
      .def("fwd_thread_id", [](const KinetoEvent& e) {
        return e.fwdThreadId();
      })
      // together with fwd_thread_id, used to uniquely identify
      // the forward op
      .def("sequence_nr", [](const KinetoEvent& e) {
        return e.sequenceNr();
      })
      // absolute start time (since unix epoch) in us
      .def("start_us", [](const KinetoEvent& e) {
        return e.startUs();
      })
      // duration in us
      .def("duration_us", [](const KinetoEvent& e) {
        return e.durationUs();
      })
      // used for correlation between high-level PyTorch events
      // and low-level device events
      .def("correlation_id", [](const KinetoEvent& e) {
        return e.correlationId();
      })
      // shapes of input tensors
      .def("shapes", [](const KinetoEvent& e) {
        if (e.hasShapes()) {
          return e.shapes();
        } else {
          return std::vector<std::vector<int64_t>>();
        }
      })
      .def("dtypes", [](const KinetoEvent& e) {
        if (e.hasTypes()) {
          return e.dtypes();
        } else {
          return std::vector<std::string>();
        }
      })
      // stack traces of the PyTorch CPU events
      .def("stack", [](const KinetoEvent& e) {
        if (e.hasStack()) {
          return e.stack();
        } else {
          return std::vector<std::string>();
        }
      })
      // type of the RecordFunction that generated a PyTorch CPU event
      // (op, torchscript function, user label, etc)
      .def("scope", [](const KinetoEvent& e) {
        return e.scope();
      })
      // device number, for CPU - process id
      .def("device_index", [](const KinetoEvent& e) {
        return e.deviceIndex();
      })
      // for CUDA - stream id, for CPU - start thread id
      .def("device_resource_id", [](const KinetoEvent& e) {
        return e.deviceResourceId();
      })
      // device type
      .def("device_type", [](const KinetoEvent& e) {
        return e.deviceType();
      })
      // correlation id of a linked event
      .def("linked_correlation_id", [](const KinetoEvent& e) {
        return e.linkedCorrelationId();
      })
      // compute flops
      .def("flops", [](const KinetoEvent& e) {
        return e.flops();
      })
      // Whether this is async event or not
      .def("is_async", [](const KinetoEvent& e) {
        return e.isAsync();
      })
      .def("cuda_elapsed_us", &KinetoEvent::cudaElapsedUs)
      .def("nbytes", [](const KinetoEvent& e) {
        return e.nBytes();
      });

  py::class_<ProfilerResult>(m, "_ProfilerResult")
    .def("trace_start_us", &ProfilerResult::trace_start_us)
    .def("events", &ProfilerResult::events)
#ifdef USE_KINETO
    .def("save", &ProfilerResult::save)
#endif // USE_KINETO
    ;

  m.def("_enable_profiler",
        &enableProfiler,
        py::arg("config"),
        py::arg("activities"),
        py::arg("scopes") = std::unordered_set<at::RecordScope>());
  m.def("_disable_profiler", disableProfiler);
  m.def("_prepare_profiler", prepareProfiler);

  m.def("_add_metadata_json", [](const std::string& key, const std::string& value) {
#ifdef USE_KINETO
      addMetadataJson(key, value);
#else
      LOG(WARNING) << "Adding profiling metadata requires using "
                   << "torch.profiler with Kineto support (USE_KINETO=1)";
#endif // USE_KINETO
  });

  m.def("kineto_available", []() {
#ifdef USE_KINETO
    return true;
#else
    return false;
#endif
  });

  // NOTICE: These record functions are not torch operators and may not show up
  // in TorchScript tracing, FX transforms, or operator serialization. For these
  // use cases, please use `torch.profiler.record_function`.
  // Creates a new profiling scope using RecordFunction and invokes its starting
  // callbacks.
  m.def("_record_function_with_args_enter", [](const std::string& name, py::args args) {
    auto rec = std::make_unique<at::RecordFunction>(at::RecordScope::USER_SCOPE);
    if (rec->isActive()) {
      if (rec->needsInputs()) {
        auto iv_inputs = std::vector<c10::IValue>();
        for (const auto& arg : args) {
            iv_inputs.push_back(torch::jit::toTypeInferredIValue(arg));
        }
        rec->before(name, iv_inputs);
      } else {
        rec->before(name);
      }
    }
    return at::cpp_custom_type_hack::create(std::move(rec), at::TensorOptions());
  });

  // Ends the profiling scope created with record_function_with_param_enter.
  m.def("_record_function_with_args_exit", [](const at::Tensor& handle) {
    // We don't actually need to do anything with handle just need to persist the
    // lifetime until now.
    auto& rec = at::cpp_custom_type_hack::cast<at::RecordFunction>(handle);
    rec.end();
  });

  m.def("_supported_activities", []() {
    std::set<ActivityType> activities {ActivityType::CPU};
#if defined(USE_KINETO) && !defined(LIBKINETO_NOCUPTI)
    if (at::getNumGPUs() > 0 && !at::hasHIP()) {
      activities.insert(ActivityType::CUDA);
    }
#endif
    return activities;
  });

  m.def("_enable_profiler_legacy", enableProfilerLegacy);
  py::class_<ProfilerDisableOptions>(m, "_ProfilerDisableOptions")
      .def(py::init<bool, bool>());
  m.def(
      "_disable_profiler_legacy",
      disableProfilerLegacy,
      py::arg("profiler_disable_options") = ProfilerDisableOptions());
  m.def("_profiler_enabled", profilerEnabled);
  m.def("_enable_record_function", [](bool enable) {
    at::enableRecordFunction(enable);
  });
  m.def("_set_empty_test_observer", [](bool is_global, double sampling_prob) {
    auto cb = at::RecordFunctionCallback(nullptr)
      .needsInputs(true)
      .samplingProb(sampling_prob);
    if (is_global) {
      at::addGlobalCallback(cb);
    } else {
      at::addThreadLocalCallback(cb);
    }
  });
  m.def("_clear_callbacks", []() {
    at::clearCallbacks();
  });
  m.def("_register_saved_tensors_default_hooks", [](py::function &pack_hook, py::function &unpack_hook) {
    torch::autograd::PyDefaultSavedVariableHooks::set_hooks(pack_hook, unpack_hook);
  });
  m.def("_reset_saved_tensors_default_hooks", []() {
    torch::autograd::PyDefaultSavedVariableHooks::reset_hooks();
  });

  _C_m.def("_register_py_class_for_device", [](const std::string& device, py::object python_type_class) {
    auto cls = python_type_class.ptr();
    registerPythonTensorClass(device, cls);
  });

  py::class_<c10::InferenceMode>(_C_m, "_InferenceMode")
      .def(py::init<bool>());

  py::class_<DisableTorchDispatch>(_C_m, "_DisableTorchDispatch")
      .def(py::init<>());

  py::class_<torch::autograd::SavedVariable>(m, "SavedTensor")
    .def(py::init([]()->torch::autograd::SavedVariable {
      TORCH_CHECK(false, "Trying to create a SavedTensor object from Python is forbidden.");
    }))
    .def("register_hooks", [](torch::autograd::SavedVariable &s, py::function &pack_hook, py::function &unpack_hook) {
        // Because we use a py::object, pybind will increment the refcount of the hook functions for us
        s.register_hooks(std::make_unique<torch::autograd::PySavedVariableHooks>(pack_hook, unpack_hook));
    });

  torch::autograd::profiler::python_tracer::init();
  Py_RETURN_TRUE;
}

namespace torch { namespace autograd {

static PyObject * set_autocast_enabled(PyObject* _unused, PyObject *arg) {
  HANDLE_TH_ERRORS
  if (!PyBool_Check(arg)) {
    throw TypeError("enabled must be a bool (got %s)", Py_TYPE(arg)->tp_name);
  }
  at::autocast::set_enabled(arg == Py_True);
  Py_RETURN_NONE;
  END_HANDLE_TH_ERRORS
}

static PyObject * is_autocast_enabled(PyObject* _unused, PyObject *arg) {
  HANDLE_TH_ERRORS
  if (at::autocast::is_enabled()) {
    Py_RETURN_TRUE;
  } else {
    Py_RETURN_FALSE;
  }
  END_HANDLE_TH_ERRORS
}

static PyObject * set_autocast_cpu_enabled(PyObject* _unused, PyObject *arg) {
  HANDLE_TH_ERRORS
  if (!PyBool_Check(arg)) {
    throw TypeError("enabled must be a bool (got %s)", Py_TYPE(arg)->tp_name);
  }
  at::autocast::set_cpu_enabled(arg == Py_True);
  Py_RETURN_NONE;
  END_HANDLE_TH_ERRORS
}

static PyObject * is_autocast_cpu_enabled(PyObject* _unused, PyObject *arg) {
  HANDLE_TH_ERRORS
  if (at::autocast::is_cpu_enabled()) {
    Py_RETURN_TRUE;
  } else {
    Py_RETURN_FALSE;
  }
  END_HANDLE_TH_ERRORS
}

static PyObject * set_autocast_gpu_dtype(PyObject* _unused, PyObject *arg) {
  HANDLE_TH_ERRORS
  if (!THPDtype_Check(arg)) {
    throw TypeError(
        "dtype must be a torch.dtype (got %s)", Py_TYPE(arg)->tp_name);
  }
  at::ScalarType targetType = reinterpret_cast<THPDtype*>(arg)->scalar_type;
  at::autocast::set_autocast_gpu_dtype(targetType);
  Py_RETURN_NONE;
  END_HANDLE_TH_ERRORS
}

static PyObject * set_autocast_cpu_dtype(PyObject* _unused, PyObject *arg) {
  HANDLE_TH_ERRORS
  if (!THPDtype_Check(arg)) {
    throw TypeError(
        "dtype must be a torch.dtype (got %s)", Py_TYPE(arg)->tp_name);
  }
  at::ScalarType targetType = reinterpret_cast<THPDtype*>(arg)->scalar_type;
  at::autocast::set_autocast_cpu_dtype(targetType);
  Py_RETURN_NONE;
  END_HANDLE_TH_ERRORS
}

static const char* scalarTypeName(const at::ScalarType type) {
  switch (type) {
#define DEFINE_CASE(ctype, name) \
  case at::ScalarType::name:     \
    return #ctype;
    AT_FORAUTOCAST_SCALAR_TYPES(DEFINE_CASE)
#undef DEFINE_CASE
    default:
      throw std::runtime_error("unknown scalar type for autocast");
  }
}

static PyObject * get_autocast_gpu_dtype(PyObject* _unused, PyObject *arg){
  HANDLE_TH_ERRORS
  at::ScalarType current_dtype = at::autocast::get_autocast_gpu_dtype();
  auto dtype = (PyObject*)torch::getTHPDtype(current_dtype);
  Py_INCREF(dtype);
  return dtype;
  END_HANDLE_TH_ERRORS
}

static PyObject * get_autocast_cpu_dtype(PyObject* _unused, PyObject *arg){
  HANDLE_TH_ERRORS
  at::ScalarType current_dtype = at::autocast::get_autocast_cpu_dtype();
  auto dtype = (PyObject*)torch::getTHPDtype(current_dtype);
  Py_INCREF(dtype);
  return dtype;
  END_HANDLE_TH_ERRORS
}

static PyObject * clear_autocast_cache(PyObject* _unused, PyObject *arg) {
  HANDLE_TH_ERRORS
  at::autocast::clear_cache();
  Py_RETURN_NONE;
  END_HANDLE_TH_ERRORS
}

static PyObject * autocast_increment_nesting(PyObject* _unused, PyObject *arg) {
  HANDLE_TH_ERRORS
  return THPUtils_packInt64(at::autocast::increment_nesting());
  END_HANDLE_TH_ERRORS
}

static PyObject * autocast_decrement_nesting(PyObject* _unused, PyObject *arg) {
  HANDLE_TH_ERRORS
  return THPUtils_packInt64(at::autocast::decrement_nesting());
  END_HANDLE_TH_ERRORS
}

static PyObject * is_autocast_cache_enabled(PyObject* _unused, PyObject *arg) {
  HANDLE_TH_ERRORS
  if (at::autocast::is_autocast_cache_enabled()) {
    Py_RETURN_TRUE;
  } else {
    Py_RETURN_FALSE;
  }
  END_HANDLE_TH_ERRORS
}

static PyObject * set_autocast_cache_enabled(PyObject* _unused, PyObject *arg) {
  HANDLE_TH_ERRORS
  if (!PyBool_Check(arg)) {
    throw TypeError("enabled must be a bool (got %s)", Py_TYPE(arg)->tp_name);
  }
  at::autocast::set_autocast_cache_enabled(arg == Py_True);
  Py_RETURN_NONE;
  END_HANDLE_TH_ERRORS
}

static PyObject * set_grad_enabled(PyObject* _unused, PyObject *arg) {
  HANDLE_TH_ERRORS
  if (!PyBool_Check(arg)) {
    throw TypeError("enabled must be a bool (got %s)", Py_TYPE(arg)->tp_name);
  }
  GradMode::set_enabled(arg == Py_True);
  Py_RETURN_NONE;
  END_HANDLE_TH_ERRORS
}

static PyObject * is_grad_enabled(PyObject* _unused, PyObject *arg) {
  HANDLE_TH_ERRORS
  if (GradMode::is_enabled()) {
    Py_RETURN_TRUE;
  } else {
    Py_RETURN_FALSE;
  }
  END_HANDLE_TH_ERRORS
}

static PyObject * is_inference_mode_enabled(PyObject* _unused, PyObject *arg) {
  HANDLE_TH_ERRORS
  if (c10::InferenceMode::is_enabled()) {
    Py_RETURN_TRUE;
  } else {
    Py_RETURN_FALSE;
  }
  END_HANDLE_TH_ERRORS
}

static PyObject * set_anomaly_mode_enabled(PyObject* _unused, PyObject *arg) {
  HANDLE_TH_ERRORS
  if (!PyBool_Check(arg)) {
    throw TypeError("enabled must be a bool (got %s)", Py_TYPE(arg)->tp_name);
  }
  AnomalyMode::set_enabled(arg == Py_True);
  Py_RETURN_NONE;
  END_HANDLE_TH_ERRORS
}

static PyObject * is_anomaly_mode_enabled(PyObject* _unused, PyObject *arg) {
  HANDLE_TH_ERRORS
  if (AnomalyMode::is_enabled()) {
    Py_RETURN_TRUE;
  } else {
    Py_RETURN_FALSE;
  }
  END_HANDLE_TH_ERRORS
}

static PyObject * python_enter_dual_level(PyObject* _unused, PyObject* arg) {
  HANDLE_TH_ERRORS
  // It is unlikely that the depth of forward nesting will overflow int64_t so we
  // just static cast here.
  return utils::wrap(static_cast<int64_t>(forward_ad::enter_dual_level()));
  END_HANDLE_TH_ERRORS
}

static PyObject * python_exit_dual_level(PyObject* _unused, PyObject* args, PyObject* kwargs) {
  HANDLE_TH_ERRORS
  static PythonArgParser parser({
    "exit_dual_level(int64_t level)"
  });

  ParsedArgs<1> parsed_args;
  auto _r = parser.parse(args, kwargs, parsed_args);

  auto idx = _r.toInt64(0);
  // Make sure the given index is valid before casting it
  TORCH_CHECK(idx >= 0, "Dual level must be a positive number.");
  forward_ad::exit_dual_level(static_cast<uint64_t>(idx));
  Py_RETURN_NONE;
  END_HANDLE_TH_ERRORS
}

static PyObject * enter_python_mode(PyObject* _unused, PyObject* arg) {
  HANDLE_TH_ERRORS
  PythonMode::enter(arg);
  Py_RETURN_NONE;
  END_HANDLE_TH_ERRORS
}

static PyObject * exit_python_mode(PyObject* _unused, PyObject* arg) {
  HANDLE_TH_ERRORS
  PythonMode::exit();
  Py_RETURN_NONE;
  END_HANDLE_TH_ERRORS
}

// autograd methods on torch._C
static PyMethodDef methods[] = { // NOLINT
  {"_set_grad_enabled", set_grad_enabled, METH_O, nullptr},
  {"is_grad_enabled", is_grad_enabled, METH_NOARGS, nullptr},
  {"is_inference_mode_enabled", is_inference_mode_enabled, METH_NOARGS, nullptr},
  {"set_autocast_enabled", set_autocast_enabled, METH_O, nullptr},
  {"is_autocast_enabled", is_autocast_enabled, METH_NOARGS, nullptr},
  {"clear_autocast_cache", clear_autocast_cache, METH_NOARGS, nullptr},
  {"set_autocast_cpu_enabled", set_autocast_cpu_enabled, METH_O, nullptr},
  {"is_autocast_cpu_enabled", is_autocast_cpu_enabled, METH_NOARGS, nullptr},
  {"set_autocast_cpu_dtype", set_autocast_cpu_dtype, METH_O, nullptr},
  {"get_autocast_cpu_dtype", get_autocast_cpu_dtype, METH_NOARGS, nullptr},
  {"set_autocast_gpu_dtype", set_autocast_gpu_dtype, METH_O, nullptr},
  {"get_autocast_gpu_dtype", get_autocast_gpu_dtype, METH_NOARGS, nullptr},
  {"autocast_increment_nesting", autocast_increment_nesting, METH_NOARGS, nullptr},
  {"autocast_decrement_nesting", autocast_decrement_nesting, METH_NOARGS, nullptr},
  {"is_autocast_cache_enabled", is_autocast_cache_enabled, METH_NOARGS, nullptr},
  {"set_autocast_cache_enabled", set_autocast_cache_enabled, METH_O, nullptr},
  {"set_anomaly_enabled", set_anomaly_mode_enabled, METH_O, nullptr},
  {"is_anomaly_enabled", is_anomaly_mode_enabled, METH_NOARGS, nullptr},
  {"_enter_dual_level", python_enter_dual_level, METH_NOARGS, nullptr},
  {"_exit_dual_level", castPyCFunctionWithKeywords(python_exit_dual_level), METH_VARARGS | METH_KEYWORDS, nullptr},
  {"_enter_python_mode", enter_python_mode, METH_O, nullptr},
  {"_exit_python_mode", exit_python_mode, METH_NOARGS, nullptr},
  {nullptr, nullptr, 0, nullptr}
};

PyMethodDef* python_functions() {
  return methods;
}

}} // namespace torch::autograd
