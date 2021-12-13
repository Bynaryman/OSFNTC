if "%BUILD_ENVIRONMENT%"=="" (
  set CONDA_PARENT_DIR=%CD%
) else (
  set CONDA_PARENT_DIR=C:\Jenkins
)
if "%REBUILD%"=="" (
  IF EXIST %CONDA_PARENT_DIR%\Miniconda3 ( rd /s /q %CONDA_PARENT_DIR%\Miniconda3 )
  curl --retry 3 -k https://repo.anaconda.com/miniconda/Miniconda3-latest-Windows-x86_64.exe --output %TMP_DIR_WIN%\Miniconda3-latest-Windows-x86_64.exe
  if errorlevel 1 exit /b
  if not errorlevel 0 exit /b
  %TMP_DIR_WIN%\Miniconda3-latest-Windows-x86_64.exe /InstallationType=JustMe /RegisterPython=0 /S /AddToPath=0 /D=%CONDA_PARENT_DIR%\Miniconda3
  if errorlevel 1 exit /b
  if not errorlevel 0 exit /b
)
call %CONDA_PARENT_DIR%\Miniconda3\Scripts\activate.bat %CONDA_PARENT_DIR%\Miniconda3
if "%REBUILD%"=="" (
  call conda install -y -q python=%PYTHON_VERSION% numpy cffi pyyaml boto3
  if errorlevel 1 exit /b
  if not errorlevel 0 exit /b
  call conda install -y -q -c conda-forge cmake
  if errorlevel 1 exit /b
  if not errorlevel 0 exit /b
  call conda install -y -q -c conda-forge libuv=1.39
  if errorlevel 1 exit /b
  if not errorlevel 0 exit /b
)

:: Get installed libuv path
@echo off
set libuv_ROOT=%CONDA_PARENT_DIR%\Miniconda3\Library
@echo on
echo libuv_ROOT=%libuv_ROOT%
