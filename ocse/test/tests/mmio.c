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
#include "../../libocxl/libocxl.h"

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
    int opt, option_index;
    int rc;
    uint64_t wed, result;
    ocxl_afu_h mafu_h;
    uint32_t result32;

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
    wed = 0x0102030405060708;
    printf("WED data = 0x%016lx WED address = 0x%016lx\n", wed, (long)&wed);
    printf("Attempt mmio write\n");
    if(ocxl_mmio_write64(mafu_h, 0x08, wed) != 0) {
	printf("FAILED: ocxl_mmio_write64\n");
	goto done;
    }
    printf("Attempt mmio read\n");
    if(ocxl_mmio_read64(mafu_h, 0x8, &result) != 0) {
	printf("FAILED: ocxl_mmio_read64\n");
	goto done;
    }

    printf("RESULT = 0x%016lx\n", result);
    
    wed = 0x0a0b0c0d0e0f1122;
    printf("WED data = 0x%016lx WED address = 0x%016lx\n", wed, (long)&wed);
    printf("Attempt mmio write\n");
    if(ocxl_mmio_write32(mafu_h, 0x20, wed) != 0) {
	printf("FAILED: ocxl_mmio_write32\n");
	goto done;
    }
    printf("Attempt mmio read\n");
    if(ocxl_mmio_read32(mafu_h, 0x20, &result32) != 0) {
	printf("FAILED: ocxl_mmio_read32\n");
	goto done;
    }

    printf("RESULT32 = 0x%08x\n", result32);

done:
    // free device
    printf("Freeing device ... \n");
    ocxl_afu_close(mafu_h);

    return 0;
}
