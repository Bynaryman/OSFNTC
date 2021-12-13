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

#ifndef __afu_h__
#define __afu_h__

#include "Descriptor.h"
#include "TagManager.h"
#include "MachineController.h"
#include "Commands.h"
#include "Lpc.h"

extern "C" {
#include "tlx_interface.h"
#include "utils.h"
}

#include <string>
#include <vector>

#define RIGHT	1
#define LEFT	0

//uint8_t memory[128];

class AFU
{
private:
    enum AFU_State
    { IDLE, RESET, READY, RUNNING, WAITING_FOR_DATA, WAITING_FOR_LAST_RESPONSES, HALT };

    AFU_EVENT afu_event;
    Descriptor descriptor;
    Lpc	lpc;
    std::map < uint16_t, MachineController * >context_to_mc;
    std::map < uint16_t,
        MachineController * >::iterator highest_priority_mc;

    MachineController *machine_controller;

    AFU_State state;
    AFU_State config_state;
    AFU_State mem_state, resp_state;
    uint8_t *status_address;
//    uint8_t  memory[128];
    uint64_t global_configs[3];	// stores MMIO registers for global configurations
    uint8_t  tlx_afu_cmd_max_credit;
    uint8_t  tlx_afu_data_max_credit;

    int reset_delay;

    void resolve_tlx_afu_cmd();
    void resolve_tlx_afu_resp();
    void resolve_cfg_cmd();
    void tlx_afu_config_read();
    void tlx_afu_config_write();
    void tlx_pr_rd_mem();
    void tlx_pr_wr_mem();
    void byte_shift(unsigned char* array, uint8_t size, uint8_t offset, uint8_t direction);
    void resolve_control_event ();
    void resolve_response_event (uint32_t cycle);
    void write_app_status(uint8_t *address, uint32_t data);
    void read_app_status(uint8_t *address);
    void set_seed ();
    void set_seed (uint32_t);
    bool afu_is_enabled();
    bool afu_is_reset();
    void reset ();
    void reset_machine_controllers ();
    bool get_machine_context();
    void request_assign_actag();
    uint32_t is_mmio_addr(uint64_t addr);
    bool get_mmio_read_parity ();
    bool set_jerror_not_run;
    
public:
    /* constructor sets up descriptor from config file, establishes server socket connection
       and waits for client to connect */
    AFU (int port, std::string filename, bool parity, bool jerror);

    /* starts the main loop of the afu test platform */
    void start ();

    /* destrutor close the socket connection */
    ~AFU ();

};


#endif
