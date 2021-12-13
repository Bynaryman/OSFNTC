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

#ifndef _UTILS_H_
#define _UTILS_H_

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include "tlx_interface_t.h"

#ifdef DEBUG
#define DPRINTF(...) printf(__VA_ARGS__)
#else
#define DPRINTF(...)
#endif

#define MAX_LINE_CHARS 1024

#define ODD_PARITY 1
#define BYTES_PER_DWORD 8
#define DWORDS_PER_CACHELINE 8
#define CACHELINE_BYTES 64
#define TLX_IDLE_CYCLES 200

#define OCSE_VERSION_MAJOR	0x03
#define OCSE_VERSION_MINOR	0x00

#define OCSE_CONNECT		0x01
#define OCSE_QUERY		0x02
#define OCSE_MAX_INT		0x03
#define OCSE_OPEN		0x04
#define OCSE_ATTACH		0x05
#define OCSE_DETACH		0x06
#define OCSE_MEMORY_READ	0x07
#define OCSE_MEMORY_WRITE	0x08
#define OCSE_MEMORY_TOUCH	0x09
#define OCSE_MEM_SUCCESS	0x0a
#define OCSE_MEM_FAILURE	0x0b
#define OCSE_MMIO_MAP		0x0c
#define OCSE_MMIO_READ64	0x0d
#define OCSE_MMIO_WRITE64	0x0e
#define OCSE_MMIO_READ32	0x0f
#define OCSE_MMIO_WRITE32	0x10
#define OCSE_MMIO_ACK		0x11
#define OCSE_MMIO_FAIL		0x12
#define OCSE_INTERRUPT		0x13
#define OCSE_AFU_ERROR		0x14
#define OCSE_WAKE_HOST_THREAD	0x15
#define OCSE_INTERRUPT_D	0x16
#define OCSE_WR_BE		0x21
#define OCSE_AMO_RD		0x22
#define OCSE_AMO_RW		0x23
#define OCSE_AMO_WR		0x24
#define OCSE_GLOBAL_MMIO_MAP	        0x25
#define OCSE_GLOBAL_MMIO_READ64	        0x26
#define OCSE_GLOBAL_MMIO_WRITE64	0x27
#define OCSE_GLOBAL_MMIO_READ32	        0x28
#define OCSE_GLOBAL_MMIO_WRITE32	0x29
#define OCSE_LPC_MAP     	        0x2a
#define OCSE_LPC_READ	                0x2b
#define OCSE_LPC_WRITE          	0x2c
#define OCSE_LPC_WRITE_BE          	0x2d
#define OCSE_LPC_ACK            	0x2e
#define OCSE_LPC_FAIL            	0x2f
#define OCSE_FIND                       0x30
#define OCSE_FIND_NTH                   0x31
#define OCSE_FIND_ACK                   0x32

#define OCSE_FAILED                     0xff

#define MAX_INT32 0x7fffffffU
#define MIN_INT32 0x80000000U
#define MAX_UINT32 0xffffffffU
#define MAX_INT64   0x7FFFFFFFFFFFFFFF
#define MIN_INT64   0x8000000000000000
#define MAX_UINT64  0xFFFFFFFFFFFFFFFF


// OCSE states
enum ocse_state {
	OCSE_IDLE,
	OCSE_RESET,
	OCSE_DESC,
	OCSE_RD_RQ_PENDING,
	OCSE_PENDING,
	OCSE_BUFFER,
	OCSE_RUNNING,
	OCSE_DONE
};

#ifndef __APPLE__
// Convert host to network byte ordering
uint64_t htonll(uint64_t hostlonglong);

// Convert network to host byte ordering
uint64_t ntohll(uint64_t netlonglong);
#endif				/* __APPLE__ */

// Display fatal message (For catching code bugs, not AFU bugs)
void fatal_msg(const char *format, ...);

// Display error message
void error_msg(const char *format, ...);

// Display warning message
void warn_msg(const char *format, ...);

// Display informational message
void info_msg(const char *format, ...);

// Display debug message
void debug_msg(const char *format, ...);

// Delay for up to ns nanoseconds
void ns_delay(long ns);

// Delay to allow another thread to have mutex lock
void lock_delay(pthread_mutex_t * lock);

// Is there incoming data on socket?
int bytes_ready(int fd, int timeout, int *abort);

// Allocate memory for data and get size bytes from fd, no debug
int get_bytes_silent(int fd, int size, uint8_t * data, int timeout, int *abort);

// Allocate memory for data and get size bytes from fd
int get_bytes(int fd, int size, uint8_t * data, int timeout, int *abort,
	      FILE * dbg_fp, uint8_t dbg_id, uint16_t context);

// Put bytes on socket and return number of bytes successfully written, no debug
int put_bytes_silent(int fd, int size, uint8_t * data);

// Put bytes on socket and return number of bytes successfully written
int put_bytes(int fd, int size, uint8_t * data, FILE * dbg_fp, uint8_t dbg_id,
	      uint16_t context);

// Generate parity for outbound data and checking inbound data
// 1 bit of parity for up to 64 bits of data
uint8_t generate_parity(uint64_t data, uint8_t odd);

// Generate parity for entire cacheline
// 1 bit of parity for each 64 bits of data
void generate_cl_parity(uint8_t * data, uint8_t * parity);

// Gracefully shutdown and close socket connection
int close_socket(int *sockfd);

//sign extend a 32b integer
int32_t sign_extend(uint32_t in_op);

//sign extend a 64b integer
int64_t sign_extend64(uint64_t in_op);

// convert dl (dLengh) to a 32 bit integer
// a size less than 0 indicates an bad combination of dl/pl
int32_t dl_to_size (uint8_t dl);

// convert pl (pLength) to a 32 bit integer
// a size less than 0 indicates an bad combination of dl/pl
int32_t pl_to_size (uint8_t pl);

// convert size to a combination of dl
// size must be a multiple of 64...  and no greater than 256
uint8_t size_to_dl (int16_t size);

// dl into a number...
int32_t decode_dl (uint8_t resp_rd_cnt);

// resp_rd_cnt into a number...
int32_t decode_rd_cnt (uint8_t resp_rd_cnt);

#endif				/* _UTILS_H_ */
