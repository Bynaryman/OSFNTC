/*
 * Copyright 2014,2017 International Business Machines
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

#ifndef _MMIO_H_
#define _MMIO_H_

#include <pthread.h>
#include <stdint.h>
#include <stdint.h>
#include <stdio.h>

#include "ocl.h"
#include "client.h"
#include "parms.h"
#include "../common/tlx_interface.h"
#include "../common/utils.h"

#define MMIO_FULL_RANGE 0x4000000
#define PSA_MASK             0x00FFFFFFFFFFFFFFL
#define PSA_REQUIRED         0x0100000000000000L
#define PROCESS_PSA_REQUIRED 0x0200000000000000L
#define FOUR_K 0x1000
#define CXL_MMIO_BIG_ENDIAN 0x1
#define CXL_MMIO_LITTLE_ENDIAN 0x2
#define CXL_MMIO_HOST_ENDIAN 0x3
#define CXL_MMIO_ENDIAN_MASK 0x3


// need to abstract dw - add a size element
// need to allow for dl and dp - add dl and dp elements
struct mmio_event {
	uint32_t rnw;
	uint32_t dw;    // TODO remove this ?  Maybe, we need to know 4/8 byte mmio  cmd_pL is an encoded length
	uint32_t cfg;
	uint64_t cmd_data;
	uint64_t cmd_PA;
	uint16_t cmd_CAPPtag;
	uint8_t cmd_opcode;
	uint8_t cmd_pL;
  // uint8_t cmd_TorR;  //may not need this
  // uint8_t cmd_rd_cnt;
  // parallel records for general capp commands
        uint16_t partial_index;  // this keeps track of where we are if multiple beats of data are coming with this response
        uint8_t ack;    // use this to hold the ack value for the message back to libocxl
        uint8_t resp_dL;     // the encoded length of the data in this part of the response 
        uint8_t resp_dP;     // the encoded offset of the location of this portion of the response
        uint8_t resp_code;    // use this to hold the resp value for the message back to libocxl
        uint8_t resp_opcode;    // use this to hold the resp opcode for the message back to libocxl
        uint8_t be_valid;  // use this to let us know whether or not to use the byte enable
        uint32_t size;  // if size = 0, we use dw to imply size
        uint32_t size_received;  // keep track of the total amount of data we have received from the afu response
        uint8_t *data;  // if size = 0, we use cmd_data as the data field
        uint64_t be;  // if be_valid, use this as the byte enable in the command
        uint8_t cmd_dL;     // dL, dP, and pL are encoded from either size or dw in send_mmio
        uint8_t cmd_dP;
	enum ocse_state state;
	struct mmio_event *_next;
};

// per afu structure
// this is where we save the per afu config space data during discovery
// query/open will look for this based on the afu index parsed from the given device name
struct afu_cfg {
      // from AFU Control DVSEC
      uint8_t pasid_base;
      uint8_t pasid_len_enabled;
      uint8_t pasid_len_supported;
      uint16_t actag_base;
      uint16_t actag_length_enabled;
      uint16_t actag_length_supported;
      
      // from AFU Descriptor Template 0 via AFU Information DVSEC
      char namespace[25];  // (24 characters +1 to capture \0)
      uint8_t afu_version_major;
      uint8_t afu_version_minor;
      uint8_t  global_mmio_bar;
      uint64_t global_mmio_offset;
      uint32_t global_mmio_size;
      uint8_t  pp_mmio_bar;
      uint64_t pp_mmio_offset;
      uint32_t pp_mmio_stride;
      uint64_t mem_base_address;
      uint8_t  mem_size;
};

// per function structure
// this is where we save the interesting per function config space data during discovery and configuration
// query/open will look for this based on function number parsed from the given device name
struct fcn_cfg {
      // from config space header
      uint16_t device_id;
      uint16_t vendor_id;
      uint64_t bar0; // TODO: discover - write all 1's, read back size, configure - write configured base address
      uint64_t bar1; // "
      uint64_t bar2; // "

      // from process address space id extended capability
      uint8_t max_pasid_width; // per process dvsec.max pasid width

      // from OpenCAPI Transport Layer DVSEC (designated vendor specific extended capability)
      uint8_t tl_major_version_capability; // 0x0c
      uint8_t tl_minor_version_capability; // 0x0c
      uint32_t tl_xmit_template_cfg; // 0x24 - we only look at the cfg for templates 31 downto 0
      uint32_t tl_xmit_rate_per_template_cfg; // 0x6c - we only look at the cfg for templates 7 to 0

      // from Function DVSEC
      uint64_t function_dvsec_pa;
      uint8_t afu_present;
      uint8_t max_afu_index;
      uint16_t function_actag_base;
      uint16_t function_actag_length_enabled;

      // pointer to an array of pointers to per afu structures null if no afus (afu_function_dvsec.afu_present=0), 
      // length of array is function_dvsec.max_afu_index+1
      // this array will be indexed by the afu index part of the device name
      uint64_t afu_information_dvsec_pa;
      struct afu_cfg **afu_cfg_array;
};

/* struct afu_cfg_sp { */
/*         uint16_t cr_device; */
/*         uint16_t cr_vendor; */
/*         uint32_t PASID_CP; */
/*         uint32_t PASID_CTL_STS; */
/*         uint32_t OCAPI_TL_CP; */
/*         uint32_t OCAPI_TL_REVID; */
/*         uint32_t OCAPI_TL_VERS; */
/*         uint32_t OCAPI_TL_TMP_CFG; */
/*         uint32_t OCAPI_TL_TX_RATE; */
/*         uint32_t OCAPI_TL_MAXAFU; */
/*         uint32_t FUNC_CFG_CP; */
/*         uint32_t FUNC_CFG_REVID; */
/*         uint32_t FUNC_CFG_MAXAFU; */
/*         uint32_t AFU_INFO_CP; */
/*         uint32_t AFU_INFO_REVID; */
/*         uint32_t AFU_INFO_INDEX; */
/*         uint32_t AFU_CTL_CP_0; */
/*         uint32_t AFU_CTL_REVID_4; */
/*         uint32_t AFU_CTL_EN_RST_INDEX_8; */
/*         uint32_t AFU_CTL_WAKE_TERM_C; */
/*         uint32_t AFU_CTL_PASID_LEN_10; */
/*         uint32_t AFU_CTL_PASID_BASE_14; */
/*         uint32_t AFU_CTL_ACTAG_LEN_EN_S; */
/*         uint32_t AFU_CTL_ACTAG_BASE; */
/*         uint8_t  name_space[25]; */
/*         uint32_t global_MMIO_offset_high; */
/*         uint32_t global_MMIO_offset_low; */
/*         uint32_t global_MMIO_BAR; */
/*         uint32_t global_MMIO_size; */
/*         uint32_t pp_MMIO_offset_high; */
/*         uint32_t pp_MMIO_offset_low; */
/*         uint32_t pp_MMIO_BAR; */
/*         uint32_t pp_MMIO_stride; */
/* 	uint32_t num_ints_per_process; */
/* 	uint32_t num_of_processes; */
/* }; */


/* struct afu_ctl_desc_sp { */
/*         uint32_t AFU_CTL_CP_0; */
/*         uint32_t AFU_CTL_REVID_4; */
/*         uint32_t AFU_CTL_EN_RST_INDEX_8; */
/*         uint32_t AFU_CTL_WAKE_TERM_C; */
/*         uint32_t AFU_CTL_PASID_LEN_10; */
/*         uint32_t AFU_CTL_PASID_BASE_14; */
/*         uint32_t AFU_CTL_ACTAG_LEN_EN_S; */
/*         uint32_t AFU_CTL_ACTAG_BASE; */
/*         uint8_t  name_space[25]; */
/*         uint32_t global_MMIO_offset_high; */
/*         uint32_t global_MMIO_offset_low; */
/*         uint32_t global_MMIO_BAR; */
/*         uint32_t global_MMIO_size; */
/*         uint32_t pp_MMIO_offset_high; */
/*         uint32_t pp_MMIO_offset_low; */
/*         uint32_t pp_MMIO_BAR; */
/*         uint32_t pp_MMIO_stride; */
/* 	uint32_t num_ints_per_process; */
/* 	uint32_t num_of_processes; */
/* }; */

/* struct fun_cfg_sp { */
/*         uint16_t cr_device; */
/*         uint16_t cr_vendor; */
/*         uint32_t PASID_CP; */
/*         uint32_t PASID_CTL_STS; */
/*         uint32_t OCAPI_TL_CP; */
/*         uint32_t OCAPI_TL_REVID; */
/*         uint32_t OCAPI_TL_VERS; */
/*         uint32_t OCAPI_TL_TMP_CFG; */
/*         uint32_t OCAPI_TL_TX_RATE; */
/*         uint32_t OCAPI_TL_MAXAFU; */
/*         uint32_t FUNC_CFG_CP; */
/*         uint32_t FUNC_CFG_REVID; */
/*         uint32_t FUNC_CFG_MAXAFU; */
/*         uint32_t AFU_INFO_CP; */
/*         uint32_t AFU_INFO_REVID; */
/*         uint32_t AFU_INFO_INDEX; */
/* 	struct afu_ctl_desc_sp *acdsptr; */
/* }; */

struct mmio {
	struct AFU_EVENT *afu_event;
        struct fcn_cfg **fcn_cfg_array;  // this array will be indexed by the function part of the device name
        //struct afu_cfg_sp cfg;
	//struct fun_cfg_sp *fun_array;
	struct mmio_event *list;
	char *afu_name;
	FILE *dbg_fp;
	uint8_t dbg_id;
	uint32_t flags;
	int timeout;
};

struct mmio *mmio_init(struct AFU_EVENT *afu_event, int timeout, char *afu_name,
		       FILE * dbg_fp, uint8_t dbg_id);

int read_afu_config(struct ocl *ocl, uint8_t bus, pthread_mutex_t * lock);

struct mmio_event *add_mmio(struct mmio *mmio, uint32_t rnw, uint32_t dw,
			    uint64_t addr, uint64_t data);

void send_mmio(struct mmio *mmio);

void handle_mmio_ack(struct mmio *mmio);

void handle_ap_resp(struct mmio *mmio);

void handle_ap_resp_data(struct mmio *mmio);

void handle_mmio_map(struct mmio *mmio, struct client *client);

struct mmio_event *handle_mmio(struct mmio *mmio, struct client *client,
			       int rnw, int dw, int global);

struct mmio_event *handle_mmio_done(struct mmio *mmio, struct client *client);


struct mmio_event *handle_mem(struct mmio *mmio, struct client *client,
			      int rnw, int region, int be_valid);

#endif				/* _MMIO_H_ */
