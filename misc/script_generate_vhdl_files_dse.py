import os
import sys
sys.path.append('~/Documents/PhD/high_end/misc/')
from configs import configs

# 1. copy recursively the oc-accel template into /tmp for each config in configs
# 2. call action_config.sh in each of these folders to prepare the HW
# 3. folders are ready to be sent (over SSH) to compute server (e.g. EPI) to build bitstreams

cmd_cp_r = "cp -r ~/Documents/PhD/high_end/misc/oc-accel_template_for_bitstream_gen /tmp/SA_200_{}_{}_{}_{}_m{}_{}_HSSD" # N M arith_in msb lsb ovf
cmd_prep_hw = '/tmp/SA_200_{}_{}_{}_{}_m{}_{}_HSSD/actions/cgemm/hw/action_config.sh'
cmd_clean = "rm ./BitHeap* ./vivado*.log"

fpga_chip = "xcvu3p-ffvc1517-2-i"

for c in configs:
	exec_cmd = cmd_cp_r.format(c[2], c[3], c[1].replace(":","_"), c[7], abs(c[8]), c[9])
	print(exec_cmd)
	os.system(exec_cmd)

	action_path="/tmp/SA_200_{}_{}_{}_{}_m{}_{}_HSSD/actions/cgemm/".format(c[2],c[3],c[1].replace(":","_"),c[7],abs(c[8]),c[9])
	with open(action_path + "/hw/action_config.sh", "r+") as orig_file:
		orig_content = orig_file.read()
		orig_content = orig_content.replace('$1', str(c[2]))
		orig_content = orig_content.replace('$2', str(c[3]))
		orig_content = orig_content.replace('$3', c[1])
		orig_content = orig_content.replace('$4', str(c[7]))
		orig_content = orig_content.replace('$5', str(c[8]))
		orig_content = orig_content.replace('$6', str(c[9]))
		orig_content = orig_content.replace('$7', '$(dirname "$0")/../')
		orig_content = orig_content.replace('$8', fpga_chip)
		dest_content = orig_content
		#with open(os.path.dirname(__file__) + "/my_sv_wrapper.sv", "w") as dest_file:
		orig_file.seek(0)
		orig_file.write(dest_content)

	exec_cmd = cmd_prep_hw.format(c[2], c[3], c[1].replace(":","_"), c[7], abs(c[8]), c[9])
	print(exec_cmd)
	os.system(exec_cmd)

	exec_cmd = cmd_clean
	print(exec_cmd)
	os.system(exec_cmd)
