import os
import sys
sys.path.append('~/Documents/PhD/high_end/misc/')
from configs import configs

# 1. copy recursively the oc-accel template into /tmp for each config in configs
# 2. call action_config.sh in each of these folders to prepare the HW
# 3. folders are ready to be sent (over SSH) to compute server (e.g. EPI) to build bitstreams

cmd_cp_r = "cp -r ~/Documents/PhD/high_end/misc/oc-accel_template_for_bitstream_gen /tmp/SA_200_{}_{}_{}_{}_m{}_{}_HSSD" # N M arith_in msb lsb ovf
cmd_prep_hw = '/tmp/SA_200_{}_{}_{}_{}_m{}_{}_HSSD/actions/cgemm/hw/action_config.sh {} {} {} {} {} {} {} {}'

fpga_chip = "xcvu3p"

for c in configs:
	exec_cmd = cmd_cp_r.format(c[2], c[3], c[1], c[7], abs(c[8]), c[9])
	print(exec_cmd)
	os.system(exec_cmd)

	action_path="/tmp/SA_200_{}_{}_{}_{}_m{}_{}_HSSD/actions/cgemm/".format(c[2],c[3],c[1],c[7],abs(c[8]),c[9])
	exec_cmd = cmd_prep_hw.format(c[2], c[3], c[1], c[7], abs(c[8]), c[9],c[2], c[3], c[1], c[7], c[8], c[9], action_path, fpga_chip)
	print(exec_cmd)
	os.system(exec_cmd)
