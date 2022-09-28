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
need open blas installed in /usr
git clone --recursive https://github.com/OpenNMT/CTranslate2.git
mkdir build && cd build
cmake -DWITH_MKL=OFF -DWITH_OPENBLAS=ON -DOPENMP_RUNTIME=COMP -DENABLE_CPU_DISPATCH=OFF ..
make -j8
sudo make install

for python wrapper
set CTRANSLATE2_ROOT to build folder
export CTRANSLATE2_ROOT="$(pwd)"
cd python
pip install -r install_requirements.txt
python setup.py bdist_wheel
pip install dist/*.whl

for runtime
export LD_LIBRARY_PATH to build path
pip install  OpenNMT-py sentencepiece
wget https://s3.amazonaws.com/opennmt-models/transformer-ende-wmt-pyOnmt.tar.gz
tar xf transformer-ende-wmt-pyOnmt.tar.gz
ct2-opennmt-py-converter --model_path averaged-10-epoch.pt --output_dir ende_ctranslate2

# misc
some pre-made configuration files for installation of other repositories

## oc-accel_template


# script

## create bitstreams
as flopoco does not compile in EPI servers we generate the vhdls locally and send them in the corresponding scratchpad folders in EPI server.
Those folders are generated from a base template and duplicated according to a list of config to be evaluated
The folder names differ like action_config.sh that will generate my_sv_wrapper.sv and action_cgemm_capi3.vhd according to the config
