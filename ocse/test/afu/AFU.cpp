/*
 * Copyright 2015,2017 International Business Machines
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

#include "AFU.h"

#include <string>
#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <time.h>

using std::string;
using std::cout;
using std::endl;
using std::vector;

#define GLOBAL_CONFIG_OFFSET 0x400
#define CONTEXT_SIZE 0x400
#define CONTEXT_MASK (CONTEXT_SIZE - 1)

uint8_t memory[256];
uint8_t next_cmd = 0;
uint8_t retry_cmd = 0;
uint8_t interrupt_pending = 0;
uint8_t read_resp_completed = 0;
uint8_t write_resp_completed = 0;
uint8_t cmd_ready = 1;
uint8_t status_data[256];
uint8_t status_resp_valid = 0;
uint8_t status_updated = 0;
uint8_t insert_cycle = 0;
uint8_t afu_enable_reset = 0;
uint32_t wr_config_data;
uint32_t bar_h0, bar_l0, bar_h1, bar_l1, bar_h2, bar_l2;
uint64_t bar = 0x00000000ll;
uint8_t enable_bar = 0;
uint8_t read_status_resp = 0;
uint8_t write_status_resp = 0;
uint32_t write_status_tag;
uint32_t afu_function = 0;
uint16_t gBDF = 0; 
uint16_t gACTAG = 0;
uint16_t gDUT = 0;
uint16_t other_resp_completed = 0;

AFU::AFU (int port, string filename, bool parity, bool jerror):
    descriptor (filename),
    context_to_mc ()
{

    // initializes AFU socket connection as server
    if (tlx_serv_afu_event (&afu_event, port) == TLX_BAD_SOCKET)
        error_msg ("AFU: unable to create socket");

    if (jerror)
	set_jerror_not_run = true;
    else
	set_jerror_not_run = false;
    set_seed ();

    state = IDLE;
    config_state = IDLE;
    mem_state = IDLE;
    resp_state = IDLE;
    debug_msg("AFU: Set AFU and CONFIG state = IDLE");
    afu_event.afu_tlx_resp_initial_credit = MAX_AFU_TLX_RESP_CREDITS; 
    afu_event.afu_tlx_cmd_initial_credit = MAX_AFU_TLX_CMD_CREDITS;
    afu_tlx_send_initial_credits(&afu_event, MAX_AFU_TLX_CMD_CREDITS,
		10, MAX_AFU_TLX_RESP_CREDITS);
    debug_msg("AFU: Send initial afu cmd and resp credits to ocse");
    reset ();
}

bool 
AFU::afu_is_enabled()
{
    uint32_t  enable;
    enable = descriptor.get_vsec_reg(afu_function+0x50c);
    if(enable & 0x01000000)
	return true;
    else
	return false;	
}

bool
AFU::afu_is_reset()
{
    uint32_t reset;
 
    reset = descriptor.get_afu_desc_reg(0x50c);
    if(reset & 0x00800000)
	return true;
    else
	return false;   
}
void
AFU::start ()
{
    uint32_t cycle = 0;
    uint8_t  initial_credit_flag = 0;
    uint16_t index=0;

    while (1) {
        fd_set watchset;

        FD_ZERO (&watchset);
        FD_SET (afu_event.sockfd, &watchset);
        select (afu_event.sockfd + 1, &watchset, NULL, NULL, NULL);

	// check socket if there are new events from ocse to process
	printf("getting tlx events\n");
        int rc = tlx_get_tlx_events (&afu_event);

        //info_msg("Cycle: %d", cycle);
        ++cycle;

	// connection dropped
        if (rc < 0) {
            info_msg ("AFU: connection lost");
            break;
        }

	// get TLX initial cmd and data credits run once
	if(initial_credit_flag == 0) {
	    debug_msg("AFU: afu read initial credit");
	    if(tlx_afu_read_initial_credits(&afu_event, &tlx_afu_cmd_max_credit,
		&tlx_afu_data_max_credit) != TLX_SUCCESS) {
		error_msg("AFU: Failed tlx_afu_read_initial_credits");
	    }
	    TagManager::reset_tlx_credit(tlx_afu_cmd_max_credit, tlx_afu_data_max_credit);
	    info_msg("AFU: Receive TLX cmd and data initial credits");
    	    debug_msg("AFU:  tlx_afu_cmd_max_credit = %d", tlx_afu_cmd_max_credit);
    	    debug_msg("AFU:  tlx_afu_data_max_credit = %d", tlx_afu_data_max_credit);
	    initial_credit_flag = 1;
	}

	// no new events to be processed
        if (rc <= 0)		
            continue;
	
	// Return TLX credit
	if(afu_event.tlx_afu_resp_credit) {
	    TagManager::release_tlx_credit(RESP_CREDIT);
	    //afu_event.tlx_afu_resp_credit = 0;
	}
	if(afu_event.tlx_afu_resp_data_credit) {
	    TagManager::release_tlx_credit(RESP_DATA_CREDIT);
	    //afu_event.tlx_afu_resp_data_credit = 0;
	}
	if(afu_event.tlx_afu_cmd_credit) {
	    TagManager::release_tlx_credit(CMD_CREDIT);
	    //afu_event.tlx_afu_cmd_credit = 0;
	}
	if(afu_event.tlx_afu_cmd_data_credit) {
	    TagManager::release_tlx_credit(CMD_DATA_CREDIT);
	    //afu_event.tlx_afu_cmd_data_credit = 0;
	}
	// process config commands
	if (afu_event.tlx_cfg_valid) {
	    debug_msg("AFU: Received config command 0x%x", afu_event.tlx_cfg_opcode);
	    resolve_cfg_cmd();
	    //afu_event.cfg_tlx_credit_return = 1;
	}
	// process tlx commands
	if (afu_event.tlx_afu_cmd_valid || afu_event.tlx_cfg_valid) {
	    debug_msg("AFU: Received TLX command 0x%x", afu_event.tlx_afu_cmd_opcode);
	    debug_msg("AFU: Process TLX command");
	    resolve_tlx_afu_cmd();
	    afu_event.afu_tlx_cmd_credit = 1;	// return TLX cmd credit
	}
	// process tlx response
	if (afu_event.tlx_afu_resp_valid) {
	    debug_msg("AFU: Received TLX response 0x%x", afu_event.tlx_afu_resp_opcode);
	    resolve_tlx_afu_resp();
	    afu_event.afu_tlx_resp_credit = 1;	// return TLX resp credit
	    afu_event.tlx_afu_resp_valid = 0;
	}
	// process tlx config response
	if (afu_event.tlx_cfg_resp_ack) {
	    debug_msg("AFU: Received TLX config response 0x%x", afu_event.tlx_afu_resp_opcode);
	    afu_event.cfg_tlx_credit_return = 1;	
	    afu_event.afu_tlx_credit_req_valid = 1;	
	    afu_event.tlx_cfg_resp_ack = 0;
	}
	// process tlx response data
	//if(afu_event.tlx_afu_resp_data_valid && mem_state == WAITING_FOR_DATA) {
	if(afu_event.tlx_afu_resp_data_valid && resp_state == WAITING_FOR_DATA) {
	    debug_msg("AFU: Received TLX response data");
	    resolve_tlx_afu_resp();
	    afu_event.tlx_afu_resp_data_credit = 1;	// return TLX resp data credit
	    afu_event.tlx_afu_resp_data_valid = 0;
	}
	//if(afu_event.tlx_afu_resp_data_valid && mem_state != WAITING_FOR_DATA) {
	if(afu_event.tlx_afu_resp_data_valid && resp_state != WAITING_FOR_DATA) {
	    afu_event.tlx_afu_resp_data_credit = 1;	// return TLX resp data credit
	}
	// configuration write response
	//if (afu_event.tlx_afu_cmd_data_valid && config_state == READY) {
	if (afu_event.tlx_cfg_resp_ack && config_state == READY) {
	    info_msg("AFU: calling tlx_afu_config_write");
	    tlx_afu_config_write();
	}

	// partial write mem response
	if (afu_event.tlx_afu_cmd_data_valid && mem_state == READY) {
	    info_msg("AFU: calling tlx_pr_wr_mem");
	    tlx_pr_wr_mem();
 	}

	//printf("checking afu reset bit\n");
	//  reset AFU
	if(afu_enable_reset) {
	    if(afu_is_reset()) {
	    	debug_msg("AFU is resetting");
		afu_enable_reset = 0;
	    }
	}
	// enable AFU
	if(state == IDLE) {
	    if(afu_enable_reset) {
	    	printf("checking afu enable bit\n");
	    	if(afu_is_enabled()) {
	    	    debug_msg("AFU is enabled");
	    	    debug_msg("AFU: set state = READY");
	    	    state = READY;
		    afu_enable_reset = 0;
	        }
	    }
	}
	// get machine context and create new MachineController
	else if(state == READY) {
	    if(get_machine_context()) {
		if(gDUT==1) {
		    printf("gDUT = %d\n", gDUT);
		    gBDF = 1;
		    gACTAG++;
		}
		else if(gDUT == 2) {
		    printf("gDUT = %d\n", gDUT);
		    gBDF++;
		    gACTAG++;
	  	}
		printf("AFU: gBDF = %d gACTAG = %d\n", gBDF, gACTAG);
		afu_event.afu_tlx_cmd_bdf = gBDF;
		afu_event.afu_tlx_cmd_actag = gACTAG;
		printf("AFU: request afu assign actag\n");
		request_assign_actag();
	    	printf("AFU: set state = RUNNING\n");
	    	state = RUNNING;
		//debug1
		read_resp_completed = 0;
		write_resp_completed = 0;
		other_resp_completed = 0;
	    }
	}
  	
        // generate commands, initial read_resp_completed=0; write_resp_completed=0
	// initial cmd_ready=1; next_cmd=0, other_resp_competed=0;
        else if (state == RUNNING) {
	    if((read_resp_completed ||  write_resp_completed || other_resp_completed) &&
		!(next_cmd) && !(cmd_ready)) {
		printf("AFU: writing app status\n");
		printf("AFU: read resp = %d write resp = %d other resp = %d\n", read_resp_completed,
			write_resp_completed, other_resp_completed);
		write_app_status(status_address, 0x0);
		read_resp_completed = 0;
		write_resp_completed = 0;
		other_resp_completed = 0;
		next_cmd = 1;
	    }
	    else if(next_cmd) {
	 	if(write_status_resp) {
		    debug_msg("AFU: reading app status");	
		    read_app_status(status_address);
		    write_status_resp = 0;
		}
		else if(read_status_resp) {
		    if(status_data[0] == 0xff) {
			printf("AFU: status data = 0x%x\n", status_data[0]);
		    	debug_msg("AFU: get next cmd from app");
		    	next_cmd = 0;
		    	cmd_ready = 1;
			read_status_resp = 0;
			read_resp_completed = 0; //debug1
		    	get_machine_context();
		    }
		    else if(status_data[0] == 0x0) {
			printf("AFU: status data = 0x%x\n", status_data[0]);
			debug_msg("AFU: reading app status");
			read_app_status(status_address);
			printf("AFU: waiting for read resp\n");
		    }
		    else if(status_data[0] == 0x55) {
			printf("AFU: status data = 0x%x\n", status_data[0]);
			printf("AFU: test is done\n");
			printf("AFU: set AFU state to READY\n");
			write_app_status(status_address, 0x00);
			state = READY;
		    }
		}
		else {
		    debug_msg("AFU: waiting for new cmd from app");
		}
	    }
            else if(cmd_ready) {
		cmd_ready = 0;
		if(context_to_mc.size () != 0) {
			printf("AFU: context to mc size = %d\n", context_to_mc.size());
                	std::map < uint16_t, MachineController * >::iterator prev = highest_priority_mc;
                    do {
                    if(highest_priority_mc == context_to_mc.end ())
                        highest_priority_mc = context_to_mc.begin ();
		// calling MachineController send command
			printf("AFU: context = %d mc = 0x%x\n", highest_priority_mc->first, highest_priority_mc->second);
	
                    	if(highest_priority_mc->second->send_command(&afu_event, cycle)) {
                        	++highest_priority_mc;
                        	break;
                    	    }
                        ++highest_priority_mc;
                	} while (++highest_priority_mc != prev);
		}
            }
	    else if(retry_cmd) {
		printf("AFU: attempt retry command\n");
		highest_priority_mc->second->resend_command(&afu_event, cycle);
		retry_cmd = 0;
	    }
        }
        else if (state == RESET) {
	    debug_msg("AFU: resetting");
	    debug_msg("AFU: set AFU state = READY");
      	    reset ();
	    state = READY;
	}
        else if (state == WAITING_FOR_LAST_RESPONSES) {
            //debug_msg("AFU: waiting for last responses");
            bool all_machines_completed = true;
	    uint32_t  response;
	    uint32_t  offset;
	  
	    response = 0x00000000;
	    offset  = 0x1008 + 0x1000 * afu_event.afu_tlx_cmd_pasid;
	    descriptor.set_mmio_mem(offset, (char*)&response, 4);
            for (std::map < uint16_t, MachineController * >::iterator it =
                        context_to_mc.begin (); it != context_to_mc.end (); ++it)
            {
                if (!(it->second)->all_machines_completed ())
                    all_machines_completed = false;
            }

            if (all_machines_completed) {
                debug_msg ("AFU: machine completed");

                reset_machine_controllers ();
              
                state = HALT;
		debug_msg("AFU: state = HALT");
            }
        }
    }
}

AFU::~AFU ()
{
    // close socket connection
    tlx_close_afu_event (&afu_event);

    for (std::map < uint16_t, MachineController * >::iterator it =
                context_to_mc.begin (); it != context_to_mc.end (); ++it)
        delete it->second;
  
    context_to_mc.clear ();
}


void
AFU::reset ()
{
    for (uint32_t i = 0; i < 3; ++i)
        global_configs[i] = 0;

    reset_delay = 0;

    reset_machine_controllers ();
}

void
AFU::reset_machine_controllers ()
{
    TagManager::reset ();

    for (std::map < uint16_t, MachineController * >::iterator it =
                context_to_mc.begin (); it != context_to_mc.end (); ++it)
        delete it->second;

    context_to_mc.clear ();

}

// get machine context from mmio and create new MachineController
bool 
AFU::get_machine_context()
{
    MachineController *mc = NULL;
    uint64_t  data;
    uint8_t  size = 8;
    uint16_t  context, machine_number, i;
    uint32_t  mmio_base;

    debug_msg("AFU: get machine context");
    machine_number = 0;
    for(context=0; context<4; context++) {
	descriptor.get_mmio_mem(0x1000*context+machine_number, (char*)&data, size);
	if(data) {
	    mmio_base = 0x1000*context + machine_number;
	    afu_event.afu_tlx_cmd_pasid = context;
	    context = (uint16_t)((data & 0x0000FFFF00000000LL) >> 32);
	    debug_msg("AFU: context = %d", context);
	    if(context == 0) {
		afu_event.afu_tlx_cmd_pasid = context;
		//afu_event.afu_tlx_cmd_bdf = 0x0001;
		context_to_mc[context] = new MachineController(context);
	    }
	    else if(context == 1) {
		printf("AFU: clear context to mc\n");
		context_to_mc.clear();
		printf("AFU: context = %d\n", context);
		afu_event.afu_tlx_cmd_pasid = context;
	    	context_to_mc[context] = new MachineController(context);
	    }
	    highest_priority_mc = context_to_mc.end();
	    mc = context_to_mc[context];
	    printf("AFU: context_to_mc = %p size = %d\n", mc, context_to_mc.size());
	    for(i=0; i< 4; i++) {
		descriptor.get_mmio_mem(mmio_base+i*8, (char*)&data, size);
		if(i==1) {
		    status_address = (uint8_t *)(data >> 32);
		    debug_msg("AFU: get status address = %p", status_address);
		}
		mc->change_machine_config(i, machine_number, data);
	    }
	    return true;
	}
    }
    return false;		
}

void
AFU::request_assign_actag()
{
    uint8_t ea[12];
    uint32_t afutag;

    TagManager::request_tag(&afutag);
    printf("afu_tlx_cmd_bdf = %d\n", afu_event.afu_tlx_cmd_bdf);
    afu_event.afu_tlx_cmd_afutag = afutag;
    printf("AFU: afu_tag = 0x%x\n", afutag);
    printf("AFU: assign actag BDF = 0x%x PASID = 0x%x\n", afu_event.afu_tlx_cmd_bdf,
    afu_event.afu_tlx_cmd_pasid);
    if(afu_tlx_send_cmd(&afu_event,
		 AFU_CMD_ASSIGN_ACTAG, afu_event.afu_tlx_cmd_actag,
  	 	 0, ea, afu_event.afu_tlx_cmd_afutag, 1, 3,
#ifdef TLX4
  		 0,     /* 1 bit ordered segment CAPI 4 */
#endif
  	 	 0, 0, 0, afu_event.afu_tlx_cmd_bdf,
 		 afu_event.afu_tlx_cmd_pasid, 0) != TLX_SUCCESS) {
	    printf("FAILED: request_assign_actag\n");
	}
	else {
	    printf("PASS: request_assign_actag\n");
	    //afu_event.afu_tlx_cmd_valid = 0;
	}
}

// process commands from ocse to AFU
void 
AFU::resolve_tlx_afu_cmd()
{
    uint8_t cmd_opcode;
    uint8_t cmd_data_bus[64];
    uint16_t cmd_capptag;
    uint8_t  cmd_dl, cmd_pl, cmd_end, cmd_t, cmd_data_bdi;
    uint64_t cmd_be;
    int rc;
#ifdef	TLX4
    uint8_t  cmd_flag;
    uint8_t  cmd_os;
#endif
    uint64_t  cmd_pa;
    
    if (tlx_afu_read_cmd(&afu_event, &cmd_opcode, &cmd_capptag, 
		&cmd_dl, &cmd_pl, &cmd_be, &cmd_end, //&cmd_t, 
#ifdef	TLX4
		&cmd_os, &cmd_flag,
#endif
		&cmd_pa) != TLX_SUCCESS) {
	error_msg("Failed: tlx_afu_read_cmd");
    }

    afu_event.tlx_afu_cmd_opcode = cmd_opcode;
    afu_event.afu_tlx_resp_capptag = cmd_capptag;
    info_msg("AFU:resolve_tlx_afu_cmd");
    info_msg("cmd_opcode = 0x%x", cmd_opcode);
    info_msg("cmd_pa = 0x%08lx", cmd_pa);
    info_msg("cmd_capptag = 0x%x", cmd_capptag);
    info_msg("cmd_pl = 0x%x", cmd_pl);

    switch (cmd_opcode) {
	case TLX_CMD_NOP:
	case TLX_CMD_XLATE_DONE:
	    printf("AFU: receive xlate done command\n");
	    if(interrupt_pending) {
		interrupt_pending = 0;
		retry_cmd = 1;
	    }
	    break;
	case TLX_CMD_RETURN_ADR_TAG:
	case TLX_CMD_INTRP_RDY:
	    break;
	case TLX_CMD_RD_MEM:
	case TLX_CMD_PR_RD_MEM:
	    debug_msg("AFU: pr_rd_mem cmd:");
	    tlx_pr_rd_mem();
	    break;
	case TLX_CMD_AMO_RD:
	case TLX_CMD_AMO_RW:
	case TLX_CMD_AMO_W:
	case TLX_CMD_WRITE_MEM:
	    debug_msg("AFU: wr_mem cmd");
	    tlx_pr_wr_mem();
	    break;
	case TLX_CMD_WRITE_MEM_BE:
	case TLX_CMD_WRITE_META:
	    break;
	case TLX_CMD_PR_WR_MEM:
	    debug_msg("AFU: pr_wr_mem cmd:");
	    tlx_pr_wr_mem();
	    break;
	case TLX_CMD_FORCE_EVICT:
	case TLX_CMD_FORCE_UR:
	case TLX_CMD_WAKE_AFU_THREAD:
	    break;
	case TLX_CMD_CONFIG_READ:
	    info_msg("AFU: Configuration Read command");
	    info_msg("AFU: calling tlx_afu_config_read");
	    afu_event.cfg_tlx_resp_capptag = cmd_capptag;
	    tlx_afu_config_read();
	    break;
	case TLX_CMD_CONFIG_WRITE:
	    info_msg("AFU: Configuration Write command");
	    info_msg("AFU: calling tlx_afu_config_write");
	    afu_event.cfg_tlx_resp_capptag = cmd_capptag;
	    tlx_afu_config_write();
	    break;
	default:
	    break;
    }
}

// process responses from ocse to AFU
void
AFU::resolve_tlx_afu_resp()
{
    uint8_t tlx_resp_opcode;
    uint16_t resp_afutag;
    uint8_t  resp_code;
    uint8_t  resp_pg_size;
    uint8_t  resp_resp_dl;
#ifdef	TLX4
    uint32_t resp_host_tag;
    uint8_t  resp_cache_state;
#endif
    uint8_t  resp_dp;
    uint32_t resp_addr_tag;
    uint8_t  cmd_rd_req, cmd_rd_cnt;
    uint8_t  resp_data_bdi;
    uint8_t  cdata_bad;
    uint8_t  i;

    tlx_resp_opcode = 0;
    resp_data_bdi = afu_event.tlx_afu_resp_data_bdi;

    //if(mem_state == WAITING_FOR_DATA) {
    if(resp_state == WAITING_FOR_DATA) {
	debug_msg("AFU: calling tlx_afu_read_resp_data");
	if(status_resp_valid) {
	    debug_msg("AFU: read_resp_data for status_resp_valid");
	    status_resp_valid = 0;
	    tlx_afu_read_resp_data(&afu_event, &resp_data_bdi, status_data);
	    printf("status data = 0x");
	    for(i=0; i<64; i++) {
		printf("%02x", (uint8_t)status_data[i]);
	    }
	    printf("\n");
	}
	else {
	    debug_msg("AFU: read_resp_data for memory");
	    tlx_afu_read_resp_data(&afu_event, &resp_data_bdi, memory);
	    printf("memory = 0x");
	    for(i=0; i<64; i++) {
		printf("%02x", (uint8_t)memory[i]);
	    }
	    printf("\n");
	}
	//debug_msg("AFU: set mem_state = IDLE");
	//mem_state = IDLE;
	debug_msg("AFU: set resp_state = IDLE");
	resp_state = IDLE;
    }
    else {
    	tlx_afu_read_resp(&afu_event, &tlx_resp_opcode, &resp_afutag, 
		&resp_code, &resp_pg_size, &resp_resp_dl,
#ifdef	TLX4
		&resp_host_tag, &resp_cache_state,
#endif
		&resp_dp, &resp_addr_tag);
 
//	read_resp_completed = 1;	//debug1
    }

    switch (tlx_resp_opcode) {
	case TLX_RSP_NOP:
	    break;
	case TLX_RSP_RET_TLX_CREDITS:
	    break;
	case TLX_RSP_TOUCH_RESP:
	    break;
	case TLX_RSP_READ_RESP:
	    read_resp_completed = 1;	// debug1
	    debug_msg("AFU: read_resp: calling afu_tlx_resp_data_read_req");
	    read_status_resp = 1;
	    cmd_rd_req = 0x1;	
	    cmd_rd_cnt = 0x1; 	// 0=512B, 1=64B, 2=128B
	    if(afu_tlx_resp_data_read_req(&afu_event, cmd_rd_req, cmd_rd_cnt) != TLX_SUCCESS) {
		error_msg("AFU: FAILED tlx_afu_read_resp");
	    }
	    else {
		//debug_msg("AFU: set mem_state = WAITING_FOR_DATA");
		//mem_state = WAITING_FOR_DATA;
		debug_msg("AFU: set resp_state = WAITING_FOR_DATA");
		resp_state = WAITING_FOR_DATA;
	    }

	    break;
	case TLX_RSP_UGRADE_RESP:
	    break;
	case TLX_RSP_READ_FAILED:
	    printf("AFU: TLX read response failed\n");
	    break;
	case TLX_RSP_CL_RD_RESP:
	    break;
	case TLX_RSP_WRITE_RESP:
	    printf("AFU: received response write\n");
	    printf("memory = 0x");
	    for(i=0; i<64; i++) {
		//memory[i]=i;
	 	printf("%02x", (uint8_t)memory[i]);
	    }
	    printf("\n");
	    cdata_bad = 0;
	    afu_tlx_send_cmd_data(&afu_event, cdata_bad, memory);
	    write_resp_completed = 1;
	    printf("write status tag = 0x%x\n", write_status_tag);
	    printf("afutag = 0x%x\n", afu_event.tlx_afu_resp_afutag);
	    if(write_status_tag == afu_event.tlx_afu_resp_afutag)
	    	write_status_resp = 1;	
	    break;
	case TLX_RSP_WRITE_FAILED:
	    printf("AFU: TLX write response failed\n");
	    break;
	case TLX_RSP_MEM_FLUSH_DONE:
	case TLX_RSP_INTRP_RESP:
	    printf("Receive interrupt response code = 0x%x\n",resp_code);
	    switch(resp_code) {
		case 0x0:
		    printf("AFU: Interrupt request accepted\n");
		    other_resp_completed = 1;
		    break;
		case 0x2:
		    printf("AFU: Retry request\n");
		    retry_cmd = 1;
		    break;
		case 0x4:
		    printf("AFU: Interrupt pending\n");
		    interrupt_pending = 1;
		    break;
		case 0xe:
		    printf("AFU: Interrupt failed\n");
		    break;
		default:
		    break;
		}
	    //printf("AFU: write_app_status\n");
	    //write_app_status(status_address, 0x0);
	    break;
	case TLX_RSP_READ_RESP_OW:
	case TLX_RSP_READ_RESP_XW:
	case TLX_RSP_WAKE_HOST_RESP:
	    printf("Received wake host response code = 0x%x\n", resp_code);
	    write_app_status(status_address, 0x0);
	    break;
	case TLX_RSP_CL_RD_RESP_OW:
	default:
	    break;
    }
}

void
AFU::resolve_cfg_cmd()
{
    uint8_t cfg_opcode;
    uint8_t cfg_data_bus[64];
    uint16_t cfg_capptag;
    uint8_t  cfg_pl, cfg_data_bdi, cfg_t;
    uint64_t  cfg_pa;
    int rc;

    rc = tlx_cfg_read_cmd_and_data(&afu_event, &cfg_data_bdi, cfg_data_bus, &cfg_opcode,
	&cfg_capptag, &cfg_pl, &cfg_t, &cfg_pa);
     if(rc != TLX_SUCCESS) {
	printf("rc = 0x%x\n", rc);
    }
    
    afu_event.cfg_tlx_resp_capptag = cfg_capptag;

    // configuration write cmd
    if(cfg_opcode == TLX_CMD_CONFIG_WRITE) {
	info_msg("AFU: configuration write cmd");
	memcpy(&wr_config_data, &cfg_data_bus, 4);
	printf("AFU: wr_config_data = 0x%x\n", wr_config_data);
	tlx_afu_config_write();
    }
    else if(cfg_opcode == TLX_CMD_CONFIG_READ) {
    	info_msg("AFU: calling tlx_afu_config read");
    	tlx_afu_config_read();
    }

}

void
AFU::tlx_afu_config_read()
{
    uint32_t vsec_offset, vsec_data;
    uint16_t bdf;
    uint8_t resp_pl, resp_opcode, rdata_bad;
    uint16_t resp_capptag, byte_cnt;
    uint8_t  resp_code, data_size;
    uint8_t rdata_valid, byte_offset;
    //uint64_t  cmd_pa;
    int rc;

    info_msg("AFU:tlx_afu_config_read");
    resp_opcode = 0x01;
    //resp_dl = 0x01;	// length 64 byte
    byte_cnt = 4;		
    resp_capptag = afu_event.cfg_tlx_resp_capptag;
    resp_pl = afu_event.tlx_cfg_pl;
    vsec_offset = 0x000FFFFC & afu_event.tlx_cfg_pa;
    byte_offset = 0x0000003F & afu_event.tlx_cfg_pa;
//    if(vsec_offset >= 0x10 && vsec_offset <= 0x24) {
//	vsec_data = descriptor.get_vsec_reg(vsec_offset);
//    }
//    else {
//    	vsec_data  = descriptor.get_vsec_reg(vsec_offset);	// get vsec data
//    }
    if(vsec_offset > 0x20700) {
	resp_opcode = 0x02;	// read failed resp
	debug_msg("AFU: configuration read failed response offset = 0x%08x", vsec_offset);
    }
    else {
    	vsec_data = descriptor.get_vsec_reg(vsec_offset);
    	debug_msg("AFU: vsec data = 0x%x vsec offset = 0x%x byte offset = 0x%x",
	    	vsec_data, vsec_offset, byte_offset);
    }
    if(resp_pl == 0x00) {
	data_size = 1;
	switch(vsec_offset) {
	    case 0:
		vsec_data = 0x000000FF & vsec_data;
		break;
	    case 1:
		vsec_data = 0x0000FF00 & vsec_data;
		vsec_data = vsec_data >> 8;
		break;
	    case 2:
		vsec_data = 0x00FF0000 & vsec_data;
		vsec_data = vsec_data >> 16;
		break;
	    case 3:
		vsec_data = 0xFF000000 & vsec_data;
		vsec_data = vsec_data >> 24;
		break;
	    default:
		error_msg("Configuration read offset is not supported 0x%x", vsec_offset);
		break;
	}
    }
    else if(resp_pl == 0x01) {
	data_size = 2;
	switch(vsec_offset) {
	    case 0:
		vsec_data = vsec_data & 0x0000FFFF;
		break;
	    case 2:
		vsec_data = vsec_data & 0xFFFF0000;
		vsec_data = vsec_data >> 16;
		break;
	    default:
		error_msg("Configuration read offset is not supported 0x%x", vsec_offset);
		break;
	}
    }
    else if(resp_pl == 0x02) {
	data_size = 4;
    }

    bdf = (0xFFFF0000 & afu_event.tlx_cfg_pa) >> 16;
    afu_event.afu_tlx_cmd_bdf = bdf;
    info_msg("AFU: BDF = 0x%x", bdf);
    info_msg("AFU: cfg_capptag = 0x%x", resp_capptag);
    info_msg("AFU: cfg pl = 0x%x data size = %d", resp_pl, data_size);
    memcpy(&afu_event.cfg_tlx_rdata_bus, &vsec_data, data_size); 
    //byte_shift(afu_event.cfg_tlx_rdata_bus, data_size, byte_offset, RIGHT);  
    printf("rdata_bus = 0x");
    for(uint8_t i=0; i<4; i++)
	printf("%02x", afu_event.cfg_tlx_rdata_bus[i]);
    printf("\n");
    info_msg("AFU: vsec_offset = 0x%x vsec_data = 0x%x", vsec_offset, vsec_data);
    printf("calling ocse\n");
    rc = afu_cfg_send_resp_and_data(&afu_event, resp_opcode, resp_capptag,
	resp_code, byte_cnt, rdata_valid, afu_event.cfg_tlx_rdata_bus, rdata_bad);
    printf("config read rc = 0x%x\n", rc);
    if(rc != TLX_SUCCESS) {

		error_msg("AFU: Failed afu_tlx_send_resp_and_data");
    	}
}

void
AFU::tlx_afu_config_write()
{
    uint8_t  resp_opcode, rdata_valid;
    uint16_t byte_cnt, resp_capptag;
    uint8_t  resp_code = 0;
    uint8_t  rdata_bad;
    uint32_t port_data, port_offset, vsec_offset;  //config_offset
    uint32_t cmd_pa;
    int rc;
    uint16_t bdf;

    debug_msg("AFU::tlx_afu_config_write");
    resp_capptag = afu_event.cfg_tlx_resp_capptag;
    cmd_pa = afu_event.tlx_cfg_pa & 0x000FFFFC;   
    //resp_dl = 1;

    debug_msg("AFU: cmd_pa = 0x%x", cmd_pa);
    // get BDF during configuration
    if(afu_event.tlx_cfg_t == 0) {
    	bdf = (afu_event.tlx_cfg_pa & 0xFFFF0000) >> 16;
    	gDUT = bdf;
    }    
    printf("AFU: bdf = 0x%x\n", (afu_event.tlx_cfg_pa & 0xFFFF0000) >> 16);
//    if(config_state == IDLE) {
	//afu_tlx_cmd_rd_req = 0x1;
	//afu_tlx_cmd_rd_cnt = 0x1;
	byte_cnt = 0;
	resp_opcode = 4;
	// get config write afu descriptor offset
	byte_cnt = 0;
//	data_size = 4;
//	byte_offset = 0x0000003F & afu_event.tlx_afu_cmd_pa;
	
	// get config write data
	// set afu_enable_reset to check for enable or reset bit
	if((cmd_pa & 0x0FFC) == 0x50c) {
	    afu_enable_reset = 1;
	    afu_function = cmd_pa & 0x000F0000;	//afu function number
	}
	// 0x40c afu descriptor offset port, 0x410 afu descriptor data port
   	if((cmd_pa & 0x0FFC) == 0x40c) {	
	    port_offset = wr_config_data;	// get afu descriptor offset
	    printf("AFU: port_offset = 0x%x\n", port_offset);
	    if(port_offset < 0x0FFF) {
		port_data = descriptor.get_afu_desc_reg(port_offset);	// get afu desc data
		printf("AFU: afu descriptor data port = 0x%x\n", port_data);
		//descriptor.set_afu_desc_reg(0x410, port_data);		// write afu desc data to read port
		descriptor.set_vsec_reg((cmd_pa & 0xF0000)+0x410, port_data);
	    }
	    else {
	    	port_data = descriptor.get_port_reg(port_offset);	// get vsec data
	    	descriptor.set_vsec_reg((cmd_pa & 0xF0000)+0x410, port_data);		// write data to read port
	    }
	    port_offset = port_offset | 0x80000000;		// set bit 31 to write port 0x40c
	    descriptor.set_vsec_reg((cmd_pa & 0xF0000)+0x40c, port_offset);
	    debug_msg("AFU: afu descriptor data port 0x410 = 0x%x", descriptor.get_vsec_reg((cmd_pa & 0xF0000)+0x410));
	    debug_msg("AFU: afu descriptor offset port 0x40c = 0x%x", descriptor.get_vsec_reg((cmd_pa & 0xF0000)+0x40c));
	}
	// write to BAR registers
	else if(((cmd_pa & 0x0FFC) >= 0x10) && ((cmd_pa & 0x0FFC) <= 0x24)) {
	    printf("AFU: config BAR\n");
	    if(wr_config_data != 0xFFFFFFFF) {
	    	switch(cmd_pa & 0x0FFC) {
		    case 0x10:
			enable_bar = 0;
			bar_l0 = wr_config_data;
			printf("AFU: bar_l0 = 0x%x\n", bar_l0);
			break;
		    case 0x14:
			bar_h0 = wr_config_data;
		  	printf("AFU: bar_h0 = 0x%x\n", bar_h0);
			break;
		    case 0x18:
			enable_bar = 1;
			bar_l1 = wr_config_data;
			printf("AFU: bar_l1 = 0x%x\n", bar_l1);
			break;
		    case 0x1c:
			bar_h1 = wr_config_data;
			printf("AFU: bar_h1 = 0x%x\n", bar_h1);
			break;
		    case 0x20:
			enable_bar = 2;
			bar_l2 = wr_config_data;
			printf("AFU: bar_l2 = 0x%x\n", bar_l2);
			break;
		    case 0x24:
			bar_h2 = wr_config_data;
		  	printf("AFU: bar_h2 = 0x%x\n", bar_h2);
			break;
		    default:
			break;
		}	 
	    }   
	}
	// write to vsec registers
	else {	// vsec config write 
	    vsec_offset = cmd_pa & 0x000FFFFC;
	    descriptor.set_vsec_reg(vsec_offset, wr_config_data);	    
	}
	rc = afu_cfg_send_resp_and_data(&afu_event, resp_opcode, resp_capptag,
		resp_code, byte_cnt, rdata_valid, 
		afu_event.cfg_tlx_rdata_bus, rdata_bad);

}

void
AFU::tlx_pr_rd_mem()
{
    uint8_t afu_tlx_resp_opcode;
    uint8_t afu_tlx_resp_dl;
    uint8_t afu_tlx_resp_code;
    uint8_t afu_tlx_rdata_valid;
    uint16_t data_size;
    uint16_t afu_tlx_resp_capptag;
    uint32_t mem_offset;
    uint64_t mem_data;

    afu_tlx_resp_opcode = 0x01;		// mem rd response
    afu_tlx_resp_dl = 0x01;		// length 64 byte
    afu_tlx_resp_code = 0x0;
    afu_tlx_rdata_valid = 0x0;
    afu_tlx_resp_capptag = afu_event.tlx_afu_cmd_capptag;

    debug_msg("AFU: tlx_pr_rd_mem");
    // calculate data size
    switch(afu_event.tlx_afu_cmd_opcode) {
	case TLX_CMD_RD_MEM:	// 0x20
	    printf("AFU: TLX_CMD_RD_MEM 0x20\n");
	    if(afu_event.tlx_afu_cmd_dl == 1)
		data_size = 64;
	    else if(afu_event.tlx_afu_cmd_dl == 2)
		data_size = 128;
	    else if(afu_event.tlx_afu_cmd_dl == 3)
		data_size = 256;
	    break;
	case TLX_CMD_PR_RD_MEM:	// 0x28
	    printf("AFU: TLX_CMD_PR_RD_MEM 0x28\n");
    	    if(afu_event.tlx_afu_cmd_pl == 3)
		data_size = 8;
    	    else if(afu_event.tlx_afu_cmd_pl == 2)
		data_size = 4;
	    break;
	default:
	    break;
    }

    if(is_mmio_addr(afu_event.tlx_afu_cmd_pa)) {    
    // mmio read data
	mem_offset = afu_event.tlx_afu_cmd_pa;
    	descriptor.get_mmio_mem(mem_offset, (char*)&mem_data, data_size);
    	debug_msg("mem_offset = 0x%x mem_data = 0x%016llx", mem_offset, mem_data);
    	memcpy(&afu_event.afu_tlx_rdata_bus, &mem_data, data_size);
    	byte_shift(afu_event.afu_tlx_rdata_bus, data_size, mem_offset, RIGHT);

      	if(TagManager::request_tlx_credit(RESP_CREDIT)) {
            if(afu_tlx_send_resp_and_data(&afu_event, afu_tlx_resp_opcode, afu_tlx_resp_dl, 
		afu_tlx_resp_capptag, afu_event.afu_tlx_resp_dp, 
		afu_tlx_resp_code, afu_tlx_rdata_valid, 
		afu_event.afu_tlx_rdata_bus, afu_event.afu_tlx_rdata_bdi) != TLX_SUCCESS) {

		error_msg("AFU: Failed afu_tlx_send_resp_and_data");
    	    }
    	}
    	else {
	    error_msg("AFU: No response credit available");
    	}
    }
    else {	// lpc memory space
	printf("AFU: lpc addr = 0x%lx \n",afu_event.tlx_afu_cmd_pa );
	lpc.read_lpc_mem(afu_event.tlx_afu_cmd_pa, data_size, afu_event.afu_tlx_rdata_bus);
	if(TagManager::request_tlx_credit(RESP_CREDIT)) {
	    if(afu_tlx_send_resp_and_data(&afu_event, afu_tlx_resp_opcode, afu_tlx_resp_dl,
		afu_tlx_resp_capptag, afu_event.afu_tlx_resp_dp,
		afu_tlx_resp_code, afu_tlx_rdata_valid,
		afu_event.afu_tlx_rdata_bus, afu_event.afu_tlx_rdata_bdi) != TLX_SUCCESS) {
		error_msg("AFU: Failed afu_tlx_send_resp_and_data");
	    }
	}
	else {
	    error_msg("AFU: No response credit available");
	}
    }
}

void
AFU::tlx_pr_wr_mem()
{
    uint8_t  afu_tlx_cmd_rd_req;
    uint8_t  afu_tlx_cmd_rd_cnt;
    uint8_t  cmd_data_bdi;
    uint32_t cmd_pa;
    uint64_t mem_data;
    uint8_t  afu_resp_opcode;
    uint8_t  resp_dl = 0;
    uint16_t resp_capptag;
    uint8_t  resp_dp = 0;
    uint8_t  resp_code = 0;
    uint8_t  byte_offset;
    uint16_t  data_size;		

    debug_msg("AFU:tlx_pr_wr_mem");

    cmd_pa = afu_event.tlx_afu_cmd_pa & 0x0000FFFC;
    resp_capptag = afu_event.tlx_afu_cmd_capptag;
    byte_offset = 0x0000003F & afu_event.tlx_afu_cmd_pa;

    if(mem_state == IDLE) {
	afu_tlx_cmd_rd_req = 0x1;
	afu_tlx_cmd_rd_cnt = 0x1;
	    if(afu_tlx_cmd_data_read_req(&afu_event, afu_tlx_cmd_rd_req, afu_tlx_cmd_rd_cnt) !=
	        TLX_SUCCESS) {
	    	printf("AFU: Failed afu_tlx_resp_data_read_req\n");
	    }
	debug_msg("AFU: set mem_state = READY");
	mem_state = READY;
    }
    else if(mem_state == READY) {
	    if(tlx_afu_read_cmd_data(&afu_event, &cmd_data_bdi, afu_event.tlx_afu_cmd_data_bus) !=
		TLX_SUCCESS) {
		printf("AFU: Failed tlx_afu_read_cmd_data\n");
	    }
	switch (afu_event.tlx_afu_cmd_opcode) {
	    case TLX_CMD_WRITE_MEM:
		if(afu_event.tlx_afu_cmd_dl == 1)
		    data_size = 64;
		else if(afu_event.tlx_afu_cmd_dl == 2)
		    data_size = 128;
		else if(afu_event.tlx_afu_cmd_dl == 3)
		    data_size = 256;
		break;
	    case TLX_CMD_PR_WR_MEM:
		if(afu_event.tlx_afu_cmd_pl == 3)
	    	    data_size = 8;
		else if(afu_event.tlx_afu_cmd_pl == 2)
	    	    data_size = 4;
		break;
	    default:
		break;
	}
	printf("wr cmd data bus = 0x");
	for(int i=0; i<64; i++)
	    printf("%02x", afu_event.tlx_afu_cmd_data_bus[i]);
	printf("\n");
	//byte_shift(afu_event.tlx_afu_cmd_data_bus, data_size, byte_offset, LEFT);
	//memcpy(&mem_data, afu_event.tlx_afu_cmd_data_bus, data_size);
	//debug_msg("mem_data offset = 0x%x mem_data = 0x%016llx", cmd_pa, mem_data);
	if(is_mmio_addr(afu_event.tlx_afu_cmd_pa)) {	// mmio address space
	    byte_shift(afu_event.tlx_afu_cmd_data_bus, data_size, byte_offset, LEFT);
	    memcpy(&mem_data, afu_event.tlx_afu_cmd_data_bus, data_size);
	    // mmio write
	    descriptor.set_mmio_mem(cmd_pa, (char*)&mem_data, data_size);
	    //descriptor.set_port_reg(cmd_pa, mem_data);
	    afu_resp_opcode = 0x04;		// mem write resp
	    resp_code = 0x0;
	    if(TagManager::request_tlx_credit(RESP_CREDIT)) {
                if(afu_tlx_send_resp(&afu_event, afu_resp_opcode, resp_dl, resp_capptag,
			resp_dp, resp_code) != TLX_SUCCESS) {
	    	    printf("AFU: Failed afu_tlx_send_resp\n");
	    	}
	    }
	    else {
		    error_msg("AFU: no resp credit available");
	    }
	    debug_msg("set mem_state = IDLE");
	    mem_state = IDLE;
	}
	else { 	// lpc memory address space
	    printf("AFU: lpc addr = 0x%lx\n", afu_event.tlx_afu_cmd_pa);
	    printf("AFU: cmd data bus addr = 0x%lx\n", afu_event.tlx_afu_cmd_data_bus);
	    lpc.write_lpc_mem(afu_event.tlx_afu_cmd_pa, data_size, afu_event.tlx_afu_cmd_data_bus);
	    if(TagManager::request_tlx_credit(RESP_CREDIT)) {
		if(afu_tlx_send_resp(&afu_event, afu_resp_opcode, resp_dl, resp_capptag,
			resp_dp, resp_code) != TLX_SUCCESS) {
		    printf("AFU: Failed afu_tlx_send_resp\n");
		}
	    }
	    debug_msg("set mem_state = IDLE");
	    mem_state = IDLE;
	}
    }
}
uint32_t
AFU::is_mmio_addr(uint64_t addr)
{
    printf("AFU: mmio addr = 0x%016lx\n", addr);
    printf("AFU: bar addr  = 0x%016lx\n", bar);
    addr = addr & 0xFFFFFFFFFFF10000;
    if(addr >= bar && addr < bar+0x10000) {
	return 1;
    }
    else {
	return 0;
    }
}

void
AFU::byte_shift(unsigned char *array, uint8_t size, uint8_t offset, uint8_t direction)
{
    uint8_t i;
    switch(direction) {
    case LEFT: 
    	for(i=0; i<size; i++)
    	{
	    array[i] = array[offset+i];
    	}
	break;
    case RIGHT:
	if(size == 0) {
	    break;
	}
	else {
	    for(i=0; i<size; i++)
	    {
	        array[offset+i] = array[i];
	    }
	    break;
	}
    default:
	break;
    }
}

void 
AFU::write_app_status(uint8_t *address, uint32_t data)
{
    uint8_t ea_addr[9];
    uint32_t cmd_afutag;

    TagManager::request_tag(&cmd_afutag);

    memcpy((void*)&ea_addr, (void*)&address, sizeof(uint64_t));
    printf("AFU: status address = 0x%p and data = 0x%x\n", address, data);
    printf("AFU: afutag = 0x%x\n", cmd_afutag);
    //cmd_op=0x20, dl=0x01, pl=0x02
    memcpy(afu_event.afu_tlx_cdata_bus, &data, 64);

    afu_tlx_send_cmd_and_data(&afu_event, 0x20, afu_event.afu_tlx_cmd_actag, 
	afu_event.afu_tlx_cmd_stream_id, ea_addr, 
	(uint16_t)cmd_afutag, 0x01, 0x03,
#ifdef	TLX4
	afu_event.afu_tlx_cmd_os,
#endif
	afu_event.afu_tlx_cmd_be, afu_event.afu_tlx_cmd_flag,
	afu_event.afu_tlx_cmd_endian, afu_event.afu_tlx_cmd_bdf,
	afu_event.afu_tlx_cmd_pasid, afu_event.afu_tlx_cmd_pg_size, 
	afu_event.afu_tlx_cdata_bus, afu_event.afu_tlx_cdata_bdi);

    write_status_tag = cmd_afutag;
    read_status_resp = 0;	// stall read status until write resp
    return;
}

void
AFU::read_app_status(uint8_t *address)
{
    uint8_t ea_addr[9];
    uint32_t cmd_afutag;

    TagManager::request_tag(&cmd_afutag);

    memcpy((void*)&ea_addr, (void*)&address, sizeof(uint64_t));
    printf("AFU: status address = 0x%p\n", address);
    printf("AFU: afutag = 0x%x\n", cmd_afutag);
    status_resp_valid = 1;
    // cmd_opcode=0x12, dl=00, pl=03
    afu_tlx_send_cmd(&afu_event, 0x12, afu_event.afu_tlx_cmd_actag,
	afu_event.afu_tlx_cmd_stream_id, ea_addr,
	(uint16_t)cmd_afutag, 0x00, 0x03,
#ifdef	TLX4
	afu_event.afu_tlx_cmd_os,
#endif
	afu_event.afu_tlx_cmd_be, afu_event.afu_tlx_cmd_flag,
	afu_event.afu_tlx_cmd_endian, afu_event.afu_tlx_cmd_bdf,
	afu_event.afu_tlx_cmd_pasid, afu_event.afu_tlx_cmd_pg_size);
    
    read_status_resp = 0;	
    return;
}

void
AFU::resolve_control_event ()
{

        
        for (std::map < uint16_t, MachineController * >::iterator it =
                    context_to_mc.begin (); it != context_to_mc.end (); ++it)
            it->second->disable_all_machines ();
        state = RESET;
	debug_msg("AFU: state = RESET");
        reset_delay = 1000;
}


void
AFU::resolve_response_event (uint32_t cycle)
{
    //if (!TagManager::is_in_use (afu_event.response_tag))
    //    error_msg ("AFU: received tag not in use");


    for (std::map < uint16_t, MachineController * >::iterator it =
                context_to_mc.begin (); it != context_to_mc.end (); ++it) {
        //if (it->second->has_tag (afu_event.response_tag)) {
            it->second->process_response (&afu_event, cycle);
         //   break;
        //}
    }
}

void
AFU::set_seed ()
{
    srand (time (NULL));
}

void
AFU::set_seed (uint32_t seed)
{
    srand (seed);
}

bool 
AFU::get_mmio_read_parity ()
{
    return (global_configs[2] & 0x8000000000000000LL) == 0x8000000000000000;
}
