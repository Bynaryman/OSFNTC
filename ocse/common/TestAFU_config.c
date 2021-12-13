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

/*
 * Description: TestAFU_config.c
 *
 * This file contains Test AFU configuration helper functions.
 */

#include <stdio.h>
#include <unistd.h>	//sleep 
#include "TestAFU_config.h"

// Initialize all machine config registers
void init_machine(MachineConfig *machine)
{
	int i;

	for (i = 0; i < 4; i++)
		machine->config[i] = 0;
}

// Function to set most commonly used elements
int config_machine(MachineConfig *machine, MachineConfigParam configparam)
{
	if (!machine){
		fprintf(stderr, "\nNo config found!\n\n");
		return -1;
	}
	set_machine_config_context(machine, configparam.context);
	set_machine_config_command_code(machine, configparam.command);
	set_machine_config_machine_number(machine, configparam.machine_number);
	set_machine_config_status_address(machine, configparam.status_address);
	set_machine_memory_base_address(machine, configparam.mem_base_address);
	set_machine_memory_size(machine, configparam.mem_size);
	if (configparam.enable_always){
		set_machine_config_enable_always(machine); 
	}
	else {
		set_machine_config_enable_once(machine);
	}
	return 0;
}

// Helper function to calculate AFU machine register MMIO space offset
static int _machine_base_address_index(uint16_t machine_number, int mode)
{
	int machine_config_base_address;

    if(machine_number < 0 || machine_number > 63) {
	printf("Failed: machine number is out of range\n");
	return -1;
    }

    machine_config_base_address = machine_number << 5;
    //if (mode == DIRECTED)
//	machine_config_base_address += 0x1000;

    return machine_config_base_address;
}

// Function to write config to AFU MMIO space
int enable_machine(ocxl_afu_h afu, MachineConfig *machine, MachineConfigParam param, int mode)
{
	int i;
	uint16_t machine_number;
	uint16_t context;

 	context = param.context;
	machine_number = param.machine_number;
	int machine_config_base_address = _machine_base_address_index(machine_number, mode);

	machine_config_base_address += context * 0x1000; //debug1
	printf("machine config base address = 0x%x\n", machine_config_base_address);
	for (i = 3; i >= 0; --i){
		uint64_t data = machine->config[i];
		printf("config[%d] = 0x%"PRIx64"\n", i, data);
		if (ocxl_mmio_write64(afu, machine_config_base_address + (i * 8),
		    data))
		{
			printf("Failed to write data config[%d]\n", i);
			return -1;
		}
	}

	return 0;
}

int clear_machine_config(ocxl_afu_h afu, MachineConfig *machine, MachineConfigParam param, int mode)
{
    int i;
    uint16_t machine_number;
    uint16_t context;
    int machine_config_base_address;

    context = param.context;
    machine_number = param.machine_number;
    machine_config_base_address = _machine_base_address_index(machine_number, mode);
    machine_config_base_address += context * 0x1000;

    if(ocxl_mmio_write64(afu, machine_config_base_address, 0x0)) {
	printf("Failed to clear machine config\n");
	return -1;
    }
    return 0;
}
// Function to read config from AFU
int poll_machine(ocxl_afu_h afu, MachineConfig *machine, uint16_t context, int mode) {
	int i;

	int machineConfig_baseaddress = _machine_base_address_index(context, mode);
	printf("polling address = 0x%x\n", machineConfig_baseaddress);

	for (i = 0; i < 2; ++i){
		uint64_t temp;
		if (ocxl_mmio_read64(afu, machineConfig_baseaddress + (i * 8),
				    &temp))
		{
			printf("Failed to read data\n");
			return -1;
		}
		machine->config[i] = temp;	
	}

	return 0;
}

// Function to set most commonly used elements and write to AFU MMIO space
int config_and_enable_machine(ocxl_afu_h afu, MachineConfig *machine, 
		MachineConfigParam param, int mode)
{
	if (config_machine(machine, param))
		return -1;
	if (enable_machine(afu, machine, param, mode))
		return -1;
	return 0;
}

// Wait for response from AFU machine
int get_response(ocxl_afu_h afu, MachineConfig *machine, uint16_t context, int mode)
{
	uint8_t response;

	do {
		if (poll_machine(afu, machine, context, mode) < 0)
			return 0xFF;
		get_machine_config_response_code(machine, &response);
		printf("get_machine_config_response_code = 0x%x\n", response);
	} while (response == 0xFF);
    response = 0;
	return response;
}

// Function to set most commonly used elements, write to AFU MMIO space and
// wait for command completion
int config_enable_and_run_machine(ocxl_afu_h afu, MachineConfig *machine, 
			MachineConfigParam param, int mode)
{
	int rc;
//	uint16_t context;

//	context = param.context;

	if (config_and_enable_machine(afu, machine, param, mode) < 0)
		return -1;

	//rc = get_response(afu, machine, context, mode);
	//if (rc==0xFF)
	//	return -1;
	rc = 0;
	return rc;
}

//////////////////////////////////
// Set machine config functions //
//////////////////////////////////

// Enable always field is bits[0] of double-word 0
void set_machine_config_enable_always(MachineConfig* machine) {
	machine->config[0] &= ~0x4000000000000000LL;
	machine->config[0] |=  0x8000000000000000LL;
}

// Enable once field is bits[1] of double-word 0
void set_machine_config_enable_once(MachineConfig* machine) {
	machine->config[0] &= ~0x8000000000000000LL;
	machine->config[0] |=  0x4000000000000000LL;
}

// Disable machine
void set_machine_config_disable(MachineConfig* machine) {
	machine->config[0] &= ~0xC000000000000000LL;
}

// Command code field is bits[3:15] of double-word 0
void set_machine_config_command_code(MachineConfig* machine, uint16_t code) {
	machine->config[0] &= ~0x1FFF000000000000LL;
	machine->config[0] |= ((uint64_t)code << 48);
}

// Context field is the second 16 bits of double-word 0
void set_machine_config_context(MachineConfig* machine, uint16_t context) {
	machine->config[0] &= ~0x0000FFFF00000000LL;
	machine->config[0] |= ((uint64_t)context << 32);
}

// Machine number field is the next to last 16 bits of double-word 0
void set_machine_config_machine_number(MachineConfig* machine, uint16_t machine_number) {
	machine->config[0] &= ~0x00000000FFFF0000LL;
	machine->config[0] |= ((uint64_t)machine_number << 16);
}

// Machine status address
void set_machine_config_status_address(MachineConfig* machine, uint32_t status_address) {
	machine->config[1] &= ~0xFFFFFFFF00000000LL;
	machine->config[1] |= ((uint64_t)status_address << 32);
}

// Max delay field is the last 16 bits of double-word 0
//void set_machine_config_max_delay(MachineConfig* machine, uint16_t max_delay) {
//	machine->config[0] &= ~0x000000000000FFFFLL;
//	machine->config[0] |= max_delay;
//}

// Abort field is bits[0:2] of double-word 1
//void set_machine_config_abort(MachineConfig * machine, uint8_t abort) {
//	machine->config[1] &= ~0x7000000000000000LL;
//	machine->config[1] |= ((uint64_t)abort << 60);
//}

// Size field is bits[4:15] of double-word 1
//void set_machine_config_command_size(MachineConfig * machine, uint16_t size) {
//	machine->config[1] &= ~0x0FFF000000000000LL;
//	machine->config[1] |= ((uint64_t)size << 48);
//}

// Address parity inject field is bit[16] of double-word 1
//void set_machine_config_command_address_parity(MachineConfig * machine, uint8_t inject) {
//	machine->config[1] &= ~0x0000800000000000LL;
//	machine->config[1] |= ((uint64_t)inject << 47);
//}

// Address parity inject field is bit[17] of double-word 1
//void set_machine_config_command_code_parity(MachineConfig * machine, uint8_t inject) {
//	machine->config[1] &= ~0x0000400000000000LL;
//	machine->config[1] |= ((uint64_t)inject << 46);
//}

// Tag parity inject field is bit[18] of double-word 1
//void set_machine_config_command_tag_parity(MachineConfig * machine, uint8_t inject) {
//	machine->config[1] &= ~0x0000200000000000LL;
//	machine->config[1] |= ((uint64_t)inject << 45);
//}

// Buffer read parity inject field is bit[18] of double-word 1
//void set_machine_config_buffer_read_parity(MachineConfig * machine, uint8_t inject) {
//	machine->config[1] &= ~0x0000100000000000LL;
//	machine->config[1] |= ((uint64_t)inject << 44);
//}

// Base address of the memory space the AFU machine operate in
void set_machine_memory_base_address(MachineConfig * machine, uint64_t addr) {
	machine->config[2] = addr;
}

// Dest address of the memory space
void set_machine_memory_dest_address(MachineConfig *machine, uint64_t addr) {
    machine->config[3] = addr;
}

// Size of the memory space the AFU machine operate in
void set_machine_memory_size(MachineConfig * machine, uint16_t size) {
    machine->config[1] &= ~0x000000000000FFFF;
	machine->config[1] |= size;
}

//////////////////////////////////
// Get machine config functions //
//////////////////////////////////

// Command code field is bit[0] of double-word 0
void get_machine_config_enable_always(MachineConfig *machine, uint8_t* enable_always) {
	*enable_always = (uint16_t)((machine->config[0] & 0x8000000000000000LL) >> 63);
}

// Command code field is bit[1] of double-word 0
void get_machine_config_enable_once(MachineConfig *machine, uint8_t* enable_once) {
	*enable_once = (uint16_t)((machine->config[0] & 0x4000000000000000LL) >> 62);
}

// Command code field is bits[3:15] of double-word 0
void get_machine_config_command_code(MachineConfig *machine, uint16_t* command_code) {
	*command_code = (uint16_t)((machine->config[0] & 0x1FFF000000000000LL) >> 48);
}

// Context field is the second 16 bits of double-word 0
void get_machine_config_context(MachineConfig *machine, uint16_t* context) {
	*context = (uint16_t)((machine->config[0] & 0x0000FFFF00000000LL) >> 32);
}

// Max delay field is the next to last 16 bits of double-word 0
void get_machine_config_machine_number(const MachineConfig *machine, uint16_t* machine_number) {
	*machine_number = (uint16_t)((machine->config[0] & 0x00000000FFFF0000LL) >> 16);
}

// Max delay field is the last 16 bits of double-word 0
//void get_machine_config_max_delay(const MachineConfig *machine, uint16_t* max_delay) {
//	*max_delay = (uint16_t)((machine->config[0]) & 0x000000000000FFFFLL);
//}

// Abort field is bits[1:3] of double-word 1
//void get_machine_config_abort(MachineConfig *machine, uint8_t* abort) {
//	*abort = (uint16_t)((machine->config[1] & 0x7000000000000000LL) >> 60);
//}

// Size field is bits[4:15] of double-word 1
//void get_machine_config_command_size(MachineConfig *machine, uint16_t* size) {
//	*size = (uint16_t)((machine->config[1] & 0x0FFF000000000000LL) >> 48);
//}

// Address parity inject field is bit[16] of double-word 1
//void get_machine_config_command_address_parity(MachineConfig *machine, uint8_t* inject) {
//	*inject = (uint16_t)((machine->config[1] & 0x0000800000000000LL) >> 47);
//}

// Command code parity inject field is bit[17] of double-word 1
//void get_machine_config_command_code_parity(MachineConfig *machine, uint8_t* inject) {
//	*inject = (uint16_t)((machine->config[1] & 0x0000400000000000LL) >> 46);
//}

// Command tag parity inject field is bit[18] of double-word 1
//void get_machine_config_command_tag_parity(MachineConfig *machine, uint8_t* inject) {
//	*inject = (uint16_t)((machine->config[1] & 0x0000200000000000LL) >> 45);
//}

// Buffer read parity inject field is bit[19] of double-word 1
//void get_machine_config_buffer_read_parity(MachineConfig *machine, uint8_t* inject) {
//	*inject= (uint16_t)((machine->config[1] & 0x0000100000000000LL) >> 44);
//}

// Idling field is bit[23] of double-word 1
void get_machine_config_machine_idling(MachineConfig *machine, uint8_t* idling) {
	*idling = (uint16_t)((machine->config[1] & 0x0000010000000000LL) >> 40);
}

// Response code field is bits[24:31] of double-word 1
void get_machine_config_response_code(MachineConfig *machine, uint8_t* response) {
	*response = (uint16_t)((machine->config[1] & 0x000000FF00000000LL) >> 32);
}

// Response status field is bit[32] of double-word 1
void get_machine_config_response_status(MachineConfig *machine, uint16_t* response_status) {
	*response_status = (uint16_t)((machine->config[1] & 0x0000000080000000LL) >> 31);
}

// Response timestamp field is bits[33:47] of double-word 1
void get_machine_config_response_timestamp(MachineConfig *machine, uint16_t* response_timestamp) {
	*response_timestamp = (uint16_t)((machine->config[1] & 0x000000007FFF0000LL) >> 16);
}

// Command status field is bit[48] of double-word 1
void get_machine_config_command_status(MachineConfig *machine, uint8_t* command_status) {
	*command_status = (uint16_t)((machine->config[1] & 0x0000000000008000LL) >> 15);
}

// Command timestamp field is bit[49:63] of double-word 1
void get_machine_config_command_timestamp(MachineConfig *machine, uint16_t* command_timestamp) {
	*command_timestamp = (uint16_t)((machine->config[1]) & 0x0000000000007FFFLL);
}

// Base address of the memory space the AFU machine operate in
void get_machine_memory_base_address(MachineConfig *machine, uint64_t* addr) {
	*addr = machine->config[2];
}

// Size of the memory space the AFU machine operate in
void get_machine_memory_size(MachineConfig *machine, uint64_t* size) {
	*size = machine->config[3];
}
