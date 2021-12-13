/*
 * Copyright 2015,2017 International Business Machines
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <inttypes.h>
#include "TestAFU_config.h"
#include "tlx_interface_t.h"
#include "../../libocxl/libocxl.h"

#define ProcessControl_REGISTER 0x0018
#define PROCESS_CONTROL_RESTART 0x0000000000000001
#define ProcessInterruptControl_REGISTER 0x0020
#define ProcessInterruptObject_REGISTER 0x0028
#define ProcessInterruptData_REGISTER 0x0030
#define CACHELINE 128
#define MDEVICE "/dev/cxl/tlx0.0000:00:00.1.0"

static int verbose;
static unsigned int buffer_cl = 64;
static unsigned int timeout   = 1;

static void print_help(char *name)
{
    printf("\nUsage:  %s [OPTIONS]\n", name);
    printf("\t--cachelines\tCachelines to copy.  Default=%d\n", buffer_cl);
    printf("\t--timeout   \tDefault=%d seconds\n", timeout);
    printf("\t--verbose   \tVerbose output\n");
    printf("\t--help      \tPrint Usage\n");
    printf("\n");
}

int main(int argc, char *argv[])
{
    int opt, option_index, i;
    int rc;
    char *status, *rcacheline;
    ocxl_afu_h mafu_h;
    ocxl_irq_h irq_h;
    ocxl_irq_h err_irq_h;
    ocxl_event event;
    uint64_t irq_id;

    MachineConfig machine_config;
    MachineConfigParam config_param;

    static struct option long_options[] = {
	{"cachelines", required_argument, 0	  , 'c'},
	{"timeout",    required_argument, 0	  , 't'},
	{"verbose",    no_argument      , &verbose,   1},
	{"help",       no_argument      , 0	  , 'h'},
	{NULL, 0, 0, 0}
    };

    while((opt = getopt_long(argc, argv, "vhc:t:", long_options, &option_index)) >= 0 )
    {
	switch(opt)
	{
	    case 'v':
	  	break;
	    case 'c':
		buffer_cl = strtoul(optarg, NULL, 0);
		break;
	    case 't':
		timeout = strtoul(optarg, NULL, 0);
		break;
	    case 'h':
		print_help(argv[0]);
		return 0;
	    default:
		print_help(argv[0]);
		return 0;
	}
    }

    // initialize machine
    init_machine(&machine_config);

    // align and randomize cacheline
    if(posix_memalign((void**)&status, 128, 128) != 0) {
	perror("FAILED: posix_memalign for status");
	goto done;
    }
    if(posix_memalign((void**)&rcacheline, 128, 128) != 0) {
	perror("FAILED: posix_memalign for rcacheline");
	goto done;
    }

    for(i=0; i<CACHELINE; i++) {
	status[i] = 0x0;
	rcacheline[i] = rand();
    }
    
    //status[0]=0xff;
    // open master device
    printf("Calling ocxl_afu_open_dev\n");
    
    rc = ocxl_afu_open_from_dev(MDEVICE, &mafu_h);
    if(rc != 0) {
	perror("cxl_afu_open_dev: "MDEVICE);
	return -1;
    }
    
    // attach device
    printf("Attaching device ...\n");
    rc = ocxl_afu_attach(mafu_h);
    if(rc != 0) {
	perror("cxl_afu_attach:"MDEVICE);
	return rc;
    }

    printf("Attempt mmio mapping afu registers\n");
    if (ocxl_mmio_map(mafu_h, OCXL_MMIO_BIG_ENDIAN) != 0) {
	printf("FAILED: ocxl_mmio_map\n");
	goto done;
    }

    // initialize the error interrupt vector
//    err_irq_h = ocxl_afu_new_irq( mafu_h );
//    if(verbose)
//    	printf("initializing interrupt address = 0x%016lx\n", (uint64_t)err_irq_h);
//    ocxl_mmio_write64( mafu_h, ProcessInterruptObject_REGISTER, (uint64_t)err_irq_h );
//    if(verbose)
//    	printf("initializing interrupt control to intrp_req\n");
//    ocxl_mmio_write64( mafu_h, ProcessInterruptControl_REGISTER, 0x00);
//    if(verbose)
//    	printf("initializing interrupt data (unused by us)\n");
//    ocxl_mmio_write64( mafu_h, ProcessInterruptData_REGISTER, 0x00000000);

    rc = ocxl_afu_irq_alloc(mafu_h, NULL, &irq_h);
    irq_id = ocxl_afu_irq_get_id(mafu_h, irq_h);
    printf("Set irq (source) ea field = 0x%016lx\n", (uint64_t)irq_id);

    printf("Attempt Interrupt command\n");
    status[0] = 0xff;
    config_param.context = 0;
    config_param.enable_always = 1;
    config_param.mem_size = CACHELINE;
    config_param.command = AFU_CMD_INTRP_REQ;
    config_param.mem_base_address = (uint64_t)irq_id;
    config_param.machine_number = 0;
    config_param.status_address = (uint32_t)status;
    printf("status address = 0x%p\n", status);
    printf("command = 0x%x\n", config_param.command);
    printf("mem base address = 0x%"PRIx64"\n", config_param.mem_base_address);

    
    rc = config_enable_and_run_machine(mafu_h, &machine_config, config_param, DIRECTED);
    if( rc != -1) {
	printf("Response = 0x%x\n", rc);
	printf("config_enable_and_run_machine PASS\n");
    }
    else {
	printf("FAILED: config_enable_and_run_machine\n");
	goto done;
    }
//    timeout = 0;
//    while(status[0] != 0x0) {
//	printf("Polling read completion status = 0x%x\n", *status);
//    }

    rc = ocxl_afu_event_check(mafu_h, NULL, &event, 1);
    printf("Returned from ocxl_read_event -> there is an interrupt\n");
    if(rc == 0) {
	printf("Error retrieving interrupt event %d\n", rc);
  	return -1;
    }

    // when we see the interrupt event, need to write the restart bit of the Process Control Register 0x18[0]
    ocxl_mmio_write64(mafu_h, ProcessControl_REGISTER, PROCESS_CONTROL_RESTART);


    while(status[0] != 0x0) {
	printf("Polling ... completion status = 0x%x\n", *status);
    }
 
done:
    // free device
    printf("Freeing device ... \n");
    ocxl_afu_close(mafu_h);

    return 0;
}
