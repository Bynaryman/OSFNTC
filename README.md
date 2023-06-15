# Project Title

![Build Status](https://travis-ci.com/user/repo.svg?branch=master)
![Coverage Status](https://coveralls.io/repos/github/user/repo/badge.svg?branch=master)

Brief description of the project and its purpose. Describe what your project does and what it is used for.

## Table of Contents
- [About the Paper](#about-the-paper)
- [Project Structure](#project-structure)
- [Installation](#installation)
- [Usage](#usage)
- [Contribution](#contribution)
- [License](#license)
- [Authors](#authors)

## About the Paper

(Write about the scientific paper this project is based on. Explain the main findings, the methodologies used, and the implications for this project.)

## Project Structure

Here's a basic project structure:

\`\`\`
/project
  /src
    /module1
    /module2
  /tests
    /module1
    /module2
  /docs
  /data
\`\`\`

- `src`: Source code files
- `tests`: Test code files
- `docs`: Documentation files
- `data`: Data files

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
