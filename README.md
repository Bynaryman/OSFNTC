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

# misc
some pre-made configuration files for installation of other repositories
