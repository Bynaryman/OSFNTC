#!/usr/bin/env python
import argparse
import re
import subprocess
from datetime import date
import os

def parse_args():
	parser = argparse.ArgumentParser(description='Hardware Configuration')
	parser.add_argument('--M', required=True, type=str)
	parser.add_argument('--N', required=True, type=str)
	parser.add_argument('--arithmetic_in', required=True, type=str)
	parser.add_argument('--arithmetic_out', required=True, type=str)
	parser.add_argument('--msb', required=True, type=str)
	parser.add_argument('--lsb', required=True, type=str)
	parser.add_argument('--bits_ovf', required=True, type=str)
	parser.add_argument('--has_HSSD', required=True, type=str)
	parser.add_argument('--chunk_size', required=True, type=str)
	return parser.parse_args()

def get_bitwidths_and_type_from_ariths(args, arithmetic_in, arithmetic_out):
	array_arith_in = arithmetic_in.split(":")
	array_arith_out = arithmetic_out.split(":")
	arith_type = -1
	if array_arith_in[0] == "posit":
		bitwidth_in  = int(array_arith_in[1])
		arith_type = 3
	elif array_arith_in[0] == "ieee":
		bitwidth_in  = int(array_arith_in[1]) + int(array_arith_in[2]) + 1
		arith_type = 0
	elif array_arith_in[0] == "tfp":
		bitwidth_in  = int(array_arith_in[1]) + int(array_arith_in[2]) + 1
		arith_type = 1
	elif array_arith_in[0] == "bfloat16":
		bitwidth_in  = 16
		arith_type = 2
		array_arith_in.append("8")
		array_arith_in.append("7")
	else:
		bitwidth_in = 8
		arith_type = 4
	if array_arith_out[0] == "exact":
		bitwidth_out = int(args.msb) - int(args.lsb) + 1 + int(args.bits_ovf) + 1
	elif array_arith_out[0] == "same":
		bitwidth_out = bitwidth_in
	else:
		if array_arith_out[0] == "posit":
			bitwidth_out  = int(array_arith_out[1])
		elif array_arith_out[0] == "ieee":
			bitwidth_out  = int(array_arith_out[1]) + int(array_arith_out[2]) + 1
		elif array_arith_out[0] == "tfp":
			bitwidth_out  = int(array_arith_out[1]) + int(array_arith_out[2]) + 1
		elif array_arith_out[0] == "bfloat16":
			bitwidth_out  = 16
		else:
			bitwidth_out = 8

	return bitwidth_in, bitwidth_out, arith_type, int(array_arith_in[1]), int(array_arith_in[2])

"""
	@brief call flopoco and creates a SA depending on HW needed
	@param the parameters as namespace
"""
def create_SA(args):
	cmd = os.path.dirname(__file__) + "/libs/systolic_array/flopoco SystolicArray N={} M={} arithmetic_in={} arithmetic_out={} msb_summand={} lsb_summand={} nb_bits_ovf={} has_HSSD={} chunk_size={} frequency=270 target=VirtexUltrascalePlus name=SystolicArray".format(args.N,args.M,args.arithmetic_in,args.arithmetic_out,args.msb,args.lsb,args.bits_ovf,args.has_HSSD,args.chunk_size)
	result = subprocess.check_output(cmd, shell=True, stderr=subprocess.STDOUT)
	# print(result)
	a = re.search("Entity l2a\n.*?(\d+)", result.decode("utf-8"), re.M)
	b = re.search("Entity s3fdp\n.*?(\d+)", result.decode("utf-8"), re.M)
	S3FDP_ppDepth = b.group(1)
	try:
		LAICPT2_to_arith_ppDepth = a.group(1)
	except: # exact case has no pp as it does not exist
		print("EXACT")
		LAICPT2_to_arith_ppDepth = "0"
	return S3FDP_ppDepth, LAICPT2_to_arith_ppDepth

'''
	@brief build strings of hexadecimal to inject as description registers to be fetch by software
'''
def create_hexstrings(arith_type, N, M, arith_in_bitwidth, arith_in_param1, arith_in_param2):
	str_action_type = ""
	str_action_version = ""
	str_arith_type = f"{arith_type:02x}"
	str_arith_in_bitwidth = f"{arith_in_bitwidth:02x}"
	str_arith_in_param1 = f"{arith_in_param1:02x}"
	str_arith_in_param2 = f"{arith_in_param2:02x}"


	str_action_type += "86"                   # B3 action type is cgemm
	str_action_type += str_arith_in_bitwidth  # B2 is arith_bitwidth in bits
	str_action_type += "_"
	str_action_type += str_arith_type         # B1 arith type
	str_action_type += "00"                   # B0 accum type

	str_action_version += f"{N:02x}"          # B3 systolic N dimensions
	str_action_version += f"{M:02x}"          # B2 systolic N dimensions
	str_action_version += "_"
	str_action_version += str_arith_in_param1 # B1 arith param 1
	str_action_version += str_arith_in_param2 # B0 arith param 2

	print(str_action_type, str_action_version)
	return str_action_type, str_action_version



def replace_templates(args, S3FDP_ppDepth, LAICPT2_to_arith_ppDepth, bitwidth_in, bitwidth_out, arith_type, arith_in_param1, arith_in_param2):
	today = date.today()
	d1 = today.strftime("%d/%m/%Y")
	with open(os.path.dirname(__file__) + "/my_sv_wrapper.sv.template", "r") as orig_file:
		orig_content = orig_file.read()
		orig_content = orig_content.replace('[[CREATION_DATE]]', d1)
		orig_content = orig_content.replace('[[HW_EXTERNAL_DATA_WIDTH]]',str(1024))  # hardcoded 1024 at the moment
		orig_content = orig_content.replace('[[HW_CONFIG_ARITH_IN_WIDTH]]',str(bitwidth_in))
		orig_content = orig_content.replace('[[HW_CONFIG_ARITH_OUT_WIDTH]]',str(bitwidth_out))
		orig_content = orig_content.replace('[[HW_CONFIG_SA_N]]', args.N)
		orig_content = orig_content.replace('[[HW_CONFIG_SA_M]]', args.M)
		orig_content = orig_content.replace('[[HW_CONFIG_S3FDP_PP_DEPTH]]', S3FDP_ppDepth)
		orig_content = orig_content.replace('[[HW_CONFIG_L2A_PP_DEPTH]]', LAICPT2_to_arith_ppDepth)
		dest_content = orig_content
		with open(os.path.dirname(__file__) + "/my_sv_wrapper.sv", "w") as dest_file:
			dest_file.write(dest_content)

	action_type, action_version = create_hexstrings(arith_type, int(args.N), int(args.M), bitwidth_in, arith_in_param1, arith_in_param2)
	with open(os.path.dirname(__file__) + "/action_cgemm_capi3.vhd.template") as orig_file:
		orig_content = orig_file.read()
		orig_content = orig_content.replace('[[HW_CONFIG_ACTION_TYPE]]', action_type)
		orig_content = orig_content.replace('[[HW_CONFIG_ACTION_VERSION]]', action_version)
		dest_content = orig_content
		with open(os.path.dirname(__file__) + "/action_cgemm_capi3.vhd", "w") as dest_file:
			dest_file.write(dest_content)

def main():
	args = parse_args()
	bitwidth_in, bitwidth_out, arith_type, arith_in_param1, arith_in_param2 = get_bitwidths_and_type_from_ariths(args, args.arithmetic_in, args.arithmetic_out)
	S3FDP_ppDepth, LAICPT2_to_arith_ppDepth = create_SA(args)
	replace_templates(args, S3FDP_ppDepth, LAICPT2_to_arith_ppDepth,bitwidth_in, bitwidth_out, arith_type, arith_in_param1, arith_in_param2)

if __name__ == '__main__':
	main()
