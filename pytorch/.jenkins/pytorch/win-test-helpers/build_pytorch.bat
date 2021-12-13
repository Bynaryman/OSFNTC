if "%DEBUG%" == "1" (
  set BUILD_TYPE=debug
) ELSE (
  set BUILD_TYPE=release
)

set PATH=C:\Program Files\CMake\bin;C:\Program Files\7-Zip;C:\ProgramData\chocolatey\bin;C:\Program Files\Git\cmd;C:\Program Files\Amazon\AWSCLI;C:\Program Files\Amazon\AWSCLI\bin;%PATH%

:: This inflates our log size slightly, but it is REALLY useful to be
:: able to see what our cl.exe commands are (since you can actually
:: just copy-paste them into a local Windows setup to just rebuild a
:: single file.)
set CMAKE_VERBOSE_MAKEFILE=1


set INSTALLER_DIR=%SCRIPT_HELPERS_DIR%\installation-helpers

call %INSTALLER_DIR%\install_mkl.bat
if errorlevel 1 exit /b
if not errorlevel 0 exit /b

call %INSTALLER_DIR%\install_magma.bat
if errorlevel 1 exit /b
if not errorlevel 0 exit /b

call %INSTALLER_DIR%\install_sccache.bat
if errorlevel 1 exit /b
if not errorlevel 0 exit /b

call %INSTALLER_DIR%\install_miniconda3.bat
if errorlevel 1 exit /b
if not errorlevel 0 exit /b

:: Install ninja and other deps
if "%REBUILD%"=="" ( pip install -q "ninja==1.10.0.post1" dataclasses typing_extensions "expecttest==0.1.3" )
if errorlevel 1 exit /b
if not errorlevel 0 exit /b

:: Override VS env here
pushd .
if "%VC_VERSION%" == "" (
    call "C:\Program Files (x86)\Microsoft Visual Studio\%VC_YEAR%\%VC_PRODUCT%\VC\Auxiliary\Build\vcvarsall.bat" x64
) else (
    call "C:\Program Files (x86)\Microsoft Visual Studio\%VC_YEAR%\%VC_PRODUCT%\VC\Auxiliary\Build\vcvarsall.bat" x64 -vcvars_ver=%VC_VERSION%
)
if errorlevel 1 exit /b
if not errorlevel 0 exit /b
@echo on
popd

if not "%USE_CUDA%"=="1" goto cuda_build_end

set CUDA_PATH=C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v%CUDA_VERSION%

if x%CUDA_VERSION:.=%==x%CUDA_VERSION% (
    echo CUDA version %CUDA_VERSION% format isn't correct, which doesn't contain '.'
    exit /b 1
)
rem version transformer, for example 10.1 to 10_1.
if x%CUDA_VERSION:.=%==x%CUDA_VERSION% (
    echo CUDA version %CUDA_VERSION% format isn't correct, which doesn't contain '.'
    exit /b 1
)
set VERSION_SUFFIX=%CUDA_VERSION:.=_%
set CUDA_PATH_V%VERSION_SUFFIX%=%CUDA_PATH%

set CUDNN_LIB_DIR=%CUDA_PATH%\lib\x64
set CUDA_TOOLKIT_ROOT_DIR=%CUDA_PATH%
set CUDNN_ROOT_DIR=%CUDA_PATH%
set NVTOOLSEXT_PATH=C:\Program Files\NVIDIA Corporation\NvToolsExt
set PATH=%CUDA_PATH%\bin;%CUDA_PATH%\libnvvp;%PATH%

set CUDNN_LIB_DIR=%CUDA_PATH%\lib\x64
set CUDA_TOOLKIT_ROOT_DIR=%CUDA_PATH%
set CUDNN_ROOT_DIR=%CUDA_PATH%
set NVTOOLSEXT_PATH=C:\Program Files\NVIDIA Corporation\NvToolsExt
set PATH=%CUDA_PATH%\bin;%CUDA_PATH%\libnvvp;%PATH%

:cuda_build_end

set DISTUTILS_USE_SDK=1
set PATH=%TMP_DIR_WIN%\bin;%PATH%

:: Target only our CI GPU machine's CUDA arch to speed up the build, we can overwrite with env var
:: default on circleci is Tesla T4 which has capability of 7.5, ref: https://developer.nvidia.com/cuda-gpus
:: jenkins has M40, which is 5.2
if "%TORCH_CUDA_ARCH_LIST%" == "" set TORCH_CUDA_ARCH_LIST=5.2

:: The default sccache idle timeout is 600, which is too short and leads to intermittent build errors.
set SCCACHE_IDLE_TIMEOUT=0
sccache --stop-server
sccache --start-server
sccache --zero-stats
set CC=sccache-cl
set CXX=sccache-cl

set CMAKE_GENERATOR=Ninja

if "%USE_CUDA%"=="1" (
  :: randomtemp is used to resolve the intermittent build error related to CUDA.
  :: code: https://github.com/peterjc123/randomtemp-rust
  :: issue: https://github.com/pytorch/pytorch/issues/25393
  ::
  :: CMake requires a single command as CUDA_NVCC_EXECUTABLE, so we push the wrappers
  :: randomtemp.exe and sccache.exe into a batch file which CMake invokes.
  curl -kL https://github.com/peterjc123/randomtemp-rust/releases/download/v0.4/randomtemp.exe --output %TMP_DIR_WIN%\bin\randomtemp.exe
  if errorlevel 1 exit /b
  if not errorlevel 0 exit /b
  echo @"%TMP_DIR_WIN%\bin\randomtemp.exe" "%TMP_DIR_WIN%\bin\sccache.exe" "%CUDA_PATH%\bin\nvcc.exe" %%* > "%TMP_DIR%/bin/nvcc.bat"
  cat %TMP_DIR%/bin/nvcc.bat
  set CUDA_NVCC_EXECUTABLE=%TMP_DIR%/bin/nvcc.bat
  for /F "usebackq delims=" %%n in (`cygpath -m "%CUDA_PATH%\bin\nvcc.exe"`) do set CMAKE_CUDA_COMPILER=%%n
  set CMAKE_CUDA_COMPILER_LAUNCHER=%TMP_DIR%/bin/randomtemp.exe;%TMP_DIR%\bin\sccache.exe
)

@echo off
echo @echo off >> %TMP_DIR_WIN%\ci_scripts\pytorch_env_restore.bat
for /f "usebackq tokens=*" %%i in (`set`) do echo set "%%i" >> %TMP_DIR_WIN%\ci_scripts\pytorch_env_restore.bat
@echo on

if "%REBUILD%" == "" (
  if NOT "%BUILD_ENVIRONMENT%" == "" (
    :: Create a shortcut to restore pytorch environment
    echo @echo off >> %TMP_DIR_WIN%/ci_scripts/pytorch_env_restore_helper.bat
    echo call "%TMP_DIR_WIN%/ci_scripts/pytorch_env_restore.bat" >> %TMP_DIR_WIN%/ci_scripts/pytorch_env_restore_helper.bat
    echo cd /D "%CD%" >> %TMP_DIR_WIN%/ci_scripts/pytorch_env_restore_helper.bat

    aws s3 cp "s3://ossci-windows/Restore PyTorch Environment.lnk" "C:\Users\circleci\Desktop\Restore PyTorch Environment.lnk"
    if errorlevel 1 exit /b
    if not errorlevel 0 exit /b
  )
)
:: tests if BUILD_ENVIRONMENT contains cuda11 as a substring
if not x%BUILD_ENVIRONMENT:cuda11=%==x%BUILD_ENVIRONMENT% (
   set BUILD_SPLIT_CUDA=ON
)

python setup.py install --cmake && sccache --show-stats && (
  if "%BUILD_ENVIRONMENT%"=="" (
    echo NOTE: To run `import torch`, please make sure to activate the conda environment by running `call %CONDA_PARENT_DIR%\Miniconda3\Scripts\activate.bat %CONDA_PARENT_DIR%\Miniconda3` in Command Prompt before running Git Bash.
  ) else (
    7z a %TMP_DIR_WIN%\%IMAGE_COMMIT_TAG%.7z %CONDA_PARENT_DIR%\Miniconda3\Lib\site-packages\torch %CONDA_PARENT_DIR%\Miniconda3\Lib\site-packages\caffe2 && copy /Y "%TMP_DIR_WIN%\%IMAGE_COMMIT_TAG%.7z" "%PYTORCH_FINAL_PACKAGE_DIR%\"
    if errorlevel 1 exit /b
    if not errorlevel 0 exit /b

    :: export test times so that potential sharded tests that'll branch off this build will use consistent data
    python test/run_test.py --export-past-test-times %PYTORCH_FINAL_PACKAGE_DIR%/.pytorch-test-times.json

    :: Also save build/.ninja_log as an artifact
    copy /Y "build\.ninja_log" "%PYTORCH_FINAL_PACKAGE_DIR%\"
  )
)

sccache --show-stats > stats.txt
python -m tools.stats.upload_sccache_stats stats.txt
rm stats.txt
