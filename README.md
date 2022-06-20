# ocse
modified libocxl/Makefile to enforce pulling of misc/ocxl

# oc-accel

# openblas
make USE_OPENMP=1
sudo make install (will put on default path in /opt which is ok)

# openmp

# pytorch
git submodule sync
git submodule update --init --recursive
pip install pyyaml
pip install typing_extensions
source misc/script_add_stuff_to_venv.sh
python setup.py install

# numpy
pip install cython
git submodule update --init

# flopoco
embeds all the code to generate systolic arrays with automated pipelining
specific command to make it work are located in a .txt file in flopoco/

# PySigmoid
our modified PySigmoid to handle any kind of accumulators.
Among other things, we use  the library to generate input matrices

# OpenNMT Ctranslate2
git clone --recursive https://github.com/OpenNMT/CTranslate2.git
mkdir build && cd build
cmake -DWITH_MKL=OFF -DWITH_OPENBLAS=ON -DOPENMP_RUNTIME=COMP ..
make -j8
sudo make install

for python wrapper
set CTRANSLATE2_ROOT to build folder
cd python
pip install -r install_requirements.txt
python setup.py bdist_wheel
pip install dist/*.whl

to run, then export LD_LIBRARY_PATH to build path

# misc
some pre-made configuration files for installation of other repositories
