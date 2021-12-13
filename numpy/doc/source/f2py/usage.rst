===========
Using F2PY
===========

F2PY can be used either as a command line tool ``f2py`` or as a Python
module ``numpy.f2py``. While we try to provide the command line tool as part
of the numpy setup, some platforms like Windows make it difficult to
reliably put the executables on the ``PATH``. We will refer to ``f2py``
in this document but you may have to run it as a module::

   python -m numpy.f2py

If you run ``f2py`` with no arguments, and the line ``numpy Version`` at the
end matches the NumPy version printed from ``python -m numpy.f2py``, then you
can use the shorter version. If not, or if you cannot run ``f2py``, you should
replace all calls to ``f2py`` here with the longer version.

Command ``f2py``
=================

When used as a command line tool, ``f2py`` has three major modes,
distinguished by the usage of ``-c`` and ``-h`` switches:

Signature file generation
^^^^^^^^^^^^^^^^^^^^^^^^^^

1. To scan Fortran sources and generate a signature file, use

   .. code-block:: sh

     f2py -h <filename.pyf> <options> <fortran files>   \
       [[ only: <fortran functions>  : ]                \
        [ skip: <fortran functions>  : ]]...            \
       [<fortran files> ...]

   .. note::

    A Fortran source file can contain many routines, and it is often
    not necessary to allow all routines be usable from Python. In such cases,
    either specify which routines should be wrapped (in the ``only: .. :`` part)
    or which routines F2PY should ignored (in the ``skip: .. :`` part).

   If ``<filename.pyf>`` is specified as ``stdout`` then signatures
   are written to standard output instead of a file.

   Among other options (see below), the following can be used
   in this mode:

   ``--overwrite-signature``
     Overwrites an existing signature file.

Extension module construction
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

2. To construct an extension module, use

   .. code-block:: sh

     f2py -m <modulename> <options> <fortran files>   \
       [[ only: <fortran functions>  : ]              \
        [ skip: <fortran functions>  : ]]...          \
       [<fortran files> ...]

   The constructed extension module is saved as
   ``<modulename>module.c`` to the current directory.

   Here ``<fortran files>`` may also contain signature files.
   Among other options (see below), the following options can be used
   in this mode:

   ``--debug-capi``
     Adds debugging hooks to the extension module. When using this extension
     module, various diagnostic information about the wrapper is written to
     the standard output, for example, the values of variables, the steps taken,
     etc.

   ``-include'<includefile>'``
     Add a CPP ``#include`` statement to the extension module source.
     ``<includefile>`` should be given in one of the following forms

       .. code-block:: cpp

        "filename.ext"
        <filename.ext>

     The include statement is inserted just before the wrapper
     functions. This feature enables using arbitrary C functions
     (defined in ``<includefile>``) in F2PY generated wrappers.

     .. note:: This option is deprecated. Use ``usercode`` statement to specify C code snippets directly in signature files.

   ``--[no-]wrap-functions``
     Create Fortran subroutine wrappers to Fortran functions.
     ``--wrap-functions`` is default because it ensures maximum
     portability and compiler independence.

   ``--include-paths <path1>:<path2>:..``
     Search include files from given directories.

   ``--help-link [<list of resources names>]``
     List system resources found by ``numpy_distutils/system_info.py``.
     For example, try ``f2py --help-link lapack_opt``.

Building a module
^^^^^^^^^^^^^^^^^

3. To build an extension module, use

   .. code-block:: sh

     f2py -c <options> <fortran files>       \
       [[ only: <fortran functions>  : ]     \
        [ skip: <fortran functions>  : ]]... \
       [ <fortran/c source files> ] [ <.o, .a, .so files> ]
 
   If ``<fortran files>`` contains a signature file, then the source for
   an extension module is constructed, all Fortran and C sources are
   compiled, and finally all object and library files are linked to the
   extension module ``<modulename>.so`` which is saved into the current
   directory.

   If ``<fortran files>`` does not contain a signature file, then an
   extension module is constructed by scanning all Fortran source codes
   for routine signatures, before proceeding to build the extension module.
 
   Among other options (see below) and options described for previous
   modes, the following options can be used in this mode:
 
   ``--help-fcompiler``
     List the available Fortran compilers.
   ``--help-compiler`` **[depreciated]**
     List the available Fortran compilers.
   ``--fcompiler=<Vendor>``
     Specify a Fortran compiler type by vendor.
   ``--f77exec=<path>``
     Specify the path to a F77 compiler
   ``--fcompiler-exec=<path>`` **[depreciated]**
     Specify the path to a F77 compiler
   ``--f90exec=<path>``
     Specify the path to a F90 compiler
   ``--f90compiler-exec=<path>`` **[depreciated]**
     Specify the path to a F90 compiler
   ``--f77flags=<string>``
     Specify F77 compiler flags
   ``--f90flags=<string>``
     Specify F90 compiler flags
   ``--opt=<string>``
     Specify optimization flags
   ``--arch=<string>``
     Specify architecture specific optimization flags
   ``--noopt``
     Compile without optimization flags
   ``--noarch``
     Compile without arch-dependent optimization flags
   ``--debug``
     Compile with debugging information
   ``-l<libname>``
     Use the library ``<libname>`` when linking.
   ``-D<macro>[=<defn=1>]``
     Define macro ``<macro>`` as ``<defn>``.
   ``-U<macro>``
     Define macro ``<macro>``
   ``-I<dir>``
     Append directory ``<dir>`` to the list of directories searched for
     include files.
   ``-L<dir>``
     Add directory ``<dir>`` to the list of directories to  be  searched
     for ``-l``.
   ``link-<resource>``
     Link the extension module with <resource> as defined by
     ``numpy_distutils/system_info.py``. E.g. to link with optimized
     LAPACK libraries (vecLib on MacOSX, ATLAS elsewhere), use
     ``--link-lapack_opt``. See also ``--help-link`` switch.

   .. note:: The ``f2py -c`` option must be applied either to an existing ``.pyf`` file (plus the source/object/library files) or one must specify the ``-m <modulename>`` option (plus the sources/object/library files). Use one of the following options:

   .. code-block:: sh

         f2py -c -m fib1 fib1.f

   or

   .. code-block:: sh

         f2py -m fib1 fib1.f -h fib1.pyf
         f2py -c fib1.pyf fib1.f

   For more information, see the `Building C and C++ Extensions`__ Python documentation for details.

   __ https://docs.python.org/3/extending/building.html


   When building an extension module, a combination of the following
   macros may be required for non-gcc Fortran compilers:

   .. code-block:: sh

     -DPREPEND_FORTRAN
     -DNO_APPEND_FORTRAN
     -DUPPERCASE_FORTRAN
 
   To test the performance of F2PY generated interfaces, use
   ``-DF2PY_REPORT_ATEXIT``. Then a report of various timings is
   printed out at the exit of Python. This feature may not work on
   all platforms, currently only Linux platform is supported.
 
   To see whether F2PY generated interface performs copies of array
   arguments, use ``-DF2PY_REPORT_ON_ARRAY_COPY=<int>``. When the size
   of an array argument is larger than ``<int>``, a message about
   the coping is sent to ``stderr``.

Other options
^^^^^^^^^^^^^

``-m <modulename>``
  Name of an extension module. Default is ``untitled``.

  .. warning:: Don't use this option if a signature file (\*.pyf) is used.
``--[no-]lower``
  Do [not] lower the cases in ``<fortran files>``.  By default,
  ``--lower`` is assumed with ``-h`` switch, and ``--no-lower``
  without the ``-h`` switch.
``--build-dir <dirname>``
  All F2PY generated files are created in ``<dirname>``.  Default is
  ``tempfile.mkdtemp()``.
``--quiet``
  Run quietly.
``--verbose``
  Run with extra verbosity.
``-v``
  Print the F2PY version and exit.

Execute ``f2py`` without any options to get an up-to-date list of
available options.

Python module ``numpy.f2py``
============================

.. warning::

  The current Python interface to the ``f2py`` module is not mature and
  may change in the future.


.. automodule:: numpy.f2py
    :members:

