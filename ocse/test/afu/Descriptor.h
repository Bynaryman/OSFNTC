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

#ifndef __descriptor_h__
#define __descriptor_h__

#include <string>
#include <stdint.h>
#include <vector>

extern "C" {
#include "utils.h"
}

#define MASK_IS_DEDICATED 0x8010
#define MASK_IS_DIRECTED 0x8004

#define DESCRIPTOR_NUM_REGS 10

// VSEC OFFSET
#define DEVICE_VENDOR_ID	0x000
#define AFU_ENABLE		0x004
#define MMIO_BAR0L		0x010
#define MMIO_BAR0H		0x014
#define SUBSYSTEM_VENDOR_ID	0x02C
#define CAP_POINTER		0x034
#define CAP_POINTER_F000	0x100
#define REV_LENGTH_F000		0x104
#define CAP_POINTER_F001	0x190
#define REV_LENGTH_F001		0x194
#define PASID_LENGTH		0x198
#define PASID_BASE		0x19C
#define PASID_MASK		0x1A0
#define CAP_POINTER_F002	0x1B0
#define REV_LENGTH_F002		0x1B4
#define CAP_POINTER_F003	0x1D0
#define REV_LENGTH_F003		0x1D4
#define AFU_DESC_OFFSET		0x1E0
#define AFU_DESC_DATA		0x1E4
#define CAP_POINTER_F0F0	0x1F0
#define REV_LENGTH_F0F0		0x1F4
#define VENDOR_ID_F0F0		0x1F8

class Descriptor
{
private:
    // VSEC registers
    std::vector < uint32_t > vsec;
    std::vector < uint32_t > vsec1;
    std::vector < uint32_t > vsec2;
    std::vector < uint64_t > port;
    std::vector < uint32_t > afu_desc;
    std::vector < uint64_t > regs;
    std::vector < uint8_t > mmio;

    /*  reg0x00 : [0:15]  num_ints_per_process
       [16:31] num_of_process
       [32:47] num_of_afu_CRs
       [48:63] reg_prog_model

       reg0x08 - reg0x18 RESERVED

       reg0x20 : [0:7]  RESERVED
       [8:63] AFU_CR_len

       reg0x28 : [0:63] AFU_CR_offset

       reg0x30 : [0:7]  PerProcessPSA_control
       [8:63] PerProcessPSA_length

       reg0x38 : [0:63] PerProcessPSA_offset

       reg0x40 : [0:7]  RESERVED
       [8:63] AFU_EB_len

       reg0x48 : [0:63] AFU_EB_offset
     */

    void parse_descriptor_file (std::string filename);
    uint32_t to_vector_index (uint32_t byte_address) const;

public:
    Descriptor (std::string filename);

    uint64_t get_reg (uint32_t word_address, uint32_t mmio_double) const;

    bool set_jerror () const;
 
    bool is_dedicated () const;
    bool is_directed () const;

    // reg0x00
    uint16_t get_num_ints_per_process () const;
    uint16_t get_num_of_process () const;
    uint16_t get_num_of_afu_CRs () const;
    uint16_t get_reg_prog_model () const;

    // reg0x20
    uint64_t get_AFU_CR_len () const;

    //reg0x28
    uint64_t get_AFU_CR_offset () const;

    // reg0x30
    uint8_t get_PerProcessPSA_control () const;
    uint64_t get_PerProcessPSA_length () const;

    // reg0x38
    uint64_t get_PerProcessPSA_offset () const;

    // reg0x40
    uint64_t get_AFU_EB_len () const;

    // reg0x48
    uint64_t get_AFU_EB_offset () const;

    // vsec registers
    uint32_t get_vsec_reg(uint32_t);
    // set vsec register
    void set_vsec_reg(uint32_t, uint32_t);
    // get port reg
    uint64_t get_port_reg(uint32_t offset);
    // set port reg
    void set_port_reg(uint32_t offset, uint64_t data);
    uint32_t get_afu_desc_reg(uint32_t offset);
    void set_afu_desc_reg(uint32_t offset, uint32_t data);
    void get_mmio_mem(uint32_t offset, char *data, uint16_t size);
    void set_mmio_mem(uint32_t offset, char *data, uint16_t size);
};

#endif
