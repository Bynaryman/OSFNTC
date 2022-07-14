#!/usr/bin/env python
#import os
import subprocess
import time
cmd_get_volt_ampere = "sudo /home/lledoux/av2r/avr2util-s-4.9.0/avr2util-s -usbcom /dev/ttyACM0 display-sensors | grep -E 'ADC00|ADC10' | awk '{print $11}'"

def main():
	starttime = time.time()
	seconds = 1.0
	while True:
		result = subprocess.check_output(cmd_get_volt_ampere, shell=True, stderr=subprocess.STDOUT)
		array_result = result.split("\n")[0]
		volts = float(array_result[0])
		amperes = float(array_result[1])
		watts = volts*amperes
		print(watts)
		time.sleep(seconds - ((time.time() - starttime) % seconds))

if __name__ == '__main__':
	main()
