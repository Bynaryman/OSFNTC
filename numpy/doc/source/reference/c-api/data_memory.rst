.. _data_memory:

Memory management in NumPy
==========================

The `numpy.ndarray` is a python class. It requires additional memory allocations
to hold `numpy.ndarray.strides`, `numpy.ndarray.shape` and
`numpy.ndarray.data` attributes. These attributes are specially allocated
after creating the python object in `__new__`. The ``strides`` and
``shape`` are stored in a piece of memory allocated internally.

The ``data`` allocation used to store the actual array values (which could be
pointers in the case of ``object`` arrays) can be very large, so NumPy has
provided interfaces to manage its allocation and release. This document details
how those interfaces work.

Historical overview
-------------------

Since version 1.7.0, NumPy has exposed a set of ``PyDataMem_*`` functions
(:c:func:`PyDataMem_NEW`, :c:func:`PyDataMem_FREE`, :c:func:`PyDataMem_RENEW`)
which are backed by `alloc`, `free`, `realloc` respectively. In that version
NumPy also exposed the `PyDataMem_EventHook` function (now deprecated)
described below, which wrap the OS-level calls.

Since those early days, Python also improved its memory management
capabilities, and began providing
various :ref:`management policies <memoryoverview>` beginning in version
3.4. These routines are divided into a set of domains, each domain has a
:c:type:`PyMemAllocatorEx` structure of routines for memory management. Python also
added a `tracemalloc` module to trace calls to the various routines. These
tracking hooks were added to the NumPy ``PyDataMem_*`` routines.

NumPy added a small cache of allocated memory in its internal
``npy_alloc_cache``, ``npy_alloc_cache_zero``, and ``npy_free_cache``
functions. These wrap ``alloc``, ``alloc-and-memset(0)`` and ``free``
respectively, but when ``npy_free_cache`` is called, it adds the pointer to a
short list of available blocks marked by size. These blocks can be re-used by
subsequent calls to ``npy_alloc*``, avoiding memory thrashing.

Configurable memory routines in NumPy (NEP 49)
----------------------------------------------

Users may wish to override the internal data memory routines with ones of their
own. Since NumPy does not use the Python domain strategy to manage data memory,
it provides an alternative set of C-APIs to change memory routines. There are
no Python domain-wide strategies for large chunks of object data, so those are
less suited to NumPy's needs. User who wish to change the NumPy data memory
management routines can use :c:func:`PyDataMem_SetHandler`, which uses a
:c:type:`PyDataMem_Handler` structure to hold pointers to functions used to
manage the data memory. The calls are still wrapped by internal routines to
call :c:func:`PyTraceMalloc_Track`, :c:func:`PyTraceMalloc_Untrack`, and will
use the deprecated :c:func:`PyDataMem_EventHookFunc` mechanism. Since the
functions may change during the lifetime of the process, each ``ndarray``
carries with it the functions used at the time of its instantiation, and these
will be used to reallocate or free the data memory of the instance.

.. c:type:: PyDataMem_Handler

    A struct to hold function pointers used to manipulate memory

    .. code-block:: c

        typedef struct {
            char name[127];  /* multiple of 64 to keep the struct aligned */
            uint8_t version; /* currently 1 */
            PyDataMemAllocator allocator;
        } PyDataMem_Handler;

    where the allocator structure is

    .. code-block:: c

        /* The declaration of free differs from PyMemAllocatorEx */ 
        typedef struct {
            void *ctx;
            void* (*malloc) (void *ctx, size_t size);
            void* (*calloc) (void *ctx, size_t nelem, size_t elsize);
            void* (*realloc) (void *ctx, void *ptr, size_t new_size);
            void (*free) (void *ctx, void *ptr, size_t size);
        } PyDataMemAllocator;

.. c:function:: PyObject * PyDataMem_SetHandler(PyObject *handler)

   Set a new allocation policy. If the input value is ``NULL``, will reset the
   policy to the default. Return the previous policy, or
   return ``NULL`` if an error has occurred. We wrap the user-provided functions
   so they will still call the python and numpy memory management callback
   hooks.
    
.. c:function:: PyObject * PyDataMem_GetHandler()

   Return the current policy that will be used to allocate data for the
   next ``PyArrayObject``. On failure, return ``NULL``.

For an example of setting up and using the PyDataMem_Handler, see the test in
:file:`numpy/core/tests/test_mem_policy.py`

.. c:function:: void PyDataMem_EventHookFunc(void *inp, void *outp, size_t size, void *user_data);

    This function will be called during data memory manipulation

.. c:function:: PyDataMem_EventHookFunc * PyDataMem_SetEventHook(PyDataMem_EventHookFunc *newhook, void *user_data, void **old_data)

    Sets the allocation event hook for numpy array data.
  
    Returns a pointer to the previous hook or ``NULL``.  If old_data is
    non-``NULL``, the previous user_data pointer will be copied to it.
  
    If not ``NULL``, hook will be called at the end of each ``PyDataMem_NEW/FREE/RENEW``:

    .. code-block:: c
   
        result = PyDataMem_NEW(size)        -> (*hook)(NULL, result, size, user_data)
        PyDataMem_FREE(ptr)                 -> (*hook)(ptr, NULL, 0, user_data)
        result = PyDataMem_RENEW(ptr, size) -> (*hook)(ptr, result, size, user_data)
  
    When the hook is called, the GIL will be held by the calling
    thread.  The hook should be written to be reentrant, if it performs
    operations that might cause new allocation events (such as the
    creation/destruction numpy objects, or creating/destroying Python
    objects which might cause a gc).

    Deprecated in v1.23

What happens when deallocating if there is no policy set
--------------------------------------------------------

A rare but useful technique is to allocate a buffer outside NumPy, use
:c:func:`PyArray_NewFromDescr` to wrap the buffer in a ``ndarray``, then switch
the ``OWNDATA`` flag to true. When the ``ndarray`` is released, the
appropriate function from the ``ndarray``'s ``PyDataMem_Handler`` should be
called to free the buffer. But the ``PyDataMem_Handler`` field was never set,
it will be ``NULL``. For backward compatibility, NumPy will call ``free()`` to
release the buffer. If ``NUMPY_WARN_IF_NO_MEM_POLICY`` is set to ``1``, a
warning will be emitted. The current default is not to emit a warning, this may
change in a future version of NumPy.

A better technique would be to use a ``PyCapsule`` as a base object:

.. code-block:: c

    /* define a PyCapsule_Destructor, using the correct deallocator for buff */
    void free_wrap(void *capsule){
        void * obj = PyCapsule_GetPointer(capsule, PyCapsule_GetName(capsule));
        free(obj); 
    };

    /* then inside the function that creates arr from buff */
    ...
    arr = PyArray_NewFromDescr(... buf, ...);
    if (arr == NULL) {
        return NULL;
    }
    capsule = PyCapsule_New(buf, "my_wrapped_buffer",
                            (PyCapsule_Destructor)&free_wrap);
    if (PyArray_SetBaseObject(arr, capsule) == -1) {
        Py_DECREF(arr);
        return NULL;
    }
    ...
