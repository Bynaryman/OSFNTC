
# Finding $SNAP_ROOT
ifndef SNAP_ROOT
# check if we are in sw folder of an action (three directories below snap root)
ifneq ("$(wildcard ../../../actions)","")
SNAP_ROOT=$(abspath ../../../)
else
$(info You are not building your software from the default directory (/path/to/snap/actions/<action_name>/sw) or specified a wrong $$SNAP_ROOT.)
$(error Please make sure that $$SNAP_ROOT is set up correctly.)
endif
endif

projs += gemm

# If you have the host code outside of the default snap directory structure,
# change to /path/to/snap/actions/software.mk
$(info SNAP ROOT : $(SNAP_ROOT))
include /home/binaryman/Documents/PhD/high_end/oc-accel/actions/software.mk
#include $(SNAP_ROOT)/actions/software.mk
