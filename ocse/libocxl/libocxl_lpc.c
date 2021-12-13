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

#include "libocxl.h"
#include "libocxl_lpc.h"
#include "libocxl_internal.h"
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

// handle routines that catch calls from libocxl._psl_loop
// are found in libocxl.c

// routines that are called by user applications.
ocxl_err ocxl_lpc_map(ocxl_afu_h afu, uint32_t flags)
{
        struct ocxl_afu *my_afu;
	my_afu = (struct ocxl_afu *)afu;
	debug_msg("ocxl_lpc_map: entered");
	if (!my_afu->opened) {
		warn_msg("ocxl_lpc_map: Must open first!");
		goto lpcmap_fail;
	}

	if (!my_afu->attached) {
		warn_msg("ocxl_lpc_map: Must attach first!");
		goto lpcmap_fail;
	}

	if (flags & ~(OCXL_LPC_FLAGS)) {
		warn_msg("ocxl_lpc_map: Invalid flags!");
		goto lpcmap_fail;
	}

	// Send mem map to OCSE
	my_afu->mem.type = OCSE_LPC_MAP;
	my_afu->mem.data = (uint8_t *)&(flags);
	my_afu->mem.state = LIBOCXL_REQ_REQUEST;
	while (my_afu->mem.state != LIBOCXL_REQ_IDLE)	/*infinite loop */
		_delay_1ms();
	my_afu->lpc_mapped = 1;

	return 0;
 lpcmap_fail:
	errno = ENODEV;
	return -1;
}

ocxl_err ocxl_lpc_unmap(ocxl_afu_h afu)
{
        struct ocxl_afu *my_afu;
	my_afu = (struct ocxl_afu *)afu;
	my_afu->lpc_mapped = 0;
	return 0;
}

// write size bytes from *data to offset in afu
//    size = arbitrary
//    offset is byte aligned
//    *data is byte aligned - but is likely 16 Byte aligned due to host OS behavior
ocxl_err ocxl_lpc_write(ocxl_afu_h afu, uint64_t offset, uint8_t *val, uint64_t size )
{

        struct ocxl_afu *my_afu;
	my_afu = (struct ocxl_afu *)afu;

        debug_msg("ocxl_lpc_write: %d bytes to lpc offset 0x%016lx", size, offset);

        if (!my_afu) {
	      warn_msg("NULL afu passed to ocxl_lpc_write");
	      goto write_fail;
	}

        if (!my_afu->lpc_mapped) {
	      warn_msg("afu lpc space is not mapped");
	      goto write_fail;
	}

        // check size legality - not required - any size is legal

        // check address alignment against size - not required - any alignment is legal

	my_afu->mem.type = OCSE_LPC_WRITE;
	my_afu->mem.be = 0;

	int i;
	uint64_t to_i;
	int stride;
	int remainder;

	for (i=0; i<size; i=i+stride) {
	  to_i = offset + i;
	  remainder = size - i;
	  if ( ( to_i & 0x1 ) !=0 ) {
	    // byte aligned address, send 1 byte, add 1 to i
	    stride = 1;
	  } else if ( ( to_i & 0x2 ) !=0 ) {
	    // 2-byte aligned address, send 2 byte, add 2 to i
	    stride = 2;
	  } else if ( ( to_i & 0x4 ) !=0 ) {
	    // 4-byte aligned address, send 4 byte, add 4 to i
	    stride = 4;
	  } else if ( ( to_i & 0x8 ) !=0 ) {
	    // 8-byte aligned address, send 8 byte, add 8 to i
	    stride = 8;
	  } else if ( ( to_i & 0x10 ) !=0 ) {
	    // 16-byte aligned address, send 16 byte, add 16 to i
	    stride = 16;
	  } else if ( ( to_i & 0x20 ) !=0 ) {
	    // 32-byte aligned address, send 32 byte, add 32 to i
	    stride = 32;
	  } else if ( ( to_i & 0x40 ) !=0 ) {
	    // 64-byte aligned address, send 64 byte, add 64 to i
	    stride = 64;
	  } else if ( ( to_i & 0x80 ) !=0 ) {
	    // 128-byte aligned address, send 128 byte, add 128 to i
	    stride = 128;
	  } else {
	    // 256-byte aligned address, send 256 byte, add 256 to i
	    stride = 256;
	  }
	  
	  while ( stride > remainder ) {
	    stride = stride / 2;
	  }

	  // Send a legally aligned and sized memory write to OCSE
	  // and wait for the ack.
	  my_afu->mem.addr = offset + i;
	  my_afu->mem.size = stride;
	  my_afu->mem.data = val + i;
	  my_afu->mem.state = LIBOCXL_REQ_REQUEST;
	  debug_msg("ocxl_lpc_write stride : %d bytes to lpc offset 0x%016lx", stride, offset + i);
	  while (my_afu->mem.state != LIBOCXL_REQ_IDLE)	/*infinite loop */
	    _delay_1ms();
	  
	  if (!my_afu->opened)
	    goto write_fail;
	}

	return 0;

 write_fail:
	errno = ENODEV;
	return -1;
}

// read 64 bytes from *data to offset in afu
//    offset is size aligned
//    *data is size aligned
//    byte_enable
ocxl_err ocxl_lpc_write_be(ocxl_afu_h afu, uint64_t offset, uint8_t *val, uint64_t byte_enable )
{

        struct ocxl_afu *my_afu;
	my_afu = (struct ocxl_afu *)afu;

	debug_msg("ocxl_lpc_write_be: to lpc offset 0x%016lx, with enable 0x%016lx", offset, byte_enable);

        if (!my_afu) {
	      warn_msg("NULL afu passed to ocxl_lpc_write_be");
	      goto write_fail;
	}

        if (!my_afu->lpc_mapped) {
	      warn_msg("afu lpc space is not mapped");
	      goto write_fail;
	}

        // check address alignment against size
	if ( offset & 0x3F ) {
	      warn_msg("ocxl_lpc_write_be: afu lpc address offset is not 64 byte aligned");
	      errno = EINVAL;
	      return -1;
	  }

	// Send memory write to OCSE - always 64 byte
	my_afu->mem.type = OCSE_LPC_WRITE_BE;
	my_afu->mem.addr = offset;
	my_afu->mem.size = 64;
	my_afu->mem.data = val;
	my_afu->mem.be = byte_enable;
	my_afu->mem.state = LIBOCXL_REQ_REQUEST;
	while (my_afu->mem.state != LIBOCXL_REQ_IDLE)	/*infinite loop */
		_delay_1ms();

	if (!my_afu->opened)
		goto write_fail;

	return 0;

 write_fail:
	errno = ENODEV;
	return -1;
}

// read size bytes from offset in afu to *data
//    size = arbitrary
//    offset is byte aligned
//    *data is byte aligned - but is likely 16 Byte aligned due to host OS behavior
ocxl_err ocxl_lpc_read(ocxl_afu_h afu, uint64_t offset, uint8_t *out, uint64_t size )
{

        struct ocxl_afu *my_afu;
	my_afu = (struct ocxl_afu *)afu;

        debug_msg("ocxl_lpc_read: %d bytes from lpc offset 0x%016lx", size, offset);

        if (!my_afu) {
	      warn_msg("NULL afu passed to ocxl_lpc_write");
	      goto read_fail;
	}

        if (!my_afu->lpc_mapped) {
	      warn_msg("afu lpc space is not mapped");
	      goto read_fail;
	}

        // check size legality - not required - any size is legal

        // check address alignment against size - not required - any alignment is legal

	my_afu->mem.type = OCSE_LPC_READ;
	my_afu->mem.be = 0;

	int i;
	uint64_t to_i;
	int stride;
	int remainder;

	for (i=0; i<size; i=i+stride) {
	  to_i = offset + i;
	  remainder = size - i;
	  if ( ( to_i & 0x1 ) !=0 ) {
	    // byte aligned address, send 1 byte, add 1 to i
	    stride = 1;
	  } else if ( ( to_i & 0x2 ) !=0 ) {
	    // 2-byte aligned address, send 2 byte, add 2 to i
	    stride = 2;
	  } else if ( ( to_i & 0x4 ) !=0 ) {
	    // 4-byte aligned address, send 4 byte, add 4 to i
	    stride = 4;
	  } else if ( ( to_i & 0x8 ) !=0 ) {
	    // 8-byte aligned address, send 8 byte, add 8 to i
	    stride = 8;
	  } else if ( ( to_i & 0x10 ) !=0 ) {
	    // 16-byte aligned address, send 16 byte, add 16 to i
	    stride = 16;
	  } else if ( ( to_i & 0x20 ) !=0 ) {
	    // 32-byte aligned address, send 32 byte, add 32 to i
	    stride = 32;
	  } else if ( ( to_i & 0x40 ) !=0 ) {
	    // 64-byte aligned address, send 64 byte, add 64 to i
	    stride = 64;
	  } else if ( ( to_i & 0x80 ) !=0 ) {
	    // 128-byte aligned address, send 128 byte, add 128 to i
	    stride = 128;
	  } else {
	    // 256-byte aligned address, send 256 byte, add 256 to i
	    stride = 256;
	  }
	  
	  while ( stride > remainder ) {
	    stride = stride / 2;
	  }

	  // Send a legally aligned and sized memory read to OCSE
	  // and wait for the ack.
	  my_afu->mem.addr = offset + i;
	  my_afu->mem.size = stride;
	  my_afu->mem.state = LIBOCXL_REQ_REQUEST;
	  debug_msg("ocxl_lpc_read stride : %d bytes from lpc offset 0x%016lx", stride, offset + i);
	  while (my_afu->mem.state != LIBOCXL_REQ_IDLE)	/*infinite loop */
	    _delay_1ms();
	  
	  // copy the data by copying the pointer
	  if (my_afu->mem.data == NULL) {
	    warn_msg("afu lpc memory not returned");
	    goto read_fail;
	  }

	  // we expect mem.data to be a full length buffer of the data we are reading
	  // that is, the length of mem.data should match mem.size
	  memcpy( out + i, my_afu->mem.data, my_afu->mem.size );
	  free( my_afu->mem.data );

	  if (!my_afu->opened)
	    goto read_fail;
	}

	return 0;

 read_fail:
	errno = ENODEV;
	return -1;
}

