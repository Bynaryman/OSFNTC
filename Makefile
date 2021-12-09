SHELL := /bin/zsh

all: install_numpy

install_openblas:
	make USE_OPENMP=1
	sudo make install

install_numpy:
	git clone https://github.com/numpy/numpy.git \
	cp misc/numpy_site.cfg numpy/site.cfg
	pip install cython
	git submodule update --init
	python setup.py install

install_pytorch:
	git clone https://github.com/pytorch/pytorch
	git submodule sync
	git submodule update --init --recursive
	pip install pyyaml
	pip install typing_extensions
	source misc/script_add_stuff_to_venv.sh
	python setup.py install


clean:

.PHONY: clean all
