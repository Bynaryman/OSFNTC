#
# Copyright 2017 International Business Machines
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# Examples:
#   xcku060-ffva1156-2-e
#   xc7vx690tffg1157-2
#   xcku115-flva1517-2-e
#FPGACHIP    ?= xcku060-ffva1156-2-e
CONFIG_FILE = $(SNAP_ROOT)/.snap_config
HLS_ACTION_CLOCK_DEFAULT = 5
ifneq ("$(wildcard $(CONFIG_FILE))","")
  FPGACHIP = $(shell grep FPGACHIP $(CONFIG_FILE) | cut -d = -f 2 | tr -d '"')
  HLS_ACTION_CLOCK = $(shell grep HLS_CLOCK_PERIOD_CONSTRAINT $(CONFIG_FILE) | cut -d = -f 2 | tr -d 'ns"')
#  $(info FPGACHIP is set to $(FPGACHIP).)
endif

PART_NUMBER ?= $(FPGACHIP)

HLS_CFLAGS ?= ""

# The wrapper name must match a function in the HLS sources which is
# taken as entry point for the HDL generation.
WRAPPER ?= hls_action

syn_dir=$(SOLUTION_DIR)_$(PART_NUMBER)/$(SOLUTION_NAME)/syn
symlinks=vhdl report

# gcc test-bench stuff
objs = $(srcs:.cpp=.o)
CXX = g++
CXXFLAGS = -Wall -W -Wextra -Werror -O2 -DNO_SYNTH -Wno-unknown-pragmas -I../include

.PHONY: $(symlinks)

all: $(syn_dir) check

$(syn_dir): $(srcs) run_hls_script.tcl
	@if [ ! -d "$(SNAP_ROOT)/hardware/logs" ]; then \
		mkdir -p $(SNAP_ROOT)/hardware/logs; \
	fi
	@echo "   Clock period used for HLS is $(HLS_ACTION_CLOCK) ns"
	@if [ "X$(HLS_VITIS_USED)" = "XTRUE" ]; then  \
		echo "   Compiling action with Vitis HLS `vitis_hls -version|head -n1|cut -d " " -f 11`"; \
		vitis_hls -f run_hls_script.tcl > $(SNAP_ROOT)/hardware/logs/action_make.log; \
	else \
		echo "   Compiling action with Vivado HLS `vivado_hls -version|head -n1|cut -d " " -f 11`"; \
		vivado_hls -f run_hls_script.tcl > $(SNAP_ROOT)/hardware/logs/action_make.log; \
	fi
	$(RM) -rf $@/systemc $@/verilog

# Create symlinks for simpler access
$(symlinks): $(syn_dir)
	@$(RM) hls_syn_$@ && ln -s $(syn_dir)/$@ hls_syn_$@

run_hls_script.tcl: $(SNAP_ROOT)/actions/scripts/create_run_hls_script.sh
	$(SNAP_ROOT)/actions/scripts/create_run_hls_script.sh	\
		-n $(SOLUTION_NAME)		\
		-d $(SOLUTION_DIR) 		\
		-w $(WRAPPER)			\
		-p $(PART_NUMBER)		\
		-c $(HLS_ACTION_CLOCK)		\
		-f "$(srcs)" 			\
		-s $(SNAP_ROOT) 		\
		-x "$(HLS_CFLAGS)" > $@

$(SOLUTION_NAME): $(objs)
	$(CXX) -o $@ $^

# FIXME That those things are not resulting in an error is problematic.
#      If we get critical warnings we stay away from continuing now,
#      since that will according to our experience with vivado/vitis_hls, lead
#      to strange problems later on. So let us work on fixing the design
#      if they occur. Rather than challenging our luck.
#
# Check that last HLS compilation of the action was done with the same clock period. Exit if occurs.
# Check for CRITICAL warnings and exit if those occur. Add more if needed.
# Check for critical warnings and exit if those occur. Add more if needed.
# Check for reserved HLS MMIO reg at offset 0x17c.

check: $(syn_dir)
	@if [ "${HLS_ACTION_CLOCK}" != "${shell grep "Setting up clock" *_hls.log |cut -d " " -f 12|cut -d "n" -f 1}" ]; then \
			echo " ERROR with Vivado/Vitis HLS. HLS Action was last compiled with a different HLS clock."; \
			echo " Please force the recompilation with a 'make clean' command";  \
			echo " ---------------------------------------------------------- "; exit 1; \
	fi
	@echo -n "   Checking for critical warnings during HLS synthesis .... "
	@grep -A8 CRITICAL *_hls.log;  \
		test $$? = 1 
	@echo "OK"
	@if [ $(HLS_ACTION_CLOCK) == $(HLS_ACTION_CLOCK_DEFAULT) ]; then                \
		echo -n "   Checking for critical timings during HLS synthesis  .... ";    \
        	grep -A8 critical *_hls.log;     \
		if [ $$? -eq 0 ]; then \
		  echo "------------------------------------------------------------------ "; \
                  echo "TIMING ERROR: Please correct your action code before going further"!; \
                  echo "-- By experience, if HLS find a timing issue, further steps may fail."!; \
                  echo "-- In most case, you may need to recode your action in a different way."!; \
                  echo "-- You can modify in the menu the HLS clock period to over constraint HLS tool. "!; \
		  echo "------------------------------------------------------------------ "; exit -1; \
          	fi; \
	  	echo "OK";                                                                    \
	else \
		echo "   --------------------------------------------------------------------------- ";    \
		echo "   By defining a HLS clock different than the default 5ns, the automatic checking"; \
		echo "   of the critical timings is disabled. You need to manually check them."; \
		echo "   FYI action was compiled with following HLS clock:"; \
		grep "Setting up clock" *_hls.log ; \
		echo "   --------------------------------------------------------------------------- ";    \
		echo "   please CHECK the below list (if any) for HLS synthesis critical timing .... ";    \
		echo "   --------------------------------------------------------------------------- ";    \
        	grep -A8 critical *_hls.log ;     \
		echo "   --------------------------------------------------------------------------- ";    \
		if [ $$? -ne 0 ]; then \
	  	  echo "OK";                                                                    \
		fi; \
	fi
	@echo -n "   Checking for reserved MMIO area during HLS synthesis ... "
	@grep -A8 0x17c $(syn_dir)/vhdl/$(WRAPPER)_ctrl_reg_s_axi.vhd | grep reserved > \
		/dev/null; test $$? = 0;
	@echo "OK"
	@sleep 2; 

clean:
	@$(RM) -r $(SOLUTION_DIR)* run_hls_script.tcl *~ *.log \
		$(objs) $(SOLUTION_NAME)
	@for link in $(symlinks); do $(RM) hls_syn_$$link; done
