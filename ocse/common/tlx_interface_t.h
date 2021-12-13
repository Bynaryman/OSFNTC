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

#ifndef __tlx_interface_t_h__
#define __tlx_interface_t_h__ 1

#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

// Choose ONE to define what TLX support level will be
#define TLX3 1

// this is the size of the transimit and receive buffers in the afu_event
// it needs to be large enough to transmit/receive the maximum size of legally concurrent events
// for example from tlx to afu this might be response, command, command data, resp data and credit xchange.
// we'll set it at 1070 for now and see if we can come up with the correct value later. (Do we have to read entire
// data buffer in one socket transaction? If not, this size can be reduced....
#define TLX_BUFFER_SIZE 1070

#ifdef TLX3
#define PROTOCOL_PRIMARY 3
#define PROTOCOL_SECONDARY 0000
#define PROTOCOL_TERTIARY 0
#endif /* TLX3 */

/* Select the initial value for credits??  */
#define MAX_AFU_TLX_CMD_CREDITS 5
#define MAX_AFU_TLX_RESP_CREDITS 10
#define MAX_TLX_AFU_CMD_CREDITS 8
#define MAX_TLX_AFU_CMD_DATA_CREDITS 32
#define MAX_TLX_AFU_RESP_CREDITS 7
#define MAX_TLX_AFU_RESP_DATA_CREDITS 32


/* Return codes for TLX interface functions */

#define TLX_SUCCESS 0
#define TLX_RESPONSE_DONE 0
#define TLX_AFU_DOUBLE_COMMAND 1
#define TLX_AFU_CMD_NOT_VALID 2
#define TLX_AFU_DOUBLE_CMD_AND_DATA 3
#define TLX_AFU_CMD_DATA_NOT_VALID 4
#define TLX_AFU_DOUBLE_RESP 5
#define TLX_AFU_RESP_NOT_VALID 6
#define TLX_AFU_DOUBLE_RESP_AND_DATA 7
#define TLX_AFU_RESP_DATA_NOT_VALID 8
#define TLX_AFU_NO_CREDITS 10
#define AFU_TLX_DOUBLE_COMMAND 21
#define AFU_TLX_CMD_NOT_VALID 22
#define AFU_TLX_DOUBLE_CMD_AND_DATA 23
#define AFU_TLX_CMD_DATA_NOT_VALID 24
#define AFU_TLX_CMD_NO_DATA 25
#define AFU_TLX_DOUBLE_RESP 31
#define AFU_TLX_RESP_NOT_VALID 32
#define AFU_TLX_DOUBLE_RESP_AND_DATA 33
#define AFU_TLX_RESP_DATA_NOT_VALID 34
#define AFU_TLX_RESP_NO_DATA 35
#define AFU_TLX_DOUBLE_RESP_DATA 36
#define AFU_TLX_NO_CREDITS 40
#define AFU_TLX_RD_CNT_WRONG 41
#define CFG_TLX_NO_CREDITS 42
#define CFG_TLX_NOT_CFG_CMD 43
#define CFG_TLX_RESP_NOT_VALID 44
#define TLX_RESPONSE_FAILED 15
//#define TLX_RESPONSE_CONTEXT 17
#define TLX_BAD_SOCKET 16	/* The socket connection could not be established */
#define TLX_VERSION_ERROR 48	/* The TLX versions in use on local & remote do not match */
#define TLX_TRANSMISSION_ERROR 64	/* There was an error sending data across the socket
					   interface */
#define TLX_CLOSE_ERROR 128	/* There was an error closing the socket */

/* TL CAPP Command opcodes (from host to AFU) */

#define TLX_CMD_NOP 0
#define TLX_CMD_XLATE_DONE 	0x18
#define TLX_CMD_RETURN_ADR_TAG  0x19	// TLX4 only
#define TLX_CMD_INTRP_RDY  	0x1a
#define TLX_CMD_RD_MEM	  	 0x20
#define TLX_CMD_PR_RD_MEM  	0x28
#define TLX_CMD_AMO_RD     	0x30	// TLX4 only
#define TLX_CMD_AMO_RW     	0x31	// TLX4 only
#define TLX_CMD_AMO_W      	0x40	// TLX4 only
#define TLX_CMD_WRITE_MEM  	0x81
#define TLX_CMD_WRITE_MEM_BE	0x82
#define TLX_CMD_WRITE_META	0x83	// OMI
#define TLX_CMD_PR_WR_MEM	0x86
#define TLX_CMD_FORCE_EVICT	0xd0	// TLX4 only
#define TLX_CMD_FORCE_UR	0xd2	// TLX4 only
#define TLX_CMD_WAKE_AFU_THREAD	0xdf	// TLX4 only
#define TLX_CMD_CONFIG_READ	0xe0
#define TLX_CMD_CONFIG_WRITE	0xe1


/* TLX AP Command opcodes (from AFU to host) */

#define AFU_CMD_NOP 0
#define AFU_CMD_RD_WNITC 	0x10
#define AFU_CMD_RD_WNITC_S 	0x11 	//TLX4 only
#define AFU_CMD_RD_WNITC_N 	0x14
#define AFU_CMD_RD_WNITC_N_S 	0x15 	//TLX4 only
#define AFU_CMD_PR_RD_WNITC 	0x12
#define AFU_CMD_PR_RD_WNITC_S 	0x13 	//TLX4 only
#define AFU_CMD_PR_RD_WNITC_N 	0x16
#define AFU_CMD_PR_RD_WNITC_N_S	0x17 	//TLX4 only
#define AFU_CMD_DMA_W  		0x20
#define AFU_CMD_DMA_W_S  	0x21	// TLX4 only
#define AFU_CMD_DMA_W_P	  	0x22	// TLX4 only
#define AFU_CMD_DMA_W_P_S	0x23	// TLX4 only
#define AFU_CMD_DMA_W_N	  	0x24
#define AFU_CMD_DMA_W_N_S  	0x25	// TLX4 only
#define AFU_CMD_DMA_W_N_P 	0x26	// TLX4 only
#define AFU_CMD_DMA_W_N_P_S     0x27	// TLX4 only
#define AFU_CMD_DMA_W_BE  	0x28
#define AFU_CMD_DMA_W_BE_S     	0x29	// TLX4 only
#define AFU_CMD_DMA_W_BE_P     	0x2a	// TLX4 only
#define AFU_CMD_DMA_W_BE_P_S   	0x2b	// TLX4 only
#define AFU_CMD_DMA_W_BE_N  	0x2c
#define AFU_CMD_DMA_W_BE_N_S	0x2d	// TLX4 only
#define AFU_CMD_DMA_W_BE_N_P	0x2e	// TLX4 only
#define AFU_CMD_DMA_W_BE_N_P_S	0x2f	// TLX4 only
#define AFU_CMD_DMA_PR_W  	0x30
#define AFU_CMD_DMA_PR_W_S  	0x31	// TLX4 only
#define AFU_CMD_DMA_PR_W_P  	0x32	// TLX4 only
#define AFU_CMD_DMA_PR_W_P_S	0x33	// TLX4 only
#define AFU_CMD_DMA_PR_W_N  	0x34
#define AFU_CMD_DMA_PR_W_N_S  	0x35	// TLX4 only
#define AFU_CMD_DMA_PR_W_N_P 	0x36	// TLX4 only
#define AFU_CMD_DMA_PR_W_N_P_S  0x37	// TLX4 only
#define AFU_CMD_AMO_RD  	0x38
#define AFU_CMD_AMO_RD_S  	0x39	// TLX4 only
#define AFU_CMD_AMO_RD_N  	0x3c
#define AFU_CMD_AMO_RD_N_S  	0x3d	// TLX4 only
#define AFU_CMD_AMO_RW  	0x40
#define AFU_CMD_AMO_RW_S  	0x41	// TLX4 only
#define AFU_CMD_AMO_RW_N  	0x44
#define AFU_CMD_AMO_RW_N_S  	0x45	// TLX4 only
#define AFU_CMD_AMO_W  		0x48
#define AFU_CMD_AMO_W_S  	0x49	// TLX4 only
#define AFU_CMD_AMO_W_P  	0x4a	// TLX4 only
#define AFU_CMD_AMO_W_P_S	0x4b	// TLX4 only
#define AFU_CMD_AMO_W_N  	0x4c
#define AFU_CMD_AMO_W_N_S  	0x4d	// TLX4 only
#define AFU_CMD_AMO_W_N_P 	0x4e	// TLX4 only
#define AFU_CMD_AMO_W_N_P_S  	0x4f	// TLX4 only
#define AFU_CMD_ASSIGN_ACTAG	0x50
#define AFU_CMD_ADR_TAG_RELEASE	0x51	// TLX4 only
#define AFU_CMD_MEM_PA_FLUSH	0x52	// TLX4 only
#define AFU_CMD_CASTOUT		0x55	// TLX4 only
#define AFU_CMD_CASTOUT_PUSH	0x56	// TLX4 only
#define AFU_CMD_INTRP_REQ	0x58
#define AFU_CMD_INTRP_REQ_S	0x59	// TLX4 only
#define AFU_CMD_INTRP_REQ_D	0x5a
#define AFU_CMD_INTRP_REQ_D_S	0x5b	// TLX4 only
#define AFU_CMD_WAKE_HOST_THRD	0x5c
#define AFU_CMD_WAKE_HOST_THRD_S	0x5d // TLX4 only
#define AFU_CMD_UPGRADE_STATE		0x60 	// TLX4 only
#define AFU_CMD_READ_EXCLUSIVE		0x68 	// TLX4 only
#define AFU_CMD_READ_SHARED		0x69 	// TLX4 only
#define AFU_CMD_XLATE_TOUCH		0x78
#define AFU_CMD_XLATE_TOUCH_N		0x7c
#define AFU_CMD_RD_WNITC_T		0x90 	// TLX4 only
#define AFU_CMD_RD_WNITC_T_S		0x91 	// TLX4 only
#define AFU_CMD_RD_WNITC_T_N		0x94 	// TLX4 only
#define AFU_CMD_RD_WNITC_T_N_S		0x95 	// TLX4 only
#define AFU_CMD_PR_RD_WNITC_T		0x92 	// TLX4 only
#define AFU_CMD_PR_RD_WNITC_T_S		0x93 	// TLX4 only
#define AFU_CMD_PR_RD_WNITC_T_N		0x96 	// TLX4 only
#define AFU_CMD_PR_RD_WNITC_T_N_S	0x97 	// TLX4 only
#define AFU_CMD_DMA_W_T  		0xa0    // TLX4 only
#define AFU_CMD_DMA_W_T_S  		0xa1	// TLX4 only
#define AFU_CMD_DMA_W_T_P	  	0xa2	// TLX4 only
#define AFU_CMD_DMA_W_T_P_S		0xa3	// TLX4 only
#define AFU_CMD_DMA_W_T_N	  	0xa4    // TLX4 only
#define AFU_CMD_DMA_W_T_N_S  		0xa5	// TLX4 only
#define AFU_CMD_DMA_W_T_N_P 		0xa6	// TLX4 only
#define AFU_CMD_DMA_W_T_N_P_S    	0xa7	// TLX4 only
#define AFU_CMD_DMA_W_BE_T  		0xa8	// TLX4 only
#define AFU_CMD_DMA_W_BE_T_S     	0xa9	// TLX4 only
#define AFU_CMD_DMA_W_BE_T_P     	0xaa	// TLX4 only
#define AFU_CMD_DMA_W_BE_T_P_S   	0xab	// TLX4 only
#define AFU_CMD_DMA_W_BE_T_N  		0xac	// TLX4 only
#define AFU_CMD_DMA_W_BE_T_N_S		0xad	// TLX4 only
#define AFU_CMD_DMA_W_BE_T_N_P		0xae	// TLX4 only
#define AFU_CMD_DMA_W_BE_T_N_P_S	0xaf	// TLX4 only
#define AFU_CMD_DMA_PR_W_T  		0xb0	// TLX4 only
#define AFU_CMD_DMA_PR_W_T_S  		0xb1	// TLX4 only
#define AFU_CMD_DMA_PR_W_T_P  		0xb2	// TLX4 only
#define AFU_CMD_DMA_PR_W_T_P_S		0xb3	// TLX4 only
#define AFU_CMD_DMA_PR_W_T_N  		0xb4	// TLX4 only
#define AFU_CMD_DMA_PR_W_T_N_S  	0xb5	// TLX4 only
#define AFU_CMD_DMA_PR_W_T_N_P 		0xb6	// TLX4 only
#define AFU_CMD_DMA_PR_W_T_N_P_S  	0xb7	// TLX4 only
#define AFU_CMD_AMO_RD_T  		0xb8	// TLX4 only
#define AFU_CMD_AMO_RD_T_S  		0xb9	// TLX4 only
#define AFU_CMD_AMO_RD_T_N  		0xbc	// TLX4 only
#define AFU_CMD_AMO_RD_T_N_S  		0xbd	// TLX4 only
#define AFU_CMD_AMO_RW_T  		0xc0	// TLX4 only
#define AFU_CMD_AMO_RW_T_S  		0xc1	// TLX4 only
#define AFU_CMD_AMO_RW_T_N  		0xc4	// TLX4 only
#define AFU_CMD_AMO_RW_T_N_S  		0xc5	// TLX4 only
#define AFU_CMD_AMO_W_T  		0xc8	// TLX4 only
#define AFU_CMD_AMO_W_T_S  		0xc9	// TLX4 only
#define AFU_CMD_AMO_W_T_P  		0xca	// TLX4 only
#define AFU_CMD_AMO_W_T_P_S		0xcb	// TLX4 only
#define AFU_CMD_AMO_W_T_N  		0xcc	// TLX4 only
#define AFU_CMD_AMO_W_T_N_S  		0xcd	// TLX4 only
#define AFU_CMD_AMO_W_T_N_P 		0xce	// TLX4 only
#define AFU_CMD_AMO_W_T_N_P_S  		0xcf	// TLX4 only
#define AFU_CMD_UPGRADE_STATE_T		0xe0 	// TLX4 only
#define AFU_CMD_READ_EXCLUSIVE_T	0xe8 	// TLX4 only
#define AFU_CMD_READ_SHARED_T		0xe9 	// TLX4 only

/*  AMO_OPCODES per TL SPEC: used in LIBOCXL */
#define AMO_WRMWF_ADD	 0x00
#define AMO_WRMWF_XOR	 0x01
#define AMO_WRMWF_OR	 0x02
#define AMO_WRMWF_AND	 0x03
#define AMO_WRMWF_CAS_MAX_U	 0x04
#define AMO_WRMWF_CAS_MAX_S	 0x05
#define AMO_WRMWF_CAS_MIN_U	 0x06
#define AMO_WRMWF_CAS_MIN_S	 0x07
#define AMO_ARMWF_CAS_U	 0x08
#define AMO_ARMWF_CAS_E	 0x09
#define AMO_ARMWF_CAS_NE	 0x0a
#define AMO_ARMWF_INC_B	 0x0c
#define AMO_ARMWF_INC_E	 0x0d
#define AMO_ARMWF_DEC_B	 0x0e
#define AMO_W_CAS_T	 0x0c



/* TL CAPP responses (from host to AFU)  */
#define TLX_RSP_NOP 0
#define TLX_RSP_RET_TLX_CREDITS	0x01
#define TLX_RSP_TOUCH_RESP	0x02
#define TLX_RSP_READ_RESP	0x04
#define TLX_RSP_UGRADE_RESP	0x07 	// TLX4 only
#define TLX_RSP_READ_FAILED	0x05
#define TLX_RSP_CL_RD_RESP	0x06 	// TLX4 only
#define TLX_RSP_WRITE_RESP	0x08
#define TLX_RSP_WRITE_FAILED	0x09
#define TLX_RSP_MEM_FLUSH_DONE	0x0a 	// TLX4 only
#define TLX_RSP_INTRP_RESP	0x0c
#define TLX_RSP_READ_RESP_OW	0x0d 	// OMI
#define TLX_RSP_READ_RESP_XW	0x0e 	// OMI
#define TLX_RSP_WAKE_HOST_RESP	0x10
#define TLX_RSP_CL_RD_RESP_OW	0x16 	// TLX4 only


/* TLX AP responses (from AFU to host) */
#define AFU_RSP_NOP  0
#define AFU_RSP_MEM_RD_RESP	0x01
#define AFU_RSP_MEM_RD_FAIL	0x02
#define AFU_RSP_MEM_RD_RESP_OW	0x03 	// OMI
#define AFU_RSP_MEM_WR_RESP	0x04
#define AFU_RSP_MEM_WR_FAIL	0x05
#define AFU_RSP_MEM_RD_RESP_XW	0x07 	// OMI
#define AFU_RSP_RET_TL_CREDITS	0x08 	// OMI
#define AFU_RSP_WAKE_AFU_RESP	0x0a 	// TLX4 only
#define AFU_RSP_FORCE_UR_DONE	0x0c 	// TLX4 only



/* Create one of these structures to interface to an AFU model and use the functions below to manipulate it */

/* *INDENT-OFF* */
struct DATA_PKT {
  uint8_t data[64];
  struct DATA_PKT *_next;
};

struct AFU_EVENT {
  int sockfd;                             /* socket file descriptor */
  uint32_t proto_primary;                 /* socket protocol version 1st number */
  uint32_t proto_secondary;               /* socket protocol version 2nd number */
  uint32_t proto_tertiary;                /* socket protocol version 3rd number */
  int clock;                              /* clock */
  unsigned char tbuf[TLX_BUFFER_SIZE];    /* transmit buffer for socket communications */
  unsigned char rbuf[TLX_BUFFER_SIZE];    /* receive buffer for socket communications */
  uint32_t rbp;                           /* receive buffer position */
  // Config and Credits
  uint8_t  afu_tlx_credit_req_valid;		  /* needed for xfer of credit & req changes */
  uint8_t  tlx_afu_credit_valid;		  /* needed for xfer of credits */
  uint8_t  afu_tlx_cmd_credits_available; // init from afu_tlx_cmd_initial_credit, decrement on tlx_afu_cmd_valid, increment on afu_tlx_cmd_credit
  uint8_t  afu_tlx_resp_credits_available; // init from afu_tlx_resp_initial_credit, decrement on tlx_afu_resp_valid, increment on afu_tlx_resp_credit
  uint8_t  tlx_afu_cmd_credits_available;  // init from tlx_afu_cmd_resp_initial_credit, decrement on afu_tlx_cmd_valid, increment on tlx_afu_cmd_credit
  uint8_t  tlx_afu_resp_credits_available; // init from tlx_afu_cmd_resp_initial_credit, decrement on afu_tlx_resp_valid, increment on tlx_afu_resp_credit
  uint8_t  tlx_afu_resp_data_credits_available;
  uint8_t  tlx_afu_cmd_data_credits_available;
  uint8_t  cfg_tlx_credits_available;
  uint16_t  tlx_afu_cmd_data_byte_cnt;	/*  used for socket transfer only */
  uint16_t  tlx_afu_resp_data_byte_cnt;	/*  used for socket transfer only */
  uint16_t  afu_cfg_resp_data_byte_cnt;	/*  used for socket transfer only */

  // TLX Parser - TLX to AFU CAPP Command Interface (table 2)
  // CAPP to AP (host to afu) commands and data
  uint8_t tlx_afu_cmd_valid;              /* 1 bit command valid from from host */
  uint8_t tlx_afu_cmd_opcode;             /* 8 bit command op code */
  uint16_t tlx_afu_cmd_capptag;           /* 16 bit command tag from host */
  uint8_t tlx_afu_cmd_dl;                 /* 2 bit command encoded data length */
  uint8_t tlx_afu_cmd_pl;                 /* 3 bit command encoded partial data length */
  uint64_t tlx_afu_cmd_be;                /* 64 bit command byte enable */
  uint8_t tlx_afu_cmd_end;                /* 1 bit command endianness 0=little */
  //uint8_t tlx_afu_cmd_t;                  /* 1 bit command type 0=configuration read/write; 1=configuration read/write */
  uint64_t tlx_afu_cmd_pa;                /* 64 bit command phyiscal address */
#ifdef TLX4
  uint8_t tlx_afu_cmd_flag;               /* 4 bit command flag for atomic memory ops - OCAPI 4 */
  uint8_t tlx_afu_cmd_os;                 /* 1 bit command ordered segment - OCAPI 4 */
#endif
  uint8_t afu_tlx_cmd_credit;              /* 1 bit return a credit to tlx */
  uint8_t afu_tlx_cmd_initial_credit;      /* 7 bit initial number of credits that the afu is providing to tlx for consumption valid? */

// TLX Parser - TLX to AFU CAPP Configuration Command Interface (table 3)
  uint8_t cfg_tlx_credit_return;              /* 1 bit return a credit to tlx for config cmds */
  uint8_t cfg_tlx_initial_credit;             /* 4 bit initial number of credits that the afu is providing to tlx for consumption */
  uint8_t tlx_cfg_valid;                      /* 1 bit cfg command valid from from host */
  uint8_t tlx_cfg_opcode;                     /* 8 bit cfg command op code */
  uint16_t tlx_cfg_capptag;                   /* 16 bit cfg command tag from host */
  uint64_t tlx_cfg_pa;                /* 64 bit cfg command phyiscal address */
  uint8_t tlx_cfg_t;                  /* 1 bit command  0=type 0 configuration read/write; 1= type 1 configuration read/write */
  uint8_t tlx_cfg_pl;                 /* 3 bit cfg command encoded partial data length */
  uint8_t tlx_cfg_data_bdi;            /* 1 bit bad config data indicator */
  unsigned char tlx_cfg_data_bus[4];  /* 32 bit (4 byte) config cmd data  */

  // TLX Parser - TLX to AFU CAPP Response Interface (table 4)
  // CAPP to AP (host to afu) responses (generally to ap/capp commands and data)
  uint8_t tlx_afu_resp_valid;             /* 1 bit valid respoonse from tlx */
  uint8_t tlx_afu_resp_opcode;            /* 8 bit response op code */
  uint16_t tlx_afu_resp_afutag;           /* 16 bit response tag - match to afu_tlx_cmd_afutag */
  uint8_t tlx_afu_resp_code;              /* 4 bit response reason code */
  uint8_t tlx_afu_resp_pg_size;           /* 6 bit page size */
  uint8_t tlx_afu_resp_dl;                /* 2 bit encoded data length */
  uint8_t tlx_afu_resp_dp;                /* 2 bit data part - which part of the data is in resp data */
  uint32_t tlx_afu_resp_addr_tag;          /* 18 bit bad address tag from a translate request */
#ifdef TLX4
  uint32_t tlx_afu_resp_host_tag;            /* TLX4 */
  uint8_t tlx_afu_resp_cache_state;          /* TLX4 */
#endif
  uint8_t afu_tlx_resp_credit;              /* 1 bit return a credit to tlx */
  uint8_t afu_tlx_resp_initial_credit;      /* 7 bit initial number of credits that the afu is providing to tlx for consumption - when is this valid? */

  // TLX Parser - TLX to AFU CAPP Response DATA Interface (table 5)
  // CAPP to AP (host to afu) data responses (generally to ap/capp read commands)
  uint8_t tlx_afu_resp_data_valid;         /* 1 bit response data valid */
  unsigned char tlx_afu_resp_data[256];    /* upto 256 B of data may come back with a response - we should maybe make sure this is little endian order */
                                           /* we don't send this directly on tlx interface.  it is buffered for later distribution.   */
  uint8_t tlx_afu_resp_data_bdi;           /* 1 bit bad data indicator */
  uint8_t afu_tlx_resp_rd_req;             /* 1 bit response to a read request */
  uint8_t afu_tlx_resp_rd_cnt;             /* 3 bit encoded read count */

  // response data fifo to buffer responses for later resp_rd_req
  struct DATA_PKT *rdata_head;
  struct DATA_PKT *rdata_tail;
  uint32_t rdata_rd_cnt;

  //  TLX Parser - TLX to AFU CAPP Command DATA Interface (table 6)
  // CAPP to AP (host to afu) data (generally to capp/ap write commands)
  uint8_t tlx_afu_cmd_data_valid;          /* 1 bit command from host valid */
  unsigned char tlx_afu_cmd_data_bus[256];  /* upto 256 B of data may come with a command - we should maybe make sure this is little endian order */
                                           /* we don't send this directly on tlx interface.  it is buffered for later distribution.   */
  uint8_t tlx_afu_cmd_data_bdi;            /* 1 bit bad data indicator */
  uint8_t afu_tlx_cmd_rd_req;              /* 1 bit read request */
  uint8_t afu_tlx_cmd_rd_cnt;              /* 3 bit encoded read count */
  //uint8_t tlx_cfg_cmd_data_valid;          /* 1 bit config command data from host valid */
  //uint8_t tlx_cfg_cmd_data_bdi;            /* 1 bit bad config data indicator */
  //unsigned char tlx_cfg_cmd_data_bus[4];  /* 32 bit (4 byte) config cmd data  */

  // command data fifo to buffer responses for later cmd_rd_req
  struct DATA_PKT *cdata_head;
  struct DATA_PKT *cdata_tail;
  uint32_t cdata_rd_cnt;


  // TLX Framer Misc Interface (table 8)
  uint8_t tlx_afu_cmd_initial_credit;  /* 4 bit initial number of response credits available to the afu - when is this valid? */
  uint8_t tlx_afu_resp_initial_credit; /* 4 bit initial number of response credits available to the afu - when is this valid? */
  uint8_t tlx_afu_cmd_data_initial_credit;     /* 6 bit initial number of data credits available to the afu - when is this valid? */
  uint8_t tlx_afu_resp_data_initial_credit;    /* 6 bit initial number of data credits available to the afu - when is this valid? */

  // TLX Framer - AFU to TLX AP Command Interface (table 11)
  // AP to CAPP (afu to host) commands and data
  uint8_t tlx_afu_cmd_credit;              /* 1 bit tlx returning a command credit to the afu */
  uint8_t tlx_afu_cmd_data_credit;         /* 1 bit tlx returning a command data credit to the afu */
  uint8_t afu_tlx_cmd_valid;              /* 1 bit 0|1 indicates that a valid command is being presented by the afu to tlx */
  uint8_t afu_tlx_cmd_opcode;             /* 8 bit opcode */
  uint16_t afu_tlx_cmd_actag;             /* 12 bit address context tag */
  uint8_t afu_tlx_cmd_stream_id;          /* 4 bit address context tag */
  unsigned char afu_tlx_cmd_ea_or_obj[9]; /* 68 bit effective address or object handle */
  uint16_t afu_tlx_cmd_afutag;            /* 16 bit command tag */
  uint8_t afu_tlx_cmd_dl;                 /* 2 bits encoded data length */  /* combine dl and pl ??? */
  uint8_t afu_tlx_cmd_pl;                 /* 3 bits encoded partial data length */
#ifdef TLX4
  uint8_t afu_tlx_cmd_os;                 /* 1 bit ordered segment CAPI 4 */
#endif
  uint64_t afu_tlx_cmd_be;                /* 64 bit byte enable */
  uint8_t afu_tlx_cmd_flag;               /* 4 bit command flag for atomic opcodes */
  uint8_t afu_tlx_cmd_endian;             /* 1 bit endianness 0=little endian; 1=big endian */
  uint16_t afu_tlx_cmd_bdf;               /* 16 bit bus device function - obtained during device config n*/
  uint32_t afu_tlx_cmd_pasid;             /* 20 bit PASID */
  uint8_t afu_tlx_cmd_pg_size;            /* 6 bit page size hint */
  uint8_t afu_tlx_cdata_valid;            /* 1 bit command data valid */
  unsigned char afu_tlx_cdata_bus[64];    /* 512 bit command data bus */
  uint8_t afu_tlx_cdata_bdi;              /* 1 bit bad command data */

  // TLX Framer - AFU to TLX AP Response Interface (table 12)
  uint8_t tlx_afu_resp_credit;            /* 1 bit tlx returning a response credit to the afu */
  uint8_t tlx_afu_resp_data_credit;       /* 1 bit tlx returning a response data credit to the afu */
  uint8_t afu_tlx_resp_valid;             /* 1 bit afu response is valid */
  uint8_t afu_tlx_resp_opcode;            /* 8 bit response op code */
  uint8_t afu_tlx_resp_dl;                /* 2 bit response data length */
  uint16_t afu_tlx_resp_capptag;          /* 16 bit response capptag - should match a tlx_afu_cmd_capptag */
  uint8_t afu_tlx_resp_dp;                /* 2 bit response data part */
  uint8_t afu_tlx_resp_code;              /* 4 bit response reason code */
  uint8_t afu_tlx_rdata_valid;            /* 6 bit response data is valid */
  unsigned char afu_tlx_rdata_bus[64];    /* 512 bit response data */
  uint8_t afu_tlx_rdata_bdi;              /* 1 bit response data is bad */

  // TLX Framer - AFU to TLX AP Configuration Interface (table 13)
  uint8_t cfg_tlx_resp_valid;             /* 1 bit afu cfg response is valid */
  uint8_t cfg_tlx_resp_opcode;            /* 8 bit cfg response op code */
  uint16_t cfg_tlx_resp_capptag;          /* 16 bit  cfg response capptag - should match a tlx_cfg_cmd capptag */
  uint8_t cfg_tlx_resp_code;              /* 4 bit cfg response reason code */
  uint8_t tlx_cfg_resp_ack;		  /* 1 bit signal to AFU after taking cfg resp from interface */
  uint8_t cfg_tlx_rdata_offset;           /* 4 bit offset into 32B buffer */
  unsigned char cfg_tlx_rdata_bus[4];  	  /* 32 bit (4 byte) config response data  */
  uint8_t cfg_tlx_rdata_valid;          /* 6 bit config response data is valid */
  uint8_t cfg_tlx_rdata_bdi;              /* 1 bit config response data is bad */

  // TLX Framer - Template Configuration (table 15)
  uint8_t afu_cfg_xmit_tmpl_config_0;     /* 1 bit xmit template enable - default */
  uint8_t afu_cfg_xmit_tmpl_config_1;     /* 1 bit xmit template enable */
  uint8_t afu_cfg_xmit_tmpl_config_2;     /* 1 bit xmit template enable - not in TLX3 */
  uint8_t afu_cfg_xmit_tmpl_config_3;     /* 1 bit xmit template enable */
  uint8_t afu_cfg_xmit_rate_config_0;     /* 4 bit xmit rate */
  uint8_t afu_cfg_xmit_rate_config_1;     /* 4 bit xmit rate */
  uint8_t afu_cfg_xmit_rate_config_2;     /* 4 bit xmit rate - not in TLX3 */
  uint8_t afu_cfg_xmit_rate_config_3;     /* 4 bit xmit rate */

};
/* *INDENT-ON* */
#endif
