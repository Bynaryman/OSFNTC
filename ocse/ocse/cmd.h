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

#ifndef _CMD_H_
#define _CMD_H_

#include <stdint.h>
#include <stdio.h>

#include "client.h"
#include "mmio.h"
#include "parms.h"
#include "../common/tlx_interface.h"

#define TOTAL_PAGES_CACHED 64
#define PAGE_WAYS 4
#define LOG2_WAYS 2		// log2(PAGE_WAYS) = log2(4) = 2
#define PAGE_ENTRIES (TOTAL_PAGES_CACHED / PAGE_WAYS)
#define LOG2_ENTRIES 4		// log2(PAGE_ENTRIES) = log2(64/4) = log2(16) = 4
#define PAGE_ADDR_BITS 12
#define PAGE_MASK 0xFFF
#define BAD_OPERAND_SIZE 2
#define BAD_ADDR_OFFSET 3

enum cmd_type {
	CMD_READ,
	CMD_WRITE,
	CMD_TOUCH,
	CMD_INTERRUPT,
	CMD_WAKE_HOST_THRD,
	CMD_WR_BE,
	CMD_AMO_RD,
	CMD_AMO_RW,
	CMD_AMO_WR,
	CMD_XLAT_RD_TOUCH,
	CMD_XLAT_WR_TOUCH,
	CMD_FAILED,
	CMD_OTHER
};

enum mem_state {
	MEM_IDLE,
	MEM_XLATE_PENDING,
	MEM_INT_PENDING,
	MEM_PENDING_SENT,
	MEM_TOUCH,
	MEM_TOUCHED,
	MEM_BUFFER,
	MEM_REQUEST,
	MEM_CAS_RD,
	//MEM_CAS_WR,
	MEM_RECEIVED,
	AMO_MEM_RESP,
	DMA_MEM_RESP,
	MEM_DONE
};


struct pages {
	uint64_t entry[PAGE_ENTRIES][PAGE_WAYS];
	uint64_t entry_filter;
	uint64_t page_filter;
	int age[PAGE_ENTRIES][PAGE_WAYS];
	uint8_t valid[PAGE_ENTRIES][PAGE_WAYS];
};

struct cmd_event {
	uint64_t addr;
	int32_t context;
	uint32_t command;
	uint32_t afutag;
	uint32_t size;
	uint32_t resp;  // this is used as resp_code TODO change this to  resp_code
	uint32_t port;
	uint32_t resp_dl;
	uint32_t resp_dp;
	uint32_t resp_opcode;
	uint32_t dpartial;
	uint64_t wr_be;
	uint16_t resp_bytes_sent;
	uint8_t cmd_flag;
	uint8_t cmd_endian;
	uint8_t cmd_pg_size;
	uint8_t unlock;
	uint8_t buffer_activity;
	uint8_t *data;
	//uint8_t *parity;
	int *abort;
	enum cmd_type type;
	enum mem_state state;
	enum client_state client_state;
	struct cmd_event *_next;
};

struct cmd {
	struct AFU_EVENT *afu_event;
	struct cmd_event *list;
	struct cmd_event *buffer_read;
	struct mmio *mmio;
	struct parms *parms;
	struct client **client;
	struct pages page_entries;
	volatile enum ocse_state *ocl_state;
	char *afu_name;
	FILE *dbg_fp;
	uint8_t dbg_id;
	uint64_t lock_addr;
	//uint64_t res_addr;
	int max_clients;
	uint32_t pagesize;
	uint32_t HOST_CL_SIZE;
	uint16_t irq;
	//int locked;
};

struct cmd *cmd_init(struct AFU_EVENT *afu_event, struct parms *parms,
		     struct mmio *mmio, volatile enum ocse_state *state,
		     char *afu_name, FILE * dbg_fp, uint8_t dbg_id);

void handle_cmd(struct cmd *cmd,  uint32_t latency);

//void handle_buffer_data(struct cmd *cmd);

void handle_mem_write(struct cmd *cmd);

void handle_buffer_write(struct cmd *cmd);

void handle_afu_tlx_cmd_data_read(struct cmd *cmd);

void handle_afu_tlx_write_cmd(struct cmd *cmd);

void handle_touch(struct cmd *cmd);

void handle_interrupt(struct cmd *cmd);

void handle_mem_return(struct cmd *cmd, struct cmd_event *event, int fd);

void handle_aerror(struct cmd *cmd, struct cmd_event *event);

void handle_response(struct cmd *cmd);

void handle_write_be_or_amo(struct cmd *cmd);

void handle_xlate_intrp_pending_sent(struct cmd *cmd);


int client_cmd(struct cmd *cmd, struct client *client);

#endif				/* _CMD_H_ */
