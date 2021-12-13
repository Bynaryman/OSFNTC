#!/bin/bash

set -ex

# Optionally install conda
if [ -n "$ANACONDA_PYTHON_VERSION" ]; then
  BASE_URL="https://repo.anaconda.com/miniconda"

  MAJOR_PYTHON_VERSION=$(echo "$ANACONDA_PYTHON_VERSION" | cut -d . -f 1)

  case "$MAJOR_PYTHON_VERSION" in
    2)
      CONDA_FILE="Miniconda2-latest-Linux-x86_64.sh"
    ;;
    3)
      CONDA_FILE="Miniconda3-latest-Linux-x86_64.sh"
    ;;
    *)
      echo "Unsupported ANACONDA_PYTHON_VERSION: $ANACONDA_PYTHON_VERSION"
      exit 1
      ;;
  esac

  mkdir /opt/conda
  chown jenkins:jenkins /opt/conda

  # Work around bug where devtoolset replaces sudo and breaks it.
  if [ -n "$DEVTOOLSET_VERSION" ]; then
    SUDO=/bin/sudo
  else
    SUDO=sudo
  fi

  as_jenkins() {
    # NB: unsetting the environment variables works around a conda bug
    # https://github.com/conda/conda/issues/6576
    # NB: Pass on PATH and LD_LIBRARY_PATH to sudo invocation
    # NB: This must be run from a directory that jenkins has access to,
    # works around https://github.com/conda/conda-package-handling/pull/34
    $SUDO -H -u jenkins env -u SUDO_UID -u SUDO_GID -u SUDO_COMMAND -u SUDO_USER env "PATH=$PATH" "LD_LIBRARY_PATH=$LD_LIBRARY_PATH" $*
  }

  pushd /tmp
  wget -q "${BASE_URL}/${CONDA_FILE}"
  chmod +x "${CONDA_FILE}"
  as_jenkins ./"${CONDA_FILE}" -b -f -p "/opt/conda"
  popd

  # NB: Don't do this, rely on the rpath to get it right
  #echo "/opt/conda/lib" > /etc/ld.so.conf.d/conda-python.conf
  #ldconfig
  sed -e 's|PATH="\(.*\)"|PATH="/opt/conda/bin:\1"|g' -i /etc/environment
  export PATH="/opt/conda/bin:$PATH"

  # Ensure we run conda in a directory that jenkins has write access to
  pushd /opt/conda

  # Track latest conda update
  as_jenkins conda update -y -n base conda

  # Install correct Python version
  as_jenkins conda install -y python="$ANACONDA_PYTHON_VERSION"

  conda_install() {
    # Ensure that the install command don't upgrade/downgrade Python
    # This should be called as
    #   conda_install pkg1 pkg2 ... [-c channel]
    as_jenkins conda install -q -y python="$ANACONDA_PYTHON_VERSION" $*
  }

  # Install PyTorch conda deps, as per https://github.com/pytorch/pytorch README
  # DO NOT install cmake here as it would install a version newer than 3.10, but
  # we want to pin to version 3.10.
  SCIPY_VERSION=1.1.0
  if [ "$ANACONDA_PYTHON_VERSION" = "3.9" ]; then
    # Install llvm-8 as it is required to compile llvmlite-0.30.0 from source
    conda_install numpy=1.19.2 astunparse pyyaml mkl mkl-include setuptools cffi future six llvmdev=8.0.0 -c conda-forge
    SCIPY_VERSION=1.6.0
  elif [ "$ANACONDA_PYTHON_VERSION" = "3.8" ]; then
    # Install llvm-8 as it is required to compile llvmlite-0.30.0 from source
    conda_install numpy=1.18.5 astunparse pyyaml mkl mkl-include setuptools cffi future six llvmdev=8.0.0
  elif [ "$ANACONDA_PYTHON_VERSION" = "3.7" ]; then
    # DO NOT install dataclasses if installing python-3.7, since its part of python-3.7 core packages
    conda_install numpy=1.18.5 astunparse pyyaml mkl mkl-include setuptools cffi future six typing_extensions
  else
    conda_install numpy=1.18.5 astunparse pyyaml mkl mkl-include setuptools cffi future six dataclasses typing_extensions
  fi

  # Magma package names are concatenation of CUDA major and minor ignoring revision
  # I.e. magma-cuda102 package corresponds to CUDA_VERSION=10.2 and CUDA_VERSION=10.2.89
  if [ -n "$CUDA_VERSION" ]; then
    conda_install magma-cuda$(TMP=${CUDA_VERSION/./};echo ${TMP%.*[0-9]}) -c pytorch
  fi

  # TODO: This isn't working atm
  conda_install nnpack -c killeent

  # Install some other packages, including those needed for Python test reporting
  # TODO: Why is scipy pinned
  # Pin MyPy version because new errors are likely to appear with each release
  # Pin hypothesis to avoid flakiness: https://github.com/pytorch/pytorch/issues/31136
  as_jenkins pip install --progress-bar off pytest \
    scipy==$SCIPY_VERSION \
    scikit-image \
    psutil \
    unittest-xml-reporting \
    boto3==1.16.34 \
    hypothesis==4.53.2 \
    expecttest==0.1.3 \
    mypy==0.812 \
    tb-nightly

  # Install numba only on python-3.8 or below
  # For numba issue see https://github.com/pytorch/pytorch/issues/51511
  if [[ $(python -c "import sys; print(int(sys.version_info < (3, 9)))") == "1" ]]; then
    as_jenkins pip install --progress-bar off numba librosa>=0.6.2
  else
    as_jenkins pip install --progress-bar off numba==0.49.0 librosa>=0.6.2
  fi

  # Update scikit-learn to a python-3.8 compatible version
  if [[ $(python -c "import sys; print(int(sys.version_info >= (3, 8)))") == "1" ]]; then
    as_jenkins pip install --progress-bar off -U scikit-learn
  else
    # Pinned scikit-learn due to https://github.com/scikit-learn/scikit-learn/issues/14485 (affects gcc 5.5 only)
    as_jenkins pip install --progress-bar off scikit-learn==0.20.3
  fi

  popd
fi
