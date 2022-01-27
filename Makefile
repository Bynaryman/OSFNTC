SHELL := /bin/zsh

all: install_numpy

install_openblas:
	make USE_OPENMP=1 USE_OCAPI=1 # nofortran should be 0 as some symbols are needed by pytorch and numpy
	sudo make install

install_numpy_old:
	git clone https://github.com/numpy/numpy.git \
	cp misc/numpy_site.cfg numpy/site.cfg
	pip install cython
	git submodule update --init
	python setup.py install

install_numpy:
	python -m venv ./venv/
	. ./venv/bin/activate
	git clone https://github.com/numpy/numpy
	cd numpy
	cp site.cfg.example site.cfg
	vim site.cfg # uncomment Openblas lines
	pip install Cython
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
	# for torchvision for generating the traces there is the need to downgrade the version of Pillow from 9.0 to 6.1


clean:

.PHONY: clean all
