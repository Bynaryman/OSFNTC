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

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "parms.h"
#include "../common/utils.h"
#include "../common/debug.h"

#define DEFAULT_PAGESIZE 0

// Randomly decide based on percent chance
static inline int percent_chance(int chance)
{
	return ((rand() % 100) < chance);
}

// Randomly decide to allow response to AFU
int allow_resp(struct parms *parms)
{
	return percent_chance(parms->resp_percent);
}

// Randomly decide to allow PAGED response
int allow_paged(struct parms *parms)
{
	return percent_chance(parms->paged_percent);
}

// Randomly decide to allow RETRY response
int allow_retry(struct parms *parms)
{
	return percent_chance(parms->retry_percent);
}

// Randomly decide to allow FAILED response
int allow_failed(struct parms *parms)
{
	return percent_chance(parms->failed_percent);
}

// Randomly decide to allow dERROR response
int allow_derror(struct parms *parms)
{
	return percent_chance(parms->derror_percent);
}

// Randomly decide to allow PENDING response
int allow_pending(struct parms *parms)
{
	return percent_chance(parms->pending_percent);
}

// Randomly decide to allow RETRY response for interrupts
int allow_int_retry(struct parms *parms)
{
	return percent_chance(parms->int_retry_percent);
}

// Randomly decide to allow FAILED response for interrupts
int allow_int_failed(struct parms *parms)
{
	return percent_chance(parms->int_failed_percent);
}

// Randomly decide to allow dERROR response for interrupts
int allow_int_derror(struct parms *parms)
{
	return percent_chance(parms->int_derror_percent);
}

// Randomly decide to allow PENDING response for interrupts
int allow_int_pending(struct parms *parms)
{
	return percent_chance(parms->int_pending_percent);
}

// Randomly decide to allow BDI error for response data
int allow_bdi_resp_err(struct parms *parms)
{
	return percent_chance(parms->bdi_resp_err_percent);
}

// Randomly decide to allow BDI error for cmd data
int allow_bdi_cmd_err(struct parms *parms)
{
	return percent_chance(parms->bdi_cmd_err_percent);
}

// Randomly decide to allow command to be handled out of order
int allow_reorder(struct parms *parms)
{
	return percent_chance(parms->reorder_percent);
}

// Randomly decide to allow bogus buffer activity
int allow_buffer(struct parms *parms)
{
	return percent_chance(parms->buffer_percent);
}

// Decide a single random percentage value from a percentage range
static void percent_parm(char *value, int *parm)
{
	int min, max;
	char *comma;

	*parm = atoi(value);
	comma = strchr(value, ',');
	if (comma) {
		min = *parm;
		*comma = '\0';
		++comma;
		max = atoi(comma);
		if (max < min) {
			min = max;
			max = *parm;
		}
		*parm = min + (rand() % (1 + max - min));
	}
}

// Open and parse parms file
struct parms *parse_parms(char *filename, FILE * dbg_fp)
{
	struct parms *parms;
	char parm[MAX_LINE_CHARS];
	char *value;
	FILE *fp;
	int data;

	// Allocate memory for struct
	parms = (struct parms *)malloc(sizeof(struct parms));
	if (parms == NULL)
		return NULL;

	// Set default parameter values
	parms->timeout = 10;
	parms->pagesize = DEFAULT_PAGESIZE;
	parms->host_CL_size = 128;
	parms->seed = (unsigned int)time(NULL);
	parms->resp_percent = 20;
	parms->paged_percent = 5;
	parms->retry_percent = 5;
	parms->failed_percent = 5;
	parms->pending_percent = 5;
	parms->reorder_percent = 20;
	parms->buffer_percent = 50;

	// Open file and parse contents
	fp = fopen(filename, "r");
	if (fp == NULL) {
		perror("fopen");
		free(parms);
		return NULL;
	}
	while (fgets(parm, MAX_LINE_CHARS, fp)) {
		// Strip newline char
		value = strchr(parm, '\n');
		if (value)
			*value = '\0';

		// Skip comment lines
		value = strchr(parm, '#');
		if (value)
			continue;

		// Skip blank lines
		value = strchr(parm, ' ');
		if (value)
			*value = '\0';
		value = strchr(parm, '\t');
		if (value)
			*value = '\0';
		if (!strlen(parm))
			continue;

		// Look for valid parms
		value = strchr(parm, ':');
		if (value) {
			*value = '\0';
			++value;
		} else {
			error_msg("Invalid format in %s: Expected ':', %s",
				  filename, parm);
			continue;
		}

		// Set valid parms
		if (!(strcmp(parm, "SEED"))) {
			parms->seed = atoi(value);
			debug_parm(dbg_fp, DBG_PARM_SEED, parms->seed);
		} else if (!(strcmp(parm, "TIMEOUT"))) {
			parms->timeout = atoi(value);
			debug_parm(dbg_fp, DBG_PARM_TIMEOUT, parms->timeout);
		} else if (!(strcmp(parm, "PAGESIZE"))) {
			data = atoi(value);
			if ((data < DEFAULT_PAGESIZE) || (data == 1) || (data == 6) || (data >= 8))
				warn_msg("PAGESIZE must be either 0, 2, 3, 4, 5, or 7 ");
			else
				parms->pagesize = data;
			//debug_parm(dbg_fp, DBG_PARM_PAGESIZE, parms->pagesize);
		} else if (!(strcmp(parm, "HOST_CL_SIZE"))) {
			data = atoi(value);
			if ((data != 128) && (data != 64) && (data != 256)) 
				warn_msg("HOST_CL_SIZE must be either 64 or 128 or 256 ");
			else
				parms->host_CL_size = data;
		} else if (!(strcmp(parm, "RESPONSE_PERCENT"))) {
			percent_parm(value, &data);
			if ((data > 100) || (data <= 0))
				warn_msg("RESPONSE_PERCENT must be 1-100");
			else
				parms->resp_percent = data;
			debug_parm(dbg_fp, DBG_PARM_RESP_PERCENT,
				   parms->resp_percent);
		} else if (!(strcmp(parm, "PAGED_PERCENT"))) {
			percent_parm(value, &data);
			if ((data >= 100) || (data < 0))
				warn_msg("PAGED_PERCENT must be 0-99");
			else
				parms->paged_percent = data;
			debug_parm(dbg_fp, DBG_PARM_PAGED_PERCENT,
				   parms->paged_percent);
		} else if (!(strcmp(parm, "RETRY_PERCENT"))) {
			percent_parm(value, &data);
			if ((data >= 100) || (data < 0))
				warn_msg("RETRY_PERCENT must be 0-99");
			else
				parms->retry_percent = data;
			debug_parm(dbg_fp, DBG_PARM_RETRY_PERCENT,
				   parms->retry_percent);
		} else if (!(strcmp(parm, "FAILED_PERCENT"))) {
			percent_parm(value, &data);
			if ((data >= 100) || (data < 0))
				warn_msg("FAILED_PERCENT must be 0-99");
			else
				parms->failed_percent = data;
			debug_parm(dbg_fp, DBG_PARM_FAILED_PERCENT,
				   parms->failed_percent);
		} else if (!(strcmp(parm, "PENDING_PERCENT"))) {
			percent_parm(value, &data);
			if ((data >= 100) || (data < 0))
				warn_msg("PENDING_PERCENT must be 0-99");
			else
				parms->pending_percent = data;
			debug_parm(dbg_fp, DBG_PARM_PENDING_PERCENT,
				   parms->pending_percent);
		} else if (!(strcmp(parm, "DERROR_PERCENT"))) {
			percent_parm(value, &data);
			if ((data >= 100) || (data < 0))
				warn_msg("DERROR_PERCENT must be 0-99");
			else
				parms->derror_percent = data;
			debug_parm(dbg_fp, DBG_PARM_DERROR_PERCENT,
				   parms->derror_percent);
		} else if (!(strcmp(parm, "INT_RETRY_PERCENT"))) {
			percent_parm(value, &data);
			if ((data >= 100) || (data < 0))
				warn_msg("INT_RETRY_PERCENT must be 0-99");
			else
				parms->int_retry_percent = data;
			debug_parm(dbg_fp, DBG_PARM_INT_RETRY_PERCENT,
				   parms->int_retry_percent);
		} else if (!(strcmp(parm, "INT_FAILED_PERCENT"))) {
			percent_parm(value, &data);
			if ((data >= 100) || (data < 0))
				warn_msg("INT_FAILED_PERCENT must be 0-99");
			else
				parms->int_failed_percent = data;
			debug_parm(dbg_fp, DBG_PARM_INT_FAILED_PERCENT,
				   parms->int_failed_percent);
		} else if (!(strcmp(parm, "INT_PENDING_PERCENT"))) {
			percent_parm(value, &data);
			if ((data >= 100) || (data < 0))
				warn_msg("INT_PENDING_PERCENT must be 0-99");
			else
				parms->int_pending_percent = data;
			debug_parm(dbg_fp, DBG_PARM_INT_PENDING_PERCENT,
				   parms->int_pending_percent);
		} else if (!(strcmp(parm, "INT_DERROR_PERCENT"))) {
			percent_parm(value, &data);
			if ((data >= 100) || (data < 0))
				warn_msg("INT_DERROR_PERCENT must be 0-99");
			else
				parms->int_derror_percent = data;
			debug_parm(dbg_fp, DBG_PARM_INT_DERROR_PERCENT,
				   parms->int_derror_percent);
		} else if (!(strcmp(parm, "BDI_RESP_ERR_PERCENT"))) {
			percent_parm(value, &data);
			if ((data >= 100) || (data < 0))
				warn_msg("BDI_RESP_ERR_PERCENT must be 0-99");
			else
				parms->bdi_resp_err_percent = data;
			debug_parm(dbg_fp, DBG_PARM_BDI_RESP_ERR_PERCENT,
				   parms->bdi_resp_err_percent);
		} else if (!(strcmp(parm, "BDI_CMD_ERR_PERCENT"))) {
			percent_parm(value, &data);
			if ((data >= 100) || (data < 0))
				warn_msg("BDI_CMD_ERR_PERCENT must be 0-99");
			else
				parms->bdi_cmd_err_percent = data;
			debug_parm(dbg_fp, DBG_PARM_BDI_CMD_ERR_PERCENT,
				   parms->bdi_cmd_err_percent);
		} else if (!(strcmp(parm, "REORDER_PERCENT"))) {
			percent_parm(value, &data);
			if ((data >= 100) || (data < 0))
				warn_msg("REORDER_PERCENT must be 0-99");
			else
				parms->reorder_percent = data;
			debug_parm(dbg_fp, DBG_PARM_REORDER_PERCENT,
				   parms->reorder_percent);
		} else if (!(strcmp(parm, "BUFFER_PERCENT"))) {
			percent_parm(value, &data);
			if ((data >= 100) || (data < 0))
				warn_msg("BUFFER_PERCENT must be 0-99");
			else
				parms->buffer_percent = data;
			debug_parm(dbg_fp, DBG_PARM_BUFFER_PERCENT,
				   parms->buffer_percent);
		} else {
			warn_msg("Ignoring invalid parm in %s: %s\n",
				 filename, parm);
			continue;
		}
	}

	// Close file and set seed
	fclose(fp);
	srand(parms->seed);

	// Print out parm settings
	info_msg("OCSE parm values:");
	printf("\tSeed     = %d\n", parms->seed);
	if (parms->timeout)
		printf("\tTimeout  = %d seconds\n", parms->timeout);
	else
		printf("\tTimeout  = DISABLED\n");
	printf("\tResponse = %d%%\n", parms->resp_percent);
	printf("\tHost_CL_sz = %d\n", parms->host_CL_size);
	printf("\tPaged    = %d%%\n", parms->paged_percent);
	printf("\tRetry    = %d%%\n", parms->retry_percent);
	printf("\tFailed   = %d%%\n", parms->failed_percent);
	printf("\tPending  = %d%%\n", parms->pending_percent);
	printf("\tDerror   = %d%%\n", parms->derror_percent);
	printf("\tINT_Retry   = %d%%\n", parms->int_retry_percent);
	printf("\tINT_Failed  = %d%%\n", parms->int_failed_percent);
	printf("\tINT_Pending = %d%%\n", parms->int_pending_percent);
	printf("\tINT_Derror  = %d%%\n", parms->int_derror_percent);
	printf("\tBDI_RESP_ERR= %d%%\n", parms->bdi_resp_err_percent);
	printf("\tBDI_CMD_ERR = %d%%\n", parms->bdi_cmd_err_percent);
	printf("\tReorder  = %d%%\n", parms->reorder_percent);
	printf("\tBuffer   = %d%%\n", parms->buffer_percent);

	// Adjust timeout to milliseconds
	parms->timeout *= 1000;

	return parms;
}
