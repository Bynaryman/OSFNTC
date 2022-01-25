#!/usr/bin/env python
import argparse
import json
import os
import sys
from PySigmoid import *
from math import *
import random
import struct
import ctypes
import numpy as np

# def binary(num):
#     # Struct can provide us with the float packed into bytes. The '!' ensures that
#     # it's in network byte order (big-endian) and the 'f' says that it should be
#     # packed as a float. Alternatively, for double-precision, you could use 'd'.
#     packed = struct.pack('!f', num)
#     print 'Packed: %s' % repr(packed)
#
#     # For each character in the returned string, we'll turn it into its corresponding
#     # integer code point
#     #
#     # [62, 163, 215, 10] = [ord(c) for c in '>\xa3\xd7\n']
#     integers = [ord(c) for c in packed]
#     print 'Integers: %s' % integers
#
#     # For each integer, we'll convert it to its binary representation.
#     binaries = [bin(i) for i in integers]
#     print 'Binaries: %s' % binaries
#
#     # Now strip off the '0b' from each of these
#     stripped_binaries = [s.replace('0b', '') for s in binaries]
#     print 'Stripped: %s' % stripped_binaries
#
#     # Pad each byte's binary representation's with 0's to make sure it has all 8 bits:
#     #
#     # ['00111110', '10100011', '11010111', '00001010']
#     padded = [s.rjust(8, '0') for s in stripped_binaries]
#     print 'Padded: %s' % padded
#
#     # At this point, we have each of the bytes for the network byte ordered float
#     # in an array as binary strings. Now we just concatenate them to get the total
#     # representation of the float:
#     return ''.join(padded)
#

def double_float_to_integer64(fp_number):
	# print(float(fp_number))
 #	packed=struct.pack('!d', fp_number)
 #	print(packed)
 #	#integers = [ord(c) for c in packed]
 #	int_tmp = 0
 #	#print(len(packed))
 #	for i,integer in enumerate(packed):
 #		print(integer)
 #		int_tmp = int_tmp | (integer << (len(packed)-1-i))
	return ctypes.c_uint.from_buffer(ctypes.c_double(float(fp_number))).value
	# return int_tmp

def read_in(path):
	config_and_data = None
	with open(path, 'r') as f:
		config_and_data = json.load(f)
	return config_and_data

def parse_args():
	parser = argparse.ArgumentParser(description='In and Out paths')
	parser.add_argument('--path_in', required=True, type=str, help='The input path of human readable data')
	parser.add_argument('--path_out', required=True, type=str, help='The output path for the raw data given to C')
	parser.add_argument('--P', required=True, type=int, help='the common dimension of input matrices')
	return parser.parse_args()

"""
	@brief takes a number and convert from a source encoding to a binary representation of a target encoding
	@param source_encoding int ieee posit
	@param target_encoding {tuple} (posit_width, posit_es)
"""
def type_cast(number, source_encoding, target_encoding):
	if target_encoding[0] == "posit":
		set_posit_env(target_encoding[1],target_encoding[2])
		if source_encoding == "int" or source_encoding == "ieee":  # the Posit constructor detects the type as int 32 bit or ieee 64 bits
			#print(Posit(number).number)
			return Posit(number).number
		if source_encoding == "posit":  # the input is directly the correct binary data
			return number
	else: # should be ieee<p1,p2>, tfp<p1,p2> or bfloat16 (coded as ieee<8,7>)
		# TODO(lledoux): import lib for arbitrary fp
		print(double_float_to_integer64(number))
		return double_float_to_integer64((number))

def prepare_data(config_and_data, path_out, P):

	matA = []
	matB = []

	# get the accelerator config
	N = config_and_data["HW_config"]["SA_height"]
	M = config_and_data["HW_config"]["SA_width"]
	arith_name = config_and_data["HW_config"]["SA_arithmetic"]["name"]
	param1 = config_and_data["HW_config"]["SA_arithmetic"]["param1"]
	param2 = config_and_data["HW_config"]["SA_arithmetic"]["param2"]
	capi_version = config_and_data["HW_config"]["CAPI"]

	# get the input data to send
	number_problems = config_and_data["data_in"]["number_problems"]
	encoding = config_and_data["data_in"]["encoding"]
	# bit_width = config_and_data["data_in"]["bit_width"]
	# exponent = config_and_data["data_in"]["exponent"]
	value_type = config_and_data["data_in"]["values"]

	if arith_name == "posit":
		arith_width_in = param1
	else:
		arith_width_in = 1+param1+param2


	# perform some checkings
	if ((N+M)*arith_width_in > 512 and capi_version==2) or ((N+M)*arith_width_in > 1024 and capi_version==3):
		raise Error("FPGA does not contain data-width conversion")

	# perform data conversion
	with open(path_out, "wb") as file_out:
		for batch in range(0,number_problems):
			# print(batch)
			for k in range(0,P):
				tmp_k_A = []
				tmp_k_B = []
				if k==0: # SOB
					if capi_version==3:
						bus_number=1<<1022
					elif capi_version==2:
						bus_number=1<<510
				elif k==P-1: # EOB
					if capi_version==3:
						bus_number=1<<1023
					elif capi_version==2:
						bus_number=1<<511
				else:
					bus_number=0
				for i in range(0,N):
					if value_type == "linpack_m1_to_p1":
						value = random.uniform(-1,1)
					elif value_type == "identity":
						value = int(i==k)
					else:
						value = 0
					tmp_k_A.append(value)
					bus_number = bus_number | (type_cast(value,encoding,(arith_name, param1, param2)) << (i*arith_width_in))
				for j in range(0,M):
					if value_type == "linpack_m1_to_p1":
						value = random.uniform(-1,1)
					elif value_type == "identity":
						value = int(j==k)
					else:
						value = 0
					tmp_k_B.append(value)
					bus_number = bus_number | (type_cast(value,encoding,(arith_name, param1, param2)) << ((j*arith_width_in) + (N*arith_width_in)))
				if capi_version==3:
					file_out.write(bus_number.to_bytes(128, 'little'))
				elif capi_version==2:
					file_out.write(bus_number.to_bytes(64, 'little'))
				else:
					raise Error("Bad CAPI version")
				matA.append(tmp_k_A)
				matB.append(tmp_k_B)
	return matA, matB

def main():
	args = parse_args()
	config_and_data = read_in(args.path_in)
	matA, matB = prepare_data(config_and_data, args.path_out, args.P)
	matA_T = np.transpose(matA)
	matB_T = np.transpose(matB)
	print(len(matA))
	print(len(matB))
	matC = np.matmul(matA_T, matB)
	print(matC)


if __name__ == '__main__':
	main()
