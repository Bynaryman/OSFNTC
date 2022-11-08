import os
import sys
sys.path.append('.')
from configs import configs

# 1. copy the config c opt_routed.dcp to a folder with the corresponding name

cmd_cp = "cp ./SA_200_{}_{}_{}_{}_m{}_{}_HSSD/hardware/build/Checkpoints/opt_routed_design.dcp ./opt_routed_dcps/SA_200_{}_{}_{}_{}_m{}_{}_HSSD.dcp" # N M arith_in msb lsb ovf

for c in configs:
	exec_cmd = cmd_cp.format(c[2], c[3], c[1].replace(":","_"), c[7], abs(c[8]), c[9], c[2], c[3], c[1].replace(":","_"), c[7], abs(c[8]), c[9])
	print(exec_cmd)
	os.system(exec_cmd)
