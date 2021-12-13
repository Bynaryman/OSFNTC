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
 * Description: ocse.c
 *
 *  This file contains the main loop for the OCSE proxy that connects to AFU
 *  simulator(s) and allows client applications to connect for accessing the
 *  AFU(s).  When OCSE is executed parse_host_data() is called to find and
 *  connect to any AFU simulators specified in the shim_host.dat file. Each
 *  successful simulator connection will cause a seperate thread to be launched.
 *  The code for those threads is in ocl.c.  As long as at least one simulator
 *  connection is valid then OCSE will remain active and awaiting client
 *  connections.  Each time a valid client connection is made it will be
 *  assigned to the appropriate ocl thread for whichever AFU it is accessing.
 *  If it is the first client to connect then the AFU is reset and the AFU
 *  descriptor is read.
 */

#include <assert.h>
#include <arpa/inet.h>
#include <errno.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <termios.h>
#include <time.h>

#include "client.h"
#include "mmio.h"
#include "parms.h"
#include "ocl.h"
#include "shim_host.h"
#include "../common/debug.h"
#include "../common/utils.h"


struct ocl *ocl_list;
struct client *client_list;
pthread_mutex_t lock;
uint16_t afu_map;
int timeout;
FILE *fp;

// Disconnect client connections and stop threads gracefully on Ctrl-C
static void _INThandler(int sig)
{
	pthread_t thread;
	struct ocl *ocl;
	int i;

	// Flush debug output
	fflush(fp);

	// Shut down OCL threads
	ocl = ocl_list;
	while (ocl != NULL) {
		info_msg("Shutting down connection to %s\n", ocl->name);
		for (i = 0; i < ocl->max_clients; i++) {
			if (ocl->client[i] != NULL)
				ocl->client[i]->abort = 1;
		}
		ocl->state = OCSE_DONE;
		thread = ocl->thread;
		ocl = ocl->_next;
		pthread_join(thread, NULL);
	}
}

// Find OCL for specific AFU id
static struct ocl *_find_ocl(uint8_t id, uint8_t * major)
{
	struct ocl *ocl;

	//*major = id >> 4;
	//*minor = id & 0x3;
	*major = id;
	ocl = ocl_list;
	while (ocl) {
		if (id == ocl->dbg_id)
			break;
		ocl = ocl->_next;
	}
	return ocl;
}

// Query AFU descriptor data
static void _find_nth(struct client *client)
{
	struct ocl *ocl;
	uint8_t *buffer;
	uint8_t fcn, afuid;
	int size, offset;
	uint8_t name_length;
	char name[25];
	uint8_t card_index;
	uint8_t afu_index_valid;
	uint8_t afu_index;
	int this_iteration;

	// get the string length from the socket
	// get the string from the socket
	if (get_bytes_silent(client->fd, 1, &name_length, timeout, &(client->abort)) < 0) {
	  client_drop(client, TLX_IDLE_CYCLES, CLIENT_NONE);
	  debug_msg("_find_nth: could not communicate with socket");
	  return;
	}
	if (get_bytes_silent(client->fd, name_length, (uint8_t *)name, timeout, &(client->abort)) < 0) {
	  client_drop(client, TLX_IDLE_CYCLES, CLIENT_NONE);
	  debug_msg("_find_nth: could not communicate with socket");
	  return;
	}
	name[name_length] = '\0'; // null terminate name
	if (get_bytes_silent(client->fd, 1, &card_index, timeout, &(client->abort)) < 0) {
	  client_drop(client, TLX_IDLE_CYCLES, CLIENT_NONE);
	  debug_msg("_find_nth: could not communicate with socket");
	  return;
	}
	if (get_bytes_silent(client->fd, 1, &afu_index_valid, timeout, &(client->abort)) < 0) {
	  client_drop(client, TLX_IDLE_CYCLES, CLIENT_NONE);
	  debug_msg("_find_nth: could not communicate with socket");
	  return;
	}
	if (get_bytes_silent(client->fd, 1, &afu_index, timeout, &(client->abort)) < 0) {
	  client_drop(client, TLX_IDLE_CYCLES, CLIENT_NONE);
	  debug_msg("_find_nth: could not communicate with socket");
	  return;
	}

	debug_msg( "_find_nth: looking for the %d occurance of name: %s", card_index, name );

	// scan the ocl->function->afu structs for a matching name
	// loop through the ocl's
	ocl = ocl_list;
	this_iteration = 0;
	while (ocl != NULL) {
	      debug_msg("_find: ocl lop");
	      for (fcn = 0; fcn < 8; fcn++ ) {
		    debug_msg("_find: fcn %d lop", fcn);
		    if (ocl->mmio->fcn_cfg_array[fcn] == NULL) continue;

		    if (ocl->mmio->fcn_cfg_array[fcn]->afu_present == 0) continue;

		    for (afuid = 0; afuid <= ocl->mmio->fcn_cfg_array[fcn]->max_afu_index; afuid++ ) {
		          debug_msg("_find: afuid %d lop", afuid);
		          if (ocl->mmio->fcn_cfg_array[fcn]->afu_cfg_array[afuid] == NULL) continue;

			  // if the name here doesn't match, continue
		          debug_msg("_find: compare %s to %s", name, ocl->mmio->fcn_cfg_array[fcn]->afu_cfg_array[afuid]->namespace);
			  if ( strcmp( name, ocl->mmio->fcn_cfg_array[fcn]->afu_cfg_array[afuid]->namespace ) != 0 ) continue;

			  // the names match, so we've found an instance of this afu
			  // if this_iteration is not the card_index, increment and break the afuid loop.
			  if ( this_iteration != card_index ) {
			    this_iteration++;
			    // we only want to look once within a funciton, so break instead of continue
			    break;
			  }

			  // the names match and this_iteration is the card-indexth iteration, then return
			  // I'm not really sure this is accurate as we have ignored the afu_index that was
			  // sent in.  but the real ltc code seems to assume an afu index of 0, so this will
			  // do for now.
			  size =
			    1 +
			    sizeof(uint8_t) +
			    sizeof(uint8_t) +
			    sizeof(uint8_t) +
			    sizeof(uint8_t) ;

			  buffer = (uint8_t *) malloc(size);
			  offset = 0;

			  buffer[offset] = OCSE_FIND_ACK;
			  offset++;

			  buffer[offset] = ocl->bus;
			  offset = offset + sizeof(uint8_t);

			  buffer[offset] = 0;
			  offset = offset + sizeof(uint8_t);

			  buffer[offset] = fcn;
			  offset = offset + sizeof(uint8_t);

			  buffer[offset] = afuid;
			  offset = offset + sizeof(uint8_t);

			  if ( offset != size ) {
			    warn_msg( "anomoly in construction of OCSE_FIND_ACK message" );
			  }

		          debug_msg("_find_nth: found name the %d occurance of %s with bus %d, dev %d, fcn %d, afuid &d",
				    card_index, ocl->mmio->fcn_cfg_array[fcn]->afu_cfg_array[afuid]->namespace, ocl->bus, 0, fcn, afuid);
			  if ( put_bytes( client->fd, size, buffer, ocl->dbg_fp, ocl->dbg_id,
					  client->context ) < 0) {
			    client_drop(client, TLX_IDLE_CYCLES, CLIENT_NONE);
			  }

			  free(buffer);
			  return;
		    }
		    this_iteration++;
	      }
	      ocl = ocl->_next;
	}

	// if we are here, we did not find anything
	debug_msg( "_find_nth: did not the %d occurance of find name %s", card_index, name );
	size = 1;
	buffer = (uint8_t *) malloc(size);
	buffer[0] = OCSE_FAILED;
	if ( put_bytes_silent( client->fd, size, buffer ) < 0) {
	  client_drop(client, TLX_IDLE_CYCLES, CLIENT_NONE);
	}
	free(buffer);

	return;
}

// Query AFU descriptor data
static void _find(struct client *client)
{
	struct ocl *ocl;
	uint8_t *buffer;
	uint8_t fcn, afuid;
	int size, offset;
	uint8_t name_length;
	char name[25];

	// get the string length from the socket
	// get the string from the socket
	if (get_bytes_silent(client->fd, 1, &name_length, timeout, &(client->abort)) < 0) {
	  client_drop(client, TLX_IDLE_CYCLES, CLIENT_NONE);
	  debug_msg("_find: could not communicate with socket");
	  return;
	}
	if (get_bytes_silent(client->fd, name_length, (uint8_t *)name, timeout, &(client->abort)) < 0) {
	  client_drop(client, TLX_IDLE_CYCLES, CLIENT_NONE);
	  debug_msg("_find: could not communicate with socket");
	  return;
	}
	name[name_length] = '\0'; // null terminate name

	debug_msg( "_find: looking up name: %s", name );

	// scan the ocl->function->afu structs for a matching name
	// loop through the ocl's
	ocl = ocl_list;
	while (ocl != NULL) {
	      debug_msg("_find: ocl lop");
	      for (fcn = 0; fcn < 8; fcn++ ) {
		    debug_msg("_find: fcn %d lop", fcn);
		    if (ocl->mmio->fcn_cfg_array[fcn] == NULL) continue;

		    if (ocl->mmio->fcn_cfg_array[fcn]->afu_present == 0) continue;

		    for (afuid = 0; afuid <= ocl->mmio->fcn_cfg_array[fcn]->max_afu_index; afuid++ ) {
		          debug_msg("_find: afuid %d lop", afuid);
		          if (ocl->mmio->fcn_cfg_array[fcn]->afu_cfg_array[afuid] == NULL) continue;

			  // if the name here doesn't match, continue
		          debug_msg("_find: compare %s to %s", name, ocl->mmio->fcn_cfg_array[fcn]->afu_cfg_array[afuid]->namespace);
			  if ( strcmp( name, ocl->mmio->fcn_cfg_array[fcn]->afu_cfg_array[afuid]->namespace ) != 0 ) continue;

			  // the names match, so return bus, function, device, and afu_index
			  size =
			    1 +
			    sizeof(uint8_t) +
			    sizeof(uint8_t) +
			    sizeof(uint8_t) +
			    sizeof(uint8_t) ;

			  buffer = (uint8_t *) malloc(size);
			  offset = 0;

			  buffer[offset] = OCSE_FIND_ACK;
			  offset++;

			  buffer[offset] = ocl->bus;
			  offset = offset + sizeof(uint8_t);

			  buffer[offset] = 0;
			  offset = offset + sizeof(uint8_t);

			  buffer[offset] = fcn;
			  offset = offset + sizeof(uint8_t);

			  buffer[offset] = afuid;
			  offset = offset + sizeof(uint8_t);

			  if ( offset != size ) {
			    warn_msg( "anomoly in construction of OCSE_FIND_ACK message" );
			  }

		          debug_msg("_find: found name %s with bus %d, dev %d, fcn %d, afuid &d",
				    ocl->mmio->fcn_cfg_array[fcn]->afu_cfg_array[afuid]->namespace, ocl->bus, 0, fcn, afuid);
			  if ( put_bytes( client->fd, size, buffer, ocl->dbg_fp, ocl->dbg_id,
					  client->context ) < 0) {
			    client_drop(client, TLX_IDLE_CYCLES, CLIENT_NONE);
			  }

			  free(buffer);
			  return;
		    }
	      }
	      ocl = ocl->_next;
	}

	// if we are here, we did not find anything
	debug_msg( "_find: did not find name %s", name );
	size = 1;
	buffer = (uint8_t *) malloc(size);
	buffer[0] = OCSE_FAILED;
	if ( put_bytes_silent( client->fd, size, buffer ) < 0) {
	  client_drop(client, TLX_IDLE_CYCLES, CLIENT_NONE);
	}
	free(buffer);

	return;
}

// Query AFU descriptor data
static void _query(struct client *client)
{
	struct ocl *ocl;
	uint8_t *buffer;
	uint8_t major, bus, dev, fcn, afuid;
	int size, offset;

	if (get_bytes_silent(client->fd, 1, &bus, timeout, &(client->abort)) < 0) {
	  client_drop(client, TLX_IDLE_CYCLES, CLIENT_NONE);
	  debug_msg("_query: could not communicate with socket");
	  return;
	}
	if (get_bytes_silent(client->fd, 1, &dev, timeout, &(client->abort)) < 0) {
	  client_drop(client, TLX_IDLE_CYCLES, CLIENT_NONE);
	  debug_msg("_query: could not communicate with socket");
	  return;
	}
	if (get_bytes_silent(client->fd, 1, &fcn, timeout, &(client->abort)) < 0) {
	  client_drop(client, TLX_IDLE_CYCLES, CLIENT_NONE);
	  debug_msg("_query: could not communicate with socket");
	  return;
	}
	if (get_bytes_silent(client->fd, 1, &afuid, timeout, &(client->abort)) < 0) {
	  client_drop(client, TLX_IDLE_CYCLES, CLIENT_NONE);
	  debug_msg("_query: could not communicate with socket");
	  return;
	}

	// use bus for id into _find_ocl for now - the pointers to all of the following values will change later.
	ocl = _find_ocl(bus, &major);
	client->bus = bus;
	client->dev = dev;
	client->fcn = fcn;
	client->afuid = afuid;
	client->bdf = ( (uint16_t)bus << 8 ) | ( (uint16_t)dev << 3 ) | ( (uint16_t)fcn );

	size =
	  1 +
	  sizeof(ocl->mmio->fcn_cfg_array[fcn]->device_id) + // 2
	  sizeof(ocl->mmio->fcn_cfg_array[fcn]->vendor_id) + // 2
	  sizeof(ocl->mmio->fcn_cfg_array[fcn]->afu_cfg_array[afuid]->afu_version_major) + // 1
	  sizeof(ocl->mmio->fcn_cfg_array[fcn]->afu_cfg_array[afuid]->afu_version_minor) + // 1
	  sizeof(ocl->mmio->fcn_cfg_array[fcn]->afu_cfg_array[afuid]->global_mmio_offset) + // 8
	  sizeof(ocl->mmio->fcn_cfg_array[fcn]->afu_cfg_array[afuid]->global_mmio_size) + // 4
	  sizeof(ocl->mmio->fcn_cfg_array[fcn]->afu_cfg_array[afuid]->pp_mmio_offset) + // 8
	  sizeof(ocl->mmio->fcn_cfg_array[fcn]->afu_cfg_array[afuid]->pp_mmio_stride) + // 4
	  sizeof(ocl->mmio->fcn_cfg_array[fcn]->afu_cfg_array[afuid]->mem_base_address) + // 8
	  sizeof(ocl->mmio->fcn_cfg_array[fcn]->afu_cfg_array[afuid]->mem_size) ; // 4

	buffer = (uint8_t *) malloc(size);
	buffer[0] = OCSE_QUERY;
	offset = 1;

	memcpy(&(buffer[offset]),
	       (char *)&(ocl->mmio->fcn_cfg_array[fcn]->device_id),
	       sizeof(ocl->mmio->fcn_cfg_array[fcn]->device_id));
        offset += sizeof(ocl->mmio->fcn_cfg_array[fcn]->device_id);

	memcpy(&(buffer[offset]),
	       (char *)&(ocl->mmio->fcn_cfg_array[fcn]->vendor_id),
	       sizeof(ocl->mmio->fcn_cfg_array[fcn]->vendor_id));
        offset += sizeof(ocl->mmio->fcn_cfg_array[fcn]->vendor_id);

	memcpy(&(buffer[offset]),
	       (char *)&(ocl->mmio->fcn_cfg_array[fcn]->afu_cfg_array[afuid]->afu_version_major),
	       sizeof(ocl->mmio->fcn_cfg_array[fcn]->afu_cfg_array[afuid]->afu_version_major));
	offset += sizeof(ocl->mmio->fcn_cfg_array[fcn]->afu_cfg_array[afuid]->afu_version_major);

	memcpy(&(buffer[offset]),
	       (char *)&(ocl->mmio->fcn_cfg_array[fcn]->afu_cfg_array[afuid]->afu_version_minor),
	       sizeof(ocl->mmio->fcn_cfg_array[fcn]->afu_cfg_array[afuid]->afu_version_minor));
	offset += sizeof(ocl->mmio->fcn_cfg_array[fcn]->afu_cfg_array[afuid]->afu_version_minor);

	memcpy(&(buffer[offset]),
	       (char *)&(ocl->mmio->fcn_cfg_array[fcn]->afu_cfg_array[afuid]->global_mmio_offset),
	       sizeof(ocl->mmio->fcn_cfg_array[fcn]->afu_cfg_array[afuid]->global_mmio_offset));
	offset += sizeof(ocl->mmio->fcn_cfg_array[fcn]->afu_cfg_array[afuid]->global_mmio_offset);

	memcpy(&(buffer[offset]),
	       (char *)&(ocl->mmio->fcn_cfg_array[fcn]->afu_cfg_array[afuid]->global_mmio_size),
	       sizeof(ocl->mmio->fcn_cfg_array[fcn]->afu_cfg_array[afuid]->global_mmio_size));
	offset += sizeof(ocl->mmio->fcn_cfg_array[fcn]->afu_cfg_array[afuid]->global_mmio_size);

	memcpy(&(buffer[offset]),
	       (char *)&(ocl->mmio->fcn_cfg_array[fcn]->afu_cfg_array[afuid]->pp_mmio_offset),
	       sizeof(ocl->mmio->fcn_cfg_array[fcn]->afu_cfg_array[afuid]->pp_mmio_offset));
	offset += sizeof(ocl->mmio->fcn_cfg_array[fcn]->afu_cfg_array[afuid]->pp_mmio_offset);

	memcpy(&(buffer[offset]),
	       (char *)&(ocl->mmio->fcn_cfg_array[fcn]->afu_cfg_array[afuid]->pp_mmio_stride),
	       sizeof(ocl->mmio->fcn_cfg_array[fcn]->afu_cfg_array[afuid]->pp_mmio_stride));
	offset += sizeof(ocl->mmio->fcn_cfg_array[fcn]->afu_cfg_array[afuid]->pp_mmio_stride);

	memcpy(&(buffer[offset]),
	       (char *)&(ocl->mmio->fcn_cfg_array[fcn]->afu_cfg_array[afuid]->mem_base_address),
	       sizeof(ocl->mmio->fcn_cfg_array[fcn]->afu_cfg_array[afuid]->mem_base_address));
	offset += sizeof(ocl->mmio->fcn_cfg_array[fcn]->afu_cfg_array[afuid]->mem_base_address);

	memcpy(&(buffer[offset]),
	       (char *)&(ocl->mmio->fcn_cfg_array[fcn]->afu_cfg_array[afuid]->mem_size),
	       sizeof(ocl->mmio->fcn_cfg_array[fcn]->afu_cfg_array[afuid]->mem_size));
	offset += sizeof(ocl->mmio->fcn_cfg_array[fcn]->afu_cfg_array[afuid]->mem_size);

	if (put_bytes(client->fd, size, buffer, ocl->dbg_fp, ocl->dbg_id,
		      client->context) < 0) {
		client_drop(client, TLX_IDLE_CYCLES, CLIENT_NONE);
	}

	free(buffer);
}

static void _free_client(struct client *client)
{
	if (client == NULL)
		return;

	if (client->ip)
		free(client->ip);

	free(client);
}

// Handshake with client and attach to OCL
static struct client *_client_connect(int *fd, char *ip)
{
	struct client *client;
	uint8_t buffer[MAX_LINE_CHARS];
	uint8_t ack[3];
	uint16_t map;
	int rc;

	// Parse client handshake data
	ack[0] = OCSE_DETACH;
	memset(buffer, '\0', MAX_LINE_CHARS);
	rc = get_bytes(*fd, 4, buffer, timeout, 0, fp, -1, -1);
	if ((rc < 0) || (strcmp((char *)buffer, "OCSE"))) {
		info_msg("Connecting application is not OCSE client\n");
		info_msg("Expected: \"OCSE\" Got: \"%s\"", buffer);
		put_bytes(*fd, 1, ack, fp, -1, -1);
		close_socket(fd);
		return NULL;
	}
	rc = get_bytes_silent(*fd, 2, buffer, timeout, 0);
	if ((rc < 0) || ((uint8_t) buffer[0] != OCSE_VERSION_MAJOR) ||
	    ((uint8_t) buffer[1] != OCSE_VERSION_MINOR)) {
		info_msg("Client is wrong version\n");
		put_bytes(*fd, 1, ack, fp, -1, -1);
		close_socket(fd);
		return NULL;
	}
	// Initialize client struct
	client = (struct client *)calloc(1, sizeof(struct client));
	client->fd = *fd;
	client->ip = ip;
	client->pending = 1;
	client->timeout = timeout;
	client->flushing = FLUSH_NONE;
	client->state = CLIENT_INIT;

	// lgt quick fix for bdf
	client->bdf = 0x5001;

	// Return acknowledge to client
	ack[0] = OCSE_CONNECT;
	map = htons(afu_map);
	memcpy(&(ack[1]), &map, sizeof(map));
	if (put_bytes(client->fd, 3, ack, fp, -1, -1) < 0) {
		_free_client(client);
		return NULL;
	}

	info_msg("%s connected", client->ip);
	return client;
}

// Associate client to OCL
static int _client_associate(struct client *client)
{
	struct ocl *ocl;
	uint32_t mmio_offset, mmio_size;
	uint8_t major, bus, dev, fcn, afuid;
	int i, context, clients;
	uint8_t rc[2];

	// retreive bus, dev, fcn, and afuid from socket
	if (get_bytes_silent(client->fd, 1, &bus, timeout, &(client->abort)) < 0) {
	  client_drop(client, TLX_IDLE_CYCLES, CLIENT_NONE);
	  debug_msg("_query: could not communicate with socket");
	  return -1;
	}
	if (get_bytes_silent(client->fd, 1, &dev, timeout, &(client->abort)) < 0) {
	  client_drop(client, TLX_IDLE_CYCLES, CLIENT_NONE);
	  debug_msg("_query: could not communicate with socket");
	  return -1;
	}
	if (get_bytes_silent(client->fd, 1, &fcn, timeout, &(client->abort)) < 0) {
	  client_drop(client, TLX_IDLE_CYCLES, CLIENT_NONE);
	  debug_msg("_query: could not communicate with socket");
	  return -1;
	}
	if (get_bytes_silent(client->fd, 1, &afuid, timeout, &(client->abort)) < 0) {
	  client_drop(client, TLX_IDLE_CYCLES, CLIENT_NONE);
	  debug_msg("_query: could not communicate with socket");
	  return -1;
	}

	// Associate with OCL
	rc[0] = OCSE_DETACH;
	ocl = _find_ocl(bus, &major);
	if (!ocl) {
		info_msg("Did not find valid OCL for tlx%d\n", major);
		put_bytes(client->fd, 1, &(rc[0]), fp, -1, -1);
		close_socket(&(client->fd));
		return -1;
	}

	// Look for open client slot
	// dedicated - client[0] is the only client.
	// afu-directed - is client[0] the master? not necessarily
	assert(ocl->max_clients > 0);
	clients = 0;
	context = -1;
	for (i = 0; i < ocl->max_clients; i++) {
		if (ocl->client[i] != NULL)
			++clients;
		if ((context < 0) && (ocl->client[i] == NULL)) {
			client->context = context = i;
			// lgt quick fix for pasid
			client->pasid = i; //???
			client->state = CLIENT_VALID;
			client->pending = 0;
			ocl->client[i] = client;
			break;
		}
	}
	if (context < 0) {
		info_msg("No room for new client on tlx%d\n", major);
		put_bytes(client->fd, 1, &(rc[0]), fp, ocl->dbg_id, -1);
		close_socket(&(client->fd));
		return -1;
	}

	// Attach to OCL
	// i should point to an open slot
	rc[0] = OCSE_OPEN;
	rc[1] = context;
	mmio_offset = 0;
	mmio_size = MMIO_FULL_RANGE;
	client->mmio_size = mmio_size;
	client->mmio_offset = mmio_offset;

	// We NO LONGER Send reset to AFU, even if no other clients are connected
	// don't even send a reset if we've dropped to 0 clients and are now opening a new one

	// Acknowledge to client
	if (put_bytes(client->fd, 2, &(rc[0]), fp, ocl->dbg_id, context) < 0) {
		close_socket(&(client->fd));
		return -1;
	}
	debug_context_add(fp, ocl->dbg_id, context);

	return 0;
}

static void *_client_loop(void *ptr)
{
	struct client *client = (struct client *)ptr;
	uint8_t data[2];
	int rc;

	pthread_mutex_lock(&lock);
	while (client->pending) {
		rc = bytes_ready(client->fd, client->timeout, &(client->abort));
		if (rc == 0) {
			lock_delay(&lock);
			continue;
		}
		if ((rc < 0) || get_bytes(client->fd, 1, data, 10,
					  &(client->abort), fp, -1, -1) < 0) {
			client_drop(client, TLX_IDLE_CYCLES, CLIENT_NONE);
			break;
		}
		if (data[0] == OCSE_QUERY) {
			_query(client);
			lock_delay(&lock);
			continue;
		}
		if (data[0] == OCSE_FIND) {
			_find(client);
			lock_delay(&lock);
			continue;
		}
		if (data[0] == OCSE_FIND_NTH) {
			_find_nth(client);
			lock_delay(&lock);
			continue;
		}
		if (data[0] == OCSE_OPEN) {
			_client_associate(client);
			debug_msg("_client_loop: client associated");
			break;
		}
		client->pending = 0;
		break;
		lock_delay(&lock);
	}
	pthread_mutex_unlock(&lock);

	// Terminate thread
	pthread_exit(NULL);
}

static int _start_server()
{
	struct sockaddr_in serv_addr;
	int listen_fd, port, bound, yes;
	char hostname[MAX_LINE_CHARS];

	// Start server
	port = 16384;
	bound = 0;
	listen_fd = -1;
	yes = 1;
	memset(&serv_addr, 0, sizeof(serv_addr));
	listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	while (!bound) {
		serv_addr.sin_port = htons(port);
		if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes,
			       sizeof(int)) < 0) {
			perror("setsockopt");
			return -1;
		}
		if (bind(listen_fd, (struct sockaddr *)&serv_addr,
			 sizeof(serv_addr)) < 0) {
			if (errno != EADDRINUSE) {
				perror("bind");
				return -1;
			}
			if (port == 0xFFFF) {
				perror("bind");
				return -1;
			}
			debug_msg("_start_server: Bumping port count");
			++port;
			continue;
		}
		bound = 1;
	}
	listen(listen_fd, 4);	// FIXME: constant 4
	hostname[MAX_LINE_CHARS - 1] = '\0';
	gethostname(hostname, MAX_LINE_CHARS - 1);
	info_msg("Started OCSE server, listening on %s:%d", hostname, port);

	return listen_fd;
}

//
// Main
//

int main(int argc, char **argv)
{
	struct sockaddr_in client_addr;
	struct client *client;
	struct client **client_ptr;
	int listen_fd, connect_fd;
	socklen_t client_len;
	sigset_t set;
	struct sigaction action;
	char *shim_host_path;
	char *parms_path;
	char *debug_log_path;
	struct parms *parms;
	char *ip;

	// Open debug.log file
	debug_log_path = getenv("DEBUG_LOG_PATH");
	if (!debug_log_path) debug_log_path = "debug.log";
	fp = fopen(debug_log_path, "w");
	if (!fp) {
		error_msg("Could not open debug.log");
		return -1;
	}

	// Mask SIGPIPE signal for all threads
	sigemptyset(&set);
	sigaddset(&set, SIGPIPE);
	if (pthread_sigmask(SIG_BLOCK, &set, NULL)) {
		perror("pthread_sigmask");
		return -1;
	}
	// Catch SIGINT for graceful termination
	action.sa_handler = _INThandler;
	sigemptyset(&(action.sa_mask));
	action.sa_flags = 0;
	sigaction(SIGINT, &action, NULL);

	// Report version
	info_msg("OCSE version %d.%03d compiled @ %s %s", OCSE_VERSION_MAJOR,
		 OCSE_VERSION_MINOR, __DATE__, __TIME__);
#ifdef TLX3
	info_msg("OCSE version supports OpenCAPI 3.0 \n");
#endif /* ifdef TLX3 */

	debug_send_version(fp, OCSE_VERSION_MAJOR, OCSE_VERSION_MINOR);

	// Parse parameters file
	parms_path = getenv("OCSE_PARMS");
	if (!parms_path) parms_path = "ocse.parms";
	parms = parse_parms(parms_path, fp);
	if (parms == NULL) {
		error_msg("Unable to parse params file \"%s\"", parms_path);
		return -1;
	}
	timeout = parms->timeout;

	// Connect to simulator(s) and start ocl thread(s)
	pthread_mutex_init(&lock, NULL);
	pthread_mutex_lock(&lock);
	shim_host_path = getenv("SHIM_HOST_DAT");
	if (!shim_host_path) shim_host_path = "shim_host.dat";
	afu_map = parse_host_data(&ocl_list, parms, shim_host_path, &lock, fp);
	if (ocl_list == NULL) {
		pthread_mutex_unlock(&lock);
		free(parms);
		fclose(fp);
		pthread_mutex_destroy(&lock);
		warn_msg("Unable to connect to any simulators");
		return -1;
	}
	// Start server
	if ((listen_fd = _start_server()) < 0) {
		pthread_mutex_unlock(&lock);
		free(parms);
		fclose(fp);
		pthread_mutex_destroy(&lock);
		return -1;
	}
	// Watch for client connections
	while (ocl_list != NULL) {
		// Wait for next client to connect
		client_len = sizeof(client_addr);
		pthread_mutex_unlock(&lock);
		connect_fd = accept(listen_fd, (struct sockaddr *)&client_addr,
				    &client_len);
		pthread_mutex_lock(&lock);
		if (connect_fd < 0) {
			lock_delay(&lock);
			continue;
		}
		ip = (char *)malloc(INET_ADDRSTRLEN + 1);
		inet_ntop(AF_INET, &(client_addr.sin_addr.s_addr), ip,
			  INET_ADDRSTRLEN);
		// Clean up disconnected clients
		client_ptr = &client_list;
		while (*client_ptr != NULL) {
			client = *client_ptr;
			if ((client->pending == 0)
			    && (client->state == CLIENT_NONE)) {
				*client_ptr = client->_next;
				if (client->_next != NULL)
					client->_next->_prev = client->_prev;
				_free_client(client);
				lock_delay(&lock);
				continue;
			}
			client_ptr = &((*client_ptr)->_next);
		}
		// Add new client
		info_msg("Connection from %s", ip);
		client = _client_connect(&connect_fd, ip);
		if (client != NULL) {
			if (client_list != NULL)
				client_list->_prev = client;
			client->_next = client_list;
			client_list = client;
			if (pthread_create(&(client->thread), NULL,
					   _client_loop, client)) {
				perror("pthread_create");
				break;
			}
		}
		lock_delay(&lock);
	}
	info_msg("No AFUs connected, Shutting down OCSE\n");
	close_socket(&listen_fd);

	// Shutdown unassociated client connections
	while (client_list != NULL) {
		client = client_list;
		client_list = client->_next;
		if (client->pending)
			client->pending = 0;
		pthread_mutex_unlock(&lock);
		pthread_join(client->thread, NULL);
		pthread_mutex_lock(&lock);

		close_socket(&(client->fd));
		_free_client(client);
	}
	pthread_mutex_unlock(&lock);

	free(parms);
	fclose(fp);
	pthread_mutex_destroy(&lock);

	return 0;
}
