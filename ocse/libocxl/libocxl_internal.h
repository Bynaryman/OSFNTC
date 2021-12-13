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

#ifndef _LIBOCXL_INTERNAL_H
#define _LIBOCXL_INTERNAL_H

#include <libocxl.h>
#include <dirent.h>
#include <inttypes.h>
#include <poll.h>
#include <pthread.h>

#define EVENT_QUEUE_MAX 3

enum libocxl_req_state {
	LIBOCXL_REQ_IDLE,
	LIBOCXL_REQ_REQUEST,
	LIBOCXL_REQ_PENDING
};

struct int_req {
	volatile enum libocxl_req_state state;
	volatile uint16_t max;
};

struct open_req {
	volatile enum libocxl_req_state state;
	volatile uint8_t context;
};

struct attach_req {
	volatile enum libocxl_req_state state;
	volatile uint64_t wed;
};

struct mmio_req {
	volatile enum libocxl_req_state state;
	volatile uint8_t type;
	volatile uint32_t addr;
	uint64_t data;
};

struct ocxl_irq {
        uint16_t irq;
        uint64_t id;
	struct ocxl_afu *afu;
	struct ocxl_irq *_next;
};

struct ocxl_waitasec {
	ocxl_event_type type;
	uint16_t size;
	uint16_t process_element;
	uint16_t reserved1;
};

struct mem_req {
	volatile enum libocxl_req_state state;
	volatile uint8_t type;
	volatile uint64_t addr;
	volatile uint64_t size;
	volatile uint64_t be;
	uint8_t *data;
};

typedef struct ocxl_afu ocxl_afu;

typedef struct ocxl_mmio_area {
	char *start; /**< The first addressable byte of the area */
	size_t length; /**< The size of the area in bytes */
	ocxl_mmio_type type; /**< The type of the area */
	ocxl_afu *afu; /**< The AFU this MMIO area belongs to */
} ocxl_mmio_area;

// struct ocxl_afu_h {
struct ocxl_afu {
	pthread_t thread;
	pthread_mutex_t event_lock;
	ocxl_event *events[EVENT_QUEUE_MAX];
        uint64_t ppc64_amr;
	char *id;
        ocxl_identifier ocxl_id;
        uint8_t bus;
        uint8_t dev;
        uint8_t fcn;
	uint16_t device_id;
	uint16_t vendor_id;
        uint8_t afu_version_major;
        uint8_t afu_version_minor;
  // uint64_t global_mmio_offset;  // moves to global_mmio.start?
  // uint32_t global_mmio_size;    // moves to global_mmio.length?
  // uint64_t pp_mmio_offset;  // moves to per_pasid_mmio.start?
  // uint32_t pp_mmio_stride;  // moves to per_pasid_mmio.length?
	ocxl_mmio_area global_mmio;
	ocxl_mmio_area per_pasid_mmio;
  // need to save pointers to "mapped" global and per pasid mmio areas
        uint32_t mmio_count;
        uint32_t mmio_max;
        ocxl_mmio_area mmios[2];
        uint64_t mem_base_address;
        uint8_t mem_size;
	uint16_t context;
	uint16_t map;  
	uint8_t dbg_id;
	int fd;
	int opened;
	int attached;
	int mapped;
	int global_mapped;
	int lpc_mapped;
	int pipe[2];
        int irq_count;
	struct int_req int_req;
	struct open_req open;
	struct attach_req attach;
	struct mmio_req mmio;
	struct mem_req mem;
	struct ocxl_irq *irq;
  //struct ocxl_afu *_head;
  //struct ocxl_afu *_next;
  //struct ocxl_afu *_next_adapter; // ???
};

#endif
