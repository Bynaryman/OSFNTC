# Some notes on hardware part
The interfacing from oc-accel to systolic arrays is made with a system verilog wrapper called my_sv_wrapper.
Additionally, some latencies have to be known to generate correct fifos. These latencies are known after a flopoco run.
This is the reason why there is a .template that will generate a .sv thanks to the scripts.

## folders
libs/ contains fifos and systolic arrrays
tcl/ contains the additional tcl scripts (if needed).

## freq and data width
for the moment 200MHz and 1024b bus are used and hardcoded in prepare_hw.py
