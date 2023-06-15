# OSFNTC

![Build Status](https://travis-ci.com/user/repo.svg?branch=master)
![Coverage Status](https://coveralls.io/repos/github/user/repo/badge.svg?branch=master)

**O**pen-**S**ource **F**ramework for **N**umerically-**T**ailored **C**omputations

This project is "An Open-Source Framework for Efficient Numerically-Tailored Computations", hence the acronym **OSFNTC**.

The title of the corresponding paper is enclosed in quotation marks.

## Table of Contents
- [About the Paper](#about-the-paper)
- [Project Structure](#project-structure)
- [Installation](#installation)
- [Usage](#usage)
- [Contribution](#contribution)
- [License](#license)
- [Authors](#authors)

## About the Paper

We introduce a flexible open-source framework specifically designed to streamline efficient and numerically-optimized Matrix-Matrix Multiplications (MMMs). This framework offers two key features: Firstly, it provides an automated pipeline for precise arithmetic datapath generation, which enables the creation of highly customized systolic MMM kernels. Secondly, it allows for the effortless integration of these generated kernels into any user code, regardless of the programming language used, without any need for modifications.

We utilize this framework within a cutting-edge platform that consists of a Power9 host, an OpenCAPI link, and a Xilinx Virtex UltraScale+ FPGA. The framework exhibits a systemic improvement in terms of accuracy per energy cost across a range of High-Performance Computing (HPC) workloads. These workloads present diverse numerical requirements, including those found in Artificial Intelligence (AI) inference and Sea Surface Height (SSH) computation.

For AI inference, we consider a variety of leading-edge neural network models. These include ResNet18, ResNet34, ResNet50, DenseNet121, DenseNet161, DenseNet169, and VGG11. We use two datasets and two computer formats in combination with 27 distinct intermediate arithmetic datapaths. Our approach consistently reduces energy consumption across all scenarios. For instance, we achieve a reduction by factors of 3.3x for IEEE754-32 and 1.4x for Bfloat16 during ImageNet inference with ResNet50, all while maintaining accuracies of 82.3% and 86%, which are comparable to the results achieved with traditional Floating-Point Units (FPUs).

In the context of SSH computation, our methodology obtains fully reproducible results using double-precision words, which exceed the accuracy of traditional double- and quad-precision arithmetic in FPUs. Our approach increases SSH computation accuracy by a minimum of 5x and 27x compared to IEEE754-64 and IEEE754-128, respectively. As a result, we achieve improvements in accuracy per power cost by 5.6x and 15.1x, respectively.

## Project Structure

OSFNTC/
├── OpenBLAS/ # OpenBLAS library directory
├── PySigmoid/ # PySigmoid library directory
├── SoftPosit/ # SoftPosit library directory
├── eval/ # Evaluation scripts and data
├── flopoco/ # FloPoCo library directory
├── misc/ # Miscellaneous scripts and files
├── oc-accel/ # OpenCAPI acceleration framework directory
├── ocse/ # OpenCAPI Software Enablement directory
├── runs/ # Directory for run scripts and logs
├── sim_config/ # Simulation configuration files
├── .gitignore # Git ignore rules
├── Makefile # Makefile for building the project
├── README.md # This file, a concise overview of the project
├── history.txt # Revision history
├── requirements.txt # Python requirements
└── requirements_P9.txt # Python requirements for Power9

Please refer to the individual directories for additional readme files and more detailed explanations where applicable.

## Installation

Describe the installation process here. These are usually a set of commands to run or a link to the download page.

## Usage

Describe how to use the project after installation. Screenshots or code examples are often helpful.

## Contribution

If you're open to contributions, great! Here are some guidelines you can follow:

1. **Fork the Repository** - Click on the "Fork" button at the top-right of this page. This will create a copy of this repository in your GitHub account.

2. **Clone the Repository** - Now, go to your GitHub account, open the forked repository, click on the "Code" button and then click the "Copy to clipboard" icon.

Open a terminal and run the following git command:

\`\`\`
git clone "url you just copied"
\`\`\`

3. **Create a New Branch** - Change to the repository directory on your computer (if you are not already there):

\`\`\`
cd repository-name
\`\`\`

Now create a new branch using the `git checkout` command:

\`\`\`
git checkout -b your-new-branch-name
\`\`\`

4. **Make Necessary Changes and Commit Those Changes** - Now you can make changes in the source code. After you've made changes or added files, you can add those new files to your local repository, which we do with the `git add .` command:

\`\`\`
git add .
\`\`\`

Now we commit those changes with the `git commit` command:

\`\`\`
git commit -m "Commit message"
\`\`\`

5. **Push Changes to GitHub** - These changes are now in the HEAD of your local working copy. To send those changes to your remote repository, execute the following `git push` command:

\`\`\`
git push origin <your-branch-name>
\`\`\`

6. **Submit your Changes for Review** - If you go to your repository on GitHub, you'll see a `Compare & pull request` button. Click on that button and you'll be taken to a page where you can create a pull request.

## License

Describe the license under which your project is released.

## Authors

- Name [@githubhandle](https://github.com/githubhandle)
- Name [@githubhandle](https://github.com/githubhandle)


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
