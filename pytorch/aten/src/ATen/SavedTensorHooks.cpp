#include <ATen/SavedTensorHooks.h>
#include <c10/util/Exception.h>

namespace at {

namespace {
  // PyObject is defined in c10/util/python_stub.h
  // Reference counting is handled by the caller of `set_hooks`.
  thread_local PyObject* pack_hook_(nullptr);
  thread_local PyObject* unpack_hook_(nullptr);

  // This flag is set to true the first time default hooks are registered
  // and left at true for the rest of the execution.
  // It's an optimization so that users who never use default hooks don't need to
  // read the thread_local variables pack_hook_ and unpack_hook_.
  static bool is_enabled(false);
}

void SavedTensorDefaultHooks::enable() {
  is_enabled = true;
}

void SavedTensorDefaultHooks::set_hooks(PyObject* pack_hook, PyObject* unpack_hook) {
  if (!is_enabled) {
    TORCH_INTERNAL_ASSERT(pack_hook == nullptr && unpack_hook == nullptr);
    return;
  }
  pack_hook_ = pack_hook;
  unpack_hook_ = unpack_hook;
}

std::pair<PyObject*, PyObject*> SavedTensorDefaultHooks::get_hooks() {
  if (!is_enabled) {
    return std::make_pair(nullptr, nullptr);
  }
  return std::make_pair(pack_hook_, unpack_hook_);
}

}
