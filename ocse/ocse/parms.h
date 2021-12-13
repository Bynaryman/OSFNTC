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

#ifndef _PARMS_H_
#define _PARMS_H_

#include <stdio.h>
#include "../common/tlx_interface.h"

struct parms {
	uint32_t timeout;
	uint32_t seed;
	uint32_t pagesize;
	uint32_t host_CL_size;
	uint32_t resp_percent;
	uint32_t paged_percent;
	uint32_t retry_percent;
	uint32_t failed_percent;
	uint32_t pending_percent;
	uint32_t derror_percent;
	uint32_t int_retry_percent;
	uint32_t int_failed_percent;
	uint32_t int_pending_percent;
	uint32_t int_derror_percent;
	uint32_t bdi_resp_err_percent;
	uint32_t bdi_cmd_err_percent;
	uint32_t reorder_percent;
	uint32_t buffer_percent;
};

// Randomly decide to allow response to AFU
int allow_resp(struct parms *parms);

// Randomly decide to allow PAGED response
int allow_paged(struct parms *parms);

// Randomly decide to allow RETRY response
int allow_retry(struct parms *parms);

// Randomly decide to allow FAILED response
int allow_failed(struct parms *parms);

// Randomly decide to allow PENDING response
int allow_pending(struct parms *parms);

// Randomly decide to allow dERROR response
int allow_derror(struct parms *parms);

// Randomly decide to allow RETRY response for interrupt
int allow_int_retry(struct parms *parms);

// Randomly decide to allow FAILED response for interrupt
int allow_int_failed(struct parms *parms);

// Randomly decide to allow PENDING response for interrupt
int allow_int_pending(struct parms *parms);

// Randomly decide to allow dERROR response for interrupt
int allow_int_derror(struct parms *parms);

// Randomly decide to allow setting of BDI bit
int allow_bdi_resp_err(struct parms *parms);

// Randomly decide to allow setting of BDI bit
int allow_bdi_cmd_err(struct parms *parms);

// Randomly decide to allow command to be handled out of order
int allow_reorder(struct parms *parms);

// Randomly decide to allow bogus buffer activity
int allow_buffer(struct parms *parms);

// Open and parse parms file
struct parms *parse_parms(char *filename, FILE * dbg_fp);

#endif				/* _PARMS_H_ */
