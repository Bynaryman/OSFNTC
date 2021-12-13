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

#ifndef _OCL_H_
#define _OCL_H_

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

#include "client.h"
#include "cmd.h"
#include "mmio.h"
#include "parms.h"
#include "../common/utils.h"


struct ocl {
	struct AFU_EVENT *afu_event;
	pthread_t thread;
	pthread_mutex_t *lock;
	FILE *dbg_fp;
	struct client **client;
	struct cmd *cmd;
	struct mmio *mmio;
	struct ocl **head;
	struct ocl *_prev;
	struct ocl *_next;
	volatile enum ocse_state state;
	uint32_t latency;
	char *name;
	char *host;
        uint8_t bus;
	uint8_t dbg_id;
	int port;
	int idle_cycles;
        int max_clients;                 // this is the sum of the max_pasids in each functions pasid dvsec
	int attached_clients;
	int timeout;
	int has_been_reset;
};

uint16_t ocl_init(struct ocl **head, struct parms *parms, char *id, char *host,
		  int port, pthread_mutex_t * lock, FILE * dbg_fp);

#endif				/* _OCL_H_ */
