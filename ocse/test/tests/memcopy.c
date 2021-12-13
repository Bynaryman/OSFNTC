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

#define CACHELINE 128
#define MDEVICE "/dev/cxl/tlx0.0000:00:0.1.0"

#define PHYSICAL_FUNCTION "1234:00:00.1"
#define NAME "tlx"

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
    ocxl_afu_h mafu_h;
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

    t.tv_sec = 0;
    t.tv_nsec = 1000000;
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
    printf("Calling ocxl_afu_open_specific\n");
    
    rc = ocxl_afu_open_specific(NAME, PHYSICAL_FUNCTION, 0, &mafu_h);
    //rc = ocxl_afu_open_from_dev(MDEVICE, &mafu_h);
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
    printf("Attempt Read command\n");
    //status[0] = 0xff;
    config_param.context = 0;
    config_param.enable_always = 1;
    //config_param.mem_size = CACHELINE;
    config_param.mem_size = 64;
    config_param.command = AFU_CMD_RD_WNITC;
    config_param.mem_base_address = (uint64_t)rcacheline;
    config_param.machine_number = 0;
    config_param.status_address = (uint32_t)status;
    printf("status address = 0x%p\n", status);
    printf("rcacheline = 0x%p\n", rcacheline);
    printf("command = 0x%x\n", config_param.command);
    printf("mem base address = 0x%"PRIx64"\n", config_param.mem_base_address);
    rc = config_enable_and_run_machine(mafu_h, &machine_config, config_param, DIRECTED);
    status[0] = 0xff;
    if( rc != -1) {
	printf("Response = 0x%x\n", rc);
	printf("config_enable_and_run_machine PASS\n");
    }
    else {
	printf("FAILED: config_enable_and_run_machine\n");
	goto done;
    }
    timeout = 0;
    while(status[0] != 0x0) {
	nanosleep(&t, &t);
	printf("Polling read completion status = 0x%x\n", *status);
    }
    // clear machine config
    rc = clear_machine_config(mafu_h, &machine_config, config_param, DIRECTED);
    if(rc != 0) {
	printf("Failed to clear machine config for read command\n");
	goto done;
    }
    // Attemp write command
    printf("Attempt Write command\n");
    //status[0] = 0xff;
    config_param.command = AFU_CMD_DMA_W;
    config_param.mem_size = 64;
    config_param.mem_base_address = (uint64_t)wcacheline;
    printf("wcacheline = 0x%p\n", wcacheline);
    printf("command = 0x%x\n",config_param.command);
    printf("wcache address = 0x%"PRIx64"\n", config_param.mem_base_address);
    rc = config_enable_and_run_machine(mafu_h, &machine_config, config_param, DIRECTED);
    //status[0] = 0xff;
    if(rc != -1) {
	printf("Response = 0x%x\n", rc);
 	printf("config_enable_and_run_machine PASS\n");
    }
    else {
	printf("FAILED: config_enable_and_run_machine\n");
	goto done;
    }
    status[0] = 0xff;
    while(status[0] != 0x00) {
	nanosleep(&t, &t);
	printf("Polling write completion status = 0x%x\n", *status);
    }
    rc = clear_machine_config(mafu_h, &machine_config, config_param, DIRECTED);
    if(rc != 0) {
	printf("Failed clear machine config for write command\n");
    	goto done;
    }
    status[0] = 0x55;	// send test complete status
    while(status[0] != 0x00) {
	nanosleep(&t, &t);
	printf("Polling test completion status\n");
    } 
    printf("wcacheline = 0x");
    for(i=0; i<CACHELINE; i++) {
	printf("%02x", (uint8_t)wcacheline[i]);
    }
    printf("\n");

done:
    // free device
    printf("Freeing device ... \n");
    ocxl_afu_close(mafu_h);

    return 0;
}
