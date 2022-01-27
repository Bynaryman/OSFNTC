/* -*- c -*- */

/*
 * The purpose of this module is to add faster sort functions
 * that are type-specific.  This is done by altering the
 * function table for the builtin descriptors.
 *
 * These sorting functions are copied almost directly from numarray
 * with a few modifications (complex comparisons compare the imaginary
 * part if the real parts are equal, for example), and the names
 * are changed.
 *
 * The original sorting code is due to Charles R. Harris who wrote
 * it for numarray.
 */

/*
 * Quick sort is usually the fastest, but the worst case scenario can
 * be slower than the merge and heap sorts.  The merge sort requires
 * extra memory and so for large arrays may not be useful.
 *
 * The merge sort is *stable*, meaning that equal components
 * are unmoved from their entry versions, so it can be used to
 * implement lexigraphic sorting on multiple keys.
 *
 * The heap sort is included for completeness.
 */

#define NPY_NO_DEPRECATED_API NPY_API_VERSION

#include "npy_sort.h"
#include "npysort_common.h"
#include "numpy_tag.h"

#include <cstdlib>

#define NOT_USED NPY_UNUSED(unused)
#define PYA_QS_STACK 100
#define SMALL_QUICKSORT 15
#define SMALL_MERGESORT 20
#define SMALL_STRING 16

/*
 *****************************************************************************
 **                            NUMERIC SORTS                                **
 *****************************************************************************
 */

template <typename Tag, typename type>
static int
heapsort_(type *start, npy_intp n)
{
    type tmp, *a;
    npy_intp i, j, l;

    /* The array needs to be offset by one for heapsort indexing */
    a = start - 1;

    for (l = n >> 1; l > 0; --l) {
        tmp = a[l];
        for (i = l, j = l << 1; j <= n;) {
            if (j < n && Tag::less(a[j], a[j + 1])) {
                j += 1;
            }
            if (Tag::less(tmp, a[j])) {
                a[i] = a[j];
                i = j;
                j += j;
            }
            else {
                break;
            }
        }
        a[i] = tmp;
    }

    for (; n > 1;) {
        tmp = a[n];
        a[n] = a[1];
        n -= 1;
        for (i = 1, j = 2; j <= n;) {
            if (j < n && Tag::less(a[j], a[j + 1])) {
                j++;
            }
            if (Tag::less(tmp, a[j])) {
                a[i] = a[j];
                i = j;
                j += j;
            }
            else {
                break;
            }
        }
        a[i] = tmp;
    }

    return 0;
}

template <typename Tag, typename type>
static int
aheapsort_(type *vv, npy_intp *tosort, npy_intp n)
{
    type *v = vv;
    npy_intp *a, i, j, l, tmp;
    /* The arrays need to be offset by one for heapsort indexing */
    a = tosort - 1;

    for (l = n >> 1; l > 0; --l) {
        tmp = a[l];
        for (i = l, j = l << 1; j <= n;) {
            if (j < n && Tag::less(v[a[j]], v[a[j + 1]])) {
                j += 1;
            }
            if (Tag::less(v[tmp], v[a[j]])) {
                a[i] = a[j];
                i = j;
                j += j;
            }
            else {
                break;
            }
        }
        a[i] = tmp;
    }

    for (; n > 1;) {
        tmp = a[n];
        a[n] = a[1];
        n -= 1;
        for (i = 1, j = 2; j <= n;) {
            if (j < n && Tag::less(v[a[j]], v[a[j + 1]])) {
                j++;
            }
            if (Tag::less(v[tmp], v[a[j]])) {
                a[i] = a[j];
                i = j;
                j += j;
            }
            else {
                break;
            }
        }
        a[i] = tmp;
    }

    return 0;
}

/*
 *****************************************************************************
 **                             STRING SORTS                                **
 *****************************************************************************
 */

template <typename Tag, typename type>
NPY_NO_EXPORT int
string_heapsort_(type *start, npy_intp n, void *varr)
{
    PyArrayObject *arr = (PyArrayObject *)varr;
    size_t len = PyArray_ITEMSIZE(arr) / sizeof(type);
    type *tmp = (type *)malloc(PyArray_ITEMSIZE(arr));
    type *a = (type *)start - len;
    npy_intp i, j, l;

    if (tmp == NULL) {
        return -NPY_ENOMEM;
    }

    for (l = n >> 1; l > 0; --l) {
        Tag::copy(tmp, a + l * len, len);
        for (i = l, j = l << 1; j <= n;) {
            if (j < n && Tag::less(a + j * len, a + (j + 1) * len, len))
                j += 1;
            if (Tag::less(tmp, a + j * len, len)) {
                Tag::copy(a + i * len, a + j * len, len);
                i = j;
                j += j;
            }
            else {
                break;
            }
        }
        Tag::copy(a + i * len, tmp, len);
    }

    for (; n > 1;) {
        Tag::copy(tmp, a + n * len, len);
        Tag::copy(a + n * len, a + len, len);
        n -= 1;
        for (i = 1, j = 2; j <= n;) {
            if (j < n && Tag::less(a + j * len, a + (j + 1) * len, len))
                j++;
            if (Tag::less(tmp, a + j * len, len)) {
                Tag::copy(a + i * len, a + j * len, len);
                i = j;
                j += j;
            }
            else {
                break;
            }
        }
        Tag::copy(a + i * len, tmp, len);
    }

    free(tmp);
    return 0;
}

template <typename Tag, typename type>
NPY_NO_EXPORT int
string_aheapsort_(type *vv, npy_intp *tosort, npy_intp n, void *varr)
{
    type *v = vv;
    PyArrayObject *arr = (PyArrayObject *)varr;
    size_t len = PyArray_ITEMSIZE(arr) / sizeof(type);
    npy_intp *a, i, j, l, tmp;

    /* The array needs to be offset by one for heapsort indexing */
    a = tosort - 1;

    for (l = n >> 1; l > 0; --l) {
        tmp = a[l];
        for (i = l, j = l << 1; j <= n;) {
            if (j < n && Tag::less(v + a[j] * len, v + a[j + 1] * len, len))
                j += 1;
            if (Tag::less(v + tmp * len, v + a[j] * len, len)) {
                a[i] = a[j];
                i = j;
                j += j;
            }
            else {
                break;
            }
        }
        a[i] = tmp;
    }

    for (; n > 1;) {
        tmp = a[n];
        a[n] = a[1];
        n -= 1;
        for (i = 1, j = 2; j <= n;) {
            if (j < n && Tag::less(v + a[j] * len, v + a[j + 1] * len, len))
                j++;
            if (Tag::less(v + tmp * len, v + a[j] * len, len)) {
                a[i] = a[j];
                i = j;
                j += j;
            }
            else {
                break;
            }
        }
        a[i] = tmp;
    }

    return 0;
}

/**end repeat**/

/*
 *****************************************************************************
 **                             GENERIC SORT                                **
 *****************************************************************************
 */

NPY_NO_EXPORT int
npy_heapsort(void *start, npy_intp num, void *varr)
{
    PyArrayObject *arr = (PyArrayObject *)varr;
    npy_intp elsize = PyArray_ITEMSIZE(arr);
    PyArray_CompareFunc *cmp = PyArray_DESCR(arr)->f->compare;
    char *tmp = (char *)malloc(elsize);
    char *a = (char *)start - elsize;
    npy_intp i, j, l;

    if (tmp == NULL) {
        return -NPY_ENOMEM;
    }

    for (l = num >> 1; l > 0; --l) {
        GENERIC_COPY(tmp, a + l * elsize, elsize);
        for (i = l, j = l << 1; j <= num;) {
            if (j < num &&
                cmp(a + j * elsize, a + (j + 1) * elsize, arr) < 0) {
                ++j;
            }
            if (cmp(tmp, a + j * elsize, arr) < 0) {
                GENERIC_COPY(a + i * elsize, a + j * elsize, elsize);
                i = j;
                j += j;
            }
            else {
                break;
            }
        }
        GENERIC_COPY(a + i * elsize, tmp, elsize);
    }

    for (; num > 1;) {
        GENERIC_COPY(tmp, a + num * elsize, elsize);
        GENERIC_COPY(a + num * elsize, a + elsize, elsize);
        num -= 1;
        for (i = 1, j = 2; j <= num;) {
            if (j < num &&
                cmp(a + j * elsize, a + (j + 1) * elsize, arr) < 0) {
                ++j;
            }
            if (cmp(tmp, a + j * elsize, arr) < 0) {
                GENERIC_COPY(a + i * elsize, a + j * elsize, elsize);
                i = j;
                j += j;
            }
            else {
                break;
            }
        }
        GENERIC_COPY(a + i * elsize, tmp, elsize);
    }

    free(tmp);
    return 0;
}

NPY_NO_EXPORT int
npy_aheapsort(void *vv, npy_intp *tosort, npy_intp n, void *varr)
{
    char *v = (char *)vv;
    PyArrayObject *arr = (PyArrayObject *)varr;
    npy_intp elsize = PyArray_ITEMSIZE(arr);
    PyArray_CompareFunc *cmp = PyArray_DESCR(arr)->f->compare;
    npy_intp *a, i, j, l, tmp;

    /* The array needs to be offset by one for heapsort indexing */
    a = tosort - 1;

    for (l = n >> 1; l > 0; --l) {
        tmp = a[l];
        for (i = l, j = l << 1; j <= n;) {
            if (j < n &&
                cmp(v + a[j] * elsize, v + a[j + 1] * elsize, arr) < 0) {
                ++j;
            }
            if (cmp(v + tmp * elsize, v + a[j] * elsize, arr) < 0) {
                a[i] = a[j];
                i = j;
                j += j;
            }
            else {
                break;
            }
        }
        a[i] = tmp;
    }

    for (; n > 1;) {
        tmp = a[n];
        a[n] = a[1];
        n -= 1;
        for (i = 1, j = 2; j <= n;) {
            if (j < n &&
                cmp(v + a[j] * elsize, v + a[j + 1] * elsize, arr) < 0) {
                ++j;
            }
            if (cmp(v + tmp * elsize, v + a[j] * elsize, arr) < 0) {
                a[i] = a[j];
                i = j;
                j += j;
            }
            else {
                break;
            }
        }
        a[i] = tmp;
    }

    return 0;
}

/***************************************
 * C > C++ dispatch
 ***************************************/

NPY_NO_EXPORT int
heapsort_bool(void *start, npy_intp n, void *NPY_UNUSED(varr))
{
    return heapsort_<npy::bool_tag>((npy_bool *)start, n);
}
NPY_NO_EXPORT int
heapsort_byte(void *start, npy_intp n, void *NPY_UNUSED(varr))
{
    return heapsort_<npy::byte_tag>((npy_byte *)start, n);
}
NPY_NO_EXPORT int
heapsort_ubyte(void *start, npy_intp n, void *NPY_UNUSED(varr))
{
    return heapsort_<npy::ubyte_tag>((npy_ubyte *)start, n);
}
NPY_NO_EXPORT int
heapsort_short(void *start, npy_intp n, void *NPY_UNUSED(varr))
{
    return heapsort_<npy::short_tag>((npy_short *)start, n);
}
NPY_NO_EXPORT int
heapsort_ushort(void *start, npy_intp n, void *NPY_UNUSED(varr))
{
    return heapsort_<npy::ushort_tag>((npy_ushort *)start, n);
}
NPY_NO_EXPORT int
heapsort_int(void *start, npy_intp n, void *NPY_UNUSED(varr))
{
    return heapsort_<npy::int_tag>((npy_int *)start, n);
}
NPY_NO_EXPORT int
heapsort_uint(void *start, npy_intp n, void *NPY_UNUSED(varr))
{
    return heapsort_<npy::uint_tag>((npy_uint *)start, n);
}
NPY_NO_EXPORT int
heapsort_long(void *start, npy_intp n, void *NPY_UNUSED(varr))
{
    return heapsort_<npy::long_tag>((npy_long *)start, n);
}
NPY_NO_EXPORT int
heapsort_ulong(void *start, npy_intp n, void *NPY_UNUSED(varr))
{
    return heapsort_<npy::ulong_tag>((npy_ulong *)start, n);
}
NPY_NO_EXPORT int
heapsort_longlong(void *start, npy_intp n, void *NPY_UNUSED(varr))
{
    return heapsort_<npy::longlong_tag>((npy_longlong *)start, n);
}
NPY_NO_EXPORT int
heapsort_ulonglong(void *start, npy_intp n, void *NPY_UNUSED(varr))
{
    return heapsort_<npy::ulonglong_tag>((npy_ulonglong *)start, n);
}
NPY_NO_EXPORT int
heapsort_half(void *start, npy_intp n, void *NPY_UNUSED(varr))
{
    return heapsort_<npy::half_tag>((npy_half *)start, n);
}
NPY_NO_EXPORT int
heapsort_float(void *start, npy_intp n, void *NPY_UNUSED(varr))
{
    return heapsort_<npy::float_tag>((npy_float *)start, n);
}
NPY_NO_EXPORT int
heapsort_double(void *start, npy_intp n, void *NPY_UNUSED(varr))
{
    return heapsort_<npy::double_tag>((npy_double *)start, n);
}
NPY_NO_EXPORT int
heapsort_longdouble(void *start, npy_intp n, void *NPY_UNUSED(varr))
{
    return heapsort_<npy::longdouble_tag>((npy_longdouble *)start, n);
}
NPY_NO_EXPORT int
heapsort_cfloat(void *start, npy_intp n, void *NPY_UNUSED(varr))
{
    return heapsort_<npy::cfloat_tag>((npy_cfloat *)start, n);
}
NPY_NO_EXPORT int
heapsort_cdouble(void *start, npy_intp n, void *NPY_UNUSED(varr))
{
    return heapsort_<npy::cdouble_tag>((npy_cdouble *)start, n);
}
NPY_NO_EXPORT int
heapsort_clongdouble(void *start, npy_intp n, void *NPY_UNUSED(varr))
{
    return heapsort_<npy::clongdouble_tag>((npy_clongdouble *)start, n);
}
NPY_NO_EXPORT int
heapsort_datetime(void *start, npy_intp n, void *NPY_UNUSED(varr))
{
    return heapsort_<npy::datetime_tag>((npy_datetime *)start, n);
}
NPY_NO_EXPORT int
heapsort_timedelta(void *start, npy_intp n, void *NPY_UNUSED(varr))
{
    return heapsort_<npy::timedelta_tag>((npy_timedelta *)start, n);
}

NPY_NO_EXPORT int
aheapsort_bool(void *vv, npy_intp *tosort, npy_intp n, void *NPY_UNUSED(varr))
{
    return aheapsort_<npy::bool_tag>((npy_bool *)vv, tosort, n);
}
NPY_NO_EXPORT int
aheapsort_byte(void *vv, npy_intp *tosort, npy_intp n, void *NPY_UNUSED(varr))
{
    return aheapsort_<npy::byte_tag>((npy_byte *)vv, tosort, n);
}
NPY_NO_EXPORT int
aheapsort_ubyte(void *vv, npy_intp *tosort, npy_intp n, void *NPY_UNUSED(varr))
{
    return aheapsort_<npy::ubyte_tag>((npy_ubyte *)vv, tosort, n);
}
NPY_NO_EXPORT int
aheapsort_short(void *vv, npy_intp *tosort, npy_intp n, void *NPY_UNUSED(varr))
{
    return aheapsort_<npy::short_tag>((npy_short *)vv, tosort, n);
}
NPY_NO_EXPORT int
aheapsort_ushort(void *vv, npy_intp *tosort, npy_intp n,
                 void *NPY_UNUSED(varr))
{
    return aheapsort_<npy::ushort_tag>((npy_ushort *)vv, tosort, n);
}
NPY_NO_EXPORT int
aheapsort_int(void *vv, npy_intp *tosort, npy_intp n, void *NPY_UNUSED(varr))
{
    return aheapsort_<npy::int_tag>((npy_int *)vv, tosort, n);
}
NPY_NO_EXPORT int
aheapsort_uint(void *vv, npy_intp *tosort, npy_intp n, void *NPY_UNUSED(varr))
{
    return aheapsort_<npy::uint_tag>((npy_uint *)vv, tosort, n);
}
NPY_NO_EXPORT int
aheapsort_long(void *vv, npy_intp *tosort, npy_intp n, void *NPY_UNUSED(varr))
{
    return aheapsort_<npy::long_tag>((npy_long *)vv, tosort, n);
}
NPY_NO_EXPORT int
aheapsort_ulong(void *vv, npy_intp *tosort, npy_intp n, void *NPY_UNUSED(varr))
{
    return aheapsort_<npy::ulong_tag>((npy_ulong *)vv, tosort, n);
}
NPY_NO_EXPORT int
aheapsort_longlong(void *vv, npy_intp *tosort, npy_intp n,
                   void *NPY_UNUSED(varr))
{
    return aheapsort_<npy::longlong_tag>((npy_longlong *)vv, tosort, n);
}
NPY_NO_EXPORT int
aheapsort_ulonglong(void *vv, npy_intp *tosort, npy_intp n,
                    void *NPY_UNUSED(varr))
{
    return aheapsort_<npy::ulonglong_tag>((npy_ulonglong *)vv, tosort, n);
}
NPY_NO_EXPORT int
aheapsort_half(void *vv, npy_intp *tosort, npy_intp n, void *NPY_UNUSED(varr))
{
    return aheapsort_<npy::half_tag>((npy_half *)vv, tosort, n);
}
NPY_NO_EXPORT int
aheapsort_float(void *vv, npy_intp *tosort, npy_intp n, void *NPY_UNUSED(varr))
{
    return aheapsort_<npy::float_tag>((npy_float *)vv, tosort, n);
}
NPY_NO_EXPORT int
aheapsort_double(void *vv, npy_intp *tosort, npy_intp n,
                 void *NPY_UNUSED(varr))
{
    return aheapsort_<npy::double_tag>((npy_double *)vv, tosort, n);
}
NPY_NO_EXPORT int
aheapsort_longdouble(void *vv, npy_intp *tosort, npy_intp n,
                     void *NPY_UNUSED(varr))
{
    return aheapsort_<npy::longdouble_tag>((npy_longdouble *)vv, tosort, n);
}
NPY_NO_EXPORT int
aheapsort_cfloat(void *vv, npy_intp *tosort, npy_intp n,
                 void *NPY_UNUSED(varr))
{
    return aheapsort_<npy::cfloat_tag>((npy_cfloat *)vv, tosort, n);
}
NPY_NO_EXPORT int
aheapsort_cdouble(void *vv, npy_intp *tosort, npy_intp n,
                  void *NPY_UNUSED(varr))
{
    return aheapsort_<npy::cdouble_tag>((npy_cdouble *)vv, tosort, n);
}
NPY_NO_EXPORT int
aheapsort_clongdouble(void *vv, npy_intp *tosort, npy_intp n,
                      void *NPY_UNUSED(varr))
{
    return aheapsort_<npy::clongdouble_tag>((npy_clongdouble *)vv, tosort, n);
}
NPY_NO_EXPORT int
aheapsort_datetime(void *vv, npy_intp *tosort, npy_intp n,
                   void *NPY_UNUSED(varr))
{
    return aheapsort_<npy::datetime_tag>((npy_datetime *)vv, tosort, n);
}
NPY_NO_EXPORT int
aheapsort_timedelta(void *vv, npy_intp *tosort, npy_intp n,
                    void *NPY_UNUSED(varr))
{
    return aheapsort_<npy::timedelta_tag>((npy_timedelta *)vv, tosort, n);
}

NPY_NO_EXPORT int
heapsort_string(void *start, npy_intp n, void *varr)
{
    return string_heapsort_<npy::string_tag>((npy_char *)start, n, varr);
}
NPY_NO_EXPORT int
heapsort_unicode(void *start, npy_intp n, void *varr)
{
    return string_heapsort_<npy::unicode_tag>((npy_ucs4 *)start, n, varr);
}

NPY_NO_EXPORT int
aheapsort_string(void *vv, npy_intp *tosort, npy_intp n, void *varr)
{
    return string_aheapsort_<npy::string_tag>((npy_char *)vv, tosort, n, varr);
}
NPY_NO_EXPORT int
aheapsort_unicode(void *vv, npy_intp *tosort, npy_intp n, void *varr)
{
    return string_aheapsort_<npy::unicode_tag>((npy_ucs4 *)vv, tosort, n,
                                               varr);
}
