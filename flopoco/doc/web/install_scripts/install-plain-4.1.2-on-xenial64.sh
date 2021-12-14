#!/bin/bash

yes | sudo apt-get update && sudo apt-get install g++ libgmp3-dev libmpfr-dev libfplll-dev libxml2-dev bison libmpfi-dev flex cmake libboost-all-dev libgsl0-dev

wget https://gforge.inria.fr/frs/download.php/33151/sollya-4.1.tar.gz   && tar xzf sollya-4.1.tar.gz && cd sollya-4.1/   && ./configure && make  && sudo make install   && cd ..

wget   https://gforge.inria.fr/frs/download.php/file/37213/flopoco-4.1.2.tgz   && tar xzf flopoco-4.1.2.tgz && cd flopoco-4.1.2/  && cmake . && make

# build the html documentation in doc/web. 
./flopoco BuildHTMLDoc

# Pure luxury: bash autocompletion. This should be called only once
./flopoco BuildAutocomplete
mkdir ~/.bash_completion.d/
mv flopoco_autocomplete ~/.bash_completion.d/flopoco
echo ". ~/.bash_completion.d/flopoco" >> ~/.bashrc

# Now show the list of operators
./flopoco  
