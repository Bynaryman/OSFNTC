#!/bin/bash

BASEDIR=$PWD
yes | sudo apt-get install make g++ libgmp3-dev libmpfr-dev libxml2-dev bison libmpfi-dev flex cmake libboost-all-dev libgsl0-dev

#FPLLL
wget https://gforge.inria.fr/frs/download.php/file/34429/libfplll-3.0.12.tar.gz && tar xzf libfplll-3.0.12.tar.gz && cd libfplll-3.0.12/ && ./configure && make -j2 && sudo make install	&& cd ..

#Sollya
wget https://gforge.inria.fr/frs/download.php/28571/sollya-3.0.tar.gz && tar xzf sollya-3.0.tar.gz && cd sollya-3.0/ && ./configure && make -j2 && sudo make install && cd ..


#Finally FloPoCo itself, 
#wget https://gforge.inria.fr/frs/download.php/31858/flopoco-2.4.0.tgz && gunzip < flopoco-2.4.0.tgz | tar xvf - && cd flopoco-2.4.0/ && cmake . && make -j2
wget https://gforge.inria.fr/frs/download.php/file/32591/flopoco-2.5.0.tgz && gunzip < flopoco-2.5.0.tgz | tar xvf - && cd flopoco-2.5.0/ && cmake . && make -j2

# Now show the list of operators -- disabled because it doesn't return 0, hence breaks docker install
#./flopoco  
