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

/*
 * Description: shim_host.c
 *
 *  This file contains parse_host_data() which reads the file with the
 *  hostname and ports of each TLX/AFU simulator and calls ocl_init for each.
 */

#include <stdlib.h>

#include "shim_host.h"
#include "../common/utils.h"

// Parse file to find hostname and ports for AFU simulator(s)
uint16_t parse_host_data(struct ocl ** head, struct parms * parms,
			 char *filename, pthread_mutex_t * lock, FILE * dbg_fp)
{
	FILE *fp;
	struct ocl *ocl;
	char *hostdata, *comment, *tlx_id, *host, *port_str;
	uint16_t location, tlx_map;
	int port;

	tlx_map = 0;
	*head = NULL;
	fp = fopen(filename, "r");
	if (!fp) {
		hostdata =
		    (char *)malloc(strlen(filename) + strlen("fopen:") + 1);
		strcpy(hostdata, "fopen:");
		strcat(hostdata, filename);
		perror(hostdata);
		free(hostdata);
		return 0;
	}
	host = NULL;
	port_str = NULL;
	hostdata = (char *)malloc(MAX_LINE_CHARS);
	while (fgets(hostdata, MAX_LINE_CHARS - 1, fp)) {
		// Parse host & port from file
		tlx_id = hostdata;
		comment = strchr(hostdata, '#');
		if (comment)
			continue;
		host = strchr(hostdata, ',');
		if (host) {
			*host = '\0';
			++host;
		} else {
			error_msg("Invalid format in %s: Expected ',' :%s\n",
				  filename, hostdata);
			continue;
		}
		port_str = strchr(host, ':');
		if (port_str) {
			*port_str = '\0';
			++port_str;
		} else {
			error_msg("Invalid format in %s: Expected ':' :%s\n",
				  filename, host);
			continue;
		}
		if (!host) {
			error_msg("Invalid format in %s, hostname not found\n");
			continue;
		}
		if (!port_str) {
			error_msg("Invalid format in %s, Port not found\n");
			continue;
		}
		port = atoi(port_str);

		// Initialize OCL
		if ((location = ocl_init(head, parms, tlx_id, host, port,
					 lock, dbg_fp)) == 0) {
			continue;
		}
		tlx_map |= location;

		// Update all ocl entries to point to new list head
		ocl = *head;
		while (ocl) {
			ocl->head = head;
			ocl = ocl->_next;
		}
	}
	free(hostdata);
	fclose(fp);

	return tlx_map;
}
