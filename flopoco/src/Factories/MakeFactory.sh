#! /bin/sh
# This file is called by CMake, see ADD_CUSTOM_COMMAND
# First arg is the path to src/Factories
# second arg is the destination file
cd $1
touch /tmp/OK.txt
#../../tools/EraseComments.sh Interfaced.txt InterfacedNoComments.txt
# and indent. Comments are #<text>\n

sed -e "s/#.*$//; s/^[[:space:]]*// ; s/[[:space:]]*$//; /^$/d" Interfaced.txt > InterfacedNoComments.txt

sed "s/.*/	DEF(&)\;/" InterfacedNoComments.txt > headers.txt
# elt -> DEF(elt);
sed "s/.*/			&::registerFactory()\;/" InterfacedNoComments.txt > registration.txt
cat head.txt headers.txt middle.txt registration.txt tail.txt > $2

rm headers.txt registration.txt InterfacedNoComments.txt
