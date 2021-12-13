#!/bin/bash

# Required environment variable: $BUILD_ENVIRONMENT
# (This is set by default in the Docker images we build, so you don't
# need to set it yourself.

# shellcheck disable=SC2034
COMPACT_JOB_NAME="${BUILD_ENVIRONMENT}"

# shellcheck source=./common.sh
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"

echo "Clang version:"
clang --version

# detect_leaks=0: Python is very leaky, so we need suppress it
# symbolize=1: Gives us much better errors when things go wrong
export ASAN_OPTIONS=detect_leaks=0:symbolize=1:detect_odr_violation=0
if [ -n "$(which conda)" ]; then
  export CMAKE_PREFIX_PATH=/opt/conda
fi

# FIXME: Remove the hardcoded "-pthread" option.
# With asan build, the cmake thread CMAKE_HAVE_LIBC_CREATE[1] checking will
# succeed because "pthread_create" is in libasan.so. However, libasan doesn't
# have the full pthread implementation. Other advanced pthread functions doesn't
# exist in libasan.so[2]. If we need some pthread advanced functions, we still
# need to link the pthread library.
# This issue is already fixed in cmake 3.13[3]. If we use the newer cmake, we
# could remove this hardcoded option.
#
# [1] https://github.com/Kitware/CMake/blob/8cabaaf054a16ea9c8332ce8e9291bd026b38c62/Modules/FindThreads.cmake#L135
# [2] https://wiki.gentoo.org/wiki/AddressSanitizer/Problems
# [3] https://github.com/Kitware/CMake/commit/e9a1ddc594de6e6251bf06d732775dae2cabe4c8
#
# TODO: Make the ASAN flags a centralized env var and unify with USE_ASAN option
CC="clang" CXX="clang++" LDSHARED="clang --shared" \
  CFLAGS="-fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all -shared-libasan -pthread" \
  CXX_FLAGS="-pthread" \
  USE_ASAN=1 USE_CUDA=0 USE_MKLDNN=0 \
  python setup.py bdist_wheel
  python -mpip install dist/*.whl

# Test building via the sdist source tarball
python setup.py sdist
mkdir -p /tmp/tmp
pushd /tmp/tmp
tar zxf "$(dirname "${BASH_SOURCE[0]}")/../../dist/"*.tar.gz
cd torch-*
python setup.py build --cmake-only
popd

assert_git_not_dirty
