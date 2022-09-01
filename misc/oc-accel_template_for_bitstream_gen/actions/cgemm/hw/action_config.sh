#!/bin/bash
N=$1
M=$2
arithmetic_in=$3
arithmetic_out="same"
msb=$4
lsb=$5
bits_ovf=$6
has_HSSD="true"
chunk_size="-1"

ACTION_ROOT=$7
FPGACHIP=$8

# Some addtional code generation or automation taks can be put here.
echo "                        action config says ACTION_ROOT is $ACTION_ROOT"
echo "                        action config says FPGACHIP is $FPGACHIP"
echo "                        action config says Systolic Array N is $N"
echo "                        action config says Systolic Array M is $M"
echo "                        action config says arithmetic_in is $arithmetic_in"
echo "                        action config says arithmetic_out is $arithmetic_out"
echo "                        action config says msb is $msb"
echo "                        action config says lsb is $lsb"
echo "                        action config says bits_ovf is $bits_ovf"
echo "                        action config says has_HSSD is $has_HSSD"
echo "                        action config says chunk_size is $chunk_size"

if [ ! -d $ACTION_ROOT/ip/action_ip_dir ]; then
	echo "                        Call create_action_ip.tcl to generate IPs"
	vivado -mode batch -source $ACTION_ROOT/ip/create_action_ip.tcl -notrace -nojournal -tclargs $ACTION_ROOT $FPGACHIP
fi

echo "                        CREATING SA"
python3 $ACTION_ROOT/hw/prepare_hw.py --N $N --M $M --arithmetic_in $arithmetic_in --arithmetic_out $arithmetic_out --msb $msb --lsb $lsb --bits_ovf $bits_ovf --has_HSSD $has_HSSD --chunk_size $chunk_size
mv flopoco.vhdl $ACTION_ROOT/hw/libs/systolic_array/

echo "                        CLEANING TEMP FILES"
rm -r $ACTION_ROOT/hw/dot
rm $ACTION_ROOT/hw/BitHeap*
rm $ACTION_ROOT/hw/vivado*

