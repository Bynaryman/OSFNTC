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
 * Description: cmd.c
 *
 *  This file contains the code for handling commands from the AFU.  This
 *  includes generating buffer writes or reads as well as the final response
 *  for the command.  The handle_cmd() function is periodically called by ocl code.
 *  If a command is received from the AFU then _parse_cmd() is called to determine command
 *  type.  Depending on command type either _add_interrupt(), _add_xlate_touch(),
 *  _add_amo(), _add_read(), _add_write() or _add_fail() will be called to
 *  format the tracking event properly.  Each of these functions calls
 *  _add_cmd() which will randomly insert the command in the list.
 *
 *  Once an event is in the list then the event will be service in random order
 *  by the periodic calling by ocl code of the functions: handle_interrupt(),
 *  handle_response(), handle_buffer_write(), handle_afu_tlx_cmd_data_read(),
 *  handle_afu_tlx_write_cmd(), handle_write_be_or_amo(), handle_xlate_intrp_pending_sent(),
 *  and handle_touch().  The state field is used to track the progress of each
 *  event until is fully completed and removed from the list completely.
 */

#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>
#include <sys/types.h>

#include "cmd.h"
#include "mmio.h"
#include "../common/debug.h"
#include "../common/utils.h"

#define CACHELINE_MASK 0xFFFFFFFFFFFFFFC0L
//Move this to ocse.parms when it works

// Initialize cmd structure for tracking AFU command activity
struct cmd *cmd_init(struct AFU_EVENT *afu_event, struct parms *parms,
		     struct mmio *mmio, volatile enum ocse_state *state,
		     char *afu_name, FILE * dbg_fp, uint8_t dbg_id)
{
	int i, j;
	struct cmd *cmd;

	cmd = (struct cmd *)calloc(1, sizeof(struct cmd));
	if (!cmd) {
		perror("malloc");
		exit(-1);
	}

	cmd->afu_event = afu_event;
	cmd->mmio = mmio;
	cmd->parms = parms;
	cmd->ocl_state = state;
	cmd->pagesize = parms->pagesize;
	cmd->HOST_CL_SIZE = parms->host_CL_size;
	cmd->page_entries.page_filter = ~((uint64_t) PAGE_MASK);
	cmd->page_entries.entry_filter = 0;
	for (i = 0; i < LOG2_ENTRIES; i++) {
		cmd->page_entries.entry_filter <<= 1;
		cmd->page_entries.entry_filter += 1;
	}
	cmd->page_entries.entry_filter <<= PAGE_ADDR_BITS;
	for (i = 0; i < PAGE_ENTRIES; i++) {
		for (j = 0; j < PAGE_WAYS; j++) {
			cmd->page_entries.valid[i][j] = 0;
		}
	}
	cmd->afu_name = afu_name;
	cmd->dbg_fp = dbg_fp;
	cmd->dbg_id = dbg_id;
	return cmd;
}

// find a client that has a matching pasid and bdf.  return pointer to client
static struct client *_find_client_by_pasid_and_bdf(struct cmd *cmd, uint16_t cmd_bdf, uint32_t cmd_pasid)
{
  // search the client array in cmd for a matching pasid and bdf
  // return NULL for no matching client
  // cmd->client[i]->pasid and bdr, right?
  int32_t i;

  debug_msg("_find_client_by_pasid_and_bdf: seeking client in %d potential clients with bdf=0x%04x; pasid=0x%08x", cmd->max_clients, cmd_bdf, cmd_pasid );
  for (i = 0; i < cmd->max_clients; i++) {
    if (cmd->client[i] != NULL) {
      debug_msg("_find_client_by_pasid_and_bdf: client i=%d; bdf=0x%04x; pasid=0x%08x", i, cmd->client[i]->bdf, cmd->client[i]->pasid );
      if ( ( cmd->client[i]->bdf == cmd_bdf ) && (cmd->client[i]->pasid == cmd_pasid ) ) {
	  return cmd->client[i];
      }
    }
  }
  return NULL;
}

// find a client that has a matching actag.  return pointer to client
static int32_t _find_client_by_actag(struct cmd *cmd, uint16_t cmd_actag)
{
  // search the client array in cmd for a matching pasid and bdf
  // return -1 for no matching client
  // cmd->client[i]->pasid and bdr, right?
  int32_t i;

  debug_msg("_find_client_by_actag: seeking client in %d potential clients with actag=0x%04x", cmd->max_clients, cmd_actag );
  for (i = 0; i < cmd->max_clients; i++) {
    if (cmd->client[i] != NULL) {
      if ( cmd->client[i]->actag == cmd_actag ) {
  	  debug_msg("_find_client_by_actag:  client with actag=0x%04x; i=0x%x", cmd_actag, i );
	  return i;
      }
    }
  }
  return -1;
}


// Update all pending responses at once to new state - do we KEEP?
/*static void _update_pending_resps(struct cmd *cmd, uint32_t resp)
{
	struct cmd_event *event;
	event = cmd->list;
	while (event) {
		if (event->state == MEM_IDLE) {
			event->state = MEM_DONE;
			event->resp = resp;
			debug_cmd_update(cmd->dbg_fp, cmd->dbg_id, event->afutag,
					 event->context, event->resp);
		}
		event = event->_next;
	}
} */

static struct client *_get_client(struct cmd *cmd, struct cmd_event *event)
{
	// Make sure cmd and client are still valid
	if ((cmd == NULL) || (cmd->client == NULL) ||
	    (event->context >= cmd->max_clients))
		return NULL;

	// Abort if client disconnected
	if (cmd->client[event->context] < 0) {
		event->resp = TLX_RESPONSE_FAILED;
		event->state = MEM_DONE;
		debug_cmd_update(cmd->dbg_fp, cmd->dbg_id, event->afutag,
				 event->context, event->resp);
	}
	return cmd->client[event->context];
}

static int _incoming_data_expected(struct cmd *cmd)
{
	struct cmd_event *event;
	event = cmd->list;
	while (event != NULL) {
		if ((event->type == CMD_WRITE) &&
			(event->state == MEM_BUFFER)) {
			break;
		}
		event = event->_next;
	}

	if (event == NULL)  {
		debug_msg("INCOMING_DATA_EXPECTED CHECK  and we found NO CMD WRITE in MEM_BUFFER state");
		return 0;
	} else {
		debug_msg("INCOMING_DATA_EXPECTED CHECK  and we found CMD WRITE IN MEM_BUFFER state");
		return 1;
	}

}
// Add new command to list
static void _add_cmd(struct cmd *cmd, uint32_t context, uint32_t afutag,
		     uint32_t command, enum cmd_type type,
		     uint64_t addr, uint16_t size, enum mem_state state,
		     uint32_t resp, uint8_t unlock , uint8_t cmd_data_is_valid,
		     uint64_t wr_be, uint8_t cmd_flag, uint8_t cmd_endian, uint32_t resp_opcode)

{
	struct cmd_event **head;
	struct cmd_event *event;

	if (cmd == NULL)
		return;
	event = (struct cmd_event *)calloc(1, sizeof(struct cmd_event));
	event->context = context;
	event->command = command;
	event->afutag = afutag;
	event->type = type;
	event->addr = addr;
	event->size = size;
	event->state = state;
	event->resp = resp;
	event->wr_be = wr_be;
	event->cmd_flag = cmd_flag;
	event->cmd_endian = cmd_endian;
	event->resp_opcode = resp_opcode;
	// if size = 0 it doesn't matter what we set, in this case, 1, otherwise find resp_dl from size but resp_dp always 0 
	if (size <= 64)
		event->resp_dl = 1;
	else
		event->resp_dl = size_to_dl(size);
	event->resp_dp = 0;

	// Temporary hack for now, as we don't touch/look @ TLX_SPAP reg <-- not part of OC, remove this!
	//if (event->resp == TLX_RESPONSE_CONTEXT)
	//	event->resp_extra = 1;
	//else
	//	event->resp_extra = 0;

	event->unlock = unlock;

	// make sure data buffer is big enough to hold 256B (MAX memory transfer for OpenCAPI 3.0)
	event->data = (uint8_t *) malloc(CACHELINE_BYTES * 4);
//	memset(event->data, 0xFF, CACHELINE_BYTES * 4);
//	issue10: 03/Apr/2021: trial to initialize the value of the buffer to 0x0, instead of all ones
	memset(event->data, 0x00, CACHELINE_BYTES * 4);

	event->resp_bytes_sent = 0;  //init this to 0 (used for split responses)
	// lgt may not need parity
	//event->parity = (uint8_t *) malloc(DWORDS_PER_CACHELINE / 8);
	//memset(event->parity, 0xFF, DWORDS_PER_CACHELINE / 8);

	// Test for client disconnect
	if (_get_client(cmd, event) == NULL) {
		event->resp = TLX_RESPONSE_FAILED;
		event->state = MEM_DONE;
	}

	head = &(cmd->list);
	while ((*head != NULL) && !allow_reorder(cmd->parms))
		head = &((*head)->_next);
	event->_next = *head;
	*head = event;
	debug_msg("_add_cmd:created cmd_event @ 0x%016"PRIx64":command=0x%02x, size=0x%04x, type=0x%02x, afutag=0x%04x, state=0x%03x",
		 event, event->command, event->size, event->type, event->afutag, event->state );
	debug_cmd_add(cmd->dbg_fp, cmd->dbg_id, afutag, context, command);
	// Check to see if event->cmd_data_is_valid is, and if so, set event->buffer_data
	// TODO check to see if data is bad...if so, what???
	//if ((event->command >= 0x20) && (event->command <= 0x2f) && (cmd_data_is_valid == 1)) {
	if (cmd_data_is_valid) {
		if (_incoming_data_expected( cmd)  == 0) {
		cmd->buffer_read = event;
		debug_msg("Ready to copy first 64B of write data to buffer, add=0x%016"PRIx64" , size=0x%x , afutag= 0x%x.\n",
		 	event->addr, size, event->afutag);
		// alway copy 64 bytes...
		memcpy((void *)&(event->data[0]), (void *)&(cmd->afu_event->afu_tlx_cdata_bus), 64);
		// for type = cmd_interrupt, event->state is already correctly set to MEM_IDLE
		if (event->type != CMD_INTERRUPT) {
		  if (size > 64) {
		    // but if size is greater that 64, we have to gather more data
		    event->dpartial =64;
		    event->state = MEM_BUFFER;
		  } else {
		    event->state = MEM_RECEIVED;
		    event->dpartial =0;
		  	}
		}

  	   } else  {
		cmd->afu_event->afu_tlx_cdata_valid = 1;
		debug_msg("SAVING DATA FOR READ CMD DATA TO FIND LATER");
		}
	}
}


// Format and add interrupt to command list
 static void _add_interrupt(struct cmd *cmd, uint16_t actag, uint16_t afutag,
 			   uint8_t cmd_opcode, uint8_t *cmd_ea_or_obj, uint16_t size, uint8_t cmd_data_is_valid, uint8_t cmd_flag)
 {
 	//uint32_t resp = TLX_RSP_INTRP_RESP;
 	uint32_t resp= 0; //FOR NOW, always a good response
 	//enum cmd_type type = CMD_INTERRUPT;
        uint32_t context;
        uint64_t addr;

        context = _find_client_by_actag(cmd, actag);
	if (context < 0) warn_msg( "_add_interrupt: actag does not match a client" );
	if (context == -1) {
		warn_msg("_add_interrupt: INVALID CONTEXT! COMMAND WILL BE IGNORED actag received= 0x%x", actag);
		return;
	   }

	memcpy( (void *)&addr, (void *)&(cmd_ea_or_obj[0]), sizeof(uint64_t));

        // setting MEM_IDLE will tell handle_interrupt to send req to libocxl
        if (cmd_opcode == AFU_CMD_WAKE_HOST_THRD)
		_add_cmd(cmd, context, afutag, cmd_opcode, CMD_WAKE_HOST_THRD, addr, size, MEM_IDLE,
		 resp, 0, cmd_data_is_valid, 0, cmd_flag, 0, TLX_RSP_WAKE_HOST_RESP);
	else  //must be some type of INTR request
		_add_cmd(cmd, context, afutag, cmd_opcode, CMD_INTERRUPT, addr, size, MEM_IDLE,
		 resp, 0, cmd_data_is_valid, 0, cmd_flag, 0, TLX_RSP_INTRP_RESP);
 }

// Format and add failed command to list
static void _add_fail(struct cmd *cmd, uint16_t actag, uint32_t afutag,
		       uint32_t cmd_opcode, uint32_t resp, uint32_t resp_opcode)
{
        int32_t context;

        context = _find_client_by_actag(cmd, actag);
	if (context == -1) {
		warn_msg("_add_fail: INVALID CONTEXT! COMMAND WILL BE IGNORED actag received= 0x%x", actag);
		return;
	   }
	_add_cmd(cmd, context, afutag, cmd_opcode, CMD_FAILED, 0, 0, MEM_DONE,
		 resp, 0, 0, 0, 0, 0, resp_opcode);
}

// Check address alignment
static int _aligned(uint64_t addr, uint32_t size)
{
	// Check valid size
        // that is, size must be a power of 2
	if ((size == 0) || (size & (size - 1))) {
		warn_msg("AFU issued command with invalid size %d", size);
		return 2;
	}
	// Check aligned address
	if (addr & (size - 1)) {
		warn_msg("AFU issued command with unaligned address %016"
			 PRIx64, addr);
		return 3;
	}

	return 1;
}


// Format and add memory xlate touch to command list
 static void _add_xlate_touch(struct cmd *cmd, uint16_t actag, uint16_t afutag,
 			   uint8_t cmd_opcode, uint8_t *cmd_ea_or_obj, uint8_t cmd_flag, uint8_t cmd_pg_size)
 {
        int64_t addr;
	uint32_t size = 64;
        // convert 68 bit ea/obj to 64 bit addr
        // for ap write commands, ea_or_obj is a 64 bit thing...
        memcpy( (void *)&addr, (void *)&(cmd_ea_or_obj[0]), sizeof(int64_t));
 	// Check command size and address */
 	if (_aligned(addr, size) == BAD_OPERAND_SIZE) {
 		_add_fail(cmd, actag, afutag, cmd_opcode, 0x09, TLX_RSP_TOUCH_RESP);
 		return;
 	}
	else if (_aligned(addr, size) == BAD_ADDR_OFFSET) { //invalid address alignment
 		_add_fail(cmd, actag, afutag, cmd_opcode,  0x0b, TLX_RSP_TOUCH_RESP);
 		return;
 	}
	// In future, check to see if cmd_pg_size is a supported value? Send it
	// over to libocxl?
        int32_t context;
        context = _find_client_by_actag(cmd, actag);
	if (context == -1) {
		warn_msg("_add_xlate_touch: INVALID CONTEXT! COMMAND WILL BE IGNORED actag received= 0x%x", actag);
		return;
	   }
	// TODO actually do something. For now, we always send back success for touch_resp (0x00)
	// We could send request to libocxl for processing, especially for OpenCAPI 4
	// when a translation address is expected as return
	_add_cmd(cmd, context, afutag, cmd_opcode, CMD_TOUCH, addr, 0, MEM_DONE,
		 0x00, 0, 0, 0, 0, 0, TLX_RSP_TOUCH_RESP);
 }


// Format and add memory read to command list
static void _assign_actag(struct cmd *cmd, uint16_t cmd_bdf, uint32_t cmd_pasid, uint16_t actag)
{
  struct client *client;
  // search the client array in cmd for a matching pasid and bdf.  fill in the actag field.
  client = _find_client_by_pasid_and_bdf(cmd, cmd_bdf, cmd_pasid);
  if (client == NULL) {
    // some kind of error and return...  no way to respond to afu, so just a message???
    debug_msg("_assign_actag: client not found with bdf=0x%04x; pasid=0x%08x", cmd_bdf, cmd_pasid );
    return;
  }
  client->actag = actag;
  return;
}

// Format and add memory read to command list
static void _add_read(struct cmd *cmd, uint16_t actag, uint16_t afutag,
		      uint8_t cmd_opcode, uint8_t *cmd_ea_or_obj, uint32_t size)
{
        int32_t context;
        int64_t addr;

	debug_msg("_add_read:entered" );
        // convert 68 bit ea/obj to 64 bit addr
        // for ap read commands, ea_or_obj is a 64 bit thing...
        memcpy( (void *)&addr, (void *)&(cmd_ea_or_obj[0]), sizeof(int64_t));

	// Check command size and address
 	if (_aligned(addr, size) == BAD_OPERAND_SIZE) {
 		_add_fail(cmd, actag, afutag, cmd_opcode, 0x09,TLX_RSP_READ_FAILED );
 		return;
 	}
	else if (_aligned(addr, size) == BAD_ADDR_OFFSET) { //invalid address alignment
 		_add_fail(cmd, actag, afutag, cmd_opcode,  0x0b, TLX_RSP_READ_FAILED);
 		return;
	}

        // convert actag to a context - search the client array contained in cmd for a client with matching actag
	context = _find_client_by_actag(cmd, actag);

	if (context == -1) {
		warn_msg("_add_read: INVALID CONTEXT! COMMAND WILL BE IGNORED actag received= 0x%x", actag);
		return;
	   }
	debug_msg("_add_read:calling _add_cmd context=%d; command=0x%02x; addr=0x%016"PRIx64"; size=0x%04x; afutag=0x%04x",
		context, cmd_opcode, addr, size, afutag );
	// Reads will be added to the list and will next be processed
	// in the function handle_buffer_write()
	_add_cmd(cmd, context, afutag, cmd_opcode, CMD_READ, addr, size,
		 MEM_IDLE, TLX_RESPONSE_DONE, 0, 0, 0, 0, 0, TLX_RSP_READ_RESP);
}

// Format and add AMO read or write to command list
static void _add_amo(struct cmd *cmd, uint16_t actag, uint16_t afutag,
		      uint8_t cmd_opcode, enum cmd_type type, uint8_t *cmd_ea_or_obj,
		      uint8_t cmd_pl, uint8_t cmd_data_is_valid, uint8_t cmd_flag, uint8_t cmd_endian)
{
        int32_t context, size, sizecheck, resp_opcode;
        int64_t addr;

        // convert 68 bit ea/obj to 64 bit addr
        // for ap write commands, ea_or_obj is a 64 bit thing...
        memcpy( (void *)&addr, (void *)&(cmd_ea_or_obj[0]), sizeof(int64_t));
	// check cmd_pl  size. Only certain values are valid
	// TODO NOTE: Expect to get 64B un data buffer, with 16B of data payload starting
	// at address offset. When data goes over to libocxl we extract and send 16B ALWAYS
	// for AMO_WR and AMO_RW unless told elsewise. AMO_RD has no immediate data

	size = 16;
  	switch (cmd_pl) {
  		case 2:
  		case 3:
			if ((cmd_opcode == AFU_CMD_AMO_RW) || (cmd_opcode == AFU_CMD_AMO_RW_N))  {
				if (cmd_flag >= 0x8)  {
					warn_msg("AMO_RW has invalid cmd_pl:%d", cmd_pl);
					resp_opcode = TLX_RSP_READ_FAILED;
					size= -1;
				}
			}
			if (cmd_pl == 2)
				sizecheck = 4;
			else sizecheck = 8;
			break;
 
  		case 6:
  		case 7:
			if ((cmd_opcode == AFU_CMD_AMO_W) || (cmd_opcode == AFU_CMD_AMO_W_N))  {
				warn_msg("AMO_WR has invalid cmd_pl:%d", cmd_pl);
				resp_opcode = TLX_RSP_WRITE_FAILED;
				size= -1;
			}
			if (cmd_pl == 6)
				sizecheck = 4;
			else sizecheck = 8;
    			break;
  		default:
    			warn_msg("AMO with Unsupported pl: %d", cmd_pl);
			if ((cmd_opcode == AFU_CMD_AMO_RW) || (cmd_opcode == AFU_CMD_AMO_RW_N))  
				resp_opcode = TLX_RSP_READ_FAILED;
			else
				resp_opcode = TLX_RSP_WRITE_FAILED;
    			size = -1;
    			break;
  		}
	if ( size == -1) {
	debug_msg("AMO CMD FAILED SIZE CHECKS cmd_pl= 0x%x, cmd_flag=0x%x !!! ", cmd_pl, cmd_flag);
	  _add_fail(cmd, actag, afutag, cmd_opcode,  0x09,resp_opcode );
		return;
	}
	// Check command size and address
	if (_aligned(addr, sizecheck) == BAD_ADDR_OFFSET) { //invalid address alignment
		if ((cmd_opcode == AFU_CMD_AMO_RW) || (cmd_opcode == AFU_CMD_AMO_RW_N) || (cmd_opcode == AFU_CMD_AMO_RD))  
			resp_opcode = TLX_RSP_READ_FAILED;
		else
			resp_opcode = TLX_RSP_WRITE_FAILED;
 		_add_fail(cmd, actag, afutag, cmd_opcode,  0x0b, resp_opcode);
 		return;
	}

	// Also need to check with libocxl to be sure the address AFU sent us is in user's space
	// TODO create new OCSE_ADDR_VALID cmd, send to lib0cxl, set status to MEM_CHECK and
	// wait for response.
	//

        // convert actag to a context - search the client array contained in cmd for a client with matching actag
	context = _find_client_by_actag(cmd, actag);

	if (context == -1) {
		warn_msg("_add_amo: INVALID CONTEXT! COMMAND WILL BE IGNORED actag received= 0x%x", actag);
		return;
	   }
	// Command data comes over with the command for amo_rw and amo_w, so now we need to read it from event
	// Then, next step is to send over to client/libocxl for processing
	if ((cmd_opcode == AFU_CMD_AMO_RW) || (cmd_opcode == AFU_CMD_AMO_RW_N) || (cmd_opcode == AFU_CMD_AMO_RD))  
		resp_opcode = TLX_RSP_READ_RESP;
	else
		resp_opcode = TLX_RSP_WRITE_RESP;

	_add_cmd(cmd, context, afutag, cmd_opcode, type, addr, (uint16_t)sizecheck,
		 MEM_IDLE, TLX_RESPONSE_DONE, 0, cmd_data_is_valid, 0, cmd_flag, cmd_endian, resp_opcode);
}



// Format and add memory write to command list
static void _add_write(struct cmd *cmd, uint16_t actag, uint16_t afutag,
		      uint8_t cmd_opcode, uint8_t *cmd_ea_or_obj,
		      uint32_t size, uint8_t cmd_data_is_valid, uint64_t cmd_be)
{
        int32_t context;
        int64_t addr;

        // convert 68 bit ea/obj to 64 bit addr
        // for ap write commands, ea_or_obj is a 64 bit thing...
        memcpy( (void *)&addr, (void *)&(cmd_ea_or_obj[0]), sizeof(int64_t));

	// Check command size and address
 	if (_aligned(addr, size) == BAD_OPERAND_SIZE) {  //invalid operand size
 		_add_fail(cmd, actag, afutag, cmd_opcode, 0x09, TLX_RSP_WRITE_FAILED);
 		return;
 	}
	else if (_aligned(addr, size) == BAD_ADDR_OFFSET) { //invalid address alignment
 		_add_fail(cmd, actag, afutag, cmd_opcode,  0x0b, TLX_RSP_WRITE_FAILED);
 		return;
	}
	// Also need to check with libocxl to be sure the address AFU sent us is in user's space
	// TODO create new OCSE_ADDR_VALID cmd, send to lib0cxl, set status to MEM_CHECK and
	// wait for response.

        // convert actag to a context - search the client array contained in cmd for a client with matching actag
	context = _find_client_by_actag(cmd, actag);

	if (context == -1) {
		warn_msg("_add_write: INVALID CONTEXT! COMMAND WILL BE IGNORED actag received= 0x%x", actag);
		return;
	   }
	// Command data comes over with the command, so read it from event and put it in buffer in add_cmd
	// Then, next step is to make the memory write request?

	// Longer Writes will be added to the list and will next be processed
	// in handle_afu_tlx_cmd_data_read
	// TODO add CAPI4 BE opcodes to if
	debug_msg("_add_write:calling _add_cmd context=%d; command=0x%02x; addr=0x%016"PRIx64"; size=0x%04x; afutag=0x%04x", 
		context, cmd_opcode, addr, size, afutag );
	if ((cmd_opcode == AFU_CMD_DMA_W_BE) || (cmd_opcode == AFU_CMD_DMA_W_BE_N))
		_add_cmd(cmd, context, afutag, cmd_opcode, CMD_WR_BE, addr, size,
			 MEM_IDLE, TLX_RESPONSE_DONE, 0, cmd_data_is_valid, cmd_be, 0, 0, TLX_RSP_WRITE_RESP);
	else
		_add_cmd(cmd, context, afutag, cmd_opcode, CMD_WRITE, addr, size,
			 MEM_IDLE, TLX_RESPONSE_DONE, 0, cmd_data_is_valid, 0, 0, 0, TLX_RSP_WRITE_RESP);
}

// Determine what type of command to add to list
static void _parse_cmd(struct cmd *cmd,
		       uint8_t cmd_opcode, uint16_t cmd_actag,
		       uint8_t cmd_stream_id, uint8_t *cmd_ea_or_obj,
		       uint16_t cmd_afutag, uint8_t cmd_dl,
		       uint8_t cmd_pl,
#ifdef TLX4
		       uint8_t cmd_os,
#endif
		       uint64_t cmd_be, uint8_t cmd_flag,
		       uint8_t cmd_endian, uint16_t cmd_bdf,
		       uint32_t cmd_pasid, uint8_t cmd_pg_size, uint8_t cmd_data_is_valid,
		       uint8_t *cdata_bus, uint8_t cdata_bad)
{
	//uint8_t unlock = 0;
	// TODO FIX THIS WHEN WE DETERMINE #OF CONTEXTS
	//if (handle >= cmd->mmio->cfg.num_of_processes) {
	//	_add_fail(cmd, handle, tag, command, abort,
	//		   TLX_RESPONSE_CONTEXT);
	//	return;
	//}

        // how do we model stream_id?

        // Based on the cmd_opcode we have received from the afu, add a cmd_event to the list associated with our cmd struct
	switch (cmd_opcode) {
		// assign actag to map an actag to a pasid/bdf (a context for us)
	case AFU_CMD_ASSIGN_ACTAG:
		debug_msg("YES! AFU cmd is ASSIGN_ACTAG\n");
		if (cmd_data_is_valid)
		    cmd->afu_event->afu_tlx_cdata_valid = 1;
                _assign_actag( cmd, cmd_bdf, cmd_pasid, cmd_actag );
		break;
		// Memory Reads
	case AFU_CMD_RD_WNITC:
	case AFU_CMD_RD_WNITC_N:
		debug_msg("YES! AFU cmd is some sort of read\n");
		// calculate size from dl
		if (cmd_data_is_valid)
		    cmd->afu_event->afu_tlx_cdata_valid = 1;
		_add_read(cmd, cmd_actag, cmd_afutag, cmd_opcode,
			 cmd_ea_or_obj, dl_to_size( cmd_dl ));
		break;
	case AFU_CMD_PR_RD_WNITC:
	case AFU_CMD_PR_RD_WNITC_N:
		debug_msg("YES! AFU cmd is some sort of partial read\n");
		// calculate size from pl
		if (cmd_data_is_valid)
		    cmd->afu_event->afu_tlx_cdata_valid = 1;
		_add_read(cmd, cmd_actag, cmd_afutag, cmd_opcode,
			 cmd_ea_or_obj, pl_to_size( cmd_pl ));
		break;
		// Memory Writes
	case AFU_CMD_DMA_W:
	case AFU_CMD_DMA_W_N:
		debug_msg("YES! AFU cmd is some sort of write\n");
		_add_write(cmd, cmd_actag, cmd_afutag, cmd_opcode,
			  cmd_ea_or_obj, dl_to_size( cmd_dl ), cmd_data_is_valid, 0);
		break;
	case AFU_CMD_DMA_PR_W:
	case AFU_CMD_DMA_PR_W_N:
		debug_msg("YES! AFU cmd is some sort of partial write\n");
		_add_write(cmd, cmd_actag, cmd_afutag, cmd_opcode,
			  cmd_ea_or_obj, pl_to_size( cmd_pl ), cmd_data_is_valid, 0);
		break;
		// Memory Writes with Byte Enable
	case AFU_CMD_DMA_W_BE:
	case AFU_CMD_DMA_W_BE_N:
		debug_msg("YES! AFU cmd is some sort of write w/BE\n");
		_add_write(cmd, cmd_actag, cmd_afutag, cmd_opcode,
			  cmd_ea_or_obj, 64, cmd_data_is_valid, cmd_be);
		break;
		// AMO reads and writes
	case AFU_CMD_AMO_RD:
	case AFU_CMD_AMO_RD_N:
		debug_msg("YES! AFU cmd is some sort of AMO read\n");
		_add_amo(cmd, cmd_actag, cmd_afutag, cmd_opcode, CMD_AMO_RD,
			  cmd_ea_or_obj, cmd_pl, cmd_data_is_valid, cmd_flag, cmd_endian);
		break;
	case AFU_CMD_AMO_RW:
	case AFU_CMD_AMO_RW_N:
		debug_msg("YES! AFU cmd is some sort of AMO read/write w/cmd_pl= 0x%x\n", cmd_pl);
		_add_amo(cmd, cmd_actag, cmd_afutag, cmd_opcode, CMD_AMO_RW,
			  cmd_ea_or_obj, cmd_pl, cmd_data_is_valid, cmd_flag, cmd_endian);
		break;
	case AFU_CMD_AMO_W:
	case AFU_CMD_AMO_W_N:
		debug_msg("YES! AFU cmd is some sort of AMO read or write");
		_add_amo(cmd, cmd_actag, cmd_afutag, cmd_opcode, CMD_AMO_WR,
			  cmd_ea_or_obj, cmd_pl, cmd_data_is_valid, cmd_flag, cmd_endian);
		break;
		// Interrupt
	case AFU_CMD_INTRP_REQ_D: // not sure POWER supports this one?
		debug_msg("YES! AFU cmd is  INTRPT REQ WITH DATA");
		_add_interrupt(cmd, cmd_actag, cmd_afutag, cmd_opcode,
			  cmd_ea_or_obj, pl_to_size( cmd_pl), cmd_data_is_valid, cmd_flag);
		break;
	case AFU_CMD_INTRP_REQ:
	case AFU_CMD_WAKE_HOST_THRD:
		debug_msg("YES! AFU cmd is either INTRPT REQ or WAKE HOST THREAD");
		_add_interrupt(cmd, cmd_actag, cmd_afutag, cmd_opcode,
			  cmd_ea_or_obj, 0, cmd_data_is_valid, cmd_flag);
	// TODO what about stream_id ?
		break;
	case AFU_CMD_NOP:
		debug_msg("NOP CMD - No response needed");
		break;
	case AFU_CMD_XLATE_TOUCH:
	case AFU_CMD_XLATE_TOUCH_N:
		debug_msg("YES! AFU cmd is some kind of XLATE_TOUCH\n");
		if (cmd_data_is_valid)
		    cmd->afu_event->afu_tlx_cdata_valid = 1;
		_add_xlate_touch(cmd, cmd_actag, cmd_afutag, cmd_opcode,
			  cmd_ea_or_obj, cmd_flag, cmd_pg_size);
		break;
	default:
		warn_msg("Unsupported command 0x%04x", cmd_opcode);
		// TODO this type of error is signaled as "malformed packet error type 0 event" but how??  
		_add_fail(cmd, cmd_actag, cmd_afutag, cmd_opcode, TLX_RESPONSE_FAILED, TLX_RESPONSE_FAILED);
		break;
	}
}


// See if a command was sent by AFU and process if so
void handle_cmd(struct cmd *cmd, uint32_t latency)
{
	struct cmd_event *event;
	uint64_t cmd_be;
	uint32_t cmd_pasid;
	uint16_t cmd_actag, cmd_afutag, cmd_bdf;
	uint8_t  cmd_ea_or_obj[9];
	uint8_t  cmd_opcode, cmd_stream_id, cmd_dl, cmd_pl, cmd_flag, cmd_endian, cmd_pg_size, cmd_data_is_valid, cdata_bad;
#ifdef TLX4
	uint8_t cmd_os;
#endif
	unsigned char cdata_bus[64];
	uint8_t * dptr = cdata_bus;
	int rc;

	// debug_msg( "ocse:handle_cmd:" );
	if (cmd == NULL)
		return;

	// Check for command from AFU
	// maybe read command and data separately to facilitate the parse_cmd and handle_buffer_data separation a 
	// little bit later in this routine
	rc =  afu_tlx_read_cmd_and_data(cmd->afu_event,
  		    &cmd_opcode, &cmd_actag,
  		    &cmd_stream_id, &cmd_ea_or_obj[0],
 		    &cmd_afutag, &cmd_dl,
  		    &cmd_pl,
#ifdef TLX4
		    &cmd_os,
#endif
		    &cmd_be, &cmd_flag,
 		    &cmd_endian, &cmd_bdf,
  	  	    &cmd_pasid, &cmd_pg_size, &cmd_data_is_valid,
 		    dptr, &cdata_bad);



	// No command ready */
	if (rc != TLX_SUCCESS)
		return;

	debug_msg( "%s:COMMAND actag=0x%02x afutag=0x%04x cmd=0x%x cmd_data_is_valid= 0x%x ",
		   cmd->afu_name,
		   cmd_actag,
		   cmd_afutag,
		   cmd_opcode,
		   cmd_data_is_valid );


	// Check for duplicate afutag
	event = cmd->list;
	while (event != NULL) {
		if (event->afutag == cmd_afutag) {
			error_msg("Duplicate afutag 0x%04x", cmd_afutag);
			return;
		}
		event = event->_next;
	}

	_parse_cmd(cmd, cmd_opcode, cmd_actag, cmd_stream_id, cmd_ea_or_obj, cmd_afutag, cmd_dl, cmd_pl,
#ifdef TLX4
		   cmd_os,
#endif
		   cmd_be, cmd_flag, cmd_endian, cmd_bdf, cmd_pasid, cmd_pg_size, cmd_data_is_valid, dptr, cdata_bad);
}

// Handle randomly selected pending read by either generating early buffer
// write with bogus data, send request to client for real data or do final
// buffer write with valid data after it has been received from client.
// lgt:  in opencapi, we don't really have a separate buffer write.  Instead,
// when get the data back from the host, we just send it with the response.
// should we defer some of this to handle_response?  Or should we process
// the data and response here and also free the event?
void handle_buffer_write(struct cmd *cmd)
{
	struct cmd_event *event;
	struct client *client;
	uint8_t buffer[11];  // 1 message byte + 2 size bytes + 8 address bytes
	uint64_t *addr;
	uint16_t *size;
	//int quadrant, byte;

	// Make sure cmd structure is valid
	if (cmd == NULL)
		return;

	// Randomly select a pending read or read_pe (or none)
	// for now, make sure allow_reorder_parms is not allowed.
	// lgt: if we want to free the cmd event later, we should find the event with the same method as handle_response...
	// lgt: decided to put the call to tlx_afu_send_resp_and_data in the handle_response routine since it will also free the cmd event
	//      so here we just set MEM_DONE and TLX_RESPONSE_DONE for the event that we selected
	event = cmd->list;
	while ( event != NULL ) {
	        if ( ( event->type == CMD_READ ) &&
		     ( event->state != MEM_DONE ) &&
		     ( ( event->client_state != CLIENT_VALID ) ) ) { 
			break;
		}
		event = event->_next;
	}

	// Test for client disconnect
	if ((event == NULL) || ((client = _get_client(cmd, event)) == NULL))
		return;


	debug_msg( "handle_buffer_write: we've picked a non-NULL event and the client is still there" );

	if ((event->state == MEM_IDLE) && (client->mem_access == NULL)) {
	        // Check to see if this cmd gets selected for a RETRY or FAILED or PENDING or DERROR read_failed response
		if ( allow_retry(cmd->parms)) {
			event->state = MEM_DONE;
			event->type = CMD_FAILED;
			event->resp_opcode = TLX_RSP_READ_FAILED;
			event->resp = 0x02;
			debug_msg("handle_buffer_write:RETRY this cmd =0x%x \n", event->command);
			return;
		}
		if ( allow_failed(cmd->parms)) {
			event->state = MEM_DONE;
			event->type = CMD_FAILED;
			event->resp_opcode = TLX_RSP_READ_FAILED;
			event->resp = 0x0e;
			debug_msg("handle_buffer_write: FAIL this cmd =0x%x \n", event->command);
			return;
		}
		if ( allow_derror(cmd->parms)) {
			event->state = MEM_DONE;
			event->type = CMD_FAILED;
			event->resp_opcode = TLX_RSP_READ_FAILED;
			event->resp = 0x08;
			debug_msg("handle_buffer_write: DERROR this cmd =0x%x \n", event->command);
			return;
		}
		// for xlate_pending response, ocse has to THEN follow up with an xlate_done response
		// (at some unknown time later) and that will "complete" the original cmd (no rd/write )
		if ( allow_pending(cmd->parms)) {
		        event->state = MEM_XLATE_PENDING;
			event->type = CMD_FAILED;
			event->resp_opcode = TLX_RSP_READ_FAILED;
			event->resp = 0x04;
			debug_msg("handle_buffer_write: send XLATE_PENDING for this cmd =0x%x \n", event->command);
			return;
		}
	}

	// after the client returns data with a call to the function _handle_mem_read,
	// we need to generate one or more capp responses.  We need to chunk data into 64 byte pieces
	// to honor the txl/afu interface.
	// in ocse, we will always send responses with all the data appropriate for the response we generate
	// see handle_response
	// for partial read, we can probabaly just return data as we have already inserted the data into
	// the appropriate place in the event->data buffer
	// for "full" reads, we need to chunk data
	// use tlx_afu_send_resp_and_data, for each 64B chunk - look at the old pslse dma code...
	// concerns
	//    chunk spacing
	//    interleaving
	//    and so on.
	if ((event->state == MEM_RECEIVED) && (event->type == CMD_READ)) {
	  debug_msg( "memory read data received, formulate capp response" );
	  if ( (event->command == AFU_CMD_PR_RD_WNITC) || (event->command == AFU_CMD_PR_RD_WNITC_N) ) {
	    // we can just complete the event and let handle_response send the response and 64 bytes of data back
	    event->resp = TLX_RESPONSE_DONE;
	    event->state = MEM_DONE;
	  } else if ( (event->command == AFU_CMD_RD_WNITC) || (event->command == AFU_CMD_RD_WNITC_N) ) {
	    // we need to send back 1 or more 64B response
	    // we can:
	    //    send a complete response, with all the data
	    //    send partial responses, in any order, with aligned partial data (vary dl and dp in the response
	    //       power will likely send back chunks in <= 128 B responses...
	    //    responses can come back in any order
	    // I'm thinking ocse decides what response to send and whether or not to split it.
	    // and sends all the data associated with the selected response.
	    // then tlx_interface/afu_driver forward the response portion and hold the data in a fifo linked list of 64 B values.
	    // then when the afu does a resp_rd_req of some resp_rd_cnt, tlx_interaface/afu_driver just starts pumping values out of the
	    // fifo.  This method actually works for partial read as well as the minimum size of a split response is 64 B.
	    // it is the afu's responsiblity to manage resp_rd_cnt correctly, and this is not information for us to check
	    // anything other than an overrun (i.e. resp_rd_req of an empty fifo, or resp_rd_cnt exceeds the amount of data in the fifo)
	      	event->resp = TLX_RESPONSE_DONE;
	      	event->state = MEM_DONE;
	  } 
	}

	debug_msg( "event->state is not MEM_RECEIVED and event->type is not CMD_READ" );

        if (event->state == MEM_CAS_RD) {
	  	buffer[0] = (uint8_t) OCSE_MEMORY_READ;
		size = (uint16_t *)&(buffer[1]);
		*size = htons(event->size);
		addr = (uint64_t *) & (buffer[3]);
		*addr = htonll(event->addr);
		event->abort = &(client->abort);
		debug_msg("%s:MEMORY READ FOR CAS afutag=0x%02x size=%d addr=0x%016"PRIx64,
		    cmd->afu_name, event->afutag, event->size, event->addr);
		if (put_bytes(client->fd, 10, buffer, cmd->dbg_fp,
			cmd->dbg_id, event->context) < 0) {
		    client_drop(client, TLX_IDLE_CYCLES, CLIENT_NONE);
		  }
		  event->state = MEM_REQUEST;
		  client->mem_access = (void *)event;
	        debug_msg("Setting client->mem_access in handle_buffer_write ");
	        return; //exit immediately
	}

	debug_msg( "event->state is not MEM_CAS_RD" );

	if (event->state != MEM_IDLE) { //?
	        debug_msg( "event->state is not equal MEM_IDLE" );
		return;
	}

	debug_msg( "event->state is MEM_IDLE" );

	// lgt removed code that would send bogus data to the afu.  doesn't happen in opencapi

	if (client->mem_access == NULL) {
	        // if read:
		// Send read request to client, set client->mem_access
		// to point to this event blocking any other memory
		// accesses to client until data is returned by call
		// to the _handle_mem_read() function.
                if (event->type == CMD_READ) {
		    buffer[0] = (uint8_t) OCSE_MEMORY_READ;

		    size = (uint16_t *)&(buffer[1]);
		    *size = htons(event->size);

		    addr = (uint64_t *) & (buffer[3]);
		    *addr = htonll(event->addr);

		    event->abort = &(client->abort);

		    debug_msg("%s:MEMORY READ afutag=0x%04x size=%d addr=0x%016"PRIx64,
			    cmd->afu_name, event->afutag, event->size, event->addr);

		    if (put_bytes(client->fd, 11, buffer, cmd->dbg_fp,
				cmd->dbg_id, event->context) < 0) {
		          client_drop(client, TLX_IDLE_CYCLES, CLIENT_NONE);
		    }
		    event->state = MEM_REQUEST;
		    debug_cmd_client( cmd->dbg_fp, cmd->dbg_id, event->afutag,
				      event->context );
		    client->mem_access = (void *)event;
	            debug_msg("Setting client->mem_access in handle_buffer_write 2 for event @ 0x%016" PRIx64 , event);
		}
	} else
		debug_msg( "client->mem_access was not NULL meaning we have a memory action in progress" );
}

// Handle incoming write data from AFU
void handle_afu_tlx_cmd_data_read(struct cmd *cmd)
{
	struct cmd_event *event;
	unsigned char cdata_bus[64];
	uint8_t * dptr = cdata_bus;
	uint8_t cmd_data_is_valid, cdata_bad;
	int rc;

	// debug_msg( "ocse:handle_afu_tlx_cmd_data_read:" );
	// Check that cmd struct is valid buffer read is available
	if (cmd == NULL)
		return;
	//First, let's look to see if any one is in MEM_BUFFER state...data still coming over the interface (should only be ONE @time)
	// or if anyone is in MEM_RECEIVED...all data is here & ready to go (should only be ONE of these @time)
	event = cmd->list;
	while (event != NULL) {
		if ((event->type == CMD_WRITE) &&
			 (event->state == MEM_BUFFER)) {
			break;
		}
		event = event->_next;
	}

	// Test for client disconnect
	if (event == NULL)
		return;
	debug_msg("entering HANDLE_AFU_TLX_CMD_DATA_READ");
	rc = afu_tlx_read_cmd_data(cmd->afu_event, &cmd_data_is_valid, dptr,  &cdata_bad);
	if (rc == TLX_SUCCESS) {
		if (cmd_data_is_valid) {
			debug_msg("Copy another 64B of write data to buffer, addr=0x%016"PRIx64", total read so far=0x%x , afutag= 0x%x .\n",
			 event->addr, event->dpartial, event->afutag);
			if ((event->size - event->dpartial) > 64) {
				memcpy((void *)&(event->data[event->dpartial]), (void *)&(cmd->afu_event->afu_tlx_cdata_bus), 64);
				debug_msg("SHOULD BE INTERMEDIATE COPY");
				//int i;
				//for ( i = 0; i < 64; i++ ) printf("%02x",cmd->afu_event->afu_tlx_cdata_bus[i]); printf( "\n" );

				event->dpartial +=64;
				event->state = MEM_BUFFER;
			 }
			else  {
				memcpy((void *)&(event->data[event->dpartial]), (void *)&(cmd->afu_event->afu_tlx_cdata_bus), (event->size - event->dpartial));
				debug_msg("SHOULD BE FINAL COPY and event->dpartial=0x%x , afutag= 0x%x", event->dpartial, event->afutag);
				//for ( i = 0; i < 64; i++ ) printf("%02x",cmd->afu_event->afu_tlx_cdata_bus[i]); printf( "\n" );
				event->state = MEM_RECEIVED;
				}

		} else
		debug_msg("event->state == MEM_BUFFER and event->afutag = 0x%x and cmd_data_is_valid= 0x%x", event->afutag, cmd_data_is_valid);

	} else
		return;

	return;
// end of handle_afu_tlx_cmd_data_read
}


/// Handle pending write cmd from AFU once all data is received
void handle_afu_tlx_write_cmd(struct cmd *cmd)
{
	struct cmd_event *event;
	struct client *client;
	uint64_t *addr;
	uint64_t offset;
	uint8_t *buffer;

	 //debug_msg( "ocse:handle_afu_tlx_write_cmd:" );
	// Check that cmd struct is valid buffer read is available
	//if ((cmd == NULL) || (cmd->buffer_read == NULL))
	if (cmd == NULL)
		return;

	event = cmd->list;
	while (event != NULL) {
		if ((event->type == CMD_WRITE) && (event->state == MEM_RECEIVED))
			break;
		event = event->_next;
		}
	if (event == NULL)
		return;

	if ((client = _get_client(cmd, event)) == NULL)
		return;

	if (client->mem_access != NULL) {
		debug_msg("client->mem_access NOT NULL so can't send MEMORY write for afutag=0x%x yet!!!!!", event->afutag);
		return;
	}
	debug_msg("entering HANDLE_AFU_TLX_WRITE_CMD");
	// Check to see if this cmd gets selected for a RETRY or FAILED or PENDING read_failed response
	if ( allow_retry(cmd->parms)) {
		event->state = MEM_DONE;
		event->type = CMD_FAILED;
		event->resp_opcode = TLX_RSP_WRITE_FAILED;
		event->resp = 0x02;
		debug_msg("handle_afu_tlx_write_cmd: RETRY this cmd =0x%x \n", event->command);
		return;
	}
	if ( allow_failed(cmd->parms)) {
		event->state = MEM_DONE;
		event->type = CMD_FAILED;
		event->resp_opcode = TLX_RSP_WRITE_FAILED;
		event->resp = 0x0e;
		debug_msg("handle_afu_tlx_write_cmd: FAIL this cmd =0x%x \n", event->command);
		return;
	}
	if ( allow_derror(cmd->parms)) {
		event->state = MEM_DONE;
		event->type = CMD_FAILED;
		event->resp_opcode = TLX_RSP_WRITE_FAILED;
		event->resp = 0x08;
		debug_msg("handle_afu_tlx_write_cmd: DERROR this cmd =0x%x \n", event->command);
		return;
	}

	// for xlate_pending response, ocse has to THEN follow up with an xlate_done response
	// (at some unknown time later) and that will "complete" the original cmd (no rd/write )
	if ( allow_pending(cmd->parms)) {
		event->state = MEM_XLATE_PENDING;
		event->type = CMD_FAILED;
		event->resp_opcode = TLX_RSP_WRITE_FAILED;
		event->resp = 0x04;
		debug_msg("handle_afu_tlx_write_cmd: send XLATE_PENDING for this cmd =0x%x \n", event->command);
		return;
	}

	// Send buffer read request to AFU.  Setting cmd->buffer_read
	// will block any more buffer read requests until buffer read
	// data is returned and handled in handle_buffer_data().
	debug_msg("%s:BUFFER READY TO GO TO CLIENT afutag=0x%04x addr=0x%016"PRIx64, cmd->afu_name,
		  event->afutag, event->addr);
	if (event->type == CMD_WRITE) {
		buffer = (uint8_t *) malloc(event->size + 11);
		buffer[0] = (uint8_t) OCSE_MEMORY_WRITE;
		buffer[1] = (uint8_t) ((event->size & 0x0F00) >>8);
		buffer[2] = (uint8_t) (event->size & 0xFF);
		addr = (uint64_t *) & (buffer[3]);
		*addr = htonll(event->addr);
		if (event->size <=32) {
			offset = event->addr & ~CACHELINE_MASK;
			debug_msg("partial write: size=0x%x and offset=0x%x", event->size, offset);
			memcpy(&(buffer[11]), &(event->data[offset]), event->size);
		} else
			memcpy(&(buffer[11]), &(event->data[0]), event->size);
		event->abort = &(client->abort);
		debug_msg("%s: MEMORY WRITE afutag=0x%02x size=%d addr=0x%016"PRIx64" port=0x%2x",
		  	cmd->afu_name, event->afutag, event->size, event->addr, client->fd);
		if (put_bytes(client->fd, event->size + 11, buffer, cmd->dbg_fp,
		      	cmd->dbg_id, client->context) < 0) {
			client_drop(client, TLX_IDLE_CYCLES, CLIENT_NONE);
		}
	}
	event->state = DMA_MEM_RESP;  //we can't set MEM_DONE until we get ACK back from client (or else SEG FAULT)
	cmd->buffer_read = NULL;
	client->mem_access = (void *)event;
}

// Handle  pending write_be or atomic op - send them to client for execution
// client will return response value for some AMO ops (state will be set to AMO_MEM_RESP)
void handle_write_be_or_amo(struct cmd *cmd)
{
	struct cmd_event **head;
	struct cmd_event *event;
	struct client *client;
	uint64_t offset;
	uint64_t *addr, *wr_be;
	uint16_t *size;
	uint8_t *buffer;

	// debug_msg( "ocse:handle_write_be_or_amo:" );
	// Check that cmd struct is valid
	if (cmd == NULL)
		return;

	// Send any ready write_be or AMO cmds to client immediately
	head = &cmd->list;
	while (*head != NULL) {
	  	//printf ("handle_write_be_or_amo: head->type is %2x, head->state is 0x%3x \n", (*head)->type, (*head)->state);
		if ((((*head)->type == CMD_WR_BE) || ((*head)->type == CMD_AMO_WR) ||
		    ((*head)->type == CMD_AMO_RW)) &&
		    ((*head)->state == MEM_RECEIVED))
			break;
		if (((*head)->type == CMD_AMO_RD)  &&
		   ((*head)->state == MEM_RECEIVED))  // TODO change this later, we did get data but it's not used
			break;

		head = &((*head)->_next);
	}
	event = *head;

	// Test for client disconnect or nothing to do....
	if ((event == NULL) || ((client = _get_client(cmd, event)) == NULL))
		return;
	// Check that memory request can be driven to client
	if (client->mem_access != NULL) {
		debug_msg("handle_write_be_or_amo: Can't send to client bc client->mem_access not NULL...retry later");
		return;
	}
	// Check to see if this cmd gets selected for a RETRY or FAILED or PENDING read_failed response
	if ( allow_retry(cmd->parms)) {
		event->state = MEM_DONE;
		if ((event->type == CMD_AMO_RD) || (event->type == CMD_AMO_RW))
			event->resp_opcode = TLX_RSP_READ_FAILED;
		else	
			event->resp_opcode = TLX_RSP_WRITE_FAILED;
		event->type = CMD_FAILED;
		event->resp = 0x02;
		debug_msg("handle_write_be_or_amo: RETRY this cmd =0x%x \n", event->command);
		return;
	}
	if ( allow_failed(cmd->parms)) {
		event->state = MEM_DONE;
		if ((event->type == CMD_AMO_RD) || (event->type == CMD_AMO_RW))
			event->resp_opcode = TLX_RSP_READ_FAILED;
		else	
			event->resp_opcode = TLX_RSP_WRITE_FAILED;
		event->type = CMD_FAILED;
		event->resp = 0x0e;
		debug_msg("handle_write_be_or_amo: FAIL this cmd =0x%x \n", event->command);
		return;
	}
	if ( allow_derror(cmd->parms)) {
		event->state = MEM_DONE;
		if ((event->type == CMD_AMO_RD) || (event->type == CMD_AMO_RW))
			event->resp_opcode = TLX_RSP_READ_FAILED;
		else	
			event->resp_opcode = TLX_RSP_WRITE_FAILED;
		event->type = CMD_FAILED;
		event->resp = 0x08;
		debug_msg("handle_write_be_or_amo: DERROR this cmd =0x%x \n", event->command);
		return;
	}

	// for xlate_pending response, ocse has to THEN follow up with an xlate_done response 
	// (at some unknown time later) and that will "complete" the original cmd (no rd/write )
	if ( allow_pending(cmd->parms)) {
		event->state = MEM_XLATE_PENDING;
		if ((event->type == CMD_AMO_RD) || (event->type == CMD_AMO_RW))
			event->resp_opcode = TLX_RSP_READ_FAILED;
		else	
			event->resp_opcode = TLX_RSP_WRITE_FAILED;
		event->type = CMD_FAILED;
		event->resp = 0x04;
		debug_msg("handle_write_be_or_amo: send XLATE_PENDING for this cmd =0x%x \n", event->command);
		return;
	}

	// Send cmd & data (if available) to client/libocxl to process
	// The request will now await confirmation from the client that the memory write/op was
	// successful before generating a response.
	if (event->type == CMD_WR_BE) {
		buffer = (uint8_t *) malloc(event->size + 19);
		buffer[0] = (uint8_t) OCSE_WR_BE;
		size = (uint16_t *)&(buffer[1]);
		*size = htons(event->size); //value of size alwayz 64 for this cmd
		addr = (uint64_t *) & (buffer[3]);
		*addr = htonll(event->addr);
		wr_be = (uint64_t *) & (buffer[11]);
		*wr_be = htonll(event->wr_be);
		memcpy(&(buffer[19]), &(event->data[0]), event->size);
		event->abort = &(client->abort);
		debug_msg("%s:WRITE_BE wr_be=0x%016"PRIx64" size=%d addr=0x%016"PRIx64" port=0x%2x",
		  	cmd->afu_name, event->wr_be, event->size, event->addr, client->fd);
		if (put_bytes(client->fd, event->size + 19, buffer, cmd->dbg_fp,
		      cmd->dbg_id, client->context) < 0)
			client_drop(client, TLX_IDLE_CYCLES, CLIENT_NONE);
	} else if (event->type == CMD_AMO_WR || event->type == CMD_AMO_RW) { //these have data from cdata_bus

		offset = event->addr & ~CACHELINE_MASK;
			buffer = (uint8_t *) malloc(28);
		if (event->type == CMD_AMO_WR)
			buffer[0] = (uint8_t) OCSE_AMO_WR;
		 else // (event->type == CMD_AMO_RW)
			buffer[0] = (uint8_t) OCSE_AMO_RW;
		buffer[1] = (uint8_t)event->size;
		addr = (uint64_t *) & (buffer[2]);
		*addr = htonll(event->addr);
		buffer[10] = event->cmd_flag;
		buffer[11] = event->cmd_endian;
		memcpy(&(buffer[12]), &(event->data[offset]), 16);
		event->abort = &(client->abort);

		debug_msg("%s:AMO_WR or AMO_RW cmd_flag=0x%02x size=%d addr=0x%016"PRIx64" port=0x%2x",
		  	cmd->afu_name, event->cmd_flag, event->size, event->addr, client->fd);
		if (put_bytes(client->fd, 28, buffer, cmd->dbg_fp,
		      cmd->dbg_id, client->context) < 0) 
			client_drop(client, TLX_IDLE_CYCLES, CLIENT_NONE);
	} else if (event->type == CMD_AMO_RD ) {  //these have no data, use just memory ops. Still need op_size though
		buffer = (uint8_t *) malloc(12); //or 13??
		buffer[0] = (uint8_t) OCSE_AMO_RD;
		buffer[1] = (uint8_t)event->size;
		addr = (uint64_t *) & (buffer[2]);
		*addr = htonll(event->addr);
		buffer[10] = event->cmd_flag;
		buffer[11] = event->cmd_endian;
		event->abort = &(client->abort);

		debug_msg("%s:AMO_RD cmd_flag=0x%02x size=%d addr=0x%016"PRIx64" port=0x%2x",
		  	cmd->afu_name, event->cmd_flag, event->size, event->addr, client->fd);
		if (put_bytes(client->fd, 12, buffer, cmd->dbg_fp,
		      cmd->dbg_id, client->context) < 0) 
			client_drop(client, TLX_IDLE_CYCLES, CLIENT_NONE);
		}


	client->mem_access = (void *)event;
	debug_msg("Setting client->mem_access in handle_write_be_or_amo");
	return;


}


// Handle randomly selected xlate_pending or intrp_pending and send AFU back a xlate_done or intrp_rdy cmd 
void handle_xlate_intrp_pending_sent(struct cmd *cmd)
{
	struct cmd_event **head;
	struct cmd_event *event;
	struct client *client;
	uint8_t cmd_to_send;

	// Make sure cmd structure is valid
	if (cmd == NULL)
		return;

	// Randomly select a pending touch (or none)
	head = &cmd->list;
	while (*head != NULL) {
		// if the state of the event is mem_done, we can potentially stop the loop and send a response for it.
		// if we not allowing reordering, we'll break the loop and use this event.
		// don't allow reordering while we sort this out.
		if ( ( (*head)->state == MEM_PENDING_SENT )  && !allow_reorder(cmd->parms)) {
		  break;
		}
		//debug_msg("handle_xlate_pending_done:  this cmd =0x%x  this state =0x%x \n", (*head)->command, (*head)->state);
		head = &((*head)->_next);
	}

	event = *head;

	// Randomly select a pending touch (or none)
	/*event = cmd->list;
	while (event != NULL) {
		if ((event->state == MEM_PENDING_SENT)
		    && ((event->client_state != CLIENT_VALID)
			|| !allow_reorder(cmd->parms))) {
			break;
		}

		event = event->_next;
	} */

	// Test for client disconnect
	if ((event == NULL) || ((client = _get_client(cmd, event)) == NULL))
		return;

	debug_msg("%s:handle xlate_intrp_pending_done cmd_flag=0x%x tag=0x%02x addr=0x%016"PRIx64, cmd->afu_name,
		  event->cmd_flag, event->afutag, event->addr);
	// Check to see if this cmd gets selected for a RETRY or FAILED or PENDING read_failed response
	if ( allow_retry(cmd->parms)) {
		event->resp = 0x02;
		debug_msg("handle_xlate_intrp_pending_done: RETRY this cmd =0x%x \n", event->command);
	} else if ( allow_failed(cmd->parms)) {
		event->resp = 0x0f;
		debug_msg("handle_xlate_intrp_pending_done: FAIL this cmd =0x%x \n", event->command);
		return;
	} else
		event->resp = 0x0;  // send completed resp code in the xlate_done cmd

	// test for "interrupt" commands to return intrp_rdy, otherwise return xlate_done
	// this range includes: intrp_req, intrp_req_s, intrp_req_d, intrp_req_d_s, wake_host_thread, and wake_host_thread_s
	if ((event->command >= AFU_CMD_INTRP_REQ) && (event->command <= AFU_CMD_WAKE_HOST_THRD_S))
		cmd_to_send = TLX_CMD_INTRP_RDY;
	else
		cmd_to_send = TLX_CMD_XLATE_DONE;

	if (tlx_afu_send_posted_cmd(cmd->afu_event,
			cmd_to_send, event->afutag, event->resp) == TLX_SUCCESS){
			debug_msg("%s:XLATE_INTRP_DONE CMD event @ 0x%016" PRIx64 ", sent tag=0x%02x code=0x%x cmd=0x%x", cmd->afu_name,
			    event, event->afutag, event->resp, cmd_to_send);
			*head = event->_next;
		 	free(event->data);
		 	//free(event->parity);
		 	free(event);
			//cmd->credits++;
		}
	}




// Handle randomly selected memory touch
void handle_touch(struct cmd *cmd)
{
	struct cmd_event *event;
	struct client *client;
	uint8_t *buffer;
	uint64_t *addr;

	// Make sure cmd structure is valid
	if (cmd == NULL)
		return;

	// Randomly select a pending touch (or none)
	event = cmd->list;
	while (event != NULL) {
		if (((event->type == AFU_CMD_XLATE_TOUCH) || (event->type == AFU_CMD_XLATE_TOUCH_N))
		    && (event->state == MEM_IDLE)
		    && ((event->client_state != CLIENT_VALID)
			|| !allow_reorder(cmd->parms))) {
			break;
		}

		event = event->_next;
	}

	// Test for client disconnect
	if ((event == NULL) || ((client = _get_client(cmd, event)) == NULL))
		return;

	// Check that memory request can be driven to client
	if (client->mem_access != NULL)
		return;

	debug_msg("%s:XLATE TOUCH cmd_flag=0x%x tag=0x%02x addr=0x%016"PRIx64, cmd->afu_name,
		  event->cmd_flag, event->afutag, event->addr);
	// Check to see if this cmd gets selected for a RETRY or FAILED or PENDING read_failed response
	if ( allow_retry(cmd->parms)) {
		event->state = MEM_DONE;
		event->resp_opcode = TLX_RSP_TOUCH_RESP;
		event->type = CMD_FAILED;
		event->resp = 0x02;
		debug_msg("handle_touch: RETRY this cmd =0x%x \n", event->command);
		return;
	}
	if ( allow_failed(cmd->parms)) {
		event->state = MEM_DONE;
		event->resp_opcode = TLX_RSP_TOUCH_RESP;
		event->type = CMD_FAILED;
		event->resp = 0x0e;
		debug_msg("handle_touch: FAIL this cmd =0x%x \n", event->command);
		return;
	}
	// for xlate_pending response, ocse has to THEN follow up with an xlate_done response 
	// (at some unknown time later) and that will "complete" the original cmd (no rd/write )
	if ( allow_pending(cmd->parms)) {
		event->state = MEM_XLATE_PENDING;
		event->resp_opcode = TLX_RSP_TOUCH_RESP;
		event->type = CMD_FAILED;
		event->resp = 0x04;
		debug_msg("handle_touch: send XLATE_PENDING for this cmd =0x%x \n", event->command);
		return;
	}


	// Send xlate touch request to client
	buffer = (uint8_t *) malloc(11);
	buffer[0] = (uint8_t) OCSE_MEMORY_TOUCH;
	addr = (uint64_t *) & (buffer[1]);
	*addr = htonll(event->addr);
	buffer[9] = event->cmd_flag;
	buffer[10] = event->cmd_pg_size;
	debug_msg("%s:XLATE TOUCH cmd_flag=0x%x afutag=0x%02x addr=0x%016"PRIx64, cmd->afu_name,
		  event->cmd_flag, event->afutag, event->addr);
	if (put_bytes(client->fd, 11, buffer, cmd->dbg_fp, cmd->dbg_id,
		      event->context) < 0) {
		client_drop(client, TLX_IDLE_CYCLES, CLIENT_NONE);
	}
	event->state = MEM_TOUCH;
	client->mem_access = (void *)event;
	debug_msg("Setting client->mem_access in handle_touch");
	debug_cmd_client(cmd->dbg_fp, cmd->dbg_id, event->afutag, event->context); 
}

// Send pending interrupt to client as soon as possible
void handle_interrupt(struct cmd *cmd)
{
	struct cmd_event **head;
	struct cmd_event *event;
	struct client *client;
	uint64_t offset;
	uint16_t byte_count;
	uint8_t buffer[45];

	// debug_msg( "ocse:handle_interrupt:" );

	// Make sure cmd structure is valid
	if (cmd == NULL)
		return;
	//debug_msg( "ocse:handle_interrupt:valid cmd available" );

	// Send any interrupts to client immediately
	head = &cmd->list;
	while (*head != NULL) {
		if ((((*head)->type == CMD_INTERRUPT) || ((*head)->type == CMD_WAKE_HOST_THRD)) &&
		    (((*head)->state == MEM_IDLE) || ((*head)->state == MEM_RECEIVED)))
			break;
		head = &((*head)->_next);
	}
	event = *head;

	// Test for client disconnect
	if ((event == NULL) || ((client = _get_client(cmd, event)) == NULL))
		return;

	// Check to see if this cmd gets selected for a RETRY or FAILED or PENDING response
	// No need to set event->resp_opcode if FAILED bc resp_opcode is TLX_RSP_INTRP_RESP  or TLX_RSP_WAKE_HOST_RESP already
	if ( allow_int_retry(cmd->parms)) {
		event->state = MEM_DONE;
		event->type = CMD_FAILED;
		event->resp = 0x02;
		debug_msg("handle_interrupt: RETRY this cmd =0x%x \n", event->command);
		return;
	}
	if ( allow_int_failed(cmd->parms)) {
		event->state = MEM_DONE;
		event->type = CMD_FAILED;
		event->resp = 0x0e;
		debug_msg("handle_interrupt: FAIL this cmd =0x%x \n", event->command);
		return;
	}
	// for int_pending response, ocse has to THEN follow up with an xlate_done response
	// (at some unknown time later) and that will "complete" the original cmd (no rd/write )
	if (( event->type != CMD_WAKE_HOST_THRD) && ( allow_int_pending(cmd->parms))) { // CMD_WAKE_HOST pending is OCAPI4
		event->state = MEM_INT_PENDING;
		event->type = CMD_FAILED;
		event->resp = 0x04;
		debug_msg("handle_interrupt: send INT_PENDING for this cmd =0x%x \n", event->command);
		return;
	}

	if (( event->command == AFU_CMD_INTRP_REQ_D) && ( allow_int_derror(cmd->parms))) { //DERROR only for one cmd type
		event->state = MEM_DONE;
		event->type = CMD_FAILED;
		event->resp = 0x08;
		debug_msg("handle_interrupt: DERROR this cmd =0x%x \n", event->command);
		return;
	}

	// Send interrupt or wake_host_thread request to client
	if (event->type == CMD_WAKE_HOST_THRD)
		buffer[0] = OCSE_WAKE_HOST_THREAD;
	else if (event->command == AFU_CMD_INTRP_REQ_D)
			buffer[0] = OCSE_INTERRUPT_D;
		else
			buffer[0] = OCSE_INTERRUPT;

	memcpy(&(buffer[1]), &event->cmd_flag, 1);
	memcpy(&(buffer[2]), &event->addr, 8);
	byte_count = 10;

	if (event->command == AFU_CMD_INTRP_REQ_D) {
		offset = event->addr & ~CACHELINE_MASK;
		memcpy(&(buffer[10]), &event->size, 2);
		byte_count += 2;
		memcpy(&(buffer[12]), &(event->data[offset]), event->size);
		byte_count += event->size;
	}

	// do we still need this event->abort???
	event->abort = &(client->abort);

	debug_msg( "ocse:handle_interrupt: cmd=0x%02x cmd_flag=%d addr=0x%016"PRIx64,
		   event->command,
		   event->cmd_flag,
		   event->addr );

	if (put_bytes(client->fd, byte_count, buffer, cmd->dbg_fp, cmd->dbg_id,
		      event->context) < 0) {
		client_drop(client, TLX_IDLE_CYCLES, CLIENT_NONE);
	}
	debug_cmd_client(cmd->dbg_fp, cmd->dbg_id, event->afutag, event->context);

	// this assumes the wake host thread finds a thread
	// should add a path for a negative response from libocxl application
	event->state = MEM_DONE;
}

//void handle_buffer_data(struct cmd *cmd)
/*{
	uint8_t *parity_check;
	int rc = 0;
	struct cmd_event *event;
	int quadrant, byte;

	// debug_msg( "ocse:handle_buffer_data:" );
	// Has struct been initialized?
	if ((cmd == NULL) || (cmd->buffer_read == NULL))
		return;

	// Check if buffer read data has returned from AFU
	event = cmd->buffer_read;
	if (rc == TLX_SUCCESS) {
		debug_msg("%s:BUFFER READ afutag=0x%02x", cmd->afu_name,
			  event->afutag);
		for (quadrant = 0; quadrant < 4; quadrant++) {
			DPRINTF("DEBUG: Q%d 0x", quadrant);
			for (byte = 0; byte < CACHELINE_BYTES / 4; byte++) {
				DPRINTF("%02x", event->data[byte]);
			}
			DPRINTF("\n");
		}
	debug_msg("handle_buffer_data parity_enable is 0x%x ", parity_enable);
		if (parity_enable) {
			parity_check =
			    (uint8_t *) malloc(DWORDS_PER_CACHELINE / 8);
			generate_cl_parity(event->data, parity_check);
			if (strncmp((char *)event->parity,
				    (char *)parity_check,
				    DWORDS_PER_CACHELINE / 8)) {
				error_msg("Buffer read parity error afutag=0x%02x",
					  event->afutag);
			}
			free(parity_check);
		}
		// Free buffer interface for another event
		cmd->buffer_read = NULL;
		// Randomly decide to not send data to client yet
		if (!event->buffer_activity && allow_buffer(cmd->parms)) {
			event->state = MEM_TOUCHED;
			event->buffer_activity = 1;
			return;
		}

		event->state = MEM_RECEIVED;
	}

} */

// TODO Do we even need this anymore? It isn't called from ocl.c
/*void handle_mem_write(struct cmd *cmd)
{
	struct cmd_event **head;
	struct cmd_event *event;
	struct client *client;
	uint64_t *addr;
	uint16_t *size;
	uint8_t *buffer;
	uint64_t offset;

	// debug_msg( "ocse:handle_mem_write:" );
	// Make sure cmd structure is valid
	if (cmd == NULL)
		return;

	// Send any ready write data to client immediately
	head = &cmd->list;
	while (*head != NULL) {
		if (((*head)->type == CMD_WRITE) &&
		    ((*head)->state == MEM_RECEIVED))
			break;
		head = &((*head)->_next);
	}
	event = *head;

	// Test for client disconnect
	if ((event == NULL) || ((client = _get_client(cmd, event)) == NULL))
		return;

	// Check that memory request can be driven to client
	if (client->mem_access != NULL)
		return;

	// Send data to client and clear event to allow
	// the next buffer read to occur.  The request will now await
	// confirmation from the client that the memory write was
	// successful before generating a response.  The client
	// response will cause a call to either handle_aerror() or
	// handle_mem_return().
	buffer = (uint8_t *) malloc(event->size + 11);
	offset = event->addr & ~CACHELINE_MASK;
	buffer[0] = (uint8_t) OCSE_MEMORY_WRITE;
	size = (uint16_t *)&(buffer[1]);
	*size = htons(event->size);
	addr = (uint64_t *) & (buffer[3]);
	*addr = htonll(event->addr);
	memcpy(&(buffer[10]), &(event->data[offset]), event->size);
	event->abort = &(client->abort);
	debug_msg("%s:MEMORY WRITE afutag=0x%04x size=%d addr=0x%016"PRIx64,
		  cmd->afu_name, event->afutag, event->size, event->addr);
	if (put_bytes(client->fd, event->size + 10, buffer, cmd->dbg_fp,
		      cmd->dbg_id, client->context) < 0) {
		client_drop(client, TLX_IDLE_CYCLES, CLIENT_NONE);
	}
	debug_cmd_client(cmd->dbg_fp, cmd->dbg_id, event->afutag, event->context);
	client->mem_access = (void *)event;
	debug_msg("Setting client->mem_access in handle_mem_write");
} */

// Handle data returning from client for memory read
static void _handle_mem_read(struct cmd *cmd, struct cmd_event *event, int fd)
{
	uint8_t data[MAX_LINE_CHARS];
	uint64_t offset = event->addr & ~CACHELINE_MASK;

	// printf ("_handle_mem_read: event->type is %2x, event->state is 0x%3x \n", event->type, event->state);
	if (event->type == CMD_READ) {
	        //printf ("_handle_mem_read: CMD_READ \n" );
		// Client is returning data from memory read
		if (get_bytes_silent(fd, event->size, data, cmd->parms->timeout,
			     event->abort) < 0) {
	        	debug_msg("%s:_handle_mem_read failed afutag=0x%04x size=%d addr=0x%016"PRIx64,
				  cmd->afu_name, event->afutag, event->size, event->addr);
			event->state = MEM_DONE;
			event->type = CMD_FAILED;
			event->resp = 0x0e;
			debug_cmd_update(cmd->dbg_fp, cmd->dbg_id, event->afutag,
				 event->context, event->resp);
			return;
		}
		// we used to put the data in the event->data at the offset implied by the address
		// should we still do that?  It might depend on the the actual ap command that we received.
		memcpy((void *)&(event->data[offset]), (void *)&data, event->size);
		// parity is no long required. although we might want to set the bad data indicator for
		// bad machine path simulations.
		//generate_cl_parity(event->data, event->parity);
		event->state = MEM_RECEIVED;
	}
        // have to expect data back from some AMO ops
	else if ((event->type == CMD_AMO_RD) || (event->type == CMD_AMO_RW)) {
		// Client is returning data from AMO memory read
                 debug_msg( "_handle_mem_read: AFU_CMD_AMO_RD or AFU_CMD_AMO_RW \n" );
		if (get_bytes_silent(fd, event->size, data, cmd->parms->timeout,
			     event->abort) < 0) {
	        	debug_msg("%s:_handle_amo_mem_read failed afutag=0x%02x size=%d addr=0x%016"PRIx64,
				  cmd->afu_name, event->afutag, event->size, event->addr);
			event->state = MEM_DONE;
			event->type = CMD_FAILED;
			event->resp = 0x0e;
			debug_cmd_update(cmd->dbg_fp, cmd->dbg_id, event->afutag,
				 event->context, event->resp);
			return;
		}
		// DMA return data goes at offset 0 in the event data instead of some other offset.
                // should we clear event->data first?
		memcpy((void *)&(event->data[offset]), (void *)&data, event->size);
	        debug_msg("%s:_handle_amo_mem_read DONE afutag=0x%02x size=%d addr=0x%016"PRIx64,
			  cmd->afu_name, event->afutag, event->size, event->addr);
		event->state = MEM_DONE;

	}
}

// Calculate page address in cached index for translation
static void _calc_index(struct cmd *cmd, uint64_t * addr, uint64_t * index)
{
	*addr &= cmd->page_entries.page_filter;
	*index = *addr & cmd->page_entries.entry_filter;
	*index >>= PAGE_ADDR_BITS;
}

// Update age of translation entries and create new entry if needed
static void _update_age(struct cmd *cmd, uint64_t addr)
{
	uint64_t index;
	int i, set, age, oldest, empty;

	_calc_index(cmd, &addr, &index);
	set = age = oldest = 0;
	empty = PAGE_WAYS;
	for (i = 0; i < PAGE_WAYS; i++) {
		if (cmd->page_entries.valid[index][i] &&
		    (cmd->page_entries.entry[index][i] != addr)) {
			cmd->page_entries.age[index][i]++;
			if (cmd->page_entries.age[index][i] > age) {
				age = cmd->page_entries.age[index][i];
				oldest = i;
			}
		}
		if (!cmd->page_entries.valid[index][i] && (empty == PAGE_WAYS)) {
			empty = i;
		}
		if (cmd->page_entries.valid[index][i] &&
		    (cmd->page_entries.entry[index][i] == addr)) {
			cmd->page_entries.age[index][i] = 0;
			set = 1;
		}
	}

	// Entry found and updated
	if (set)
		return;

	// Empty slot exists
	if (empty < PAGE_WAYS) {
		cmd->page_entries.entry[index][empty] = addr;
		cmd->page_entries.valid[index][empty] = 1;
		cmd->page_entries.age[index][empty] = 0;
		return;
	}
	// Evict oldest entry and replace with new entry
	cmd->page_entries.entry[index][oldest] = addr;
	cmd->page_entries.valid[index][oldest] = 1;
	cmd->page_entries.age[index][oldest] = 0;
}

// Determine if page translation is already cached
/* static int _page_cached(struct cmd *cmd, uint64_t addr) */
/* { */
/* 	uint64_t index; */
/* 	int i, hit; */

/* 	_calc_index(cmd, &addr, &index); */
/* 	i = hit = 0; */
/* 	while ((i < PAGE_WAYS) && cmd->page_entries.valid[index][i] && */
/* 	       (cmd->page_entries.entry[index][i] != addr)) { */
/* 		i++; */
/* 	} */

/* 	// Hit entry */
/* 	if ((i < PAGE_WAYS) && cmd->page_entries.valid[index][i]) */
/* 		hit = 1; */

/* 	return hit; */
/* } */

// Decide what to do with a client memory acknowledgement
void handle_mem_return(struct cmd *cmd, struct cmd_event *event, int fd)
{
	struct client *client;

	// debug_msg( "ocse:handle_mem_return:" );
	// Test for client disconnect
	if ((event == NULL) || ((client = _get_client(cmd, event)) == NULL))
		return;

	debug_msg("%s:MEMORY ACK afutag=0x%02x addr=0x%016"PRIx64, cmd->afu_name,
		  event->afutag, event->addr);

	// Randomly cause paged response TODO, if still needed, this needs to be updated for ocse
	/*if (((event->type != CMD_WRITE) || (event->state != MEM_REQUEST)) &&
	    (client->flushing == FLUSH_NONE) && !_page_cached(cmd, event->addr)
	    && allow_paged(cmd->parms)) {
		if (event->type == CMD_READ)
			_handle_mem_read(cmd, event, fd);
		//event->resp = TLX_RESPONSE_PAGED;
		event->state = MEM_DONE;
		client->flushing = FLUSH_PAGED;
		debug_cmd_update(cmd->dbg_fp, cmd->dbg_id, event->afutag,
				 event->context, event->resp);
		return;
	} */
	_update_age(cmd, event->addr);
	if (event->type == CMD_READ)
		_handle_mem_read(cmd, event, fd);
	if (event->type == CMD_WRITE)
		event->state = MEM_DONE;
 	// have to account for AMO RD or RW cmds with returned data
	else if ((event->type == CMD_AMO_RD) || (event->type == CMD_AMO_RW)) {
		// Client is returning data from AMO memory read or rw
                 debug_msg( "_handle_mem_return: CMD_DMA_RD or CMD_DMA_WR_AMO " );
		_handle_mem_read(cmd,event,fd);
		// have to set size back
		if (event->size == 4)
			event->size = 2;
		else
			event->size = 3;
		  }

	else if (event->type == CMD_TOUCH)
		event->state = MEM_DONE;
	else if (event->state == MEM_TOUCH)	// Touch before write
		event->state = MEM_TOUCHED;
	else			// Write after touch
		event->state = MEM_DONE;
	debug_cmd_return(cmd->dbg_fp, cmd->dbg_id, event->afutag, event->context);
}

// Mark memory event as address error in preparation for response
void handle_aerror(struct cmd *cmd, struct cmd_event *event)
{
  	debug_msg( "ocse:handle_aerror:" );
	event->state = MEM_DONE;
	event->type = CMD_FAILED;
	event->resp = 0x0e;
	debug_cmd_update(cmd->dbg_fp, cmd->dbg_id, event->afutag,
			 event->context, event->resp);
}

// Send a randomly selected pending response back to AFU
void handle_response(struct cmd *cmd)
{
	struct cmd_event **head;
	struct cmd_event *event;
	struct client *client;
	//uint8_t resp_dl, resp_dp;
	int rc = 0;

	// debug_msg( "ocse:handle_response:" );
	// Select a random pending response (or none)
	client = NULL;
	head = &cmd->list;
	while (*head != NULL) {
		// if the state of the event is mem_done, we can potentially stop the loop and send a response for it.
		// if we not allowing reordering, we'll break the loop and use this event.
		// don't allow reordering while we sort this out.
		if ( ( (*head)->state == MEM_DONE )  || ((*head)->state == MEM_XLATE_PENDING)
			|| ((*head)->state == MEM_INT_PENDING)) { // && !allow_reorder(cmd->parms))
		  //debug_msg( "%s:RESPONSE event @ 0x%016" PRIx64 ", drive response because MEM_DONE",
		  //	   cmd->afu_name, (*head) );
		  break;
		}

		head = &((*head)->_next);
	}

	event = *head;

	// Randomly decide not to drive response yet - skip this for now
	// if ( ( event == NULL ) || ( ( event->client_state == CLIENT_VALID ) &&
	// 			    ( !allow_resp(cmd->parms) ) ) ) {
	//      debug_msg( "%s:RESPONSE event @ 0x%016" PRIx64 "skipped because suppressed by allow_resp", cmd->afu_name, event );
	// 	return;
	// }

	// Test for client disconnect
	if ((event == NULL) || ((client = _get_client(cmd, event)) == NULL)) {
	  //debug_msg( "%s:RESPONSE event @ 0x%016" PRIx64 " skipped because event or client NULL", cmd->afu_name, event );
	  // maybe we should free it too???
	  return;
	}


 //`drive_resp:
	// debug - dump the event we picked...
	debug_msg( "%s:RESPONSE event @ 0x%016" PRIx64 ", command=0x%x, afutag=0x%08x, type=0x%02x, state=0x%02x, resp=0x%x",
		   cmd->afu_name,
		   event,
		   event->command,
		   event->afutag,
		   event->type,
		   event->state,
		   event->resp );



	if (event->type == CMD_FAILED)   //send a failed response; add_cmd set resp_dl & dp; function that FAILED cmd set event->resp_opcode appropriately
		rc = tlx_afu_send_resp( cmd->afu_event, event->resp_opcode, event->afutag, event->resp,0,event->resp_dl,event->resp_dp,0);


	else if ( (event->command == AFU_CMD_PR_RD_WNITC) || (event->command == AFU_CMD_PR_RD_WNITC_N) ||
		    (event->command == AFU_CMD_AMO_RD) || (event->command == AFU_CMD_AMO_RD_N) ||
		    (event->command == AFU_CMD_AMO_RW) || (event->command == AFU_CMD_AMO_RW_N) ||
		    (event->command == AFU_CMD_RD_WNITC) || (event->command == AFU_CMD_RD_WNITC_N) ){
		// if not AMO or PR_RD, check to see if split response is warranted
		// For this initial implementation, there is a parm called HOST_CL_SIZE that is used to determine max resp length
		// This could alternatively be thought of as an internal bus transfer restriction. Valid sizes are 64, 128 and 256.
		// Default is 128. Right now this value is fixed. This could change in the future to be a randomized selection of 64, 128 or 256.
		if  ((event->command == AFU_CMD_RD_WNITC) || (event->command == AFU_CMD_RD_WNITC_N) ){
			if (event->size > cmd->HOST_CL_SIZE)  { // a split resp
				if (event->resp_bytes_sent != 0) { //continue a split resp
					debug_msg("handle_response: continue a split read response \n");
					// should event->resp_dp be & 0x3 ?
					// if HOST_CL_SIZE = 128, resp_dp will be 0x2
					// if HOST_CL_SIZE = 64, resp_dp will be 0x1, 0x2, 0x3 
					if (cmd->HOST_CL_SIZE == 128)
					    event->resp_dp = 0x2;
					else
					    event->resp_dp += 1;
				}  
				event->resp_dl =  size_to_dl (cmd->HOST_CL_SIZE);
				event->resp_bytes_sent += cmd->HOST_CL_SIZE;
			}
		}
	    // we can just send the 64 bytes of data back
	    // and complete the event
			if ( allow_bdi_resp_err(cmd->parms)) {
				debug_msg("handle_response: Set BDI=1 in the resp data for afutag=0x%x \n",
				 event->afutag);
		    		 rc = tlx_afu_send_resp_and_data( cmd->afu_event, event->resp_opcode,
					     event->afutag, 0, 0, event->resp_dl, event->resp_dp, 0, 1, event->data ) ;
			 } else {
		    		rc = tlx_afu_send_resp_and_data( cmd->afu_event,
					     event->resp_opcode,
					     event->afutag,
					     0, // resp_code - not really used for a good response
					     0, // resp_pg_size - not used by response,
					     event->resp_dl, // for partials, dl is 1 (64 B)
					     event->resp_dp, // for partials, dp is 0 (the 0th part)
					     0, // resp_addr_tag, - not used by response
					     0, // - now used by response
					     event->data ) ; // data in this case is already at the proper offset in the 64 B data packet
			}
	    
		} else  { //have to send just a response
			// Check to see if, for  a dma_wr, a partial response is warranted
			if  ((event->command == AFU_CMD_DMA_W) || (event->command == AFU_CMD_DMA_W_N) ){
				if (event->size > cmd->HOST_CL_SIZE)  { // a split resp
					if (event->resp_bytes_sent != 0) { //continue a split resp
						debug_msg("handle_response: continue a split write response \n");
						// should event->resp_dp be & 0x3 ?
						// if HOST_CL_SIZE = 128, resp_dp will be 0x2
						// if HOST_CL_SIZE = 64, resp_dp will be 0x1, 0x2, 0x3 
						if (cmd->HOST_CL_SIZE == 128)
					    		event->resp_dp = 0x2;
						else
					    		event->resp_dp += 1;
					}  
					event->resp_dl =  size_to_dl (cmd->HOST_CL_SIZE);
					event->resp_bytes_sent += cmd->HOST_CL_SIZE;
				}
			}
			rc = tlx_afu_send_resp( cmd->afu_event, event->resp_opcode, event->afutag, event->resp,0,event->resp_dl,event->resp_dp,0);
		}

	if (rc == TLX_SUCCESS) {
		//if we sent a failed resp=0x4 (xlate_pending or int_pending) we need to schedule to send a xlate_done cmd
		// Can't free this event, will handle MEM_PENDING_SENT state in new routine
		// it'll send xlate_done cmd and then free (no respnse expected back from AFU)
		if (( event->state == MEM_XLATE_PENDING) || (event->state == MEM_INT_PENDING)) {
			event->state = MEM_PENDING_SENT;
			return;
		}
		// ALSO, can't free if this is not last part of a split response
		if ((event->resp_bytes_sent > 0 ) && (event->resp_bytes_sent != event->size))
			return;
		//else {
			debug_msg("%s:RESPONSE event @ 0x%016" PRIx64 ", sent afutag=0x%02x code=0x%x", cmd->afu_name,
			    event, event->afutag, event->resp);
			debug_cmd_response(cmd->dbg_fp, cmd->dbg_id, event->afutag, event->resp_opcode, event->resp);
		            debug_msg( "%s:RESPONSE event @ 0x%016" PRIx64 ", free event",
			    cmd->afu_name, event );
			*head = event->_next;
		 	free(event->data);
		 	//free(event->parity);
		 	free(event);
	//	}
	} else {
		 if (rc == AFU_TLX_NO_CREDITS)
				debug_msg ("NO AFU_TLX_RESP_CREDITS TO SEND RESP for AFUTAG 0x%x so will try LATER ", event->afutag);
		 else
			 debug_msg( "%s:RESPONSE event @ 0x%016" PRIx64 ", _response() failed for AFUTAG 0x%x so will try LATER",
			     cmd->afu_name, event, event->afutag );
		if (event->resp_bytes_sent != 0)
			event->resp_bytes_sent -= cmd->HOST_CL_SIZE;  // back up byte count if we really didn't send the split response......
		 return;
	}
}

int client_cmd(struct cmd *cmd, struct client *client)
{
	int rc = 0;
	struct cmd_event *event = cmd->list;

	while (event != NULL) {
		if (event->context != client->context) {
			// Event is not for this client
			event = event->_next;
			continue;
		}
		if ((client->state == CLIENT_NONE) &&
		    (event->state != MEM_DONE)) {
			// Client dropped, terminate event
			event->state = MEM_DONE;
			if ((event->type == CMD_READ) ||
			    (event->type == CMD_WRITE) ||
			    (event->type == CMD_TOUCH)) {
				event->resp = TLX_RESPONSE_FAILED;
			}
			event = event->_next;
			continue;
		}
		if (client->state == CLIENT_VALID) {
			// Event is for client in valid state
			return 1;
		}
		event = event->_next;
	}
	return rc;
}
