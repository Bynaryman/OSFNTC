# ocse

OpenCAPI Simulation Engine

Copyright 2015,2017 International Business Machines

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

--- ATTENTION ATTENTION ATTENTION ---
issue12: changes to tlx interface will require changes to your afu
issue13: changes to libocxl will require changes to your host application

This is a simulation environment that allows a programmer to use the opencapi reference user api (libocxl) in their 
software to communicate with an accelerator design that is running in a 3rd party event simulator.  This permits a 
degree of hardware/software co-verification.  The accelerator design must use the opencapi reference tlx hardware 
interface and protocol to communicate with ocse.  The reference tlx hardware interface supported was version 0.5 
dated 12 October 2017.  

NOTE: we currently include common/misc/ocxl.h.  When the Linux Technical Center creates a "generic reference kernel 
driver," they will likely deliver the official version of ocxl.h.  At that time, due to differences in licensing 
terms, we will no longer be allowed to distribute ocxl.h.  Our Makefiles will be adjusted to obtain a copy of the 
linux ocxl.h from the linux repository to statisfy the various compile steps.

See QUICK_START for general instructions on how to start the evironment.  You will need to understand your specific
vendor simulator to perform some of the steps specific to your simulator.

Demo kit(s) (sample, toy designs) are being provided to allow you to build the environment, a design, and a
host application.  This should a) allow you to validate your installation, and b) provide some coding examples
of how to used various interfaces.  Please note that the demo kit designs are not exhaustively tested, best in class
designs.  While they may be used to start your own design, they are not intended to be the final answer to your
specific acceleration problem.

version 1.0 Known limitations:
	- we've simulated with Cadence NCSim, Xilinx Xsim, Synopsys VCS, and Mentor Graphics Questa
	- we allow up to 16 tlx event simulations (numbered tlx0 to tlxf in shim_host.dat)
	- ocse performs a subset of the discovery and configuration process.  
	      - we think it does enough to give you a good idea the configuration is working
	      - feedback is certainly welcome
	- ocxl_afu_open_specific is not supported
	- ocxl_afu_open_by_id is not supported
	- ocxl_afu_use* are not supported
	- ocxl_lpc_read and _write require offsets and sizes that are naturally aligned.
	- the afu is required to send a complete response to a command from the host; that is, no partial responses
	- ocse always generates a complete response.
	
