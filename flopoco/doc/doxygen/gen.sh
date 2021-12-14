#! /bin/sh
HEADPATH="\.\./\.\./src/"
echo "INPUT =\\ " > activeops.txt
../../tools/EraseComments.sh ../../src/SourceFileList.txt /dev/stdout | sed "s:.*:$HEADPATH\0.hpp \\\:"  >> activeops.txt
doxygen	Doxyfile
rm activeops.txt
