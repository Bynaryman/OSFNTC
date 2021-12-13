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

// first implement ocxl_afu_open_dev and required stack - check
// next implement ocxl_afu_attach and required stack
// then mmio helpers
// then lpc helpers

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <limits.h>

#include "libocxl_internal.h"
#include "libocxl.h"
#include "../common/utils.h"

#define API_VERSION            1
#define API_VERSION_COMPATIBLE 1

#ifdef DEBUG
#define DPRINTF(...) printf(__VA_ARGS__)
#else
#define DPRINTF(...)
#endif

#ifndef MAX
#define MAX(a,b)(((a)>(b))?(a):(b))
#endif /* #ifndef MAX */

#ifndef MIN
#define MIN(a,b)(((a)<(b))?(a):(b))
#endif /* #ifndef MIN */

/*
 * System constants
 */

#define MAX_LINE_CHARS 1024

#define FOURK_MASK        0xFFFFFFFFFFFFF000L

#define DSISR 0x4000000040000000L
#define ERR_BUFF_MAX_COPY_SIZE 4096

static int _delay_1ms()
{
	struct timespec ts;
	ts.tv_sec = 0;
	ts.tv_nsec = 1000000;
	return nanosleep(&ts, &ts);
}

static int _testmemaddr(uint8_t * memaddr)
{
	int fd[2];
	int ret = 0;
	if (pipe(fd) >= 0) {
		if (write(fd[1], memaddr, 1) > 0)
			ret = 1;
	}

	close(fd[0]);
	close(fd[1]);

	return ret;
}

ocxl_wait_event *_alloc_wait_event( uint16_t tid )
{
  ocxl_wait_event *this_wait_event;

  // scan for tid in list first - return it if you find it
  // scan list
  // pthread_mutex_lock(&(this_wait_event->wait_lock));
  this_wait_event = ocxl_wait_list;
  // pthread_mutex_unlock(&(this_wait_event->wait_lock));

  debug_msg( "_alloc_wait_event: list=0x%016llx ; this=0x%016llx", (uint64_t)ocxl_wait_list, (uint64_t)this_wait_event );

  while ( this_wait_event != NULL ) {
    // pthread_mutex_lock(&(this_wait_event->wait_lock));
      debug_msg( "_alloc_wait_event: checking @ 0x%016llx -> 0x%04x = 0x%04x", (uint64_t)this_wait_event, this_wait_event->tid, tid );
    if ( this_wait_event->tid == tid ) {
      // match
      // pthread_mutex_unlock(&(this_wait_event->wait_lock));
      debug_msg( "_alloc_wait_event: match @ 0x%016llx -> 0x%04x = 0x%04x", (uint64_t)this_wait_event, this_wait_event->tid, tid );
      return this_wait_event;
    }
  
    this_wait_event = this_wait_event->_next;
    // pthread_mutex_unlock(&(this_wait_event->wait_lock));
  }

  // if not found, create it
  this_wait_event = (ocxl_wait_event *)malloc( sizeof(ocxl_wait_event) );
  this_wait_event->tid = tid;
  this_wait_event->enabled = 0;
  this_wait_event->received = 0;

  debug_msg( "_alloc_wait_event: new wait event @ 0x%016llx -> 0x%04x", (uint64_t)this_wait_event, this_wait_event->tid );

  // put it at the head of the list
  this_wait_event->_next = ocxl_wait_list;
  ocxl_wait_list = this_wait_event;

  debug_msg( "_alloc_wait_event: list starts @ 0x%016llx", (uint64_t)ocxl_wait_list );

  return this_wait_event;
}

void _free_wait_event( ocxl_wait_event *free_wait_event )
{
  ocxl_wait_event *this_wait_event;

  if ( ocxl_wait_list == NULL ) {
    // somehow the list is empty so just free what we got
    free( free_wait_event );
    return;
  }

  if ( ocxl_wait_list == free_wait_event ) {
    // free_wait_event is the first in the list, so do it specially
    ocxl_wait_list = free_wait_event->_next;
    free( free_wait_event );
    return;
  }

  // scan the list
  this_wait_event = ocxl_wait_list;
  while ( this_wait_event != NULL ) {
    if ( this_wait_event->_next == free_wait_event ) {
      // found it - adjust the pointer, free, and leave
      this_wait_event->_next = free_wait_event->_next;
      free( free_wait_event );
      return;
    } else {
      // next!
      this_wait_event = this_wait_event->_next;
    }
  }

  // we've been through the list, but free_wait_event was not there...  free it anyway
  free( free_wait_event );
  return;
}

static void _all_idle(struct ocxl_afu *afu_h)
{
	if (!afu_h)
		fatal_msg("NULL afu passed to libocxl.c:_all_idle");
	afu_h->int_req.state = LIBOCXL_REQ_IDLE;
	afu_h->open.state = LIBOCXL_REQ_IDLE;
	afu_h->attach.state = LIBOCXL_REQ_IDLE;
	afu_h->mmio.state = LIBOCXL_REQ_IDLE;
	afu_h->mem.state = LIBOCXL_REQ_IDLE;
	afu_h->mapped = 0;
	afu_h->global_mapped = 0;
	afu_h->attached = 0;
	afu_h->opened = 0;
}

static int _handle_dsi(struct ocxl_afu *afu, uint64_t addr)
{
	int i;

	if (!afu)
		fatal_msg("NULL afu passed to libocxl.c:_handle_dsi");
	// Only track a single DSI at a time
	pthread_mutex_lock(&(afu->event_lock));
	i = 0;
	while (afu->events[i] != NULL) {
		if (afu->events[i]->type == OCXL_EVENT_TRANSLATION_FAULT) {
			pthread_mutex_unlock(&(afu->event_lock));
			return 0;
		}
		++i;
	}
	assert(i < EVENT_QUEUE_MAX);

	afu->events[i] = (struct ocxl_event *)calloc(1, sizeof( ocxl_event ) );
	afu->events[i]->type = OCXL_EVENT_TRANSLATION_FAULT;
	// afu->events[i]->header.size = size;
	// afu->events[i]->header.process_element = afu->context;
	afu->events[i]->translation_fault.addr = (void *)(addr & FOURK_MASK);
	afu->events[i]->translation_fault.dsisr = DSISR;

	do {
		i = write(afu->pipe[1], &(afu->events[i]->type), 1);
	}
	while ((i == 0) || (errno == EINTR));
	pthread_mutex_unlock(&(afu->event_lock));
	return i;
}

static int _handle_wake_host_thread(struct ocxl_afu *afu)
{
        ocxl_wait_event *this_wait_event;
	uint64_t addr;
	uint8_t cmd_flag;
	uint8_t adata[8];

	if (!afu) fatal_msg("_handle_wake_host_thread:NULL afu passed");

	debug_msg("AFU WAKE HOST THREAD");

	// in opencapi, we should get a 64 bit address (to be interpretted as a thread id)
	// we should find a match to that thread id in our ocxl_wait_list

	//buffer[0] = OCSE_WAKE (already read)
	//buffer[1] = event->cmd_flag
	//buffer[2] = event->addr 

	if (get_bytes_silent(afu->fd, 1, &cmd_flag, 1000, 0) < 0) {
		warn_msg("Socket failure getting cmd_flags");
		_all_idle(afu);
		return -1;
	}
	if (get_bytes_silent(afu->fd, 8, adata, 1000, 0) < 0) {
		warn_msg("Socket failure getting address");
		_all_idle(afu);
		return -1;
	}
	memcpy(&addr, adata, 8);
	// addr = ntohs(addr);
	debug_msg("_handle_wake_host_thread: received wake_host_thread thread id 0x%016lx", addr);

	
	// scan list
	// pthread_mutex_lock(&(this_wait_event->wait_lock));
	this_wait_event = _alloc_wait_event( (uint16_t)addr );
	// pthread_mutex_unlock(&(this_wait_event->wait_lock));

	debug_msg("_handle_wake_host_thread: waking @ 0x%016llx -> 0x%04x", (uint64_t)this_wait_event, addr);
	this_wait_event->received = 1;
	
	return 0;
}

static int _handle_interrupt(struct ocxl_afu *afu, uint8_t data_is_valid)
{

	uint16_t data_size;
	struct ocxl_irq *irq;
	uint64_t addr;
	uint8_t cmd_flag;
	uint8_t adata[8];
	uint8_t ddata[32];
	int i;

	if (!afu) fatal_msg("_handle_interrupt:NULL afu passed");

	debug_msg( "_handle_interrupt for afu %d:", afu->context );

	// in opencapi, we should get a 64 bit address (and maybe data)
	// we should find that address in the afu's irq list
	// if we find it, we should put some stuff in the event array

	//buffer[0] = OCSE_INTERRUPT (already read)
	//buffer[1] = event->cmd_flag
	//buffer[2] = event->addr 

	if (get_bytes_silent(afu->fd, 1, &cmd_flag, 1000, 0) < 0) {
		warn_msg("Socket failure getting cmd_flags");
		_all_idle(afu);
		return -1;
	}
	if (get_bytes_silent(afu->fd, 8, adata, 1000, 0) < 0) {
		warn_msg("Socket failure getting address");
		_all_idle(afu);
		return -1;
	}
	memcpy(&addr, adata, 8);
	// addr = ntohs(addr);
	debug_msg("_handle_interrupt: afu %d received intrp_req addr 0x%016lx", afu->context, addr);
	
	if (data_is_valid) {  //this is an AFU_CMD_INTRP_REQ_D
	// For now, up to 32bytes of data is sent over, pulled  from the addr offset 
	// in the 64B data flit, If you prefer  it floating in a 64B buffer, edit _handle_interrupt in cmd.c
		if (get_bytes_silent(afu->fd, sizeof(uint16_t), ddata, 1000, 0) < 0) {
			warn_msg("Socket failure getting data_size");
			_all_idle(afu);
			return -1;
		}
		memcpy((char *)&data_size, (char *)ddata, sizeof(uint16_t));
	  	if (get_bytes_silent(afu->fd, data_size, ddata, 1000, 0) < 0) { 
	    		 warn_msg("Socket failure getting interrupt data "); 
	     		_all_idle(afu); 
	     		return -1; 

		}
	}

	// TODO Update the rest of this to actually search for address and then do 
	// whatever is needed if it's valid.....

	// search for addr in irq list of afu
	// if we don't find it, warn_msg
	// if we do find it, add an event if it is new for this irq
	irq = afu->irq;
	while (irq != NULL) {
	  debug_msg("_handle_interrupt: compare irq id to addr : 0x%016lx ?= 0x%016lx", irq->id, addr);
	  if ( irq->id == addr ) {
	    break;
	  }
	  irq = irq->_next;
	}
	if ( irq == NULL ) {
	  warn_msg( "_handle_interrupt: no matching irqs allocated in this application" );
	  return OCXL_NO_IRQ;
	}

	// we have the matching irq pointer

	// Only track a single interrupt at a time
	// but what about a second afu_interrupt to a different irq address?  
	// should that be saved or coalecsed?
	// this code would coalesce them
	pthread_mutex_lock(&(afu->event_lock));
	i = 0;
	while (afu->events[i] != NULL) {
		if (afu->events[i]->type == OCXL_EVENT_IRQ) {
			// we could search deeper here to see if this event is for the
			// incoming irq.  if it is, increment count and return, if not, check the next event
			pthread_mutex_unlock(&(afu->event_lock));
			return 0;
		}
		++i;
	}
	assert(i < EVENT_QUEUE_MAX);

	afu->events[i] = (ocxl_event *)calloc(1, sizeof( ocxl_event ) );
	afu->events[i]->type = OCXL_EVENT_IRQ;
	//afu->events[i]->header.size = size;
	//afu->events[i]->header.process_element = afu->context; // might not need this
	afu->events[i]->irq.irq = irq->irq;  // which came in and matched irq
	afu->events[i]->irq.handle = addr;  // which came in and matched irq
	afu->events[i]->irq.count = 1;  
	// should we store data from an interrupt d at the info pointer?
	// afu->events[i]->irq.flags = cmd_flag;
	// notice we don't put ddata anywhere - that is because we don't have a place for it in Power ISA's interrupt scheme

	do {
		i = write(afu->pipe[1], &(afu->events[i]->type), 1);
	}
	while ((i == 0) || (errno == EINTR));
	pthread_mutex_unlock(&(afu->event_lock));
	return i;
}


static void _handle_read(struct ocxl_afu *afu, uint64_t addr, uint16_t size)
{
	uint8_t buffer[MAX_LINE_CHARS];

	DPRINTF("_handle_read: addr @ 0x%016" PRIx64 ", size = %d\n", addr, size);
	if (!afu)
		fatal_msg("NULL afu passed to libocxl.c:_handle_read");
	if (!_testmemaddr((uint8_t *) addr)) {
		if (_handle_dsi(afu, addr) < 0) {
			perror("DSI Failure");
			return;
		}
		warn_msg("READ from invalid addr @ 0x%016" PRIx64, addr);
		buffer[0] = (uint8_t) OCSE_MEM_FAILURE;
		if (put_bytes_silent(afu->fd, 1, buffer) != 1) {
			afu->opened = 0;
			afu->attached = 0;
		}
		return;
	}
	buffer[0] = OCSE_MEM_SUCCESS;
	memcpy(&(buffer[1]), (void *)addr, size);
	if (put_bytes_silent(afu->fd, size + 1, buffer) != size + 1) {
		afu->opened = 0;
		afu->attached = 0;
	}
	DPRINTF("READ from addr @ 0x%016" PRIx64 "\n", addr);
}

static void _handle_write_be(struct ocxl_afu *afu, uint64_t addr, uint16_t size,
			     uint8_t * data, uint64_t be)
{
        int i;
        uint8_t buffer;
	uint64_t enable;
	uint64_t be_copy;

	if (!afu)
		fatal_msg("NULL afu passed to libocxl.c:_handle_write_be");
	if (!_testmemaddr((uint8_t *) addr)) {
		if (_handle_dsi(afu, addr) < 0) {
			perror("DSI Failure");
			return;
		}
		warn_msg("WRITE to invalid addr @ 0x%016" PRIx64, addr);
		buffer = OCSE_MEM_FAILURE;
		if (put_bytes_silent(afu->fd, 1, &buffer) != 1) {
			afu->opened = 0;
			afu->attached = 0;
		}
		return;
	}

	// we'll have to loop through data byte by byte
	// and if the corresponding bit of be is on, 
	// write the data byte to the address offset by the loop index

	be_copy = be;

	for ( i=0; i<64; i++ ) {
	        enable = be_copy && 0x0000000000000001; // mask everything but bit 0
		if (enable) {
		          *((char *)addr + i) = data[i];  // add i to addr and deref???
		}
		be_copy = be_copy >> 1; // shift be_copy right 1 bit.
	}
	
	buffer = OCSE_MEM_SUCCESS;
	if (put_bytes_silent(afu->fd, 1, &buffer) != 1) {
		afu->opened = 0;
		afu->attached = 0;
	}
	DPRINTF("WRITE to addr @ 0x%016" PRIx64 "\n", addr);
}

static void _handle_write(struct ocxl_afu *afu, uint64_t addr, uint16_t size,
			  uint8_t * data)
{
	uint8_t buffer;

	if (!afu)
		fatal_msg("NULL afu passed to libocxl.c:_handle_write");
	if (!_testmemaddr((uint8_t *) addr)) {
		if (_handle_dsi(afu, addr) < 0) {
			perror("DSI Failure");
			return;
		}
		warn_msg("WRITE to invalid addr @ 0x%016" PRIx64, addr);
		buffer = OCSE_MEM_FAILURE;
		if (put_bytes_silent(afu->fd, 1, &buffer) != 1) {
			afu->opened = 0;
			afu->attached = 0;
		}
		return;
	}
	memcpy((void *)addr, data, size);
	buffer = OCSE_MEM_SUCCESS;
	if (put_bytes_silent(afu->fd, 1, &buffer) != 1) {
		afu->opened = 0;
		afu->attached = 0;
	}
	DPRINTF("WRITE to addr @ 0x%016" PRIx64 "\n", addr);
}

static void _handle_touch(struct ocxl_afu *afu, uint64_t addr, uint8_t function_code, uint8_t cmd_pg_size)
{
	uint8_t buffer;
// TODO check pg size; decide if to fail cmd for various other reasons and send back a fail resp code
	if (!afu)
		fatal_msg("NULL afu passed to libocxl.c:_handle_touch");
	if (!_testmemaddr((uint8_t *) addr)) {
		if (_handle_dsi(afu, addr) < 0) {
			perror("DSI Failure");
			return;
		}
		warn_msg("TOUCH of invalid addr @ 0x%016" PRIx64, addr);
		buffer = (uint8_t) OCSE_MEM_FAILURE;
		if (put_bytes_silent(afu->fd, 1, &buffer) != 1) {
			afu->opened = 0;
			afu->attached = 0;
		}
		return;
	}
	buffer = OCSE_MEM_SUCCESS;
	if (put_bytes_silent(afu->fd, 1, &buffer) != 1) {
		afu->opened = 0;
		afu->attached = 0;
	}
	DPRINTF("TOUCH of addr @ 0x%016" PRIx64 "\n", addr);
}

static void _handle_ack(struct ocxl_afu *afu)
{
	uint8_t data[sizeof(uint64_t)];
	uint8_t resp_code;

	if (!afu)
		fatal_msg("NULL afu passed to libocxl.c:_handle_ack");
	DPRINTF("MMIO ACK\n");

	if (get_bytes_silent(afu->fd, 1, &resp_code, 1000, 0) < 0) {
		warn_msg("Socket failure getting resp_code");
		_all_idle(afu);
	} 

	if (resp_code !=0) // TODO update this to handle resp code retry requests
		warn_msg ("handle_ack: AFU sent RD or WR FAILED response code = 0x%d ", resp_code);

	if ((afu->mmio.type == OCSE_MMIO_MAP) | (afu->mmio.type == OCSE_GLOBAL_MMIO_MAP) ) {
	  afu->mmios[afu->mmio_count].afu = afu;
	  if (afu->mmio.type == OCSE_GLOBAL_MMIO_MAP) {
	    afu->mmios[afu->mmio_count].type = OCXL_GLOBAL_MMIO;
	    afu->mmios[afu->mmio_count].start = afu->global_mmio.start;
	    afu->mmios[afu->mmio_count].length = afu->global_mmio.length;
	  } else {
	  afu->mmios[afu->mmio_count].type = OCXL_PER_PASID_MMIO;
	    afu->mmios[afu->mmio_count].start = afu->per_pasid_mmio.start;
	    afu->mmios[afu->mmio_count].length = afu->per_pasid_mmio.length;
	  }
	}

	if ((afu->mmio.type == OCSE_MMIO_READ64) | (afu->mmio.type == OCSE_GLOBAL_MMIO_READ64) ) {
		if (get_bytes_silent(afu->fd, sizeof(uint64_t), data, 1000, 0) < 0) {
			warn_msg("Socket failure getting MMIO Ack");
			_all_idle(afu);
			afu->mmio.data = 0xFEEDB00FFEEDB00FL;
		} else {
			memcpy(&(afu->mmio.data), data, sizeof(uint64_t));
			afu->mmio.data = ntohll(afu->mmio.data);
		}
	}

	if ((afu->mmio.type == OCSE_MMIO_READ32) | (afu->mmio.type == OCSE_GLOBAL_MMIO_READ32)) {
		if (get_bytes_silent(afu->fd, sizeof(uint32_t), data, 1000, 0) < 0) {
			warn_msg("Socket failure getting MMIO Read 32 data");
			afu->mmio.data = 0xFEEDB00FL;
			_all_idle(afu);
		} else {
			memcpy(&(afu->mmio.data), data, sizeof(uint32_t));
			debug_msg("KEM:0x%08x", afu->mmio.data);
			afu->mmio.data = ntohl(afu->mmio.data);
			debug_msg("KEM:0x%08x", afu->mmio.data);
		}
	}
	afu->mmio.state = LIBOCXL_REQ_IDLE;
}


static void _handle_DMO_OPs(struct ocxl_afu *afu, uint8_t amo_op, uint8_t op_size, uint64_t addr,
			  uint8_t function_code, uint64_t op1, uint64_t op2, uint8_t cmd_endian)
{

	uint8_t atomic_op;
	uint8_t atomic_le;
	uint8_t buffer;
	uint8_t wbuffer[9];
	uint32_t lvalue, op_A, op_1, op_2;
	uint64_t llvalue, op_Al, op_1l, op_2l;
	int op_ptr;
	char wb;

	if (!afu)
		fatal_msg("NULL afu passed to libocxl.c:_handle_DMO_OPs");
	if (!_testmemaddr((uint8_t *) addr)) {
		if (_handle_dsi(afu, addr) < 0) {
			perror("DSI Failure");
			return;
		}
		warn_msg("READ from invalid addr @ 0x%016" PRIx64 "\n", addr);
		return;
	}
	
	// Size is now a uint16_t and it represents the size of the data buffer
	//  size = 4 means single op and op_size = 4
	//  size = 8 could mean single op & op_size=8 OR two ops and op_size = 4
	//  size = 16 means two ops and op_size = 8
	// Need to pull ops out of buffer that got passed in 
	// If we determine op size, can create that and might make it easier for porting existing code
	// lgt and possibly the endian hint - not coded yet
	op_ptr = (int) (addr & 0x000000000000000c);
	// at this point, op1 and op2 are memcpy's of the data that sent over ddata
	// no byte swapping has taken place, however, we have stored them here as little endian 64 bit ints
	// if we use int ops, we'll get defacto byte swapping as we go.  that might not be what we want
	switch (op_ptr) {
		case 0x0:
			if ((op_size == 4) && (amo_op != OCSE_AMO_RD)) { // only amo_wr & amo_rw have immediate data
			        // OP1 is in ((__u8 *)(&op1))[0 to 3]
			        // OP2 is in ((__u8 *)(&op2))[0 to 3]
			        // don't shift as the 32bits we want are already le on the left,
			        // so the cast will grab the correct end
 			        // op_1 = (uint32_t) op1;// (op1 >> 32);
				// op_2 = (uint32_t) op2;// (op2 >> 32);
				memcpy( (void *)&op_1, (void *)&op1, op_size);
				memcpy( (void *)&op_2, (void *)&op2, op_size);
				// printf(" case 0: op_1 is %08"PRIx32 "\n", op_1);
				// printf(" case 0: op_2 is %08"PRIx32 "\n", op_2);
			} else if ((op_size == 8) && (amo_op != OCSE_AMO_RD)) {
				op_1l = op1;
				op_2l = op2;
				// printf(" case 0: op_1l is %016"PRIx64 "\n", op_1l);
			}
			break;
		case 0x4:
			if ((op_size == 4) && (amo_op != OCSE_AMO_RD)) { // only amo_wr & amo_rw have immediate data
			        // OP1 is in (__u8 *)(&op1)[4 to 7]
			        // OP2 is in (__u8 *)(&op2)[4 to 7]
			        // if the ops are really be, we have to handle them differently
			        // I think we should switch to memcpy to extract the data...
			        // the below  worked because the mcp afu replicated the ops,
			        // architecturally, it is not correct, but how to change it?
				// op_1 = (uint32_t) op1;
				// op_2 = (uint32_t) op2;
				memcpy( (void *)&op_1, (void *)&op1 + 4, op_size);
				memcpy( (void *)&op_2, (void *)&op2 + 4, op_size);
				// printf(" case 4: op_1 is %08"PRIx32 "\n", op_1);
				// printf(" case 4: op_2 is %08"PRIx32 "\n", op_2);
			} else if (op_size == 8) {
				DPRINTF("INVALID op_size  0x%x for  addr  0x%016" PRIx64 "\n", op_size, addr);
				buffer = (uint8_t) OCSE_MEM_FAILURE;
				if (put_bytes_silent(afu->fd, 1, &buffer) != 1) {
					afu->opened = 0;
					afu->attached = 0;
				}
				return;
			}
			break;
		case 0x8:
			if ((op_size == 4) && (amo_op != OCSE_AMO_RD)) { // only amo_wr & amo_rw have immediate data
			        // OP1 is in (__u8 *)(&op2)[0 to 3] !!!
			        // OP2 is in (__u8 *)(&op1)[0 to 3] !!!
				// op_1 = (uint32_t) (op1 >>32);
				// op_2 = (uint32_t) (op2 >> 32);
				memcpy( (void *)&op_1, (void *)&op1, op_size);
				memcpy( (void *)&op_2, (void *)&op2, op_size);
				// printf(" case 8: op_1 is %08"PRIx32 "\n", op_1);
				// printf(" case 8: op_2 is %08"PRIx32 "\n", op_2);
			} else if ((op_size == 8) && (amo_op != OCSE_AMO_RD)) {
				op_1l = op2;
				op_2l = op1;
	                        // printf(" case 8: op_1l is %016"PRIx64 "\n", op_1l);
			}
			break;
		case 0xc:
			if ((op_size == 4) && (amo_op != OCSE_AMO_RD)) { // only amo_wr & amo_rw have immediate data
			        // OP1 is in (__u8 *)(&op2)[4 to 7] !!!
			        // OP2 is in (__u8 *)(&op1)[4 to 7] !!!
				// op_1 = (uint32_t) op2;
				// op_2 = (uint32_t) op1;
				memcpy( (void *)&op_1, (void *)&op2 + 4, op_size);
				memcpy( (void *)&op_2, (void *)&op1 + 4, op_size);
				// printf(" case c: op_1 is %08"PRIx32 "\n", op_1);
				// printf(" case c: op_2 is %08"PRIx32 "\n", op_2);
			} else if (op_size == 8) {
				DPRINTF("INVALID op_size  0x%x for  addr  0x%016" PRIx64 "\n", op_size, addr);
				buffer = (uint8_t) OCSE_MEM_FAILURE;
				if (put_bytes_silent(afu->fd, 1, &buffer) != 1) {
					afu->opened = 0;
					afu->attached = 0;
				}
				return;
			}
			break;
		default:
			warn_msg("received invalid value op_ptr value of 0x%x ", op_ptr);
			break;
	}

	atomic_op = function_code;
	if (cmd_endian == 0)
		atomic_le = 1;
	else
	    atomic_le = 0;
	//cmd_endian == 0 when afu is LE, our old logic needs atomic_le == 1 for LE
	// if atomic_le == 1, afu is le, so no data issues (ocse is always le).
	// if atomic_le == 0, we have to swap op1/op2 data before ops, and also swap
	// data returned by fetches
	
	debug_msg("_handle_DMO_OPs:  atomic_op = 0x%2x and atomic_le = 0x%x ", atomic_op, atomic_le);

	DPRINTF("READ from addr @ 0x%016" PRIx64 "\n", addr);
	if (op_size == 0x4) {
		memcpy((char *) &lvalue, (void *)addr, op_size);
		op_A = (uint32_t)(lvalue);
	        debug_msg("op_A is %08"PRIx32 " and op_1 is %08"PRIx32 , op_A, op_1);
		if (atomic_le == 0) {
			op_1 = ntohl(op_1);
			op_2 = ntohl(op_2);
		}
	} else if (op_size == 0x8) {

		memcpy((char *) &llvalue, (void *)addr, op_size);
		op_Al = (uint64_t)(llvalue);
		if (atomic_le == 0) {
			op_1l = ntohll(op_1l);
			op_2l = ntohll(op_2l);
		}
	        debug_msg("op_Al is %016"PRIx64 " and op_1l is %016"PRIx64 , op_Al, op_1l);
	        debug_msg("llvalue read from location -> by addr is %016" PRIx64 " and addr is 0x%016" PRIx64 , llvalue, addr);
	} else // need else error bc only valid sizes are 4 or 8
		warn_msg("unsupported op_size of 0x%2x \n", op_size);

	switch (atomic_op) {
			case AMO_WRMWF_ADD:
				if  (op_size == 4) {
				debug_msg("ADD %08"PRIx32" to %08"PRIx32 " store it & only return op_A for amo_rw ", op_A, op_1);
					op_1 += op_A;
					wb = 1;
				} else {
				debug_msg("ADD %016"PRIx64" to %016"PRIx64 " store it & return op_Al for amo_rw ", op_Al, op_1l);
					op_1l += op_Al;
					wb = 2;
				}
				if (amo_op == OCSE_AMO_WR)
					wb = 0;
				break;
			case AMO_WRMWF_XOR:
				if  (op_size == 4) {
				debug_msg("XOR %08"PRIx32" with %08"PRIx32 " store it & return op_A  for amo_rw", op_A, op_1);
					op_1 ^= op_A;
					wb = 1;
				} else {
				debug_msg("XOR %016"PRIx64" with %016"PRIx64 " store it & return op_Al for amo_rw ", op_Al, op_1l);
					op_1l ^= op_Al;
					wb = 2;
				}
				if (amo_op == OCSE_AMO_WR)
					wb = 0;
				break;
			case AMO_WRMWF_OR:
				if  (op_size == 4) {
				debug_msg("OR %08"PRIx32" with %08"PRIx32 " store it & return op_A  for amo_rw", op_A, op_1);
					op_1 |= op_A;
					wb = 1;
				} else {
				debug_msg("OR %016"PRIx64" with %016"PRIx64 " store it & return op_Al for amo_rw ", op_Al, op_1l);
					op_1l |= op_Al;
					wb = 2;
				}
				if (amo_op == OCSE_AMO_WR)
					wb = 0;
				break;
			case AMO_WRMWF_AND:
				if  (op_size == 4) {
				debug_msg("AND %08"PRIx32" with %08"PRIx32 " store it & return op_A for amo_rw ", op_A, op_1);
					op_1 &= op_A;
					wb = 1;
				} else {
				debug_msg("AND %016"PRIx64" with %016"PRIx64 " store it & return op_Al for amo_rw ", op_Al, op_1l);
					op_1l &= op_Al;
					wb = 2;
				}
				if (amo_op == OCSE_AMO_WR)
					wb = 0;
				break;
			case AMO_WRMWF_CAS_MAX_U:
				if  (op_size == 4) {
				debug_msg("UNSIGNED COMPARE %08"PRIx32" with %08"PRIx32 " , store larger & return op_A for amo_rw ", op_A, op_1);
					if (op_A > op_1)
						op_1 = op_A;
					wb = 1;
				} else {
				debug_msg("UNSIGNED COMPARE %016"PRIx64" with %016"PRIx64 " , store larger & return op_Al for amo_rw  ", op_Al, op_1l);
					if (op_Al > op_1l)
						op_1l = op_Al;
					wb = 2;
				}
				if (amo_op == OCSE_AMO_WR)
					wb = 0;
				break;
			case AMO_WRMWF_CAS_MAX_S:
				// sign extend op_A and op_1 and then cast as int and do comparison
				if (op_size == 4) {
					op_A = sign_extend(op_A);
					op_1 = sign_extend(op_1);
				debug_msg("SIGNED COMPARE %08"PRIx32" with %08"PRIx32 " store larger & return op_A for amo_rw ", op_A, op_1);
					if ((int32_t)op_A > (int32_t)op_1)
						op_1 = op_A;
					wb = 1;
				} else {
					op_Al = sign_extend64(op_Al);
					op_1l = sign_extend64(op_1l);
				debug_msg("SIGNED COMPARE %016"PRIx64" with %016"PRIx64 " store larger & return op_Al for amo_rw ", op_Al, op_1l);
					if ((int64_t)op_Al > (int64_t)op_1l)
						op_1l = op_Al;
					wb = 2;
				}
				if (amo_op == OCSE_AMO_WR)
					wb = 0;
				break;
			case AMO_WRMWF_CAS_MIN_U:
				if  (op_size == 4) {
				debug_msg("UNSIGNED COMPARE %08"PRIx32" with %08"PRIx32 " store smaller & return op_A for amo_rw ", op_A, op_1);
					if (op_A < op_1)
						op_1 = op_A;
					wb = 1;
				} else {
				debug_msg("UNSIGNED COMPARE %016"PRIx64" with %016"PRIx64 " store smaller & return op_Al for amo_rw ", op_Al, op_1l);
					if (op_Al < op_1l)
						op_1l = op_Al;
					wb = 2;
				}
				if (amo_op == OCSE_AMO_WR)
					wb = 0;
				break;
			case AMO_WRMWF_CAS_MIN_S:
				if (op_size == 4) {
					op_A = sign_extend(op_A);
					op_1 = sign_extend(op_1);
				debug_msg("SIGNED COMPARE %08"PRIx32" with %08"PRIx32 " store smaller & return op_A for amo_rw ", op_A, op_1);
					if ((int32_t)op_A < (int32_t)op_1)
						op_1 = op_A;
					wb = 1;
				} else {
					op_Al = sign_extend64(op_Al);
					op_1l = sign_extend64(op_1l);
				debug_msg("SIGNED COMPARE %016"PRIx64" with %016"PRIx64 " store smaller & return op_Al for amo_rw ", op_Al, op_1l);
					if ((int64_t)op_Al < (int64_t)op_1l)
						op_1l = op_Al;
					wb = 2;
				}
				if (amo_op == OCSE_AMO_WR)
					wb = 0;
				break;
			case AMO_ARMWF_CAS_U:
				if ((amo_op == OCSE_AMO_WR) || (amo_op == OCSE_AMO_RD)) {
					info_msg("INVALID FUNCTION CODE FOR AMO_WR or AMO_RD - treated as NOP \n");
					wb = 0; 
					break; }
				if  (op_size == 4) {
				debug_msg("COMPARE & SWAP  %08"PRIx32" with %08"PRIx32 " ,store op_2 & return op_A ", op_A, op_1);
					op_1 = op_2;
					wb = 1;
				} else {
				debug_msg("COMPARE & SWAP  %016"PRIx64" with %016"PRIx64 " ,store op_2l & return op_Al ", op_Al, op_1l);
					op_1l = op_2l;
					wb = 2;
				}
				break;
			case AMO_ARMWF_CAS_E:
				if ((amo_op == OCSE_AMO_WR) || (amo_op == OCSE_AMO_RD)) {
					info_msg("INVALID FUNCTION CODE FOR AMO_WR or AMO_RD - treated as NOP \n");
					wb = 0; 
					break; }
				if  (op_size == 4) {
				debug_msg("COMPARE & SWAP == %08"PRIx32" with %08"PRIx32 " ,if true store op_2 & return op_A ", op_A, op_1);
					if (op_A == op_1)
						op_1 = op_2;
					else
						op_1 = op_A;
					wb = 1;
				} else {
				debug_msg("COMPARE & SWAP == %016"PRIx64" with %016"PRIx64 " ,if true store op_2l & return op_Al ", op_Al, op_1l);
					if (op_Al == op_1l)
						op_1l = op_2l;
					else
						op_1l = op_Al;
					wb = 2;
				}
				if (amo_op == OCSE_AMO_WR)
					wb = 0;
				break;
			case AMO_ARMWF_CAS_NE: //0x0a 
				if ((amo_op == OCSE_AMO_WR) || (amo_op == OCSE_AMO_RD)) {
					info_msg("INVALID FUNCTION CODE FOR AMO_WR or AMO_RD - treated as NOP \n");
					wb = 0; 
					break; }
				if  (op_size == 4) {
				debug_msg("COMPARE & SWAP != %08"PRIx32" with %08"PRIx32 " ,if true, store op_2 & return op_A ", op_A, op_1);
					if (op_A != op_1)
						op_1 = op_2;
					else
						op_1 = op_A;
					wb = 1;
				} else {
				debug_msg("COMPARE & SWAP != %016"PRIx64" with %016"PRIx64 " ,if true, store op_2l & return op_Al ", op_Al, op_1l);
					if (op_Al != op_1l)
						op_1l = op_2l;
					else
						op_1l = op_Al;
					wb = 2;
				}
				if (amo_op == OCSE_AMO_WR)
					wb = 0;
				break;
			case AMO_ARMWF_INC_B: //0xc0
			//case AMO_W_CAS_T:
				if (amo_op == OCSE_AMO_RW)  {
					info_msg("INVALID FUNCTION CODE FOR AMO_RW - treated as NOP \n");
					wb = 0; 
					break; }
				if (amo_op == OCSE_AMO_WR) { //this is the amo_wr store & compare twin

					if  (op_size == 4) {
						memcpy((char *) &lvalue, (void *)addr+4, op_size);
						op_2 = (uint32_t)(lvalue);
					debug_msg("STORE TWIN compare %08"PRIx32" with %08"PRIx32 ", if == store op_1 to both locations", op_A, op_2);
						if (op_A == op_2)
							op_2 = op_1;
						else
							op_1 = op_A;
						wb = 0;
					} else {
						memcpy((char *) &llvalue, (void *)addr+8, op_size);
						op_2l = (uint64_t)(llvalue);
					debug_msg("STORE TWIN compare %016"PRIx64" with %016"PRIx64 ", if == store op_1l to both locations", op_Al, op_2l);
						if (op_Al == op_2l)
							op_2l = op_1l;
						else
							op_1l = op_Al;
						wb = 0;
					}
					break;
				   }
				if  (op_size == 4) {
					memcpy((char *) &lvalue, (void *)addr+4, op_size);
					op_1 = (uint32_t)(lvalue);
				debug_msg("COMPARE & INC Bounded %08"PRIx32" with %08"PRIx32 ", if !=, inc op_A, ret orig op_A, else..", op_A, op_1);
					if (op_A != op_1)
						op_1 = op_A +1;
					else {
						op_1 = op_A;
						op_A = MIN_INT32;
						//op_A = (1 << 31);
					     }
					wb = 1;
				} else {
					memcpy((char *) &llvalue, (void *)addr+8, op_size);
					op_1l = (uint64_t)(llvalue);
				debug_msg("COMPARE & INC Bounded %016"PRIx64" with %016"PRIx64 ", if !=, inc op_A, ret orig op_a, else..", op_Al, op_1l);
					if (op_Al != op_1l)
						op_1l = op_Al +1;
					else  {
						op_1l = op_Al;
						op_Al = MIN_INT64;
						//op_Al = (1ULL << 63);
					      }
					wb = 2;
				}
				break;
			case AMO_ARMWF_INC_E:
				if ((amo_op == OCSE_AMO_WR) || (amo_op == OCSE_AMO_RW)) {
					info_msg("INVALID FUNCTION CODE FOR AMO_WR or AMO_RW - treated as NOP \n");
					wb = 0; 
					break; }
				if  (op_size == 4) {
					memcpy((char *) &lvalue, (void *)addr+4, op_size);
					op_1 = (uint32_t)(lvalue);
				debug_msg("COMPARE & INC Equal %08"PRIx32" with %08"PRIx32 ", if =, inc op_A, ret orig op_A, else..", op_A, op_1);
					if (op_A == op_1)
						op_1 = op_A +1;
					else   {
						op_1 = op_A;
						op_A = MIN_INT32;
						//op_A = (1 << 31);
					       }
					wb = 1;
				} else {
					memcpy((char *) &llvalue, (void *)addr+8, op_size);
					op_1l = (uint64_t)(llvalue);
				debug_msg("COMPARE & INC Equal %016"PRIx64" with %016"PRIx64 ", if =, inc op_A, ret orig op_a, else..", op_Al, op_1l);
					if (op_Al == op_1l)
						op_1l = op_A +1;
					else    {
						op_1l = op_Al;
						op_Al = MIN_INT64;
						//op_Al = (int64_t) (1ULL <<63);
						}
					wb = 2;
				}
				break;
			case AMO_ARMWF_DEC_B:
				if ((amo_op == OCSE_AMO_WR) || (amo_op == OCSE_AMO_RW)) {
					info_msg("INVALID FUNCTION CODE FOR AMO_WR or AMO_RW - treated as NOP \n");
					wb = 0; 
					break; }
				if  (op_size == 4) {
					memcpy((char *) &lvalue, (void *)addr-4, op_size);
					op_1 = (uint32_t)(lvalue);
				debug_msg("COMPARE & DEC Bounded %08"PRIx32" with %08"PRIx32 ", if != dec op_A, ret orig op_A, else..", op_A, op_1);
					if (op_A != op_1)
						op_1 = op_A -1;
					else  {
						op_1 = op_A;
						op_A = MIN_INT32;
						//op_A = (1 << 31);
					      }
					wb = 1;
				} else {
					memcpy((char *) &llvalue, (void *)addr-8, op_size);
					op_1l = (uint64_t)(llvalue);
				debug_msg("COMPARE & DEC Bounded %016"PRIx64" with %016"PRIx64 ", if !=, dec op_A, ret orig op_a, else..", op_Al, op_1l);
					if (op_Al != op_1l)
						op_1l = op_Al -1;
					else   {
						op_1l = op_Al;
						op_Al = MIN_INT64;
						//op_Al = (1ULL << 63);
					       }
					wb = 2;
				}
				break;
			default:
				wb = 0xf;
				warn_msg("Unsupported AMO command 0x%04x", atomic_op);
				break;
			}
	// every VALID op has a write to store something to the original EA, unless STORE TWIN !=
	if (wb != 0xf) {
		if (op_size == 4) {
			memcpy ((void *)addr, &op_1, op_size);
			DPRINTF("WRITE to addr @ 0x%016" PRIx64 " with results of 0x%08" PRIX32 " \n", addr, op_1);
			// if this was STORE TWIN, write op_2 to addr+4
			if ((atomic_op) == AMO_W_CAS_T) {
				memcpy ((void *)addr+4, &op_2, op_size);
				DPRINTF("WRITE to addr+4 @ 0x%016" PRIx64 " with results of 0x%08" PRIX32 " \n", addr+4, op_2);
			}
		} else  {// only other supported size is 8
			memcpy ((void *)addr, &op_1l, op_size);
			DPRINTF("WRITE to addr @ 0x%016" PRIx64 " with results of 0x%016" PRIX64 "\n", addr, op_1l);
			// if this was STORE TWIN, write op_2l to addr+8
			if ((atomic_op) == AMO_W_CAS_T) {
				memcpy ((void *)addr+8, &op_2l, op_size);
				DPRINTF("WRITE to addr+8 @ 0x%016" PRIx64 " with results of 0x%016" PRIX64 " \n", addr+8, op_2l);
			}
		}
	}

	// only AMO_ARMWF_* commands return back original data from EA, otherwise just MEM ACK
	switch (wb)  {
			case 0:
				buffer = OCSE_MEM_SUCCESS;
				if (put_bytes_silent(afu->fd, 1, &buffer) != 1) {
					afu->opened = 0;
					afu->attached = 0;
				}
				break;
			case 1:
				wbuffer[0] = OCSE_MEM_SUCCESS;
				if (atomic_le == 0)
					op_A = htonl(op_A);
				memcpy(&(wbuffer[1]), (void *)&op_A, op_size);
				if (put_bytes_silent(afu->fd, op_size + 1, wbuffer) != op_size + 1) {
					afu->opened = 0;
					afu->attached = 0;
				}
				DPRINTF("READ from addr @ 0x%016" PRIx64 "\n", addr);
				break;
			case 2:
				wbuffer[0] = OCSE_MEM_SUCCESS;
				if (atomic_le == 0)
					op_Al = htonll(op_Al);
				memcpy(&(wbuffer[1]), (void *)&op_Al, op_size);
				if (put_bytes_silent(afu->fd, op_size + 1, wbuffer) != op_size + 1) {
					afu->opened = 0;
					afu->attached = 0;
				}
				DPRINTF("READ from addr @ 0x%016" PRIx64 "\n", addr);
				break;

			default:
				warn_msg("invalid wb! ");
				wb = 0;
				break;
			} 


}



static void _req_max_int(struct ocxl_afu *afu)
{
	uint8_t *buffer;
	int size;
	uint16_t value;

	if (!afu)
		fatal_msg("NULL afu passed to libocxl.c:_req_max_int");
	size = 1 + sizeof(uint16_t);
	buffer = (uint8_t *) malloc(size);
	buffer[0] = OCSE_MAX_INT;
	value = htons(afu->int_req.max);
	memcpy((char *)&(buffer[1]), (char *)&value, sizeof(uint16_t));
	if (put_bytes_silent(afu->fd, size, buffer) != size) {
		free(buffer);
		close_socket(&(afu->fd));
		afu->int_req.max = 0;
		_all_idle(afu);
		return;
	}
	free(buffer);
	afu->int_req.state = LIBOCXL_REQ_PENDING;
}

static void _ocse_attach(struct ocxl_afu *afu)
{
	uint8_t *buffer;
	// uint64_t *wed_ptr;
	int size;
	// int offset;

	if (!afu)
		fatal_msg("NULL afu passed to libocxl.c:_ocse_attach");
	size = 1; // + sizeof(uint64_t);
	buffer = (uint8_t *) malloc(size);
	buffer[0] = OCSE_ATTACH;
	// lgt - remove - offset = 1;
	// lgt - remove - wed_ptr = (uint64_t *) & (buffer[offset]);
	// lgt - remove - *wed_ptr = htonll(afu->attach.wed);
	if (put_bytes_silent(afu->fd, size, buffer) != size) {
		free(buffer);
		close_socket(&(afu->fd));
		afu->opened = 0;
		afu->attached = 0;
		afu->attach.state = LIBOCXL_REQ_IDLE;
		return;
	}
	free(buffer);
	afu->attach.state = LIBOCXL_REQ_PENDING;
}

static void _mmio_map(struct ocxl_afu *afu)
{
	uint8_t *buffer;
	//uint32_t *flags_ptr;
	//uint32_t flags;
	int size;

	if (!afu)
		fatal_msg("NULL afu passed to libocxl.c:_mmio_map");
	size = 1; // + sizeof(uint32_t);
	buffer = (uint8_t *) malloc(size);
	buffer[0] = afu->mmio.type;
	// flags = (uint32_t) afu->mmio.data;
	// flags_ptr = (uint32_t *) & (buffer[1]);
	// *flags_ptr = htonl(flags);
	if (put_bytes_silent(afu->fd, size, buffer) != size) {
		free(buffer);
		close_socket(&(afu->fd));
		afu->opened = 0;
		afu->attached = 0;
		afu->mmio.state = LIBOCXL_REQ_IDLE;
		return;
	}
	free(buffer);
	afu->mmio.state = LIBOCXL_REQ_PENDING;
}

static void _mmio_write64(struct ocxl_afu *afu)
{
	uint8_t *buffer;
	uint64_t data;
	uint32_t addr;
	int size, offset;

	if (!afu)
		fatal_msg("NULL afu passed to libocxl.c:_mmio_write64");
	size = 1 + sizeof(addr) + sizeof(data);
	buffer = (uint8_t *) malloc(size);
	buffer[0] = afu->mmio.type;
	offset = 1;
	addr = htonl(afu->mmio.addr);
	memcpy((char *)&(buffer[offset]), (char *)&addr, sizeof(addr));
	offset += sizeof(addr);
	data = htonll(afu->mmio.data);
	memcpy((char *)&(buffer[offset]), (char *)&data, sizeof(data));
	debug_msg( "_mmio_write64: type = %02x, offset = %08x", afu->mmio.type, afu->mmio.addr );
	if (put_bytes_silent(afu->fd, size, buffer) != size) {
		free(buffer);
		close_socket(&(afu->fd));
		afu->opened = 0;
		afu->attached = 0;
		afu->mmio.state = LIBOCXL_REQ_IDLE;
		return;
	}
	free(buffer);
	afu->mmio.state = LIBOCXL_REQ_PENDING;
}

static void _mmio_write32(struct ocxl_afu *afu)
{
	uint8_t *buffer;
	uint32_t data;
	uint32_t addr;
	int size, offset;

	if (!afu)
		fatal_msg("NULL afu passed to libocxl.c:_mmio_write32");
	size = 1 + sizeof(addr) + sizeof(data);
	buffer = (uint8_t *) malloc(size);
	buffer[0] = afu->mmio.type;
	offset = 1;
	addr = htonl(afu->mmio.addr);
	memcpy((char *)&(buffer[offset]), (char *)&addr, sizeof(addr));
	offset += sizeof(addr);
	data = htonl(afu->mmio.data);
	memcpy((char *)&(buffer[offset]), (char *)&data, sizeof(data));
	debug_msg( "_mmio_write32: type = %02x, offset = %08x", afu->mmio.type, afu->mmio.addr );
	if (put_bytes_silent(afu->fd, size, buffer) != size) {
		free(buffer);
		close_socket(&(afu->fd));
		afu->opened = 0;
		afu->attached = 0;
		afu->mmio.state = LIBOCXL_REQ_IDLE;
		return;
	}
	free(buffer);
	afu->mmio.state = LIBOCXL_REQ_PENDING;
}

static void _mmio_read(struct ocxl_afu *afu)
{
	uint8_t *buffer;
	uint32_t addr;
	int size, offset;

	if (!afu)
		fatal_msg("NULL afu passed to libocxl.c:_mmio_read");
	size = 1 + sizeof(addr);
	buffer = (uint8_t *) malloc(size);
	buffer[0] = afu->mmio.type;
	offset = 1;
	addr = htonl(afu->mmio.addr);
	memcpy((char *)&(buffer[offset]), (char *)&addr, sizeof(addr));
	debug_msg( "_mmio_read: type = %02x, offset = %08x", afu->mmio.type, afu->mmio.addr );
	if (put_bytes_silent(afu->fd, size, buffer) != size) {
	        warn_msg("_mmio_read: put_bytes_silent failed");
		free(buffer);
		close_socket(&(afu->fd));
		afu->opened = 0;
		afu->attached = 0;
		afu->mmio.state = LIBOCXL_REQ_IDLE;
		afu->mmio.data = 0xFEEDB00FFEEDB00FL;
		return;
	}
	free(buffer);
	afu->mmio.state = LIBOCXL_REQ_PENDING;
}

static void _mem_map(struct ocxl_afu *afu)
{
        // _mem_map doesn't really need to do anything for ocse...  the fact that we have a socket is enough
        // all the information we need is over in ocse already as it has gone through the config space

        if (!afu)
	      fatal_msg("NULL afu passed to libocxl.c:_mem_map");

	afu->mem.state = LIBOCXL_REQ_IDLE; // make pending if we really have to send something to ocse...
	return;
}

static void _mem_read(struct ocxl_afu *afu)
{
	uint8_t *buffer;
	int buffer_length;
	int buffer_offset;

	uint32_t offset;
	uint32_t size;

	debug_msg("_mem_read:");

	if (!afu)
		fatal_msg("NULL afu passed to libocxl.c:_mem_read");

	// buffer length = 1 byte for type, buffer remainder?, 4 bytes for offset, 4 bytes for size
	buffer_length = 1 + sizeof(offset) + sizeof(size);
	debug_msg("_mem_read: buffer length %d", buffer_length);
	buffer = (uint8_t *)malloc( buffer_length );

	debug_msg("_mem_read: buffer[0]");
	buffer[0] = afu->mem.type;

	buffer_offset = 1;
	debug_msg( "_mem_read: buffer[%d]", buffer_offset );
	offset = htonl(afu->mem.addr);
	memcpy( (char *)&(buffer[buffer_offset]), (char *)&offset, sizeof(offset));
	buffer_offset += sizeof(offset);

	debug_msg( "_mem_read: buffer[%d]", buffer_offset );
	size = htonl(afu->mem.size);
	memcpy((char *)&(buffer[buffer_offset]), (char *)&size, sizeof(size));

	if (put_bytes_silent(afu->fd, buffer_length, buffer) != buffer_length) {
		free(buffer);
		close_socket(&(afu->fd));
		afu->opened = 0;
		afu->attached = 0;
		afu->mem.state = LIBOCXL_REQ_IDLE;
		return;
	}

	free(buffer);
	afu->mem.state = LIBOCXL_REQ_PENDING;
}

static void _mem_write(struct ocxl_afu *afu)
{
	uint8_t *buffer;
	int buffer_length;
	int buffer_offset;

	uint32_t offset;
	uint32_t size;

	debug_msg("_mem_write:");

	if (!afu)
		fatal_msg("NULL afu passed to libocxl.c:_mem_write");

	// buffer length = 1 byte for type, buffer remainder?, 4 bytes for offset, n bytes for size, m bytes for data
	buffer_length = 1 + sizeof(offset) + sizeof(size) + afu->mem.size;
	debug_msg("_mem_write: buffer length %d", buffer_length);
	buffer = (uint8_t *)malloc( buffer_length );

	debug_msg("_mem_write: buffer[0]");
	buffer[0] = afu->mem.type;

	buffer_offset = 1;
	debug_msg( "_mem_write: buffer[%d]", buffer_offset );
	offset = htonl(afu->mem.addr); 
	memcpy( (char *)&(buffer[buffer_offset]), (char *)&offset, sizeof(offset));
	buffer_offset += sizeof(offset);

	debug_msg( "_mem_write: buffer[%d]", buffer_offset );
	size = htonl(afu->mem.size);
	memcpy((char *)&(buffer[buffer_offset]), (char *)&size, sizeof(size));
	buffer_offset += sizeof(size);

	// data = htonll(afu->mmio.data);
	debug_msg( "_mem_write: buffer[%d]", buffer_offset );
	memcpy( (char *)&(buffer[buffer_offset]), afu->mem.data, afu->mem.size );
	if (put_bytes_silent(afu->fd, buffer_length, buffer) != buffer_length) {
		free(buffer);
		close_socket(&(afu->fd));
		afu->opened = 0;
		afu->attached = 0;
		afu->mem.state = LIBOCXL_REQ_IDLE;
		return;
	}

	free(buffer);
	afu->mem.state = LIBOCXL_REQ_PENDING;
}

static void _mem_write_be(struct ocxl_afu *afu)
{
	uint8_t *buffer;
	int buffer_length;
	int buffer_offset;

	uint32_t offset;
	uint64_t be;

	debug_msg("_mem_write_be:");

	if (!afu)
		fatal_msg("NULL afu passed to libocxl.c:_mem_write");

	// buffer length = 1 byte for type, 4 bytes for offset, 8 bytes for be, 64 bytes for data
	buffer_length = 1 + sizeof(offset) + sizeof( be ) + afu->mem.size;
	debug_msg("_mem_write_be: buffer length %d", buffer_length);
	buffer = (uint8_t *)malloc( buffer_length );

	debug_msg("_mem_write_be: buffer[0]");
	buffer[0] = afu->mem.type;

	buffer_offset = 1;
	debug_msg( "_mem_write_be: buffer[%d]", buffer_offset );
	offset = htonl(afu->mem.addr);
	memcpy( (char *)&(buffer[buffer_offset]), (char *)&offset, sizeof(offset));
	buffer_offset += sizeof(offset);

	debug_msg( "_mem_write: buffer[%d]", buffer_offset );
	be = htonl(afu->mem.be);
	memcpy((char *)&(buffer[buffer_offset]), (char *)&be, sizeof(be));
	buffer_offset += sizeof(be);

	// data = htonll(afu->mmio.data);
	debug_msg( "_mem_write: buffer[%d]", buffer_offset );
	memcpy( (char *)&(buffer[buffer_offset]), afu->mem.data, afu->mem.size );
	if (put_bytes_silent(afu->fd, buffer_length, buffer) != buffer_length) {
		free(buffer);
		close_socket(&(afu->fd));
		afu->opened = 0;
		afu->attached = 0;
		afu->mem.state = LIBOCXL_REQ_IDLE;
		return;
	}

	free(buffer);
	afu->mem.state = LIBOCXL_REQ_PENDING;
}

static void _handle_mem_ack(struct ocxl_afu *afu)
{
	uint8_t resp_code;

	debug_msg( "_handle_mem_ack" );

	if (!afu)
		fatal_msg("NULL afu passed to libocxl.c:_handle_mem_ack");

	if (get_bytes_silent(afu->fd, 1, &resp_code, 1000, 0) < 0) {
		warn_msg("Socket failure getting resp_code");
		_all_idle(afu);
	} 
	if (resp_code !=0) // TODO update this to handle resp code retry requests
		error_msg ("handle_mem_ack: AFU sent RD or WR FAILED response code = 0x%d ", resp_code);
	if ( afu->mem.type == OCSE_LPC_READ ) {
	        // assuming it all worked, we already know the size in afu->mem.size
	        debug_msg( "_handle_mem_ack: getting %d bytes from socket", afu->mem.size );
		afu->mem.data = (uint8_t *)malloc( afu->mem.size );
		if (get_bytes_silent(afu->fd, afu->mem.size, afu->mem.data, 1000, 0) < 0) {
		      warn_msg("Socket failure getting MEM Ack data");
		      free( afu->mem.data );
		      _all_idle(afu);
		}
	}

	afu->mem.state = LIBOCXL_REQ_IDLE;
}


static void *_psl_loop(void *ptr)
{
	struct ocxl_afu *afu = (struct ocxl_afu *)ptr;
	uint8_t buffer[MAX_LINE_CHARS];
	uint8_t op_size, function_code, amo_op, cmd_endian, cmd_pg_size;
	uint64_t addr, wr_be;
	uint16_t size;
	uint8_t bvalue;
	uint16_t value;
	uint32_t lvalue;
	uint64_t llvalue;
	uint64_t op1, op2;
	int rc;
	int offset;

	if (!afu)
		fatal_msg("NULL afu passed to libocxl.c:_psl_loop");
	afu->opened = 1;

	while (afu->opened) {
		_delay_1ms();
		// Send any requests to OCSE over socket
		if (afu->int_req.state == LIBOCXL_REQ_REQUEST)
			_req_max_int(afu);
		if (afu->attach.state == LIBOCXL_REQ_REQUEST)
			_ocse_attach(afu);
		if (afu->mmio.state == LIBOCXL_REQ_REQUEST) {
			switch (afu->mmio.type) {
			case OCSE_MMIO_MAP:
			case OCSE_GLOBAL_MMIO_MAP:
				_mmio_map(afu);
				break;
			case OCSE_MMIO_WRITE64:
			case OCSE_GLOBAL_MMIO_WRITE64:
				_mmio_write64(afu);
				break;
			case OCSE_MMIO_WRITE32:
			case OCSE_GLOBAL_MMIO_WRITE32:
				_mmio_write32(afu);
				break;
			case OCSE_MMIO_READ64:
			case OCSE_MMIO_READ32:	
			case OCSE_GLOBAL_MMIO_READ64:
			case OCSE_GLOBAL_MMIO_READ32: /*fall through */
				_mmio_read(afu);
				break;
			default:
				break;
			}
		}
		if (afu->mem.state == LIBOCXL_REQ_REQUEST) {
			switch (afu->mem.type) {
			case OCSE_LPC_MAP:
				_mem_map(afu);
				break;
			case OCSE_LPC_WRITE:
				_mem_write(afu);
				break;
			case OCSE_LPC_WRITE_BE:
				_mem_write_be(afu);
				break;
			case OCSE_LPC_READ:
				_mem_read(afu);
				break;
			default:
				break;
			}
		}

		// Process socket input from OCSE
		rc = bytes_ready(afu->fd, 1000, 0);
		if (rc == 0)
			continue;
		if (rc < 0) {
			warn_msg("Socket failure testing bytes_ready");
			_all_idle(afu);
			break;
		}
		if (get_bytes_silent(afu->fd, 1, buffer, 1000, 0) < 0) {
			warn_msg("Socket failure getting OCL event");
			_all_idle(afu);
			break;
		}

		debug_msg("OCL EVENT = 0x%02x", buffer[0]);
		switch (buffer[0]) {
		case OCSE_OPEN:
			if (get_bytes_silent(afu->fd, 1, buffer, 1000, 0) < 0) {
				warn_msg("Socket failure getting OPEN context");
				_all_idle(afu);
				break;
			}
			afu->context = (uint16_t) buffer[0];
			afu->open.state = LIBOCXL_REQ_IDLE;
			break;
		case OCSE_ATTACH:
			afu->attach.state = LIBOCXL_REQ_IDLE;
			break;
		case OCSE_DETACH:
		        info_msg("detach response from ocse");
			afu->mapped = 0;
			afu->global_mapped = 0;
			afu->attached = 0;
			afu->opened = 0;
			afu->open.state = LIBOCXL_REQ_IDLE;
			afu->attach.state = LIBOCXL_REQ_IDLE;
			afu->mmio.state = LIBOCXL_REQ_IDLE;
			afu->mem.state = LIBOCXL_REQ_IDLE;
			afu->int_req.state = LIBOCXL_REQ_IDLE;
			break;
		case OCSE_MAX_INT:
			size = sizeof(uint16_t);
			if (get_bytes_silent(afu->fd, size, buffer, 1000, 0) <
			    0) {
				warn_msg
				    ("Socket failure getting max interrupt acknowledge");
				_all_idle(afu);
				break;
			}
			memcpy((char *)&value, (char *)buffer,
			       sizeof(uint16_t));
			// afu->irqs_max = ntohs(value);
			afu->int_req.state = LIBOCXL_REQ_IDLE;
			break;
		case OCSE_QUERY: {
		        size = 
			  sizeof(uint16_t) + // device_id
			  sizeof(uint16_t) + // vendor_id
			  sizeof(uint8_t)  + // afu_version_major
			  sizeof(uint8_t)  + // afu_version_minor
			  sizeof(uint64_t) + // global_mmio_offset
			  sizeof(uint32_t) + // global_mmio_size
			  sizeof(uint64_t) + // pp_mmio_offset
			  sizeof(uint32_t) + // pp_mmio_stride
			  sizeof(uint64_t) + // mem_base_address
			  sizeof(uint8_t)  ; // mem_size

			if (get_bytes_silent(afu->fd, size, buffer, 1000, 0) <
			    0) {
				warn_msg("Socket failure getting OCSE query");
				_all_idle(afu);
				break;
			}

			offset = 0;

                	memcpy((char *)&value, (char *)&(buffer[offset]), 2); // device_id
			afu->device_id = value;
			offset += sizeof(uint16_t);

                        memcpy((char *)&value, (char *)&(buffer[offset]), 2); // vendor_id
			afu->vendor_id = value;
			offset += sizeof(uint16_t);

                        memcpy((char *)&bvalue, (char *)&(buffer[offset]), 1); // afu_version_major
			afu->afu_version_major = bvalue;
			offset += sizeof(uint8_t);

                        memcpy((char *)&bvalue, (char *)&(buffer[offset]), 1); // afu_version_minor
			afu->afu_version_minor = bvalue;
			offset += sizeof(uint8_t);

			afu->global_mmio.type = OCXL_GLOBAL_MMIO;

                        memcpy((char *)&llvalue, (char *)&(buffer[offset]), 8); // global_mmio_offset
			// afu->global_mmio_offset = llvalue;
			afu->global_mmio.start = (char *)llvalue;
			offset += sizeof(uint64_t);

                        memcpy((char *)&lvalue, (char *)&(buffer[offset]), 4); // global_mmio_size
			// afu->global_mmio_size = lvalue;
			afu->global_mmio.length = lvalue;
			offset += sizeof(uint32_t);

			afu->per_pasid_mmio.type = OCXL_PER_PASID_MMIO;

                        memcpy((char *)&llvalue, (char *)&(buffer[offset]), 8); // pp_mmio_offset
			// afu->pp_mmio_offset = llvalue;
			afu->per_pasid_mmio.start = (char *)llvalue;
			offset += sizeof(uint64_t);

                        memcpy((char *)&lvalue, (char *)&(buffer[offset]), 4); // pp_mmio_stride
			// afu->pp_mmio_stride = lvalue;
			afu->per_pasid_mmio.length = lvalue;
			offset += sizeof(uint32_t);

			// we will only allow 2 mmio areas per attach.  one for global and the second for per pasid.
			// we will only allow 1 per pasid area per attach and it must be the full area for this pasid,
			// that is, the full stride.  and the offset is 0 from this pasid's (context) area

			afu->mmio_count = 0;
			afu->mmio_max = 2;
			
                        memcpy((char *)&llvalue, (char *)&(buffer[offset]), 8); // mem_base_address
			afu->mem_base_address = llvalue;
			offset += sizeof(uint64_t);

                        memcpy((char *)&bvalue, (char *)&(buffer[offset]), 1); // mem_size
			afu->mem_size = bvalue;
			offset += sizeof(uint8_t);

			break;
		}
		case OCSE_MEMORY_READ:
			DPRINTF("AFU MEMORY READ\n");
			if (get_bytes_silent(afu->fd, sizeof( size ), buffer, 1000, 0) < 0) {
				warn_msg
				    ("Socket failure getting memory read size");
				_all_idle(afu);
				break;
			}
			// size = (uint16_t *)buffer;
			memcpy( (char *)&size, buffer, sizeof( size ) );
			size = ntohs(size);
			DPRINTF( "of size=%d \n", size );
			if (get_bytes_silent(afu->fd, sizeof(uint64_t), buffer,
					     -1, 0) < 0) {
				warn_msg
				    ("Socket failure getting memory read addr");
				_all_idle(afu);
				break;
			}
			memcpy((char *)&addr, (char *)buffer, sizeof(uint64_t));
			addr = ntohll(addr);
			DPRINTF("from addr 0x%016" PRIx64 "\n", addr);
			_handle_read(afu, addr, size);
			break;
		case OCSE_MEMORY_WRITE:
			DPRINTF("AFU MEMORY WRITE\n");
			if (get_bytes_silent(afu->fd, sizeof( size ), buffer, 1000, 0) < 0) {
				warn_msg
				    ("Socket failure getting memory write size");
				_all_idle(afu);
				break;
			}
			//size = (uint16_t) buffer[0];
			memcpy( (char *)&size, buffer, sizeof( size ) );
			size = ntohs(size);
			DPRINTF( "of size=%d \n", size );
			if (get_bytes_silent(afu->fd, sizeof(uint64_t), buffer,
						 -1, 0) < 0) {
				warn_msg
				    ("Socket failure getting memory write addr");
				_all_idle(afu);
				break;
			}
			memcpy((char *)&addr, (char *)buffer, sizeof(uint64_t));
			addr = ntohll(addr);
			DPRINTF("to addr 0x%016" PRIx64 "\n", addr);
			if (get_bytes_silent(afu->fd, size, buffer, 1000, 0) <
			    0) {
				warn_msg
				    ("Socket failure getting memory write data");
				_all_idle(afu);
				break;
			}
			_handle_write(afu, addr, size, buffer);
			break;
		// add the case for ocse_memory_be_write
		// need to size, addr and data as above in ocse_memory_write
	        // and then need to get byte enable in manner similar to addr (maybe)
		case OCSE_WR_BE:
			DPRINTF("AFU MEMORY WRITE BE\n");
			if (get_bytes_silent(afu->fd, sizeof(size), buffer, 1000, 0) < 0) {
				warn_msg
				    ("Socket failure getting memory write be size");
				_all_idle(afu);
				break;
			}
			memcpy( (char *)&size, buffer, sizeof( size ) );
			size = ntohs(size);
			DPRINTF( "of size=%d \n", size );
			if (get_bytes_silent(afu->fd, sizeof(uint64_t), buffer,
					     -1, 0) < 0) {
				warn_msg
				    ("Socket failure getting memory write be addr");
				_all_idle(afu);
				break;
			}
			memcpy((char *)&addr, (char *)buffer, sizeof(uint64_t));
			addr = ntohll(addr);
			DPRINTF("to addr 0x%016" PRIx64 "\n", addr);
			if (get_bytes_silent(afu->fd, sizeof(uint64_t), buffer,
					     -1, 0) < 0) {
				warn_msg
				    ("Socket failure getting memory write be byte enable");
				_all_idle(afu);
				break;
			}
			memcpy((char *)&wr_be, (char *)buffer, sizeof(uint64_t));
			wr_be = ntohll(wr_be);
			DPRINTF("byte enable mask= 0x%016" PRIx64 "\n", wr_be);

			if (get_bytes_silent(afu->fd, size, buffer, 1000, 0) <
			    0) {
				warn_msg
				    ("Socket failure getting memory write data");
				_all_idle(afu);
				break;
			}
			_handle_write_be(afu, addr, size, buffer, wr_be);
			break;

		case OCSE_AMO_WR:
		case OCSE_AMO_RW:
			amo_op = buffer[0];
			if (amo_op == OCSE_AMO_WR)
				DPRINTF("AFU AMO_WRITE \n");
			else
				DPRINTF("AFU AMO__READ/WRITE\n");
			if (get_bytes_silent(afu->fd, sizeof(uint8_t), buffer, -1, 0) < 0) {
				warn_msg
				    ("Socket failure getting amo_wr or amo_rw size");
				_all_idle(afu);
				break;
			}
			memcpy( (char *)&op_size, buffer, sizeof( uint8_t ) );
			//memcpy( (char *)&size, buffer, sizeof( size ) );
			//size = ntohs(size);
			DPRINTF( "op_size=%d \n", op_size );
			if (get_bytes_silent(afu->fd, sizeof(uint64_t), buffer,
					     -1, 0) < 0) {
				warn_msg
				    ("Socket failure getting amo_wr or amo_rw addr");
				_all_idle(afu);
				break;
			}
			memcpy((char *)&addr, (char *)buffer, sizeof(uint64_t));
			addr = ntohll(addr);
			DPRINTF("to addr 0x%016" PRIx64 "\n", addr);
			if (get_bytes_silent(afu->fd, 18, buffer,
				     -1, 0) < 0) {
				warn_msg
			   	 ("Socket failure getting amo_wr or amo_rw cmd_flag, cmd_endian and op1/op2 data");
				_all_idle(afu);
				break;
			}
			memcpy( (char *)&function_code, &buffer[0], sizeof( function_code ) );
			DPRINTF("amo_wr or amo_rw cmd_flag= 0x%x\n", function_code);
			memcpy( (char *)&cmd_endian, &buffer[1], sizeof( cmd_endian ) );
			DPRINTF("amo_wr or amo_rw cmd_endian= 0x%x\n", cmd_endian);

			// TODO FIX THIS TO CORRECTLY EXTRACT OP_1 and OP_2 if needed !!!
			memcpy((char *)&op1, (char *)&buffer[2], sizeof(uint64_t));
			debug_msg("op1 bytes 1-8 are 0x%016" PRIx64, op1);
			//op1 = ntohll (op1);
			//printf("op1 bytes 1-8 are 0x%016" PRIx64 " \n", op1);
			memcpy((char *)&op2, (char *)&buffer[10], sizeof(uint64_t));
			debug_msg("op2 bytes 1-8 are 0x%016" PRIx64, op2);
			//op_size = (uint8_t) size;
			
			_handle_DMO_OPs(afu, amo_op, op_size, addr, function_code, op1, op2, cmd_endian);
			break;

		case OCSE_AMO_RD:
			DPRINTF("AFU AMO READ \n");
			amo_op = buffer[0];
			if (get_bytes_silent(afu->fd, sizeof(op_size), buffer, -1, 0) < 0) {
				warn_msg
				    ("Socket failure getting amo_rd size");
				_all_idle(afu);
				break;
			}
			memcpy( (char *)&op_size, buffer, sizeof( op_size ) );
			//memcpy( (char *)&size, buffer, sizeof( size ) );
			//size = ntohs(size);
		//	op_size = (uint8_t) size;
			DPRINTF( "op_size=%d \n", op_size );
			if (get_bytes_silent(afu->fd, sizeof(uint64_t), buffer,
					     -1, 0) < 0) {
				warn_msg
				    ("Socket failure getting amo_rd addr");
				_all_idle(afu);
				break;
			}
			memcpy((char *)&addr, (char *)buffer, sizeof(uint64_t));
			addr = ntohll(addr);
			DPRINTF("to addr 0x%016" PRIx64 "\n", addr);
			if (get_bytes_silent(afu->fd, 2, buffer,
					     -1, 0) < 0) {
				warn_msg
				    ("Socket failure getting amo_rd cmd_flag and cmd_endian");
				_all_idle(afu);
				break;
			}
			memcpy( (char *)&function_code, &buffer[0], sizeof( function_code ) );
			memcpy( (char *)&cmd_endian, &buffer[1], sizeof( cmd_endian ) );
			DPRINTF("amo_rd cmd_flag= 0x%x\n", function_code);
			DPRINTF("amo_rd cmd_endian= 0x%x\n", cmd_endian);

			_handle_DMO_OPs(afu, amo_op, op_size, addr, function_code, 0, 0, cmd_endian);
			break;


		case OCSE_MEMORY_TOUCH:
			DPRINTF("AFU XLATE TOUCH\n");
			if (get_bytes_silent(afu->fd, sizeof(uint64_t), buffer,
					     -1, 0) < 0) {
				warn_msg
				    ("Socket failure getting memory touch addr");
				_all_idle(afu);
				break;
			}
			memcpy((char *)&addr, (char *)buffer, sizeof(uint64_t));
			addr = ntohll(addr);
			DPRINTF("to addr 0x%016" PRIx64 "\n", addr);
			if (get_bytes_silent(afu->fd, 2, buffer,
					     -1, 0) < 0) {
				warn_msg
				    ("Socket failure getting cmd_flag and cmd_pg_size");
				_all_idle(afu);
				break;
			}
			memcpy( (char *)&function_code, &buffer[0], sizeof( function_code ) );
			memcpy( (char *)&cmd_pg_size, &buffer[1], sizeof( cmd_pg_size ) );
			DPRINTF("xlate_touch cmd_flag= 0x%x\n", function_code);
			DPRINTF("xlate_touch cmd_pg_size= 0x%x\n", cmd_pg_size);

			_handle_touch(afu, addr, function_code, cmd_pg_size);
			break;
		case OCSE_MMIO_ACK:
			_handle_ack(afu);
			break;
		case OCSE_LPC_ACK:
			_handle_mem_ack(afu);
			break;
		case OCSE_INTERRUPT_D:
			debug_msg("AFU INTERRUPT D");
			if (_handle_interrupt(afu, 1) < 0) {
				perror("Interrupt d Failure");
				goto ocl_fail;
			}
			break;
		case OCSE_INTERRUPT:
			debug_msg("AFU INTERRUPT");
			if (_handle_interrupt(afu, 0) < 0) {
				perror("Interrupt Failure");
				goto ocl_fail;
			}
			break;
		case OCSE_WAKE_HOST_THREAD:
			debug_msg("AFU WAKE HOST THREAD");
			if (_handle_wake_host_thread(afu) < 0) {
				perror("Wake Host Thread Failure");
				goto ocl_fail;
			}
			break;
		/* case OCSE_AFU_ERROR: */
		/* 	if (_handle_afu_error(afu) < 0) { */
		/* 		perror("AFU ERROR Failure"); */
		/* 		goto ocl_fail; */
		/* 	} */
		/* 	break; */
		default:
			DPRINTF("UNKNOWN CMD IS 0x%2x \n", buffer[0]);
			break;
		}
	}

 ocl_fail:
	afu->attached = 0;
	pthread_exit(NULL);
}

static int _ocse_connect(uint16_t * afu_map, int *fd)
{
	char *ocse_server_dat_path;
	FILE *fp;
	uint8_t buffer[MAX_LINE_CHARS];
	struct sockaddr_in ssadr;
	struct hostent *he;
	char *host, *port_str;
	int port;

	// Get hostname and port of OCSE server
	DPRINTF("AFU CONNECT\n");
	ocse_server_dat_path = getenv("OCSE_SERVER_DAT");
	if (!ocse_server_dat_path) ocse_server_dat_path = "ocse_server.dat";
	fp = fopen(ocse_server_dat_path, "r");
	if (!fp) {
		perror("fopen:ocse_server.dat");
		goto connect_fail;
	}
	do {
		if (fgets((char *)buffer, MAX_LINE_CHARS - 1, fp) == NULL) {
			perror("fgets:ocse_server.dat");
			fclose(fp);
			goto connect_fail;
		}
	}
	while (buffer[0] == '#');
	fclose(fp);
	host = (char *)buffer;
	port_str = strchr((char *)buffer, ':');
	*port_str = '\0';
	port_str++;
	if (!host || !port_str) {
		warn_msg
		    ("ocxl_afu_open_dev:Invalid format in ocse_server.data");
		goto connect_fail;
	}
	port = atoi(port_str);

	info_msg("Connecting to host '%s' port %d", host, port);

	// Connect to OCSE server
	if ((he = gethostbyname(host)) == NULL) {
		herror("gethostbyname");
		puts(host);
		goto connect_fail;
	}
	memset(&ssadr, 0, sizeof(ssadr));
	memcpy(&ssadr.sin_addr, he->h_addr_list[0], he->h_length);
	ssadr.sin_family = AF_INET;
	ssadr.sin_port = htons(port);
	if ((*fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		goto connect_fail;
	}
	ssadr.sin_family = AF_INET;
	ssadr.sin_port = htons(port);
	if (connect(*fd, (struct sockaddr *)&ssadr, sizeof(ssadr)) < 0) {
		perror("connect");
		goto connect_fail;
	}
	strcpy((char *)buffer, "OCSE");
	buffer[4] = (uint8_t) OCSE_VERSION_MAJOR;
	buffer[5] = (uint8_t) OCSE_VERSION_MINOR;
	if (put_bytes_silent(*fd, 6, buffer) != 6) {
		warn_msg("ocxl_afu_open_dev:Failed to write to socket!");
		goto connect_fail;
	}
	if (get_bytes_silent(*fd, 1, buffer, -1, 0) < 0) {
		warn_msg("ocxl_afu_open_dev:Socket failed open acknowledge");
		close_socket(fd);
		goto connect_fail;
	}
	if (buffer[0] != (uint8_t) OCSE_CONNECT) {
		warn_msg("ocxl_afu_open_dev:OCSE bad acknowledge");
		close_socket(fd);
		goto connect_fail;
	}
	if (get_bytes_silent(*fd, sizeof(uint16_t), buffer, 1000, 0) < 0) {
		warn_msg("ocxl_afu_open_dev:afu_map");
		close_socket(fd);
		goto connect_fail;
	}

	// afu_map contains a 1 at each position a tlx interface exists - i.e. the bus numbers that have been "discovered"
	memcpy((char *)afu_map, (char *)buffer, 2);
	*afu_map = (long)ntohs(*afu_map);
	debug_msg("opened host-side socket %d", *fd);
	return 0;

 connect_fail:
	errno = ENODEV;
	return -1;
}

/* static struct ocxl_afu *_new_afu(uint16_t afu_map, uint16_t position, int fd) */
/* { */
/* 	uint8_t *buffer; */
/* 	int size; */
/* 	struct ocxl_afu *afu_h; */
/* 	uint16_t afu_mask = 0x8000; */
/* 	int major = 0; */
/* 	int minor = 0; */

/* 	if (position == 0) { */
/* 		errno = ENODEV; */
/* 		return NULL; */
/* 	} */
/* 	while ((position & afu_mask) == 0) { */
/* 		afu_mask >>= 1; */
/* 		++major; */
/* 	} */

/* 	afu_h = (struct ocxl_afu *)calloc(1, sizeof(struct ocxl_afu)); */
/* 	if (afu_h == NULL) { */
/* 		errno = ENOMEM; */
/* 		return NULL; */
/* 	} */

/* 	if (pipe(afu_h->pipe) < 0) */
/* 		return NULL; */

/* 	pthread_mutex_init(&(afu_h->event_lock), NULL); */
/* 	afu_h->fd = fd; */
/* 	afu_h->map = afu_map; */
/* 	afu_h->dbg_id = major; */
/* 	debug_msg("opened host-side socket %d", afu_h->fd); */

/* 	// Send OCSE query */
/* 	size = 1 + sizeof(uint8_t); */
/* 	buffer = (uint8_t *) malloc(size); */
/* 	buffer[0] = OCSE_QUERY; */
/* 	buffer[1] = afu_h->dbg_id; */
/* 	if (put_bytes_silent(afu_h->fd, size, buffer) != size) { */
/* 		free(buffer); */
/* 		close_socket(&(afu_h->fd)); */
/* 		errno = ENODEV; */
/* 		return NULL; */
/* 	} */
/* 	free(buffer); */

/* 	afu_h->adapter = major; */
/* 	afu_h->position = position; */
/* 	afu_h->id = calloc(7, sizeof(char)); */
/* 	_all_idle(afu_h); */
/* 	sprintf(afu_h->id, "afu%d.%d", major, minor); */

/* 	return afu_h; */
/* } */

ocxl_err _alloc_afu( ocxl_afu_h *afu_out ) 
{
	struct ocxl_afu *afu;

	debug_msg( "_alloc_afu" );
	afu = (struct ocxl_afu *)calloc(1, sizeof(struct ocxl_afu));
	if (afu == NULL) {
         	error_msg( "Could not alloc memory for afu structure" );
		return OCXL_NO_MEM;
	}

	*afu_out = (ocxl_afu_h)afu;

	return OCXL_OK;
}

ocxl_err _find_afu_nth( int fd, const char *name, uint8_t card_index, int16_t afu_index, uint8_t *bus, uint8_t *dev, uint8_t *fcn, uint8_t *afuid )
{
	uint8_t *buffer;
	int size;
	int offset;

	// Send OCSE query

	// size is message type (1), name length (1), name (name_length), card_index (1), afu_index_valid (1), afu_index (1)
	size = 1 + 1 + strlen( name ) + 1 + 1 + 1;
	buffer = (uint8_t *) malloc(size);
	
	offset = 0;
	buffer[offset] = OCSE_FIND_NTH;
	offset = offset + 1;

	buffer[offset] = strlen( name );
	offset = offset + 1;

	memcpy( &buffer[offset], name, strlen(name) ); // don't copy the '\0'
	offset = offset + strlen(name);
	
	buffer[offset] = card_index;
	offset = offset + 1;

	if (afu_index < 0 ) {
	  buffer[offset] = 0;  // afu index is not valid
	} else {
	  buffer[offset] = 1;  // afu index is not valid
	}
	offset = offset + 1;

	buffer[offset] = afu_index;
	offset = offset + 1;

	if (put_bytes_silent( fd, size, buffer ) != size) {
		free(buffer);
		close_socket(&fd);
		return OCXL_NO_DEV;
	}
	
	buffer[0] = 0; 
	
	if (get_bytes_silent( fd, 1, buffer, -1, 0) < 0) {
		warn_msg("ocxl_afu_open:Socket failed");
		close_socket(&fd);
		return OCXL_NO_DEV;
	}

	if ( buffer[0] == (uint8_t)OCSE_FAILED ) {
		warn_msg("ocxl_afu_open_by_id:Socket failed FIND by name");
		close_socket(&fd);
		return OCXL_NO_DEV;
	}
	if (buffer[0] != (uint8_t) OCSE_FIND_ACK) {
		warn_msg("ocxl_afu_open_by_id:OCSE bad acknowledge");
		close_socket(&fd);
		return OCXL_NO_DEV;
	}

	// read out bus, device, function, and afuid 
	if (get_bytes_silent( fd, 4, buffer, -1, 0) < 0) {
		warn_msg("ocxl_afu_open:Socket failed FIND by name and id");
		close_socket(&fd);
		return OCXL_NO_DEV;
	}

	*bus = buffer[0];
	*dev = buffer[1];
	*fcn = buffer[2];
	*afuid = buffer[3];

	free( buffer );
	return OCXL_OK;
}

ocxl_err _find_afu( int fd, const char *name, uint8_t *bus, uint8_t *dev, uint8_t *fcn, uint8_t *afuid )
{
	uint8_t *buffer;
	int size;

	// Send OCSE query
	size = 1 + 1 + strlen( name );
	buffer = (uint8_t *) malloc(size);
	buffer[0] = OCSE_FIND;
	buffer[1] = strlen( name );
	memcpy( &buffer[2], name, strlen(name) ); // don't copy the '\0'
	if (put_bytes_silent( fd, size, buffer ) != size) {
		free(buffer);
		close_socket(&fd);
		return OCXL_NO_DEV;
	}
	
	buffer[0] = 0; 
	
	if (get_bytes_silent( fd, 1, buffer, -1, 0) < 0) {
		warn_msg("ocxl_afu_open:Socket failed");
		close_socket(&fd);
		return OCXL_NO_DEV;
	}

	if ( buffer[0] == (uint8_t)OCSE_FAILED ) {
		warn_msg("ocxl_afu_open:Socket failed FIND by name");
		close_socket(&fd);
		return OCXL_NO_DEV;
	}
	if (buffer[0] != (uint8_t) OCSE_FIND_ACK) {
		warn_msg("ocxl_afu_open_dev:OCSE bad acknowledge");
		close_socket(&fd);
		return OCXL_NO_DEV;
	}

	// read out bus, device, function, and afuid 
	if (get_bytes_silent( fd, 4, buffer, -1, 0) < 0) {
		warn_msg("ocxl_afu_open:Socket failed FIND by name");
		close_socket(&fd);
		return OCXL_NO_DEV;
	}

	*bus = buffer[0];
	*dev = buffer[1];
	*fcn = buffer[2];
	*afuid = buffer[3];

	free( buffer );
	return OCXL_OK;
}

ocxl_err _query_afu( struct ocxl_afu *afu_h, int fd, uint8_t bus, uint8_t dev, uint8_t fcn, uint8_t afuid )
{
	uint8_t *buffer;
	int size;

	debug_msg( "_query_afu" );
	if ( pipe( afu_h->pipe ) < 0 )
		return OCXL_NO_DEV;

	pthread_mutex_init( &(afu_h->event_lock), NULL);

	afu_h->fd = fd;
	afu_h->bus = bus;
	afu_h->dev = dev;
	afu_h->fcn = fcn;
	afu_h->ocxl_id.afu_index = afuid;

	// Send OCSE query
	size = 1 + ( 4 * sizeof( uint8_t ) );
	buffer = (uint8_t *) malloc(size);
	buffer[0] = OCSE_QUERY;
	buffer[1] = bus;
	buffer[2] = dev;
	buffer[3] = fcn;
	buffer[4] = afuid;
	if (put_bytes_silent(afu_h->fd, size, buffer) != size) {
		free(buffer);
		close_socket(&(afu_h->fd));
		return OCXL_NO_DEV;
	}
	free(buffer);
	_all_idle( afu_h );

	afu_h->id = calloc(15, sizeof(char));
	sprintf(afu_h->id, "afu%02x.%02x.%02x.%02x", bus, dev, fcn, afuid);

	return OCXL_OK;
}

ocxl_err _open_afu( struct ocxl_afu *afu_h )
{
	uint8_t *buffer;

	debug_msg( "_open_afu" );
	buffer = (uint8_t *) calloc(1, MAX_LINE_CHARS);
	buffer[0] = (uint8_t) OCSE_OPEN;
	buffer[1] = afu_h->bus;
	buffer[2] = afu_h->dev;
	buffer[3] = afu_h->fcn;
	buffer[4] = afu_h->ocxl_id.afu_index;
	if (put_bytes_silent(afu_h->fd, 5, buffer) != 5) {
		warn_msg("open:Failed to write to socket");
		free(buffer);
		goto open_fail;
	}
	free(buffer);

	afu_h->irq = NULL;
	// afu_h->_head = afu_h;
	afu_h->open.state = LIBOCXL_REQ_PENDING;

	// Start thread
	if (pthread_create(&(afu_h->thread), NULL, _psl_loop, afu_h)) {
		perror("pthread_create");
		close_socket(&(afu_h->fd));
		goto open_fail;
	}

	// Wait for open acknowledgement
	while (afu_h->open.state != LIBOCXL_REQ_IDLE)	/*infinite loop */
		_delay_1ms();

	if (!afu_h->opened) {
		pthread_join(afu_h->thread, NULL);
		goto open_fail;
	}

	return OCXL_OK;

 open_fail:
	pthread_mutex_destroy(&(afu_h->event_lock));
	free( afu_h );
	return OCXL_INTERNAL_ERROR;
}

/* static struct ocxl_afu *_new_afu_bdfa( uint16_t afu_map,  uint8_t bus, uint8_t dev, uint8_t fcn, */
/* 				       uint8_t afuid, int fd) */
/* { */
/* 	uint8_t *buffer; */
/* 	int size; */
/* 	struct ocxl_afu *afu_h; */

/* 	afu_h = (struct ocxl_afu *)calloc(1, sizeof(struct ocxl_afu)); */
/* 	if (afu_h == NULL) { */
/* 		errno = ENOMEM; */
/* 		return NULL; */
/* 	} */

/* 	if (pipe(afu_h->pipe) < 0) */
/* 		return NULL; */

/* 	pthread_mutex_init(&(afu_h->event_lock), NULL); */
/* 	afu_h->fd = fd; */
/* 	afu_h->map = afu_map; */
/* 	// afu_h->dbg_id = major; */
/* 	afu_h->bus = bus; */
/* 	afu_h->dev = dev; */
/* 	afu_h->fcn = fcn; */
/* 	afu_h->ocxl_id.afu_index = afuid; */
/* 	debug_msg("opened host-side socket %d", afu_h->fd); */

/* 	// Send OCSE query */
/* 	size = 1 + ( 4 * sizeof( uint8_t ) ); */
/* 	buffer = (uint8_t *) malloc(size); */
/* 	buffer[0] = OCSE_QUERY; */
/* 	buffer[1] = bus; */
/* 	buffer[2] = dev; */
/* 	buffer[3] = fcn; */
/* 	buffer[4] = afuid; */
/* 	if (put_bytes_silent(afu_h->fd, size, buffer) != size) { */
/* 		free(buffer); */
/* 		close_socket(&(afu_h->fd)); */
/* 		errno = ENODEV; */
/* 		return NULL; */
/* 	} */
/* 	free(buffer); */

/* 	afu_h->id = calloc(11, sizeof(char)); */
/* 	_all_idle(afu_h); */
/* 	sprintf(afu_h->id, "afu%02x.%02x.%02x.%02x", bus, dev, fcn, afuid); */

/* 	return afu_h; */
/* } */

//static struct ocxl_afu_h *_ocse_open(int *fd, uint16_t afu_map, uint8_t major,
/* static struct ocxl_afu *_ocse_open(int *fd, uint16_t afu_map, uint8_t major, */
/* 				     uint8_t minor) */
/* { */
/* 	struct ocxl_afu *afu_h; */
/* 	uint8_t *buffer; */
/* 	uint16_t position; */

/* 	if ( !fd ) */
/* 		fatal_msg( "NULL fd passed to libocxl.c:_ocse_open" ); */
/* 	position = 0x8000; */
/* 	//position >>= 4 * major; */
/* 	//position >>= minor; */
/* 	position >>= major; */
/* 	debug_msg("afu_map = 0x%04x", afu_map); */
/* 	debug_msg("position = 0x%04x", position); */
/* 	if ((afu_map & position) != position) { */
/* 		warn_msg("open: AFU not in system"); */
/* 		close_socket(fd); */
/* 		errno = ENODEV; */
/* 		return NULL; */
/* 	} */

/* 	// Create struct for AFU */
/* 	afu_h = _new_afu(afu_map, position, *fd); */
/* 	if (afu_h == NULL) */
/* 		return NULL; */

/* 	buffer = (uint8_t *) calloc(1, MAX_LINE_CHARS); */
/* 	buffer[0] = (uint8_t) OCSE_OPEN; */
/* 	buffer[1] = afu_h->dbg_id; */
/* 	// buffer[2] = afu_type; */
/* 	afu_h->fd = *fd; */
/* 	if (put_bytes_silent(afu_h->fd, 2, buffer) != 2) { */
/* 		warn_msg("open:Failed to write to socket"); */
/* 		free(buffer); */
/* 		goto open_fail; */
/* 	} */
/* 	free(buffer); */

/* 	afu_h->irq = NULL; */
/* 	afu_h->_head = afu_h; */
/* 	afu_h->adapter = major; */
/* 	afu_h->id = (char *)malloc(7); */
/* 	afu_h->open.state = LIBOCXL_REQ_PENDING; */

/* 	// Start thread */
/* 	if (pthread_create(&(afu_h->thread), NULL, _psl_loop, afu_h)) { */
/* 		perror("pthread_create"); */
/* 		close_socket(&(afu_h->fd)); */
/* 		goto open_fail; */
/* 	} */

/* 	// Wait for open acknowledgement */
/* 	while (afu_h->open.state != LIBOCXL_REQ_IDLE)	/\*infinite loop *\/ */
/* 		_delay_1ms(); */

/* 	if (!afu_h->opened) { */
/* 		pthread_join(afu_h->thread, NULL); */
/* 		goto open_fail; */
/* 	} */

/* 	sprintf(afu_h->id, "tlx%d", major); */

/* 	return afu_h; */

/*  open_fail: */
/* 	pthread_mutex_destroy(&(afu_h->event_lock)); */
/* 	free(afu_h); */
/* 	errno = ENODEV; */
/* 	return NULL; */
/* } */

/* static struct ocxl_afu *_ocse_open_bdfa( int *fd, uint16_t afu_map, uint8_t bus, uint8_t dev, uint8_t fcn, */
/* 					 uint8_t afuid ) */
/* { */
/* 	struct ocxl_afu *afu_h; */
/* 	uint8_t *buffer; */
/* 	uint16_t position; */

/* 	if ( !fd ) */
/* 		fatal_msg( "NULL fd passed to libocxl.c:_ocse_open" ); */

/* 	position = 0x8000; */
/* 	position >>= bus; */
/* 	debug_msg("afu_map = 0x%04x", afu_map); */
/* 	debug_msg("position = 0x%04x", position); */
/* 	if ((afu_map & position) != position) { */
/* 		warn_msg("open: AFU not in system"); */
/* 		close_socket(fd); */
/* 		errno = ENODEV; */
/* 		return NULL; */
/* 	} */

/* 	// Create struct for AFU */
/* 	afu_h = _new_afu_bdfa( afu_map, bus, dev, fcn, afuid, *fd ); */
/* 	if (afu_h == NULL) */
/* 		return NULL; */

/* 	buffer = (uint8_t *) calloc(1, MAX_LINE_CHARS); */
/* 	buffer[0] = (uint8_t) OCSE_OPEN; */
/* 	buffer[1] = afu_h->bus; */
/* 	buffer[2] = afu_h->dev; */
/* 	buffer[3] = afu_h->fcn; */
/* 	buffer[4] = afu_h->ocxl_id.afu_index; */
/* 	afu_h->fd = *fd; */
/* 	if (put_bytes_silent(afu_h->fd, 5, buffer) != 5) { */
/* 		warn_msg("open:Failed to write to socket"); */
/* 		free(buffer); */
/* 		goto open_fail; */
/* 	} */
/* 	free(buffer); */

/* 	afu_h->irq = NULL; */
/* 	// afu_h->_head = afu_h; */
/* 	afu_h->open.state = LIBOCXL_REQ_PENDING; */

/* 	// Start thread */
/* 	if (pthread_create(&(afu_h->thread), NULL, _psl_loop, afu_h)) { */
/* 		perror("pthread_create"); */
/* 		close_socket(&(afu_h->fd)); */
/* 		goto open_fail; */
/* 	} */

/* 	// Wait for open acknowledgement */
/* 	while (afu_h->open.state != LIBOCXL_REQ_IDLE)	/\*infinite loop *\/ */
/* 		_delay_1ms(); */

/* 	if (!afu_h->opened) { */
/* 		pthread_join(afu_h->thread, NULL); */
/* 		goto open_fail; */
/* 	} */

/* 	return afu_h; */

/*  open_fail: */
/* 	pthread_mutex_destroy(&(afu_h->event_lock)); */
/* 	free(afu_h); */
/* 	errno = ENODEV; */
/* 	return NULL; */
/* } */

void ocxl_enable_messages( uint64_t sources )
{
  // we should think about using this to enable debug messages in at least libocxl code...  maybe even enable debug messages in ocse...
  // for now, just return
  warn_msg( "ocxl_enable_messages is not supported in ocse" );
  return;
}

void ocxl_afu_enable_messages( ocxl_afu_h *afu, uint64_t sources )
{
  // we should think about using this to enable debug messages in at least libocxl code...  maybe even enable debug messages in ocse...
  // for now, just return
  warn_msg( "ocxl_afu_enable_messages is not supported in ocse" );
  return;
}

void ocxl_set_error_message_handler( void (*handler)( ocxl_err error, const char *message ) )
{
  // we should think about using this to redirect message to some other file
  // for now, just return
  warn_msg( "ocxl_set_error_message_handler is not supported in ocse" );
  return;
}

void ocxl_afu_set_error_message_handler( ocxl_afu_h *afu, void (*handler)( ocxl_err error, const char *message ) )
{
  // we should think about using this to redirect message to some other file
  // for now, just return
  warn_msg( "ocxl_afu_set_error_message_handler is not supported in ocse" );
  return;
}

const char *ocxl_err_to_string( ocxl_err err )
{
  // we could probably convert the number to a string based on the ocxl_er type
  // for now, just return
  warn_msg( "ocxl_err_to_string is not supported in ocse" );
  return "";
}

const ocxl_identifier *ocxl_afu_get_identifier( ocxl_afu_h afu )
{
	if (!afu) {
		errno = EINVAL;
		return NULL;
	}

	return &afu->ocxl_id;
}

const char *ocxl_afu_get_device_pathname( ocxl_afu_h afu )
{
	char *pathname = NULL;

	if (!afu) {
		errno = EINVAL;
		return NULL;
	}
	// return /dev/ocxl/<name>.<domain>:<bus>:<device>.<function>.<index>
	// use sprintf and strcpy to build pathname
	sprintf( pathname, "/dev/ocxl/%s.0000:%02x:%02x.%x.%x", 
		 (char *)&(afu->ocxl_id.afu_name[0]),
		 afu->bus, 
		 afu->dev, 
		 afu->fcn, 
		 afu->ocxl_id.afu_index );

	return pathname;
}

const char *ocxl_afu_get_sysfs_pathname( ocxl_afu_h afu )
{
	char *pathname = NULL;

	if (!afu) {
		errno = EINVAL;
		return NULL;
	}
	// return /dev/sysfs/class/ocxl/<name>.<domain>:<bus>:<device>.<function>.<index>
	// use sprintf and strcpy to build pathname
	sprintf( pathname, "/dev/sysfs/class/ocxl/%s.0000:%02x:%02x.%x.%x", 
		 (char *)&(afu->ocxl_id.afu_name[0]),
		 afu->bus, 
		 afu->dev, 
		 afu->fcn, 
		 afu->ocxl_id.afu_index );

	return pathname;
	return NULL;
}

ocxl_err ocxl_afu_open_from_dev( const char *path, ocxl_afu_h *afu )
{
	uint16_t afu_map;
	uint8_t bus, dev, fcn, afuid;
	char *my_afuid;
	char *afu_id;
	char *afu_name;
	char *dev_domain;
	char *dev_bus;
	char *dev_device;
	char *dev_function;
	char *afu_index;
	int rc;
	int fd;

	// is there a way to see if this is already done?

	if ( !path ) return OCXL_NO_DEV;

	// allocate afu structure
	rc = _alloc_afu( afu );
	if (  rc != 0 ) return rc;

	if ( _ocse_connect(&afu_map, &fd) < 0 ) return OCXL_NO_DEV;

	// check the map after we know the bus or maybe just ignore it and let query fail...

	// parse the given pathname and query the "afu" bus, device, function, and index that we've asked for
	// Discover AFU position
	// ocapi - /dev/ocxl/<afu_name>.<domain>:<bus>:<device>.<function>.<afu_index>
	// we initially support only 1 afu per function per bus. bus maps to major
	// e.g. /dev/ocxl/IBM,MEMCPY3.0000:00:00.1.0

	afu_id = strrchr(path, '/');
	afu_id++;
	debug_msg("afu id = %s", afu_id);

	// copy to a non-constant string...
	my_afuid = malloc( strlen( afu_id ) + 1 );
	strcpy( my_afuid, afu_id );

	// see populate_metadata in the real libocxl for a nicer way to do this
	// afu_id is now <afu_name>.<domain>:<bus>:<device>.<function>.<afu_index>
	// we can discard domain
	afu_name = strtok( my_afuid, "." );  // something like "IBM,MEMCPY"
	dev_domain = strtok( NULL, ":" );  // probably "0000"
	dev_bus = strtok( NULL, ":" );     // two chars "bb" (0 to FF) (256 "slots") (from shimhost.dat tlxb)
	dev_device = strtok( NULL, "." );  // two chars "dd" (0) (always 0)
	dev_function = strtok( NULL, "." );// one char  "f"  (0 to 7)  (8 "slots") (from discovery always 1 for now)
	afu_index = strtok( NULL, "." );   // two chars "ii" (0 to 63) (64 "slots") (from discovery always 0 for now)

	debug_msg( "afu name = %s, domain = %s, bus = %s, device = %s, function = %s, afu control index = %s", 
		   afu_name, dev_domain, dev_bus, dev_device, dev_function, afu_index );

	// There are too many potential afus to created an effective map
	// so, afu_map only represents the "bus" values that are available according to ocse
	// AND, we have limited the number of buses to 16.
	// So we do an initial check on bus vs afu_map and let ocse do the other work

	if (dev_bus == NULL) {
		debug_msg("err: dev_bus not set");
		return OCXL_INVALID_ARGS;
	}
	bus = (uint8_t)strtol( dev_bus, NULL, 16 );

	if (dev_device == NULL) {
		debug_msg("err: dev_device not set");
		return OCXL_INVALID_ARGS;
	}
	dev = (uint8_t)strtol( dev_device, NULL, 16 );

	if (dev_function == NULL) {
		debug_msg("err: dev_function not set");
		return OCXL_INVALID_ARGS;
	}
	fcn = (uint8_t)strtol( dev_function, NULL, 16 );

	if (afu_index == NULL) {
		debug_msg("err: afu_index not set");
		return OCXL_INVALID_ARGS;
	}
	afuid = (uint8_t)strtol( afu_index, NULL, 16 );

	// makes sure we test to see that bus, dev, and fcn are within syntactic limits

	strcpy( (char *)&((*afu)->ocxl_id.afu_name[0]), afu_name );

	debug_msg("major number = 0x%01x", bus);

	rc = _query_afu( *afu, fd, bus, dev, fcn, afuid );

	// open the "afu"
	rc = _open_afu( *afu );

	return OCXL_OK;
}

ocxl_err ocxl_afu_open( const char *name, ocxl_afu_h *afu ) {

        uint8_t bus, dev, fcn, afuid;
	int rc;
	uint16_t afu_map;
	int fd;

	// connect
	// is there a way to see if this is already done?

	// allocate afu structure
	rc = _alloc_afu( afu );
	if (  rc != 0 ) return rc;

	if ( _ocse_connect(&afu_map, &fd) < 0 ) return OCXL_NO_DEV;

	// find name - returns bus, device, function, afu_index
	strcpy( (char *)&((*afu)->ocxl_id.afu_name[0]), name );

	// new routine here
	rc = _find_afu( fd, name, &bus, &dev, &fcn, &afuid );
	if (  rc != 0 ) return rc;

	// query
	rc = _query_afu( *afu, fd, bus, dev, fcn, afuid );
	if (  rc != 0 ) return rc;

	// open the "afu"
	rc = _open_afu( *afu );
	if (  rc != 0 ) return rc;

	return OCXL_OK;
}

ocxl_err ocxl_afu_open_specific( const char *name, const char *physical_function, int16_t afu_index, ocxl_afu_h *afu ) {
  // real code builds the device path name and calls ocxl_afu_open_by_dev
  // we can call open_dev because physical function + afu_index contains the information we really use there.
	int rc;
	uint16_t afu_map;
	int fd;

	// _alloc_afu
	rc = _alloc_afu( afu );
	if (  rc != 0 ) return rc;

	// _connect
	if ( _ocse_connect(&afu_map, &fd) < 0 ) return OCXL_NO_DEV;

	// parse physical function into domain, bus, dev, and fcn
        uint16_t domain;
	uint8_t bus, device, function;
        int found = sscanf( physical_function, "%hu:%hhu:%hhu.%hhu", &domain, &bus, &device, &function );
        if (found != 4) {
	  warn_msg( "physical function could not be parsed into domain, bus, device, and function" );
	  return OCXL_NO_DEV;
        }
	
	// _query_afu
	rc = _query_afu( *afu, fd, bus, device, function, afu_index );
	if (  rc != 0 ) return rc;

	// open the "afu"
	rc = _open_afu( *afu );
	if (  rc != 0 ) return rc;

	return OCXL_OK;
}

/* ocxl_err ocxl_afu_open_by_id( const char *name, uint8_t card_index, int16_t afu_index, ocxl_afu_h *afu ) { */
/*   // real code builds the device path name and calls ocxl_afu_open_by_dev */
/*   // card index is an index into a sorted list of physical_functions that have matching "name" */
/*   // afu_index may be -1 indicating that any name matching afu within the physical function may be used. */
/*   // in fact, if the afu it tries is full, we should go to the next one...  We will not do that for now. */
/*   // need a find_nth routine to find the card index for that name/afu_id */
/*         warn_msg( "ocxl_afu_open_by_id is not fully tested yet" ); */

/*         uint8_t bus, dev, fcn, afuid; */
/* 	int rc; */
/* 	uint16_t afu_map; */
/* 	int fd; */
/* 	struct ocxl_afu *my_afu; */
/* 	// connect */
/* 	// is there a way to see if this is already done? */

/* 	// allocate afu structure */
/* 	rc = _alloc_afu( (ocxl_afu_h *)&my_afu ); */
/* 	if (  rc != 0 ) return rc; */

/* 	if ( _ocse_connect(&afu_map, &fd) < 0 ) return OCXL_NO_DEV; */

/* 	// find name - returns bus, device, function, afu_index */
/* 	strcpy( (char *)&(my_afu->ocxl_id.afu_name[0]), name ); */

/* 	// new routine here */
/* 	rc = _find_afu_nth( fd, name, card_index, afu_index, &bus, &dev, &fcn, &afuid ); */
/* 	if (  rc != 0 ) return rc; */

/* 	// query */
/* 	rc = _query_afu( my_afu, fd, bus, dev, fcn, afuid ); */
/* 	if (  rc != 0 ) return rc; */

/* 	// open the "afu" */
/* 	rc = _open_afu( my_afu ); */
/* 	if (  rc != 0 ) return rc; */

/* 	*afu = ( ocxl_afu_h )my_afu; */
/* 	return OCXL_OK; */
/* } */

void _afu_free( ocxl_afu_h afu )
{
	uint8_t buffer;
	int rc;
	int loop_count = 0;

	if (!afu) {
		warn_msg("_afu_free: No AFU given");
		goto free_done_no_afu;
	}

	if (!afu->opened)
		goto free_done;

	// detach
	buffer = OCSE_DETACH;
	rc = put_bytes_silent(afu->fd, 1, &buffer);
	if (rc == 1) {
	        debug_msg("_afu_free:detach request sent from host on socket %d", afu->fd);
		while ((afu->attached) && (loop_count < 180000)) {	/*infinite loop changed to a 3 minute timeout*/
			_delay_1ms();
			loop_count = loop_count + 1;
		}
                if(loop_count == 180000)
		   fatal_msg("_afu_free: time out of 3s reached");
	}
	debug_msg( "_afu_free: closing host side socket %d", afu->fd );
	// free some other stuff in the afu like the irq list
	close_socket(&(afu->fd));
	afu->opened = 0;
	pthread_join(afu->thread, NULL);

 free_done:
	if (afu->id != NULL)
		free( afu->id );
 free_done_no_afu:
	pthread_mutex_destroy( &(afu->event_lock) );
	free( afu );
}

ocxl_err ocxl_afu_close( ocxl_afu_h afu )
{
        struct ocxl_afu *my_afu;
	struct ocxl_irq *irq;

	my_afu = (struct ocxl_afu *)afu;

	// if there are any irq's, free them
	irq = my_afu->irq;
	while ( irq != NULL ) {
	  my_afu->irq = irq->_next;
	  free( irq );
	  irq = my_afu->irq;
	}

	// mmio unmap
	my_afu->global_mapped = 0;
	my_afu->mapped = 0;
  
	_afu_free( afu );

	return OCXL_OK;
}

ocxl_err ocxl_afu_attach( ocxl_afu_h afu, __attribute__((unused)) uint64_t flags )
{
	if (!afu) {
		errno = EINVAL;
		return OCXL_NO_DEV;
	}
	DPRINTF("AFU ATTACH\n");
	if (!afu->opened) {
		warn_msg("ocxl_afu_attach: Must open AFU first");
		errno = ENODEV;
		return OCXL_NO_DEV;
	}

	if (afu->attached) {
		warn_msg("ocxl_afu_attach: AFU already attached");
		errno = ENODEV;
		return OCXL_NO_DEV;
	}
	// Perform OCSE attach
	// lgt - dont need to send amr - in fact, the parameter is gone now
	// we don't model the change in permissions
	afu->attach.state = LIBOCXL_REQ_REQUEST;
	while (afu->attach.state != LIBOCXL_REQ_IDLE)	/*infinite loop */
		_delay_1ms();
	afu->attached = 1;

	return OCXL_OK;
}

int ocxl_afu_get_event_fd( ocxl_afu_h afu )
{ 
	if (!afu) {
		warn_msg("ocxl_afu_get_event_fd: No AFU given");
		errno = ENODEV;
		return -1;
	}
	return afu->pipe[0];
}

int ocxl_irq_get_fd( ocxl_afu_h afu, ocxl_irq_h irq )
{ 
  // I don't think this is correct.  I appears that the irq can have it's own path back to the code...  but I'm not
  // sure yet.  We'll just use the afu pipe as the path for now
	if (!afu) {
		warn_msg("ocxl_afu_get_event_fd: No AFU given");
		errno = ENODEV;
		return -1;
	}
	return afu->pipe[0];
}

ocxl_err ocxl_irq_alloc( ocxl_afu_h afu, void *info, ocxl_irq_h *irq_handle )
{
        // create an irq, link it to the afu, and return the address of the irq to the caller
        struct ocxl_irq *new_irq;
        struct ocxl_irq *current_irq;

        if (!afu) {
		warn_msg("ocxl_afu_new_irq: No AFU given");
		errno = ENODEV;
		return OCXL_NO_DEV;
	}

	new_irq = (struct ocxl_irq *)malloc( sizeof(struct ocxl_irq) );

	if (!new_irq) {
	        // allocation failed
		errno = ENOMEM;
		warn_msg("ocxl_afu_new_irq: insufficient memory");
		return OCXL_NO_IRQ;
	}

	new_irq->irq = afu->irq_count; // the index of this new irq is the current counter value
	new_irq->id = (uint64_t)new_irq;  // the id of this new irq is the address of the irq
	new_irq->_next = NULL;
	new_irq->afu = afu;
	*irq_handle = (ocxl_irq_h)new_irq->irq;

	// add new irq to the end of the afu's list of irqs
	if (afu->irq == NULL) {
	  // this is the first new irq
	  afu->irq = new_irq;
	  afu->irq_count++;   
	  return OCXL_OK;
	}

	// scan the list for the last irq
	current_irq = afu->irq;
	while (current_irq->_next != NULL) {
	    current_irq = current_irq->_next;
	}
	// we have the last one now
	current_irq->_next = new_irq;
	afu->irq_count++;

	return OCXL_OK;
}

uint64_t ocxl_irq_get_handle( ocxl_afu_h afu, ocxl_irq_h irq )
{
  // scan the irq list of the afu for an irq with a matching ocxl_irq_h
  // return the id of the irq we found or 0 if none found
        struct ocxl_irq *current_irq;

	current_irq = afu->irq;
	while (current_irq != NULL) {
	  if (current_irq->irq  == irq) {
	    // this is the irq we are looking for
	    // return the id in current_irq
	    return current_irq->id;
	  } else {
	    // follow the linked list
	    current_irq = current_irq->_next;
	  }
	}

	// if we get here, we didn't find irq in the afu!
	warn_msg("ocxl_irq_free: irq not found in afu");
	return 0;
}

uint16_t ocxl_afu_event_check_versioned( ocxl_afu_h afu, int timeout, ocxl_event *events, uint16_t event_count, uint16_t event_api_version )
{
	int i;
 	uint8_t type; 

	// check for null afu
	if (afu == NULL) {
		warn_msg("ocxl_afu_event_check: NULL afu!");
		return OCXL_NO_DEV;
	}

	// we support event_api_version = 0 for now...
	if (event_api_version != 0 ) {
		warn_msg("ocxl_afu_event_check_versioned: event api version must be 0, continuing as if 0 had be sent.");
	}

	// we support event_count = 1 for now...
	if (event_count != 1) {
		warn_msg("ocxl_afu_event_check_versioned: event count must be 1, continuing as if 1 had be sent.");
	}

	// read an event - if not one, just wait here
	//     we ignore timeout for now
	debug_msg("ocxl_read_event: waiting for event");
	pthread_mutex_lock(&(afu->event_lock));
	while (afu->opened && !afu->events[0]) {	/*infinite loop */
		pthread_mutex_unlock(&(afu->event_lock));
		if (_delay_1ms() < 0)
			return -1;
		pthread_mutex_lock(&(afu->event_lock));
	}

	debug_msg("ocxl_read_event: received event");
	// Copy event data, free and move remaining events in queue
	memcpy( events, afu->events[0], sizeof( ocxl_event ) );
	free(afu->events[0]);
	for (i = 1; i < EVENT_QUEUE_MAX; i++)
		afu->events[i - 1] = afu->events[i];
	afu->events[EVENT_QUEUE_MAX - 1] = NULL;
	pthread_mutex_unlock(&(afu->event_lock));
	if (read(afu->pipe[0], &type, 1) > 0)
		return 1;

	return -1;
}

uint16_t ocxl_afu_event_check( ocxl_afu_h afu, int timeout, ocxl_event *events, uint16_t event_count )
{
	uint16_t event_api_version = 0;

	return ocxl_afu_event_check_versioned( afu, timeout, events, event_count, event_api_version );
}

ocxl_err ocxl_afu_get_p9_thread_id(ocxl_afu_h afu, uint16_t *thread_id)
{
  // obtain the current thread id - with pthread_self()
  // app must pass thread id to afu for afu to use in subsequent wake host thread command
  *thread_id = (uint16_t)pthread_self();
  return 0;
}

ocxl_err ocxl_mmio_map( ocxl_afu_h afu, ocxl_mmio_type type, ocxl_mmio_h *mmio )
{
	ocxl_err err;

	debug_msg( "MMIO MAP" );
	if (afu == NULL) {
		warn_msg("ocxl_mmio_map: NULL afu!");
		err = OCXL_NO_CONTEXT;
		goto map_fail;
	}

	if (!afu->opened) {
		warn_msg("ocxl_mmio_map: Must open afu first!");
		err = OCXL_NO_CONTEXT;
		goto map_fail;
	}

	// It is not necessary to attach the afu before the mmio areas are mapped.
	// It IS necessary to attach the afu BEFORE an mmio is issued though
	// if (!afu->attached) {
	// 	warn_msg("ocxl_mmio_map: Must attach afu first!");
	//	err = OCXL_NO_CONTEXT;
	// 	goto map_fail;
	// } 

	if (afu->mmio_count == afu->mmio_max) {
		warn_msg("ocxl_mmio_map: insufficient memory to map the new mmio area!");
		err = OCXL_NO_MEM;
		goto map_fail;
	}

	switch (type) {
	case OCXL_GLOBAL_MMIO:
	  // Send MMIO map to OCSE
	  afu->mmio.type = OCSE_GLOBAL_MMIO_MAP;
	  // my_afu->mmio.data = (uint64_t) endian;
	  afu->mmio.state = LIBOCXL_REQ_REQUEST;
	  break;
	case OCXL_PER_PASID_MMIO:
	  // Send MMIO map to OCSE
	  afu->mmio.type = OCSE_MMIO_MAP;
	  // my_afu->mmio.data = (uint64_t) endian;
	  afu->mmio.state = LIBOCXL_REQ_REQUEST;
	  break;
	default:
	  err = OCXL_INVALID_ARGS;
	  goto map_fail;
	  break;
	}

	while (afu->mmio.state != LIBOCXL_REQ_IDLE)	/*infinite loop */
		_delay_1ms();

	if (type == OCXL_GLOBAL_MMIO)
	  afu->global_mapped = 1;
	else
	  afu->mapped = 1;
	
	*mmio = (ocxl_mmio_h)&(afu->mmios[afu->mmio_count]);
	afu->mmio_count++;
	  
	return OCXL_OK;
 map_fail:
	return err;
}

ocxl_err ocxl_mmio_unmap( ocxl_mmio_h region )
{
// since we've created a static array for the areas, this is tricky...
	if (region->type == OCXL_GLOBAL_MMIO)
	  region->afu->global_mapped = 0;
	else
	  region->afu->mapped = 0;

// but what about mmio_count?

	return OCXL_OK;
}

ocxl_err ocxl_mmio_write64( ocxl_mmio_h mmio, off_t offset, ocxl_endian endian, uint64_t value )
{
	ocxl_err err;

	//debug_msg("ocxl_mmio_write64: entered");

	if (mmio->afu == NULL) {
	  err = OCXL_NO_MEM;
	  goto write64_fail;
	}
	//debug_msg("ocxl_mmio_write64: mmio->afu ok");

	if (mmio->type == OCXL_GLOBAL_MMIO) {
	  if (!mmio->afu->global_mapped) {
	    err = OCXL_NO_MEM;
	    goto write64_fail;
	  }
	} else {
	  if (!mmio->afu->mapped) {
	    err = OCXL_NO_MEM;
	    goto write64_fail;
	  }
	}
	//debug_msg("ocxl_mmio_write64: mmio->afu->*mapped ok");

	if ( offset & 0x7 ) {
		warn_msg("ocxl_mmio_write64: offset not properly aligned!");
		errno = EINVAL;
		err = OCXL_OUT_OF_BOUNDS;
		goto write64_fail;
	}

	//debug_msg("ocxl_mmio_write64: passed parameter checks");

	/* if ( offset >= my_afu->mmio_length ) { */
	/* 	warn_msg("ocxl_mmio_write64: offset out of bounds!"); */
	/* 	errno = EINVAL; */
	/* 	return OCXL_OUT_OF_BOUNDS; */
	/* } */

	// Send MMIO map to OCSE
	if (mmio->type == OCXL_GLOBAL_MMIO) {
	  mmio->afu->mmio.type = OCSE_GLOBAL_MMIO_WRITE64;
	} else {
	  mmio->afu->mmio.type = OCSE_MMIO_WRITE64;
	}
	mmio->afu->mmio.addr = (uint32_t) offset;
	// should I use endian here???  maybe
	mmio->afu->mmio.data = value;
	mmio->afu->mmio.state = LIBOCXL_REQ_REQUEST;

	//debug_msg("ocxl_mmio_write64: waiting for idle");

	while (mmio->afu->mmio.state != LIBOCXL_REQ_IDLE)	/*infinite loop */
		_delay_1ms();

	//debug_msg("ocxl_mmio_write64: mmio acked");

	if (!mmio->afu->opened) {
	  err = OCXL_NO_DEV;
	  goto write64_fail;
	}

	//debug_msg("ocxl_mmio_write64: leaving normally");
	return OCXL_OK;

 write64_fail:
	//debug_msg("ocxl_mmio_write64: leaving abnormally");
	return err;
}

ocxl_err ocxl_mmio_read64( ocxl_mmio_h mmio, off_t offset, ocxl_endian endian, uint64_t *out )
{
	ocxl_err err;

	if (mmio->afu == NULL) {
	  err = OCXL_NO_MEM;
	  goto read64_fail;
	}

	if (mmio->type == OCXL_GLOBAL_MMIO) {
	  if (!mmio->afu->global_mapped) {
	    err = OCXL_NO_MEM;
	    goto read64_fail;
	  }
	} else {
	  if (!mmio->afu->mapped) {
	    err = OCXL_NO_MEM;
	    goto read64_fail;
	  }
	}


	if ( offset & 0x7 ) {
		warn_msg("ocxl_mmio_read64: offset not properly aligned!");
		errno = EINVAL;
		err = OCXL_OUT_OF_BOUNDS;
		goto read64_fail;
	}

	/* if ( offset >= my_afu->mmio_length ) { */
	/* 	warn_msg("ocxl_mmio_read64: offset out of bounds!"); */
	/* 	errno = EINVAL; */
	/* 	return OCXL_OUT_OF_BOUNDS; */
	/* } */

	// Send MMIO map to OCSE
	if (mmio->type == OCXL_GLOBAL_MMIO) {
	  mmio->afu->mmio.type = OCSE_GLOBAL_MMIO_READ64;
	} else {
	  mmio->afu->mmio.type = OCSE_MMIO_READ64;
	}
	mmio->afu->mmio.addr = (uint32_t) offset;
	mmio->afu->mmio.state = LIBOCXL_REQ_REQUEST;
	while (mmio->afu->mmio.state != LIBOCXL_REQ_IDLE)	/*infinite loop */
		_delay_1ms();

	// should use endian here...  maybe
	*out = mmio->afu->mmio.data;

	if (!mmio->afu->opened) {
	  err = OCXL_NO_DEV;
	  goto read64_fail;
	}


	return OCXL_OK;

 read64_fail:
	return err;
}

ocxl_err ocxl_mmio_write32( ocxl_mmio_h mmio, off_t offset, ocxl_endian endian, uint32_t value )
{
	ocxl_err err;

	if (mmio->afu == NULL) {
	  err = OCXL_NO_MEM;
	  goto write32_fail;
	}

	if (mmio->type == OCXL_GLOBAL_MMIO) {
	  if (!mmio->afu->global_mapped) {
	    err = OCXL_NO_MEM;
	    goto write32_fail;
	  }
	} else {
	  if (!mmio->afu->mapped) {
	    err = OCXL_NO_MEM;
	    goto write32_fail;
	  }
	}

	if (offset & 0x3) {
		warn_msg("ocxl_mmio_write32: offset not properly aligned!");
		errno = EINVAL;
		err = OCXL_OUT_OF_BOUNDS;
		goto write32_fail;
	}
	/* if ( offset >= my_afu->mmio_length ) { */
	/* 	warn_msg("ocxl_mmio_write32: offset out of bounds!"); */
	/* 	errno = EINVAL; */
	/* 	return OCXL_OUT_OF_BOUNDS; */
	/* } */

	// Send MMIO map to OCSE
	if (mmio->type == OCXL_GLOBAL_MMIO) {
	  mmio->afu->mmio.type = OCSE_GLOBAL_MMIO_WRITE32;
	} else {
	  mmio->afu->mmio.type = OCSE_MMIO_WRITE32;
	}
	mmio->afu->mmio.addr = (uint32_t) offset;
	mmio->afu->mmio.data = (uint64_t) value;
	mmio->afu->mmio.state = LIBOCXL_REQ_REQUEST;
	while (mmio->afu->mmio.state != LIBOCXL_REQ_IDLE)	/*infinite loop */
		_delay_1ms();

	if (!mmio->afu->opened){
	  err = OCXL_NO_DEV;
	  goto write32_fail;
	}

	return OCXL_OK;

 write32_fail:
	return err;
}

ocxl_err ocxl_mmio_read32( ocxl_mmio_h mmio, off_t offset, ocxl_endian endian, uint32_t *out )
{
	ocxl_err err;

	if (mmio->afu == NULL) {
	  err = OCXL_NO_MEM;
	  goto read32_fail;
	}

	if (mmio->type == OCXL_GLOBAL_MMIO) {
	  if (!mmio->afu->global_mapped) {
	    err = OCXL_NO_MEM;
	    goto read32_fail;
	  }
	} else {
	  if (!mmio->afu->mapped) {
	    err = OCXL_NO_MEM;
	    goto read32_fail;
	  }
	}

	if (offset & 0x3) {
		warn_msg("ocxl_mmio_read32: offset not properly aligned!");
		errno = EINVAL;
		err = OCXL_OUT_OF_BOUNDS;
		goto read32_fail;
	}

	/* if ( offset >= my_afu->mmio_length ) { */
	/* 	warn_msg("ocxl_mmio_read32: offset out of bounds!"); */
	/* 	errno = EINVAL; */
	/* 	return OCXL_OUT_OF_BOUNDS; */
	/* } */

	// Send MMIO map to OCSE
	if (mmio->type == OCXL_GLOBAL_MMIO) {
	  mmio->afu->mmio.type = OCSE_GLOBAL_MMIO_READ32;
	} else {
	  mmio->afu->mmio.type = OCSE_MMIO_READ32;
	}
	mmio->afu->mmio.addr = (uint32_t) offset;
	mmio->afu->mmio.state = LIBOCXL_REQ_REQUEST;
	while (mmio->afu->mmio.state != LIBOCXL_REQ_IDLE)	/*infinite loop */
		_delay_1ms();
	*out = (uint32_t) mmio->afu->mmio.data;

	if (!mmio->afu->opened) {
	  err = OCXL_NO_DEV;
	  goto read32_fail;
	}

	return OCXL_OK;

 read32_fail:
	errno = ENODEV;
	return err;
}

/* ocxl_err ocxl_global_mmio_map( ocxl_afu_h afu, ocxl_endian endian) */
/* { */
/*         struct ocxl_afu *my_afu; */

/* 	my_afu = (struct ocxl_afu *)afu; */

/* 	debug_msg( "GLOBAL MMIO MAP" ); */
/* 	if (my_afu == NULL) { */
/* 		warn_msg("ocxl_global_mmio_map: NULL afu!"); */
/* 		goto map_fail; */
/* 	} */

/* 	if (!my_afu->opened) { */
/* 		printf("ocxl_global_mmio_map: Must open afu first!\n"); */
/* 		goto map_fail; */
/* 	} */

/* 	if (!my_afu->attached) { */
/* 		printf("ocxl_global_mmio_map: Must attach first!\n"); */
/* 		goto map_fail; */
/* 	} */

/* 	if (endian & ~(OCXL_MMIO_FLAGS)) { */
/* 		printf("ocxl_global_mmio_map: Invalid flags!\n"); */
/* 		goto map_fail; */
/* 	} */
/* 	// Send MMIO map to OCSE */
/* 	my_afu->mmio.type = OCSE_GLOBAL_MMIO_MAP; */
/* 	my_afu->mmio.data = (uint64_t) endian; */
/* 	my_afu->mmio.state = LIBOCXL_REQ_REQUEST; */
/* 	while (my_afu->mmio.state != LIBOCXL_REQ_IDLE)	/\*infinite loop *\/ */
/* 		_delay_1ms(); */
/* 	my_afu->global_mapped = 1; */

/* 	return OCXL_OK; */
/*  map_fail: */
/* 	errno = ENODEV; */
/* 	return OCXL_NO_DEV; */
/* } */

/* ocxl_err ocxl_global_mmio_unmap( ocxl_afu_h afu ) */
/* { */
/*         struct ocxl_afu *my_afu; */

/* 	my_afu = (struct ocxl_afu *)afu; */

/* 	if (my_afu == NULL) { */
/* 		warn_msg("ocxl_global_mmio_map: NULL afu!"); */
/* 		return OCXL_NO_DEV; */
/* 	} */

/* 	my_afu->global_mapped = 0; */
	
/* 	return OCXL_OK; */
/* } */

ocxl_err ocxl_global_mmio_write64( ocxl_afu_h afu, uint64_t offset, uint64_t val)
{
	if ((afu == NULL) || !afu->global_mapped)
		goto write64_fail;

	if (offset & 0x7) {
		warn_msg("ocxl_global_mmio_write64: offset not properly aligned!");
		errno = EINVAL;
		return OCXL_OUT_OF_BOUNDS;
	}

	/* if ( offset >= my_afu->mmio_offset ) { */
	/* 	warn_msg("ocxl_global_mmio_write64: offset out of bounds!"); */
	/* 	errno = EINVAL; */
	/* 	return OCXL_OUT_OF_BOUNDS; */
	/* } */

	// Send MMIO map to OCSE
	afu->mmio.type = OCSE_GLOBAL_MMIO_WRITE64;
	afu->mmio.addr = (uint32_t) offset;
	afu->mmio.data = val;
	afu->mmio.state = LIBOCXL_REQ_REQUEST;
	while (afu->mmio.state != LIBOCXL_REQ_IDLE)	/*infinite loop */
		_delay_1ms();

	if (!afu->opened)
		goto write64_fail;

	return OCXL_OK;

 write64_fail:
	errno = ENODEV;
	return OCXL_NO_DEV;
}

ocxl_err ocxl_global_mmio_read64( ocxl_afu_h afu, uint64_t offset, uint64_t *out)
{
	if ((afu == NULL) || !afu->global_mapped)
		goto read64_fail;

	if (offset & 0x7) {
		warn_msg("ocxl_global_mmio_read64: offset not properly aligned!");
		errno = EINVAL;
		return OCXL_OUT_OF_BOUNDS;
	}

	/* if ( offset >= my_afu->mmio_offset ) { */
	/* 	warn_msg("ocxl_global_mmio_read64: offset out of bounds!"); */
	/* 	errno = EINVAL; */
	/* 	return OCXL_OUT_OF_BOUNDS; */
	/* } */

	// Send MMIO map to OCSE
	afu->mmio.type = OCSE_GLOBAL_MMIO_READ64;
	afu->mmio.addr = (uint32_t)offset;
	afu->mmio.state = LIBOCXL_REQ_REQUEST;
	while (afu->mmio.state != LIBOCXL_REQ_IDLE)	/*infinite loop */
		_delay_1ms();
	*out = afu->mmio.data;

	if (!afu->opened)
		goto read64_fail;

	return OCXL_OK;

 read64_fail:
	errno = ENODEV;
	return OCXL_NO_DEV;
}

ocxl_err ocxl_global_mmio_write32( ocxl_afu_h afu, uint64_t offset, uint32_t val)
{
	if ((afu == NULL) || !afu->global_mapped)
		goto write32_fail;

	if (offset & 0x3) {
		warn_msg("ocxl_global_mmio_write32: offset not properly aligned!");
		errno = EINVAL;
		return OCXL_OUT_OF_BOUNDS;
	}

	/* if ( offset >= my_afu->mmio_offset ) { */
	/* 	warn_msg("ocxl_global_mmio_write32: offset out of bounds!"); */
	/* 	errno = EINVAL; */
	/* 	return OCXL_OUT_OF_BOUNDS; */
	/* } */

	// Send MMIO map to OCSE
	afu->mmio.type = OCSE_GLOBAL_MMIO_WRITE32;
	afu->mmio.addr = (uint32_t)offset;
	afu->mmio.data = (uint64_t)val;
	afu->mmio.state = LIBOCXL_REQ_REQUEST;
	while (afu->mmio.state != LIBOCXL_REQ_IDLE)	/*infinite loop */
		_delay_1ms();

	if (!afu->opened)
		goto write32_fail;

	return OCXL_OK;

 write32_fail:
	errno = ENODEV;
	return OCXL_NO_DEV;
}

ocxl_err ocxl_global_mmio_read32( ocxl_afu_h afu, uint64_t offset, uint32_t *out)
{
	if ((afu == NULL) || !afu->global_mapped)
		goto read32_fail;

	if (offset & 0x3) {
		warn_msg("ocxl_global_mmio_read32: invalid offset alignment");
		errno = EINVAL;
		return OCXL_OUT_OF_BOUNDS;
	}
	
	/* if (offset >= my_afu->mmio_offset) { */
	/* 	warn_msg("ocxl_global_mmio_read32: offset out of bounds"); */
	/* 	errno = EINVAL; */
	/* 	return OCXL_OUT_OF_BOUNDS; */
	/* } */

	// Send MMIO map to OCSE
	afu->mmio.type = OCSE_GLOBAL_MMIO_READ32;
	afu->mmio.addr = (uint32_t)offset;
	afu->mmio.state = LIBOCXL_REQ_REQUEST;
	while (afu->mmio.state != LIBOCXL_REQ_IDLE)	/*infinite loop */
		_delay_1ms();
	*out = (uint32_t) afu->mmio.data;

	if (!afu->opened)
		goto read32_fail;

	return OCXL_OK;

 read32_fail:
	errno = ENODEV;
	return OCXL_NO_DEV;
}

size_t ocxl_afu_get_mmio_size( ocxl_afu_h afu )
{
	if (afu == NULL)
                   return OCXL_NO_DEV;

        // this is the mmio stride for this afu
        return afu->per_pasid_mmio.length;
}

size_t ocxl_afu_get_global_mmio_size( ocxl_afu_h afu )
{
	if (afu == NULL)
                   return OCXL_NO_DEV;

        // this is the per pasid mmio offset for this afu
	// there might be a more accurate method - look for it
        return afu->global_mmio.length;

}

void  ocxl_afu_get_version( ocxl_afu_h afu, uint8_t *major, uint8_t *minor )
{
	// if (my_afu == NULL)
        //           return OCXL_NO_DEV;

        // these are from the afu descriptor that we retrieved when we opened the afu
	*major = afu->afu_version_major;
	*minor = afu->afu_version_minor;

        return;

}

uint32_t  ocxl_afu_get_pasid( ocxl_afu_h afu )
{
	// if (my_afu == NULL)
        //           return OCXL_NO_DEV;

        // we use the term context as the equivalent for the pasid
        return afu->context;

}

ocxl_err ocxl_afu_set_ppc64_amr( ocxl_afu_h afu, uint64_t amr)
{
	afu->ppc64_amr = amr;

	return OCXL_OK;
}

// ocxl_wait should behave very much like read_event
// however, I don't think we can use the event structure as is
// maybe create another event struct so that interrupt events and 
// wake host thread events cannot collide or stall each other.
// only one waitasec at a time in a context/afu pair
int ocxl_wait()
{
  // multi thread safe?
  // obtain the current thread id
  // put it in the wait event
  // a wake host thread command from the afu must supply a matching thread id for this to wake to clear
  // we'll need to add a way to have multiple active wait events for a given application
  // and remove it when the wake occurs

        ocxl_wait_event *this_wait_event;

        this_wait_event = _alloc_wait_event( (uint16_t)pthread_self() );
	
	info_msg( "ocxl_wait: waiting for wake host thread @ 0x%016llx -> 0x%04x", 
		   (uint64_t)this_wait_event, 
		   this_wait_event->tid );
	
	// enable this wake event
	this_wait_event->enabled = 1;
	
	// Function will block until wake host thread occurs and matches thread id
	// pthread_mutex_lock( &(this_wait_event->wait_lock) );
	while ( this_wait_event->received == 0 ) {	/*infinite loop */
	        // pthread_mutex_unlock( &(this_wait_event->wait_lock) );
	        // debug_msg( "ocxl_wait: stil waiting for wake host thread @ 0x%016llx -> 0x%04x", 
		//	   (uint64_t)this_wait_event, 
		//	   this_wait_event->tid );
		if (_delay_1ms() < 0)
			return -1;
		// pthread_mutex_lock(&(this_wait_event->wait_lock));
	}

	// free wait event - remove it from wait event list
	this_wait_event->enabled = 0;
	this_wait_event->received = 0;
	// pthread_mutex_unlock( &(this_wait_event->wait_lock) );

	_free_wait_event( this_wait_event );

	return OCXL_OK;
}
