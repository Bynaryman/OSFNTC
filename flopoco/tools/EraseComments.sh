#! /bin/sh
# ./EraseComments f1 f2 creates a file f2 without comments, blank lines
# and indent. Comments are #<text>\n

sed -e "s/#.*$//; s/^[[:space:]]*// ; s/[[:space:]]*$//; /^$/d" $1 > $2
