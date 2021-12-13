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
 * Description: ocl.c
 *
 *  This file contains the foundation for the OCSE code for a single AFU.
 *  ocl_init() attempts to connect to an AFU simulator and initializes a
 *  ocl struct if successful.  Finally it starts a _ocl_loop thread for
 *  that AFU that will monitor any incoming socket data from either the
 *  simulator (AFU) or any clients (applications) that attach to this
 *  AFU.  The code in here is just the foundation for the ocl.  The code
 *  for handling commands and mmios are each in separate files.
 */

#include <arpa/inet.h>
#include <assert.h>
#include <inttypes.h>
#include <poll.h>
#include <stdlib.h>
#include <sys/types.h>

#include "mmio.h"
#include "ocl.h"
#include "../common/debug.h"
#include "../common/tlx_interface.h"

// are there any pending commands with this context?
int _is_cmd_pending(struct ocl *ocl, int32_t context)
{
  struct cmd_event *cmd_event;

  if ( ocl->cmd == NULL ) {
    // no cmd struct
    return 0;
  }

  cmd_event = ocl->cmd->list;
  while ( cmd_event != NULL ) {
    if ( cmd_event->context == context ) {
      // found a matching element
      return 1;
    }
    cmd_event = cmd_event->_next;
  }

  // no matching elements found
  return 0;

}

// Attach to AFU
static void _attach(struct ocl *ocl, struct client *client)
{
	uint8_t ack;

	// track number of clients in ocl
	// increment number of clients (decrement where we handle the completion of the detach)
	if (ocl->attached_clients < ocl->max_clients) {
	 	ocl->idle_cycles = TLX_IDLE_CYCLES;
	 	ack = OCSE_ATTACH;
	 }
	ocl->attached_clients++;
	ocl->state = OCSE_RUNNING;
	info_msg( "Attached client context %d: current attached clients = %d: client type = %c\n", client->context, ocl->attached_clients, client->type );


 	//attach_done:
	if (put_bytes(client->fd, 1, &ack, ocl->dbg_fp, ocl->dbg_id,
		      client->context) < 0) {
		client_drop(client, TLX_IDLE_CYCLES, CLIENT_NONE);
	}
}

// Client is detaching from the AFU
static void _detach(struct ocl *ocl, struct client *client)
{
	uint8_t ack = OCSE_DETACH;

	debug_msg("DETACH from client context 0x%02x", client->context);
	put_bytes(client->fd, 1, &ack, ocl->dbg_fp, ocl->dbg_id,
		      client->context);
	client_drop(client, TLX_IDLE_CYCLES, CLIENT_NONE);

	struct cmd_event *mem_access;

	// DEBUG
	debug_context_remove(ocl->dbg_fp, ocl->dbg_id, client->context);
	info_msg("%s client disconnect from %s context %d", client->ip,
		 ocl->name, client->context);
	close_socket(&(client->fd));
	if (client->ip)
		free(client->ip);
	client->ip = NULL;
	mem_access = (struct cmd_event *)client->mem_access;
	if (mem_access != NULL) {
		if (mem_access->state != MEM_DONE) {
			mem_access->resp = TLX_RESPONSE_FAILED;
			mem_access->state = MEM_DONE;
		}
	}
	client->mem_access = NULL;
	client->mmio_access = NULL;
	client->state = CLIENT_NONE;

	ocl->attached_clients--;
	info_msg( "Detatched a client: current attached clients = %d\n", ocl->attached_clients );
	//  we *really* free the client struct and it's contents back in ocl_loop
	if (ocl->attached_clients == 0) {
	  ocl->state = OCSE_IDLE;
	}

}


// Handle events from AFU
static void _handle_afu(struct ocl *ocl)
{
	if (ocl->mmio->list != NULL) {
	  handle_ap_resp(ocl->mmio);
	  handle_ap_resp_data(ocl->mmio);
	}

	if (ocl->cmd != NULL) {
	  // handle_response should follow a similar flow to handle_cmd
	  // that is, the response may need subsequent resp data valid beats to complete the data for a give response, just like a command...
	  handle_response(ocl->cmd);  // sends response and data (if required)
	  handle_buffer_write(ocl->cmd);  // just finishes up the read command structures
	  handle_xlate_intrp_pending_sent(ocl->cmd);  // just finishes up an xlate_pending resp
	  handle_cmd(ocl->cmd, ocl->latency);
	  handle_afu_tlx_cmd_data_read(ocl->cmd);  // just fills up the write command structures
	  handle_afu_tlx_write_cmd(ocl->cmd);  // completes the write command
	  handle_touch(ocl->cmd);
	  handle_interrupt(ocl->cmd);
	  handle_write_be_or_amo(ocl->cmd);
	}
}

static void _handle_client(struct ocl *ocl, struct client *client)
{
	struct mmio_event *mmio;
	struct cmd_event *cmd;
	uint8_t buffer[MAX_LINE_CHARS];
	int dw = 0;  // 1 means mmio that is 64 bits
	int global = 0;  // 1 means mmio to the global space
	int region = 0;  // 0 = lpc memory, 1 = global mmio, 2 = per process mmio

	// Handle MMIO done
	 if (client->mmio_access != NULL) {
		client->idle_cycles = TLX_IDLE_CYCLES;
		client->mmio_access = handle_mmio_done(ocl->mmio, client);
	}
	// Client disconnected
	if (client->state == CLIENT_NONE)
		return;

	// Check for event from application
	cmd = (struct cmd_event *)client->mem_access;
	mmio = NULL;
	dw = 0;
	global = 0;
	region = 0;
	if (bytes_ready(client->fd, 1, &(client->abort))) {
		if (get_bytes(client->fd, 1, buffer, ocl->timeout,
			      &(client->abort), ocl->dbg_fp, ocl->dbg_id,
			      client->context) < 0) {
			client_drop(client, TLX_IDLE_CYCLES, CLIENT_NONE);
			return;
		}
		switch (buffer[0]) {
		case OCSE_DETACH:
		        debug_msg("DETACH request from client context %d on socket %d", client->context, client->fd);
		        _detach(ocl, client);
			break;
		case OCSE_ATTACH:
			_attach(ocl, client);
			break;
		case OCSE_MEM_FAILURE:
			if (client->mem_access != NULL)
				handle_aerror(ocl->cmd, cmd);
			client->mem_access = NULL;
			break;
		case OCSE_MEM_SUCCESS:
			if (client->mem_access != NULL)
				handle_mem_return(ocl->cmd, cmd, client->fd);
			client->mem_access = NULL;
			break;
		case OCSE_MMIO_MAP:
		case OCSE_GLOBAL_MMIO_MAP:
			handle_mmio_map(ocl->mmio, client);
			break;
		case OCSE_GLOBAL_MMIO_WRITE64:
			global = 1;
			dw = 1;
			mmio = handle_mmio(ocl->mmio, client, 0, dw, global);
			break;
		case OCSE_MMIO_WRITE64:
			dw = 1;
			mmio = handle_mmio(ocl->mmio, client, 0, dw, global);
			break;
		case OCSE_GLOBAL_MMIO_WRITE32:
			global = 1;
			mmio = handle_mmio(ocl->mmio, client, 0, dw, global);
			break;
		case OCSE_MMIO_WRITE32:
			mmio = handle_mmio(ocl->mmio, client, 0, dw, global);
			break;
		case OCSE_GLOBAL_MMIO_READ64:
			global = 1;
			dw = 1;
			mmio = handle_mmio(ocl->mmio, client, 1, dw, global);
			break;
		case OCSE_MMIO_READ64:
			dw = 1;
			mmio = handle_mmio(ocl->mmio, client, 1, dw, global);
			break;
		case OCSE_GLOBAL_MMIO_READ32:
			global = 1;
			mmio = handle_mmio(ocl->mmio, client, 1, dw, global);
			break;
		case OCSE_MMIO_READ32:
			mmio = handle_mmio(ocl->mmio, client, 1, dw, global);
			break;
		case OCSE_LPC_WRITE:
		  mmio = handle_mem(ocl->mmio, client, 0, region, 0);
			break;
		case OCSE_LPC_WRITE_BE:
		  mmio = handle_mem(ocl->mmio, client, 0, region, 1);
			break;
		case OCSE_LPC_READ:
		  mmio = handle_mem(ocl->mmio, client, 1, region, 0);
			break;
		default:
		  error_msg("Unexpected 0x%02x from client on socket 0x%02x", buffer[0], client->fd);
		}

		if (mmio)
			client->mmio_access = (void *)mmio;

		if (client->state == CLIENT_VALID)
			client->idle_cycles = TLX_IDLE_CYCLES;
	}
}

// TLX thread loop
static void *_ocl_loop(void *ptr)
{
	struct ocl *ocl = (struct ocl *)ptr;
	struct cmd_event *event, *temp;
	int events, i, stopped, reset;
	uint8_t ack = OCSE_DETACH;


	stopped = 1;
	pthread_mutex_lock(ocl->lock);
	while (ocl->state != OCSE_DONE) {
		// idle_cycles continues to generate clock cycles for some
		// time after the AFU has gone idle.  Eventually clocks will
		// not be presented to an idle AFU to keep simulation
		// waveforms from getting huge with no activity cycles.
		if (ocl->state != OCSE_IDLE) {
		  // if we have clients or we are in the reset state, refresh idle_cycles
		  // so that the afu clock will not be allowed to stop to save afu event simulator cycles
		  if ((ocl->attached_clients > 0) || (ocl->state == OCSE_RESET) ||
			(ocl->state == OCSE_DESC)) {
			ocl->idle_cycles = TLX_IDLE_CYCLES;
			if (stopped)
				info_msg("Clocking %s", ocl->name);
			fflush(stdout);
			stopped = 0;
		  }
		}
		if (ocl->idle_cycles) {
			// Clock AFU
			tlx_signal_afu_model(ocl->afu_event);
			// Check for events from AFU
			events = tlx_get_afu_events(ocl->afu_event);
			// Error on socket
			if (events < 0) {
				warn_msg("Lost connection with AFU");
				break;
			}
			// Handle events from AFU
			if (events > 0)
				_handle_afu(ocl);

			// Drive events to AFU
			send_mmio(ocl->mmio);

			if (ocl->mmio->list == NULL)
				ocl->idle_cycles--;
		} else {
			if (!stopped)
				info_msg("Stopping clocks to %s", ocl->name);
			stopped = 1;
			lock_delay(ocl->lock);
		}

		// Skip client section if AFU descriptor hasn't been read yet
		if (ocl->client == NULL) {
			lock_delay(ocl->lock);
			continue;
		}
		// Check for event from application
		reset = 0;
		for (i = 0; i < ocl->max_clients; i++) {
			if (ocl->client[i] == NULL)
				continue;
			    if ((ocl->client[i]->state == CLIENT_NONE) &&
			    (ocl->client[i]->idle_cycles == 0)) {
			        // we get the detach message, drop the client, and wait for idle cycle to get to 0
				put_bytes(ocl->client[i]->fd, 1, &ack,
					  ocl->dbg_fp, ocl->dbg_id,
					  ocl->client[i]->context);
				ocl->client[i] = NULL;  // aha - this is how we only called _free once the old way
				                        // why do we not free client[i]?
				                        // because this was a short cut pointer
				                        // the *real* client point is in client_list in ocse
				printf("ocl->state is %x \n", ocl->state);
				continue;
			}
			if (ocl->state == OCSE_RESET)
				continue;
			_handle_client(ocl, ocl->client[i]);
			while (ocl->client[i]->idle_cycles) {
				ocl->client[i]->idle_cycles--;
			}
			if (client_cmd(ocl->cmd, ocl->client[i])) {
				ocl->client[i]->idle_cycles = TLX_IDLE_CYCLES;
			}
		}

		// Send reset to AFU
		if (reset == 1) {
			ocl->cmd->buffer_read = NULL;
			event = ocl->cmd->list;
			while (event != NULL) {
				if (reset) {
					warn_msg
					    ("Client dropped context before AFU completed");
					reset = 0;
				}
				info_msg("Dumping command afutag=0x%02x",
					 event->afutag);
				if (event->data) {
					free(event->data);
				}
				//if (event->parity) {
				//	free(event->parity);
				//}
				temp = event;
				event = event->_next;
				free(temp);
			}
			ocl->cmd->list = NULL;
			info_msg("No longer sending reset to AFU");
		}

		lock_delay(ocl->lock);
	}

	// Disconnect clients
	for (i = 0; i < ocl->max_clients; i++) {
		if ((ocl->client != NULL) && (ocl->client[i] != NULL)) {
			// FIXME: Send warning to clients first?
			info_msg("Disconnecting %s context %d", ocl->name,
				 ocl->client[i]->context);
			close_socket(&(ocl->client[i]->fd));
		}
	}

	// DEBUG
	debug_afu_drop(ocl->dbg_fp, ocl->dbg_id);

	// Disconnect from simulator, free memory and shut down thread
	info_msg("Disconnecting %s @ %s:%d", ocl->name, ocl->host, ocl->port);
	if (ocl->client)
		free(ocl->client);
	if (ocl->_prev)
		ocl->_prev->_next = ocl->_next;
	if (ocl->_next)
		ocl->_next->_prev = ocl->_prev;
	if (ocl->cmd) {
		free(ocl->cmd);
	}
	if (ocl->mmio) {
		free(ocl->mmio);
	}
	if (ocl->host)
		free(ocl->host);
	if (ocl->afu_event) {
		tlx_close_afu_event(ocl->afu_event);
		free(ocl->afu_event);
	}
	printf("ocl->name is %s \n", ocl->name);
	if (ocl->name)
		free(ocl->name);
	if (*(ocl->head) == ocl)
		*(ocl->head) = ocl->_next;

	pthread_mutex_unlock(ocl->lock);
	free(ocl);
	pthread_exit(NULL);
}

// Initialize and start TLX thread
//
// The return value is encode int a 16-bit value divided into 4 for each
// possible adapter.  Then the 4 bits in each adapter represent the 4 possible
// AFUs on an adapter.  For example: afu0.0 is 0x8000 and afu3.0 is 0x0008.
//
// The return value is encode int a 16-bit value where each bit represents a
// possible tlx interface.  For example: tlx0 is 0x8000 and tlx5 is 0x0400.
uint16_t ocl_init(struct ocl **head, struct parms *parms, char *id, char *host,
		  int port, pthread_mutex_t * lock, FILE * dbg_fp)
{
	struct ocl *ocl;
	uint16_t location;

	location = 0x8000;
	if ((ocl = (struct ocl *)calloc(1, sizeof(struct ocl))) == NULL) {
		perror("malloc");
		error_msg("Unable to allocation memory for ocl");
		goto init_fail;
	}
	ocl->timeout = parms->timeout;
	if ( (strlen(id) != 4) || strncmp(id, "tlx", 3) ) {
		warn_msg("Invalid afu name: %s", id);
		goto init_fail;
	}

	// check and map id[3] to an integer - be mindful of the hex upper/lower case character
	if ( (id[3] >= '0') && (id[3] <= '9') ) {
	  ocl->bus = id[3] - '0';
	} else if ( (id[3] >= 'A') && (id[3] <= 'F') ) {
	  ocl->bus = id[3] - 'A' + 10;
	} else if ( (id[3] >= 'a') && (id[3] <= 'f') ) {
	  ocl->bus = id[3] - 'a' + 10;
	} else {
		warn_msg("Invalid afu bus: %c", id[3]);
		goto init_fail;
	}

	ocl->dbg_fp = dbg_fp;
	ocl->dbg_id = ocl->bus; // << 4;
	location >>= ocl->bus;
	if ((ocl->name = (char *)malloc(strlen(id) + 1)) == NULL) {
		perror("malloc");
		error_msg("Unable to allocation memory for ocl->name");
		goto init_fail;
	}
	strcpy(ocl->name, id);
	if ((ocl->host = (char *)malloc(strlen(host) + 1)) == NULL) {
		perror("malloc");
		error_msg("Unable to allocation memory for ocl->host");
		goto init_fail;
	}
	strcpy(ocl->host, host);
	ocl->port = port;
	ocl->client = NULL;
	ocl->idle_cycles = TLX_IDLE_CYCLES;
	ocl->lock = lock;

	// Connect to AFU
	ocl->afu_event = (struct AFU_EVENT *)malloc(sizeof(struct AFU_EVENT));
	if (ocl->afu_event == NULL) {
		perror("malloc");
		goto init_fail;
	}
	info_msg("Attempting to connect AFU: %s @ %s:%d", ocl->name,
		 ocl->host, ocl->port);
	if (tlx_init_afu_event(ocl->afu_event, ocl->host, ocl->port) !=
	    TLX_SUCCESS) {
		warn_msg("Unable to connect AFU: %s @ %s:%d", ocl->name,
			 ocl->host, ocl->port);
		goto init_fail;
	}
	// DEBUG
	debug_afu_connect(ocl->dbg_fp, ocl->dbg_id);

	// Initialize mmio and TL cmd handler
	debug_msg("ocl_init: %s @ %s:%d: mmio_init", ocl->name, ocl->host, ocl->port);
	if ((ocl->mmio = mmio_init(ocl->afu_event, ocl->timeout, ocl->name,
				   ocl->dbg_fp, ocl->dbg_id)) == NULL) {
		perror("mmio_init");
		goto init_fail;
	}
	// Initialize TLX cmd (response) handler
	debug_msg("ocl_init: %s @ %s:%d: cmd_init", ocl->name, ocl->host, ocl->port);
	if ((ocl->cmd = cmd_init(ocl->afu_event, parms, ocl->mmio,
				 &(ocl->state), ocl->name, ocl->dbg_fp,
				 ocl->dbg_id))
	    == NULL) {
		perror("cmd_init");
		goto init_fail;
	}

	// Set credits for TLX interface
	ocl->state = OCSE_DESC;

	debug_msg( "ocl_init: %s @ %s:%d: sending initial credits to afu: tlx_afu_cmd_credits = %d, tlx_afu_resp_credits = %d, tlx_afu_cmd_data_credits = %d, tlx_afu_resp_data_credits = %d", 
		   ocl->name, ocl->host, ocl->port, 
		   MAX_TLX_AFU_CMD_CREDITS, 
		   MAX_TLX_AFU_RESP_CREDITS, 
		   MAX_TLX_AFU_CMD_DATA_CREDITS, 
		   MAX_TLX_AFU_RESP_DATA_CREDITS );
	if (tlx_afu_send_initial_credits(ocl->afu_event,
					 MAX_TLX_AFU_CMD_CREDITS,
					 MAX_TLX_AFU_RESP_CREDITS,
					 MAX_TLX_AFU_CMD_DATA_CREDITS,
					 MAX_TLX_AFU_RESP_DATA_CREDITS) != TLX_SUCCESS) {
		warn_msg("ocl_init: Unable to set initial credits");
		goto init_fail;
	}

	debug_msg("ocl_init: transmit initial TLX_AFU credits to afu");
	tlx_signal_afu_model(ocl->afu_event);

	// Start ocl loop thread
	if (pthread_create(&(ocl->thread), NULL, _ocl_loop, ocl)) {
		perror("pthread_create");
		goto init_fail;
	}
	// Add ocl to list
	while ((*head != NULL) && ((*head)->bus < ocl->bus)) {
		head = &((*head)->_next);
	}
	ocl->_next = *head;
	if (ocl->_next != NULL)
		ocl->_next->_prev = ocl;
	*head = ocl;

	// no need to reset anymore

	// Read AFU initial credit values
	debug_msg( "ocl_init: receive initial AFU_TXL credits from afu");

	int event;
	event = tlx_get_afu_events(ocl->afu_event);
	//printf("after tlx_get_afu_events, event is 0x%3x \n", event);
	// Error on socket
	if (event < 0) {
		warn_msg("Lost connection with AFU");
		}

	// did we get credits???


	// Handle events from AFU
	if (event > 0)
		_handle_afu(ocl);

	
	// Read AFU descriptor
	debug_msg("%s @ %s:%d: Reading AFU config record and VSEC.", ocl->name, ocl->host,
	          ocl->port);
	ocl->state = OCSE_DESC;
	read_afu_config(ocl, ocl->bus, ocl->lock);

	// Finish TLX configuration
	ocl->state = OCSE_IDLE;

	// ocl->max_clients = 4; // this is set in read_afu_config now
	if (ocl->max_clients == 0) {
		error_msg("AFU programming model is invalid");
		goto init_fail;
	}
	ocl->client = (struct client **)calloc(ocl->max_clients,
					       sizeof(struct client *));
	ocl->cmd->client = ocl->client;
	ocl->cmd->max_clients = ocl->max_clients;

	return location;

 init_fail:
	if (ocl) {
		if (ocl->afu_event) {
			tlx_close_afu_event(ocl->afu_event);
			free(ocl->afu_event);
		}
		if (ocl->host)
			free(ocl->host);
		if (ocl->name)
			free(ocl->name);
		free(ocl);
	}
	pthread_mutex_unlock(lock);
	return 0;
}
