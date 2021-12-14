#!/bin/bash

yes | sudo apt update && sudo apt install -y subversion git cmake sollya wget g++ libsollya-dev flex bison libboost-all-dev autotools-dev autoconf automake f2c libblas-dev liblapack-dev libtool liblpsolve55-dev lp-solve

BASEDIR=$PWD

#WCPG
git clone https://scm.gforge.inria.fr/anonscm/git/metalibm/wcpg.git && cd wcpg && sh autogen.sh && ./configure && make && sudo make install && cd $BASEDIR

#ScaLP -- see the documentation to use other backends than lpsolve
svn checkout https://digidev.digi.e-technik.uni-kassel.de/home/svn/scalp/ && cd scalp/trunk && mkdir build && cd build && cmake -DUSE_LPSOLVE=ON -DLPSOLVE_LIBRARIES="/usr/lib/lp_solve/liblpsolve55.so" -DLPSOLVE_INCLUDE_DIRS="/usr/include/" .. && make && cd $BASEDIR

# PAGSuite for advanced shift-and-add SCM and MCM operators
svn checkout  https://digidev.digi.e-technik.uni-kassel.de/home/svn/pagsuite/

cd pagsuite/trunk/paglib && mkdir build && cd build && cmake .. && make -j2 && sudo make install  &&  cd $BASEDIR

cd pagsuite/trunk/oscm  && mkdir build && cd build && cmake .. && make -j2 && sudo make install  &&  cd $BASEDIR

cd pagsuite/trunk/rpag  && mkdir build && cd build && cmake .. && make -j2 && sudo make install  &&  cd $BASEDIR

#Finally FloPoCo itself, 
git clone https://scm.gforge.inria.fr/anonscm/git/flopoco/flopoco.git 
cd flopoco && mkdir build && cd build && cmake -DSCALP_PREFIX_DIR="$BASEDIR/scalp/trunk/" .. && make -j2 &&  cd $BASEDIR

# build the html documentation in doc/web. 
cd flopoco/build
./flopoco BuildHTMLDoc

# Pure luxury: bash autocompletion. This should be called only once
./flopoco BuildAutocomplete
mkdir ~/.bash_completion.d/
mv flopoco_autocomplete ~/.bash_completion.d/flopoco
echo ". ~/.bash_completion.d/flopoco" >> ~/.bashrc

# Now show the list of operators
./flopoco  
