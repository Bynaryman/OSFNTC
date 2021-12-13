/*
 * Copyright 2015 International Business Machines
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
 * Description: TestAFU_config.h
 *
 * This file contains Test AFU configuration helper functions.
 */

#pragma once
#include <inttypes.h>
#include "../libocxl/libocxl.h"

#define DEDICATED 1
#define DIRECTED 0
#define PPPSA_OFFSET 0x1000
#define PPPSA_SIZE 0x1000

// Strucure to configure AFU
typedef struct AFUConfig
{
	uint64_t config[4];
} MachineConfig;

// Machine Configuration Parameters
typedef struct ConfigParam
{
    uint16_t	context;
    uint16_t	command;
    uint16_t	command_size;
    uint16_t	machine_number;
//    uint16_t	max_delay;
    uint32_t	status_address;
    uint64_t	mem_base_address;
//    uint64_t	mem_size;
    uint64_t  	mem_dest_address;	// config[3]
    uint16_t	mem_size;		// config[1]
    uint8_t	enable_always;
} MachineConfigParam;

// Zero out all machine config registers
void init_machine(MachineConfig *machine);

// Function to set most commonly used elements
int config_machine(MachineConfig *machine, MachineConfigParam configparam);

// Function to write config to AFU MMIO space
int enable_machine(ocxl_afu_h afu, MachineConfig *machine, MachineConfigParam param, int mode);

// Function to clear machine config
int clear_machine_config(ocxl_afu_h afu, MachineConfig *machine, MachineConfigParam param, int mode);

// Function to set most commonly used elements and write to AFU MMIO space
int config_and_enable_machine(ocxl_afu_h afu, MachineConfig *machine, MachineConfigParam param, int mode);

// Function to read config from AFU
int poll_machine(ocxl_afu_h afu, MachineConfig *machine, uint16_t context, int mode);

// Wait for response from AFU machine
int get_response(ocxl_afu_h afu, MachineConfig *machine, uint16_t context, int mode);

// Function to set most commonly used elements, write to AFU MMIO space and
// wait for command completion
int config_enable_and_run_machine(ocxl_afu_h afu, MachineConfig *machine, MachineConfigParam param, int mode);

// Enable always field is bits[0] of double-word 0
void set_machine_config_enable_always(MachineConfig* machine);

// Enable once field is bits[1] of double-word 0
void set_machine_config_enable_once(MachineConfig* machine);

// Disable machine
void set_machine_config_disable(MachineConfig* machine);

// Command code field is bits[3:15] of double-word 0
void set_machine_config_command_code(MachineConfig* machine, uint16_t code);

// Context field is the second 16 bits of double-word 0
void set_machine_config_context(MachineConfig* machine, uint16_t context);

// Machine number field
void set_machine_config_machine_number(MachineConfig* machine, uint16_t machine_number);

// Status address
void set_machine_config_status_address(MachineConfig* machine, uint32_t status_address);

// Max delay field is the last 16 bits of double-word 0
//void set_machine_config_max_delay(MachineConfig* machine, uint16_t max_delay);

// Abort field is bits[0:2] of double-word 1
//void set_machine_config_abort(MachineConfig * machine, uint8_t abort);

// Size field is bits[4:15] of double-word 1
//void set_machine_config_command_size(MachineConfig * machine, uint16_t size);

// Address parity inject field is bit[16] of double-word 1
//void set_machine_config_command_address_parity(MachineConfig * machine, uint8_t inject);

// Address parity inject field is bit[17] of double-word 1
//void set_machine_config_command_code_parity(MachineConfig * machine, uint8_t inject);

// Tag parity inject field is bit[18] of double-word 1
//void set_machine_config_command_tag_parity(MachineConfig * machine, uint8_t inject);

// Buffer read parity inject field is bit[18] of double-word 1
//void set_machine_config_buffer_read_parity(MachineConfig * machine, uint8_t inject);

// Base address of the memory space the AFU machine operate in
void set_machine_memory_base_address(MachineConfig * machine, uint64_t addr);

// Dest address of the memory space
void set_machine_memory_dest_address(MachineConfig *machine, uint64_t addr);

// Size of the memory space the AFU machine operate in
void set_machine_memory_size(MachineConfig * machine, uint16_t size);

// Command code field is bit[0] of double-word 0
void get_machine_config_enable_always(MachineConfig *machine, uint8_t* enable_always);

// Command code field is bit[1] of double-word 0
void get_machine_config_enable_once(MachineConfig *machine, uint8_t* enable_once);

// Command code field is bits[3:15] of double-word 0
void get_machine_config_command_code(MachineConfig *machine, uint16_t* command_code);

// Context field is the second 16 bits of double-word 0
void get_machine_config_context(MachineConfig *machine, uint16_t* context);

// Max delay field is the next to last 16 bits of double-word 0
//void get_machine_config_min_delay(const MachineConfig *machine, uint16_t* min_delay);

// Max delay field is the last 16 bits of double-word 0
//void get_machine_config_max_delay(const MachineConfig *machine, uint16_t* max_delay);

// Abort field is bits[1:3] of double-word 1
//void get_machine_config_abort(MachineConfig *machine, uint8_t* abort);

// Size field is bits[4:15] of double-word 1
//void get_machine_config_command_size(MachineConfig *machine, uint16_t* size);

// Address parity inject field is bit[16] of double-word 1
//void get_machine_config_command_address_parity(MachineConfig *machine, uint8_t* inject);

// Command code parity inject field is bit[17] of double-word 1
//void get_machine_config_command_code_parity(MachineConfig *machine, uint8_t* inject);

// Command tag parity inject field is bit[18] of double-word 1
//void get_machine_config_command_tag_parity(MachineConfig *machine, uint8_t* inject);

// Buffer read parity inject field is bit[19] of double-word 1
//void get_machine_config_buffer_read_parity(MachineConfig *machine, uint8_t* inject);

// Idling field is bit[23] of double-word 1
void get_machine_config_machine_idling(MachineConfig *machine, uint8_t* idling);

// Response code field is bits[24:31] of double-word 1
void get_machine_config_response_code(MachineConfig *machine, uint8_t* response);

// Response status field is bit[32] of double-word 1
void get_machine_config_response_status(MachineConfig *machine, uint16_t* response_status);

// Response timestamp field is bits[33:47] of double-word 1
void get_machine_config_response_timestamp(MachineConfig *machine, uint16_t* response_timestamp);

// Command status field is bit[48] of double-word 1
void get_machine_config_command_status(MachineConfig *machine, uint8_t* command_status);

// Command timestamp field is bit[49:63] of double-word 1
void get_machine_config_command_timestamp(MachineConfig *machine, uint16_t* command_timestamp);

// Base address of the memory space the AFU machine operate in
void get_machine_memory_base_address(MachineConfig *machine, uint64_t* addr);

// Size of the memory space the AFU machine operate in
void get_machine_memory_size(MachineConfig *machine, uint64_t* size);

