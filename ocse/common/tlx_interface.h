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

#ifndef __tlx_interface_h__
#define __tlx_interface_h__ 1

#include "tlx_interface_t.h"

/* Call this at startup to reset all the event indicators */

void tlx_event_reset(struct AFU_EVENT *event);

/* Call this once after creation to initialize the AFU_EVENT structure and open
 * a socket conection to an AFU server.  This function initializes the TLX side
 * of the interface which is the client in the socket connection server_host
 * should be the name of the server hosting the simulation of the AFU and port
 * is the active port on that server */

int tlx_init_afu_event(struct AFU_EVENT *event, char *server_host, int port);

/* Call this to close the socket connection from either side */

int tlx_close_afu_event(struct AFU_EVENT *event);

/* Call this once after creation to initialize the AFU_EVENT structure.  This
 * function initializes the AFU side of the interface which is the server in
 * the socket connection */

int tlx_serv_afu_event(struct AFU_EVENT *event, int port);


/* Call this from ocse to set the initial tlx_afu credit values */

int tlx_afu_send_initial_credits(struct AFU_EVENT *event,
				 uint8_t  tlx_afu_cmd_initial_credit,
				 uint8_t  tlx_afu_resp_initial_credit,
				 uint8_t  tlx_afu_cmd_data_initial_credit,
				 uint8_t  tlx_afu_resp_data_initial_credit);


/* Call this from ocse to read the initial afu_tlx credit values */

int afu_tlx_read_initial_credits(struct AFU_EVENT *event,
		uint8_t * afu_tlx_cmd_initial_credit,
		uint8_t * cfg_tlx_initial_credit,
		uint8_t * afu_tlx_resp_initial_credit);


/* Call this from ocse to send a  response  to tlx/afu*/

int tlx_afu_send_resp(struct AFU_EVENT *event,
		 uint8_t tlx_resp_opcode,
		 uint16_t resp_afutag, uint8_t resp_code,
		 uint8_t resp_pg_size, uint8_t resp_dl,
#ifdef TLX4
		 uint32_t resp_host_tag, uint8_t resp_cache_state,
#endif
		 uint8_t resp_dp, uint32_t resp_addr_tag);


// TODO - DON"T CALL THIS YET - IT WON"T WORK
/* Call this from ocse to send response data to tlx/afu   assume can only send 64B
 * @ time to FIFO ?*/

int tlx_afu_send_resp_data(struct AFU_EVENT *event,
		 uint16_t resp_byte_cnt,
		 uint8_t resp_data_bdi,uint8_t * resp_data);



/* Call this from ocse to send both response & response data to tlx/afu  */

int tlx_afu_send_resp_and_data(struct AFU_EVENT *event,
		 uint8_t tlx_resp_opcode,
		 uint16_t resp_afutag, uint8_t resp_code,
		 uint8_t resp_pg_size, uint8_t resp_resp_dl,
#ifdef TLX4
		 uint32_t resp_host_tag, uint8_t resp_cache_state,
#endif
		 uint8_t resp_dp, uint32_t resp_addr_tag,
		 uint8_t resp_data_bdi,uint8_t * resp_data);

/* Call this from ocse to send an xlate_done cmd  to tlx/afu*/

int tlx_afu_send_posted_cmd(struct AFU_EVENT *event,
		 uint8_t tlx_resp_opcode,
		 uint16_t resp_afutag, uint8_t resp_code);


/* Call this from ocse to send a command to tlx/afu */

int tlx_afu_send_cmd(struct AFU_EVENT *event,
		 uint8_t tlx_cmd_opcode,
		 uint16_t cmd_capptag, uint8_t cmd_dl,
		 uint8_t cmd_pl, uint64_t cmd_be,
		 uint8_t cmd_end, //uint8_t cmd_t,
#ifdef TLX4
		 uint8_t cmd_os, uint8_t cmd_flag,
#endif
		 uint64_t cmd_pa);


/* Call this from ocse to send command data to tlx/afu   assume can only send 64B
 * @ time to FIFO */

int tlx_afu_send_cmd_data(struct AFU_EVENT *event,
		 uint16_t cmd_byte_cnt,
		 uint8_t cmd_data_bdi,uint8_t * cmd_data);


/* Call this from ocse to send a command with data (a write) to tlx/afu */
/* DO NOT USE for config_wr or config_rd commands */

int tlx_afu_send_cmd_and_data( struct AFU_EVENT *event,
			       uint8_t tlx_cmd_opcode,
			       uint16_t cmd_capptag, uint8_t cmd_dl,
			       uint8_t cmd_pl, uint64_t cmd_be,
			       uint8_t cmd_end,
#ifdef TLX4
			       uint8_t cmd_os, uint8_t cmd_flag,
#endif
			       uint64_t cmd_pa,
			       uint8_t cmd_data_bdi, uint8_t * cmd_data);

/* CALL THIS FOR CONFIG_RD/CONFIG_WR ONLY */
/* This will send config_rds and config_wr only. It will check and
 * decrement cfg_tlx_credits_available and, for config_wr cmds with data,
 * expects a pointer to a 4B buffer with up to 4 bytes of data located at
 * appropriate address offset in that buffer. This function extracts the write
 * data, and will send data (creating cmd_byte_cnt from cmd_pl) over socket
 * in same cycle as cmd. If cmd_pl = any other value than 0,1 or 2 there will
 * be NO DATA transferred. We rely on afu_driver to add the one cycle delay
 * for config data when sent to AFU.
*/

int tlx_afu_send_cfg_cmd_and_data(struct AFU_EVENT *event,
		 uint8_t tlx_cfg_opcode, uint16_t cfg_capptag,
		 uint8_t cfg_pl, uint8_t cfg_t,
		 uint64_t cfg_pa,
		 uint8_t cfg_data_bdi, uint8_t * cfg_data);


/* Call this from ocse to read AFU response. This reads both afu_tlx resp AND resp data interfaces */

int afu_tlx_read_resp_and_data(struct AFU_EVENT *event,
		    uint8_t * afu_resp_opcode, uint8_t * resp_dl,
		    uint16_t  * resp_capptag, uint8_t * resp_dp,
		    uint8_t * resp_data_is_valid, uint8_t * resp_code, uint8_t * rdata_bus, uint8_t * rdata_bdi);


/* Call this from ocse to read AFU response. This reads only the resp portion */

int afu_tlx_read_resp(struct AFU_EVENT *event,
		    uint8_t * afu_resp_opcode, uint8_t * resp_dl,
		    uint16_t  * resp_capptag, uint8_t * resp_dp,
		    uint8_t * resp_code);


/* Call this from ocse to read AFU response. This reads only the rdata portion */

int afu_tlx_read_resp_data(struct AFU_EVENT *event,
		    uint8_t * resp_data_is_valid, uint8_t * rdata_bus, uint8_t * rdata_bdi);


/* CALL THIS FOR CONFIG_RD/CONFIG_WR ONLY */
/* Call this from ocse to read AFU cfg response. This reads cfg_tlx_response interface
 * (AFU to TLX AP config). It expects the caller to provide the expected
 * resp_capptag to be matched with incoming responses.This will read resps
 * for config_rds and config_wr only. It will send back tlx_cfg_resp_ack and,
 * for config_rd cmds,expects a pointer to a 4B buffer so up to 4 bytes of data
 * can be copied to the appropriate address offset in that buffer.
*/

int afu_tlx_read_cfg_resp_and_data(struct AFU_EVENT *event,
		    uint8_t * cfg_resp_opcode, uint16_t  * cfg_resp_capptag,
		    uint16_t requested_capptag,
		    uint8_t * cfg_resp_data_is_valid,
		    uint8_t * cfg_resp_code, uint8_t * cfg_rdata_bus, uint8_t * cfg_rdata_bdi);


/* Call this from ocse to read AFU command. This reads both afu_tlx cmd AND cmd data interfaces */

int afu_tlx_read_cmd_and_data(struct AFU_EVENT *event,
  		    uint8_t * afu_cmd_opcode, uint16_t * cmd_actag,
  		    uint8_t * cmd_stream_id, uint8_t * cmd_ea_or_obj,
 		    uint16_t * cmd_afutag, uint8_t * cmd_dl,
  		    uint8_t * cmd_pl,
#ifdef TLX4
		    uint8_t * cmd_os,
#endif
		    uint64_t * cmd_be, uint8_t * cmd_flag,
 		    uint8_t * cmd_endian, uint16_t * cmd_bdf,
  	  	    uint32_t * cmd_pasid, uint8_t * cmd_pg_size, uint8_t * cmd_data_is_valid,
 		    uint8_t * cdata_bus, uint8_t * cdata_bdi);

/* Call this from ocse to read AFU command data ONLY.  */

int afu_tlx_read_cmd_data(struct AFU_EVENT *event,
  	  	    uint8_t * cmd_data_is_valid,
 		    uint8_t * cdata_bus, uint8_t * cdata_bdi);

/* Call this periodically to send events and clocking synchronization to AFU */
/* The comparable function, tlx_signal_tlx_model, is not defined here bc it
 * is called internally by tlx_get_tlx_events   */

int tlx_signal_afu_model(struct AFU_EVENT *event);


/* This function checks the socket connection for data from the external AFU
 * simulator. It needs to be called periodically to poll the socket connection.
 * It will update the AFU_EVENT structure.  It returns a 1 if there are new
 * events to process, 0 if not, -1 on error or close.
 * On a 1 return, the following functions should be called to retrieve the
 * individual events.
 * afu_tlx_read_cmd
 * afu_tlx_read_resp */

int tlx_get_afu_events(struct AFU_EVENT *event);


/* This function checks the socket connection for data from the external OCL
 * simulator. It  needs to be called periodically to poll the socket connection.
 * (every clock cycle)  It will update the AFU_EVENT structure and returns a 1
 * if there are new events to process */

int tlx_get_tlx_events(struct AFU_EVENT *event);


/* Call this from AFU to set the initial afu tlx_credit values */

int afu_tlx_send_initial_credits(struct AFU_EVENT *event,
		uint8_t afu_tlx_cmd_initial_credit,
		uint8_t cfg_tlx_initial_credit,
		uint8_t afu_tlx_resp_initial_credit);


/* Call this from AFU to read the initial tlx_afu credit values */

int tlx_afu_read_initial_credits(struct AFU_EVENT *event,
				 uint8_t * tlx_afu_cmd_initial_credit,
				 uint8_t * tlx_afu_resp_initial_credit,
				 uint8_t * tlx_afu_cmd_data_initial_credit,
				 uint8_t * tlx_afu_resp_data_initial_credit);


/* Call this on the AFU side to send a response to ocse.  */

int afu_tlx_send_resp(struct AFU_EVENT *event,
 		 uint8_t afu_resp_opcode,
 		 uint8_t resp_dl, uint16_t resp_capptag,
 		 uint8_t resp_dp, uint8_t resp_code);


// TODO - DON"T CALL THIS YET - IT WON"T WORK
/* Call this from afu to send response data to ocse   assume can only send 64B
 * @ time to FIFO ?*/

int afu_tlx_send_resp_data(struct AFU_EVENT *event,
		 uint8_t DATA_RESP_CONTINUATION,
		 uint8_t rdata_bdi,uint8_t resp_dp,
		 uint8_t resp_dl,uint8_t * rdata_bus);


/* Call this on the AFU side to send a response and response data to ocse.  */

int afu_tlx_send_resp_and_data(struct AFU_EVENT *event,
 		 uint8_t afu_resp_opcode,
 		 uint8_t resp_dl, uint16_t resp_capptag,
 		 uint8_t resp_dp, uint8_t resp_code,
  		 uint8_t rdata_valid, uint8_t * rdata_bus,
 		 uint8_t rdata_bdi);


/* CALL THIS FOR CONFIG_RD/CONFIG_WR ONLY */
/* Call this from AFU to send config_wr response and config_rd response and data.
 * This updates both afu_tlx resp and afu_cfg_rdata bus. It expects caller to provide
 * resp_opcode, resp_capptag, resp_dl, resp_dp, afu_cfg_rdata_bdi & up to 4B data.
 * Right now, there isn't a good alternative, so caller has to provide afu_cfg_resp_data_byte_cnt.
 * Expectation is that default is 4, but could be 1 or 2 in future. <- if not true, then can drop this req.
 * For responses w/o data, please set afu_cfg_resp_data_byte_cnt = 0
*/
int afu_cfg_send_resp_and_data(struct AFU_EVENT *event,
 		 uint8_t cfg_resp_opcode,
 		 uint16_t cfg_resp_capptag, uint8_t cfg_resp_code,
		 uint16_t afu_cfg_resp_data_byte_cnt,
		 //uint8_t cfg_rdata_offset,
  		 uint8_t cfg_rdata_valid, uint8_t * cfg_rdata_bus,
 		 uint8_t cfg_rdata_bdi);


/* Call this on the AFU side to send a command to ocse */

int afu_tlx_send_cmd(struct AFU_EVENT *event,
		 uint8_t afu_cmd_opcode, uint16_t cmd_actag,
  	 	 uint8_t cmd_stream_id, uint8_t * cmd_ea_or_obj,
  		 uint16_t cmd_afutag, uint8_t cmd_dl,
  		 uint8_t cmd_pl,
#ifdef TLX4
  		 uint8_t cmd_os,     /* 1 bit ordered segment CAPI 4 */
#endif
  	 	 uint64_t cmd_be,uint8_t cmd_flag,
		 uint8_t cmd_endian, uint16_t cmd_bdf,
 		 uint32_t cmd_pasid, uint8_t cmd_pg_size);


// TODO - DON"T CALL THIS YET - IT WON"T WORK
/* Call this from afu to send command data to ocse   assume can only send 64B
 * @ time to FIFO ?*/

int afu_tlx_send_cmd_data(struct AFU_EVENT *event, uint8_t cdata_bdi, uint8_t * cdata_bus);


/* Call this on the AFU side to send a command and cmd data to ocse */

int afu_tlx_send_cmd_and_data(struct AFU_EVENT *event,
		 uint8_t afu_cmd_opcode, uint16_t cmd_actag,
  	 	 uint8_t cmd_stream_id, uint8_t * cmd_ea_or_obj,
  		 uint16_t cmd_afutag, uint8_t cmd_dl,  /* combine dl and pl ??? */
  		 uint8_t cmd_pl,
#ifdef TLX4
  		 uint8_t cmd_os,     /* 1 bit ordered segment CAPI 4 */
#endif
  	 	 uint64_t cmd_be, uint8_t cmd_flag,
		 uint8_t cmd_endian, uint16_t cmd_bdf,
 		 uint32_t cmd_pasid, uint8_t cmd_pg_size,
  		 uint8_t * cdata_bus, uint8_t cdata_bdi);



/* Call this from AFU to read ocse (CAPP/TL) response. This reads just tlx_afu resp interface */

int tlx_afu_read_resp(struct AFU_EVENT *event,
		 uint8_t * tlx_resp_opcode,
		 uint16_t * resp_afutag, uint8_t * resp_code,
		 uint8_t * resp_pg_size, uint8_t * resp_resp_dl,
#ifdef TLX4
		 uint32_t * resp_host_tag, uint8_t * resp_cache_state,
#endif
		 uint8_t * resp_dp, uint32_t * resp_addr_tag);


/* Call this from AFU to request data on the response data interface */
int afu_tlx_resp_data_read_req(struct AFU_EVENT *event,
		 uint8_t resp_rd_req, uint8_t resp_rd_cnt);


/* Call this from AFU to read data on the response data interface */
int tlx_afu_read_resp_data(struct AFU_EVENT *event,
		 uint8_t * resp_data_bdi,uint8_t * resp_data);


/* Call this from AFU to read ocse (CAPP/TL) command.This reads tlx_afu cmd interface */

int tlx_afu_read_cmd(struct AFU_EVENT *event,
		 uint8_t * tlx_cmd_opcode,
		 uint16_t * cmd_capptag, uint8_t * cmd_dl,
		 uint8_t * cmd_pl, uint64_t * cmd_be,
		 uint8_t * cmd_end, //uint8_t * cmd_t,
#ifdef TLX4
		 uint8_t * cmd_flag,  /* used for atomics from host CAPI 4 */
  		 uint8_t * cmd_os,     /* 1 bit ordered segment CAPI 4 */
#endif
		 uint64_t * cmd_pa);


/* Call this from AFU to request data on the command data interface */
int afu_tlx_cmd_data_read_req(struct AFU_EVENT *event,
		 uint8_t cmd_rd_req, uint8_t cmd_rd_cnt);


/* Call this from AFU to read data on the command data interface */
int tlx_afu_read_cmd_data(struct AFU_EVENT *event,
		 uint8_t * cmd_data_bdi,uint8_t * cmd_data);


/* CALL THIS FOR CONFIG_RD/CONFIG_WR ONLY */
/* Call this from AFU to read config_rd commands and config_wr commands and data.
 * This updates both afu_tlx cmd and afu_cfg_data_bus. It expects caller to provide
 * pointers for everything expected, including pointer to data buffer for up to 4B data.
 * This function checks to see if cmd in afu_event is a config cmd and returns a nonzero
 * value if no config_cmd is present. If this is a config_wr cmd, data will be present
 * and will be provided back to caller, along with config cmd paramters. cmd_pl will
 * contain the number of data bytes, encoded per spec (0= 1 byte, 1= 2, 2= 4).
 * We rely on afu_driver to add the one cycle delay for config data when sent to AFU.
*/

int tlx_cfg_read_cmd_and_data(struct AFU_EVENT *event,
		 uint8_t * cmd_data_bdi, uint8_t * cmd_data_bus,
		 uint8_t * tlx_cmd_opcode, uint16_t * cmd_capptag,
		 uint8_t * cmd_pl, uint8_t * cmd_t,
		 uint64_t * cmd_pa);



#endif
