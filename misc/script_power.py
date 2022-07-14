#!/usr/bin/env python
import os
import time
cmd_get_volt_ampere = "sudo /home/lledoux/av2r/avr2util-s-4.9.0/avr2util-s -usbcom /dev/ttyACM0 display-sensors grep -E 'ADC00|ADC10' | awk '{print $11}'"

def main():
	starttime = time.time()
	seconds = 4.0
	while True:
		os.system(cmd_get_volt_ampere)
		time.sleep(seconds - ((time.time() - starttime) % seconds))

if __name__ == '__main__':
	main()
