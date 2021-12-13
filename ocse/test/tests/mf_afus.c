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
#include <time.h>

#define ProcessControl_REGISTER 0x0018
#define PROCESS_CONTROL_RESTART 0x0000000000000001
#define ProcessInterruptControl_REGISTER 0x0020
#define ProcessInterruptObject_REGISTER 0x0028
#define ProcessInterruptData_REGISTER 0x0030

#define CACHELINE 128
#define FUNCTION1 "/dev/cxl/tlx0.0000:00:00.1.0"
#define FUNCTION2 "/dev/cxl/tlx0.0000:00:00.2.0"
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
    struct timespec t;
    int opt, option_index, i;
    int rc;
    char *rcacheline, *wcacheline;
    char *status;
    ocxl_afu_h afu1_h, afu2_h;
    ocxl_irq_h irq_h;
    //ocxl_irq_h err_irq_h;
    ocxl_event event;
    MachineConfig machine_config;
    MachineConfigParam config_param;
    uint64_t irq_id;

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

    t.tv_sec = 0;
    t.tv_nsec = 100000;
    // initialize machine
    init_machine(&machine_config);

    // align and randomize cacheline
    if (posix_memalign((void**)&rcacheline, CACHELINE, CACHELINE) != 0) {
	perror("FAILED: posix_memalign for rcacheline");
	goto done;
    }
    if (posix_memalign((void**)&wcacheline, CACHELINE, CACHELINE) != 0) {
	perror("FAILED: posix_memalign for wcacheline");
	goto done;
    }
    if(posix_memalign((void**)&status, 128, 128) != 0) {
	perror("FAILED: posix_memalign for status");
	goto done;
    }

    printf("rcacheline = 0x");
    for(i=0; i<CACHELINE; i++) {
	rcacheline[i] = rand();
	wcacheline[i] = 0x0;
	status[i] = 0x0;
	printf("%02x", (uint8_t)rcacheline[i]);
    }
    printf("\n");
    
    //status[0]=0xff;
    // open master device
    printf("Attempt open device for afu1\n");
    
    rc = ocxl_afu_open_from_dev(FUNCTION1, &afu1_h);
    if(rc != 0) {
	perror("cxl_afu_open_dev: for afu1"FUNCTION1);
	return -1;
    }
     
    // attach device
    printf("Attaching afu1 device ...\n");
    rc = ocxl_afu_attach(afu1_h);
    if(rc != 0) {
	perror("cxl_afu_attach:"FUNCTION1);
	return rc;
    }

    printf("Attempt mmio mapping afu registers\n");
    if (ocxl_mmio_map(afu1_h, OCXL_MMIO_BIG_ENDIAN) != 0) {
	printf("FAILED: ocxl_mmio_map afu1\n");
	goto done;
    }
    printf("Attempt Read command\n");
    config_param.context = 0x0;
    config_param.enable_always = 1;
    config_param.mem_size = CACHELINE;
    config_param.command = AFU_CMD_RD_WNITC;
    config_param.mem_base_address = (uint64_t)rcacheline;
    config_param.machine_number = 0;
    config_param.status_address = (uint32_t)status;
    printf("status address = 0x%p\n", status);
    printf("rcacheline = 0x%p\n", rcacheline);
    printf("command = 0x%x\n", config_param.command);
    printf("mem base address = 0x%"PRIx64"\n", config_param.mem_base_address);
    rc = config_enable_and_run_machine(afu1_h, &machine_config, config_param, DIRECTED);
    printf("set status data = 0xff\n");
    //status[0] = 0xff;
    if( rc != -1) {
	printf("Response = 0x%x\n", rc);
	printf("config_enable_and_run_machine afu1 PASS\n");
    }
    else {
	printf("FAILED: config_enable_and_run_machine afu1\n");
	goto done;
    }
    timeout = 0;
    while(status[0] != 0x0) {
	nanosleep(&t, &t);
	printf("Polling read completion status = 0x%x\n", *status);
    }
    // clear machine config
    rc = clear_machine_config(afu1_h, &machine_config, config_param, DIRECTED);
    if(rc != 0) {
	printf("Failed to clear machine config afu1\n");
	goto done;
    }
    // Attemp write command
    printf("Attempt Write command\n");
    config_param.command = AFU_CMD_DMA_W;
    config_param.mem_size = 64;
    config_param.mem_base_address = (uint64_t)wcacheline;
    printf("wcacheline = 0x%p\n", wcacheline);
    printf("command = 0x%x\n",config_param.command);
    printf("wcache address = 0x%"PRIx64"\n", config_param.mem_base_address);

    rc = config_enable_and_run_machine(afu1_h, &machine_config, config_param, DIRECTED);
    if(rc != -1) {
	printf("Response = 0x%x\n", rc);
 	printf("config_enable_and_run_machine afu1 PASS\n");
    }
    else {
	printf("FAILED: config_enable_and_run_machine afu1\n");
	goto done;
    }

    printf("set status data = 0xff\n");
    status[0] = 0xff;
    while(status[0] != 0x00) {
	nanosleep(&t, &t);
	printf("Polling write completion status = 0x%x\n", *status);
    }

    // clear machine config
    rc = clear_machine_config(afu1_h, &machine_config, config_param, DIRECTED);
    if(rc != 0) {
	printf("Failed to clear machine config afu1\n");
	goto done;
    }

    printf("wcacheline = 0x");
    for(i=0; i<CACHELINE; i++) {
	printf("%02x", (uint8_t)wcacheline[i]);
    }
    printf("\n");
    printf("set status data = 0x55\n");
    status[0] = 0x55;
    while(status[0] != 0x00) {
	nanosleep(&t, &t);
	printf("Polling afu1 test completion status = 0x%x\n", *status);
    }

    printf("Attempt open device for afu2\n");
    rc = ocxl_afu_open_from_dev(FUNCTION2, &afu2_h);
    if(rc != 0) {
	perror("cxl_afu_open_dev: for afu2"FUNCTION2);
	return -1;
    }
    printf("Attaching afu2 device ....\n");
    rc = ocxl_afu_attach(afu2_h);
    if(rc != 0) {
	perror("cxl_afu_attach: for afu2"FUNCTION2);
	return -1;
    }
    printf("Attempt mmio map afu2 registers\n");
    if(ocxl_mmio_map(afu2_h, OCXL_MMIO_BIG_ENDIAN) != 0) {
	printf("AFILED: ocxl_mmio_map afu2\n");
	goto done;
    }

    rc = ocxl_afu_irq_alloc(afu2_h, NULL, &irq_h);
    irq_id = ocxl_afu_irq_get_id(afu2_h, irq_h);

    printf("Set irq id source ea field = 0x%016lx\n", (uint64_t)irq_id);
    printf("Attempt interrupt command\n");
    config_param.context = 0x01;
    config_param.command = AFU_CMD_INTRP_REQ;
    config_param.mem_base_address = (uint64_t)irq_id;
    rc = config_enable_and_run_machine(afu2_h, &machine_config, config_param, DIRECTED);
    if(rc != -1) {
	printf("Response = 0x%x\n", rc);
 	printf("afu2 config_enable_and_run_machine PASS\n");
    }
    else {
	printf("FAILED: config_enable_and_run_machine afu2\n");
	goto done;
    }
    printf("set status data = 0xff\n");
    status[0] = 0xff;
    while(status[0] != 0x00) {
	nanosleep(&t, &t);
	printf("Polling interrupt completion status = 0x%x\n", *status);
    }

    rc = clear_machine_config(afu2_h, &machine_config, config_param, DIRECTED);
    if(rc != 0) {
	printf("Failed to clear machine config afu2\n");
	goto done;
    }
    
    rc = ocxl_afu_event_check(afu2_h, NULL, &event, 1);
    printf("Returned from ocxl_read_event -> there is an interrupt\n");
    if(rc == 0) {
	printf("Error retrieving interrupt event %d\n", rc);
	return -1;
    }
    ocxl_mmio_write64(afu2_h, ProcessControl_REGISTER, PROCESS_CONTROL_RESTART);
    printf("set status data = 0x55\n");
    status[0] = 0x55;
    while(status[0] != 0x0) {
	nanosleep(&t, &t);
	printf("Polling afu2 interrupt completion status = 0x%x\n", status[0]);
    }

done:
    // free device
    printf("Freeing afu2 device ... \n");
    ocxl_afu_close(afu2_h);
    printf("Freeing afu1 device ...\n");
    ocxl_afu_close(afu1_h);

    return 0;
}
