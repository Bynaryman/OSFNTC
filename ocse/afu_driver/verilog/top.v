//
// Copyright 2014,2017 International Business Machines
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

`timescale 1ns / 1ns

module top (
  output          breakpoint
);

   import "DPI-C" function void tlx_bfm_init( );
   import "DPI-C" function void set_simulation_time(input [0:63] simulationTime);
   import "DPI-C" function void get_simuation_error(inout simulationError);
   import "DPI-C" function void tlx_bfm(
                                input tlx_clock,
                                input afu_clock,
                                input reset,
				// Table 1: TLX to AFU Response Interface
				inout             tlx_afu_resp_valid_top,
				inout [7:0]       tlx_afu_resp_opcode_top,
				inout [15:0]      tlx_afu_resp_afutag_top,
				inout [3:0]       tlx_afu_resp_code_top,
				inout [5:0]       tlx_afu_resp_pg_size_top,
				inout [1:0]       tlx_afu_resp_dl_top,
				inout [1:0]       tlx_afu_resp_dp_top,
				inout [23:0]      tlx_afu_resp_host_tag_top,
				inout [17:0]      tlx_afu_resp_addr_tag_top,
				inout [3:0]       tlx_afu_resp_cache_state_top,

				//	Table 2: TLX Response Credit Interface
				input		  afu_tlx_resp_credit_top,
				input	[6:0]	  afu_tlx_resp_initial_credit_top,

				//	Table 3: TLX to AFU Command Interface
				inout             tlx_afu_cmd_valid_top,
				inout [7:0]       tlx_afu_cmd_opcode_top,
				inout [15:0]      tlx_afu_cmd_capptag_top,
				inout [1:0]       tlx_afu_cmd_dl_top,
				inout [2:0]       tlx_afu_cmd_pl_top,
				inout [63:0]      tlx_afu_cmd_be_top,
				inout             tlx_afu_cmd_end_top,
//				inout             tlx_afu_cmd_t_top,
				inout [63:0]      tlx_afu_cmd_pa_top,
				inout [3:0]       tlx_afu_cmd_flag_top,
				inout             tlx_afu_cmd_os_top,

				//	Table 4: TLX Command Credit Interface
				input			afu_tlx_cmd_credit_top,
				input	[6:0]		afu_tlx_cmd_initial_credit_top,

				//	Table 5: TLX to AFU Response Data Interface
				inout             tlx_afu_resp_data_valid_top,
				inout [511:0]     tlx_afu_resp_data_bus_top,
				inout             tlx_afu_resp_data_bdi_top,
				input			afu_tlx_resp_rd_req_top,
				input	[2:0]		afu_tlx_resp_rd_cnt_top,

				//	Table 6: TLX to AFU Command Data Interface
				inout             tlx_afu_cmd_data_valid_top,
				inout [511:0]     tlx_afu_cmd_data_bus_top,
				inout             tlx_afu_cmd_data_bdi_top,
				input			afu_tlx_cmd_rd_req_top,
				input	[2:0]		afu_tlx_cmd_rd_cnt_top,

				//	Table 7: TLX Framer credit interface
				inout             tlx_afu_resp_credit_top,
				inout             tlx_afu_resp_data_credit_top,
				inout             tlx_afu_cmd_credit_top,
				inout             tlx_afu_cmd_data_credit_top,
				inout [3:0]       tlx_afu_cmd_resp_initial_credit_top,
				inout [3:0]       tlx_afu_data_initial_credit_top,
				inout [5:0]       tlx_afu_cmd_data_initial_credit_top,
				inout [5:0]       tlx_afu_resp_data_initial_credit_top,

				//	Table 8: TLX Framer Command Interface
				input			afu_tlx_cmd_valid_top,
				input	[7:0]		afu_tlx_cmd_opcode_top,
				input	[11:0]		afu_tlx_cmd_actag_top,
				input	[3:0]		afu_tlx_cmd_stream_id_top,
				input	[67:0]		afu_tlx_cmd_ea_or_obj_top,
				input	[15:0]		afu_tlx_cmd_afutag_top,
				input	[1:0]		afu_tlx_cmd_dl_top,
				input	[2:0]		afu_tlx_cmd_pl_top,
				input			afu_tlx_cmd_os_top,
				input	[63:0]		afu_tlx_cmd_be_top,
				input	[3:0]		afu_tlx_cmd_flag_top,
				input			afu_tlx_cmd_endian_top,
				input	[15:0]		afu_tlx_cmd_bdf_top,
				input	[19:0]		afu_tlx_cmd_pasid_top,
				input	[5:0]		afu_tlx_cmd_pg_size_top,
				input	[511:0]		afu_tlx_cdata_bus_top,
				input			afu_tlx_cdata_bdi_top,// TODO: TLX Ref Design doc lists this as afu_tlx_cdata_bad
				input			afu_tlx_cdata_valid_top,

				//	Table 9: TLX Framer Response Interface
				input			afu_tlx_resp_valid_top,
				input  [7:0]		afu_tlx_resp_opcode_top,
				input  [1:0]		afu_tlx_resp_dl_top,
				input  [15:0]		afu_tlx_resp_capptag_top,
				input  [1:0]		afu_tlx_resp_dp_top,
				input  [3:0]		afu_tlx_resp_code_top,
				input			afu_tlx_rdata_valid_top,
				input  [511:0]		afu_tlx_rdata_bus_top,
				input			afu_tlx_rdata_bdi_top,

				// These signals do not appear on the RefDesign Doc. However it is present
				// on the TLX spec
// mcp3 update on 12/Jun/2017 - port is absent				inout             afu_cfg_in_rcv_tmpl_capability_0_top,
// mcp3 update on 12/Jun/2017 - port is absent				inout             afu_cfg_in_rcv_tmpl_capability_1_top,
// mcp3 update on 12/Jun/2017 - port is absent				inout             afu_cfg_in_rcv_tmpl_capability_2_top,
// mcp3 update on 12/Jun/2017 - port is absent				inout             afu_cfg_in_rcv_tmpl_capability_3_top,
// mcp3 update on 12/Jun/2017 - port is absent				inout [3:0]       afu_cfg_in_rcv_rate_capability_0_top,
// mcp3 update on 12/Jun/2017 - port is absent				inout [3:0]       afu_cfg_in_rcv_rate_capability_1_top,
// mcp3 update on 12/Jun/2017 - port is absent				inout [3:0]       afu_cfg_in_rcv_rate_capability_2_top,
// mcp3 update on 12/Jun/2017 - port is absent				inout [3:0]       afu_cfg_in_rcv_rate_capability_3_top,
				inout        		tlx_afu_ready_top,
    				inout	     		tlx_cfg0_in_rcv_tmpl_capability_0_top,
    				inout	     		tlx_cfg0_in_rcv_tmpl_capability_1_top,
    				inout	     		tlx_cfg0_in_rcv_tmpl_capability_2_top,
    				inout	     		tlx_cfg0_in_rcv_tmpl_capability_3_top,
    				inout   [3:0]  		tlx_cfg0_in_rcv_rate_capability_0_top,
    				inout   [3:0] 		tlx_cfg0_in_rcv_rate_capability_1_top,
    				inout   [3:0]  		tlx_cfg0_in_rcv_rate_capability_2_top,
    				inout   [3:0]  		tlx_cfg0_in_rcv_rate_capability_3_top,
    				inout			tlx_cfg0_valid_top,
    				inout	[7:0]	 	tlx_cfg0_opcode_top,
    				inout  [63:0]		tlx_cfg0_pa_top,
    				inout		 	tlx_cfg0_t_top,
    				inout	[2:0]	 	tlx_cfg0_pl_top,
    				inout  [15:0]	 	tlx_cfg0_capptag_top,
    				inout  [31:0]	 	tlx_cfg0_data_bus_top,
    				inout		 	tlx_cfg0_data_bdi_top,
    				inout        	 	tlx_cfg0_resp_ack_top,
   				input   [3:0]		cfg0_tlx_initial_credit_top,
  				input			cfg0_tlx_credit_return_top,
   				input			cfg0_tlx_resp_valid_top	,
		   		input   [7:0]		cfg0_tlx_resp_opcode_top,
		   		input  [15:0]		cfg0_tlx_resp_capptag_top,
		   		input   [3:0]		cfg0_tlx_resp_code_top	,
		   		input   [3:0]		cfg0_tlx_rdata_offset_top,
		   		input  [31:0]		cfg0_tlx_rdata_bus_top	,
   				input			cfg0_tlx_rdata_bdi_top,
				inout   [4:0]		ro_device_top
                                       );

   parameter RESET_CYCLES = 9;
   reg             tlx_clock;
   reg             afu_clock;
   reg             reset;
  // Table 1: TLX to AFU Response Interface
   reg             tlx_afu_resp_valid_top;
   reg [7:0]       tlx_afu_resp_opcode_top;
   reg [15:0]      tlx_afu_resp_afutag_top;
   reg [3:0]       tlx_afu_resp_code_top;
   reg [5:0]       tlx_afu_resp_pg_size_top;
   reg [1:0]       tlx_afu_resp_dl_top;
   reg [1:0]       tlx_afu_resp_dp_top;
   reg [23:0]      tlx_afu_resp_host_tag_top;
   reg [17:0]      tlx_afu_resp_addr_tag_top;
   reg [3:0]       tlx_afu_resp_cache_state_top;

//	Table 3: TLX to AFU Command Interface
   reg             tlx_afu_cmd_valid_top;
   reg [7:0]       tlx_afu_cmd_opcode_top;
   reg [15:0]      tlx_afu_cmd_capptag_top;
   reg [1:0]       tlx_afu_cmd_dl_top;
   reg [2:0]       tlx_afu_cmd_pl_top;
   reg [63:0]      tlx_afu_cmd_be_top;
   reg             tlx_afu_cmd_end_top;
//   reg             tlx_afu_cmd_t_top;
   reg [63:0]      tlx_afu_cmd_pa_top;
   reg [3:0]       tlx_afu_cmd_flag_top;
   reg             tlx_afu_cmd_os_top;

//	Table 5: TLX to AFU Response Data Interface
   reg             tlx_afu_resp_data_valid_top;
   reg [511:0]     tlx_afu_resp_data_bus_top;
   reg             tlx_afu_resp_data_bdi_top;

//	Table 5: TLX to AFU Response Data Interface delays
   reg             tlx_afu_resp_data_valid_dly1;
   reg [511:0]     tlx_afu_resp_data_bus_dly1;
   reg             tlx_afu_resp_data_bdi_dly1;

//	Table 5: TLX to AFU Response Data Interface delays
   reg             tlx_afu_resp_data_valid_dly2;
   reg [511:0]     tlx_afu_resp_data_bus_dly2;
   reg             tlx_afu_resp_data_bdi_dly2;

//	Table 6: TLX to AFU Command Data Interface
   reg             tlx_afu_cmd_data_valid_top;
   reg [511:0]     tlx_afu_cmd_data_bus_top;
   reg             tlx_afu_cmd_data_bdi_top;

//	Table 7: TLX Framer credit interface
   reg             tlx_afu_resp_credit_top;
   reg             tlx_afu_resp_data_credit_top;
   reg             tlx_afu_cmd_credit_top;
   reg             tlx_afu_cmd_data_credit_top;
   reg [3:0]       tlx_afu_cmd_resp_initial_credit_top;
   reg [3:0]       tlx_afu_data_initial_credit_top;
   reg [5:0]       tlx_afu_cmd_data_initial_credit_top;
   reg [5:0]       tlx_afu_resp_data_initial_credit_top;

  // These signals do not appear on the RefDesign Doc. However it is present
  // on the TLX spec
// mcp3 release of 12/Jun does not have this port  reg             afu_cfg_in_rcv_tmpl_capability_0_top;
// mcp3 release of 12/Jun does not have this port  reg             afu_cfg_in_rcv_tmpl_capability_1_top;
// mcp3 release of 12/Jun does not have this port  reg             afu_cfg_in_rcv_tmpl_capability_2_top;
// mcp3 release of 12/Jun does not have this port  reg             afu_cfg_in_rcv_tmpl_capability_3_top;
// mcp3 release of 12/Jun does not have this port  reg [3:0]       afu_cfg_in_rcv_rate_capability_0_top;
// mcp3 release of 12/Jun does not have this port  reg [3:0]       afu_cfg_in_rcv_rate_capability_1_top;
// mcp3 release of 12/Jun does not have this port  reg [3:0]       afu_cfg_in_rcv_rate_capability_2_top;
// mcp3 release of 12/Jun does not have this port  reg [3:0]       afu_cfg_in_rcv_rate_capability_3_top;
   reg             tlx_afu_ready_top;
   reg       [4:0] ro_device_top;
   reg             tlx_cfg0_in_rcv_tmpl_capability_0_top;
   reg             tlx_cfg0_in_rcv_tmpl_capability_1_top;
   reg             tlx_cfg0_in_rcv_tmpl_capability_2_top;
   reg             tlx_cfg0_in_rcv_tmpl_capability_3_top;
   reg       [3:0] tlx_cfg0_in_rcv_rate_capability_0_top;
   reg       [3:0] tlx_cfg0_in_rcv_rate_capability_1_top;
   reg       [3:0] tlx_cfg0_in_rcv_rate_capability_2_top;
   reg       [3:0] tlx_cfg0_in_rcv_rate_capability_3_top;
   reg		   tlx_cfg0_valid_top;
   reg	     [7:0] tlx_cfg0_opcode_top;
   reg      [63:0] tlx_cfg0_pa_top;
   reg		   tlx_cfg0_t_top;
   reg      [2:0]  tlx_cfg0_pl_top;
   reg     [15:0]  tlx_cfg0_capptag_top;
   reg     [31:0]  tlx_cfg0_data_bus_top;
   reg		   tlx_cfg0_data_bdi_top;
   reg		   tlx_cfg0_resp_ack_top;
   reg	     [3:0] cfg0_tlx_initial_credit_top;
   reg		   cfg0_tlx_credit_return_top;
   reg		   cfg0_tlx_resp_valid_top	;
   reg	     [7:0] cfg0_tlx_resp_opcode_top;
   reg	    [15:0] cfg0_tlx_resp_capptag_top;
   reg       [3:0] cfg0_tlx_resp_code_top	;
   reg       [3:0] cfg0_tlx_rdata_offset_top;
   reg      [31:0] cfg0_tlx_rdata_bus_top	;
   reg		   cfg0_tlx_rdata_bdi_top	;

//	Table 2: TLX Response Credit Interface
   reg			afu_tlx_resp_credit_top               ;
   reg	[6:0]		afu_tlx_resp_initial_credit_top               ;

//	Table 4: TLX Command Credit Interface
   reg			afu_tlx_cmd_credit_top               ;
   reg	[6:0]		afu_tlx_cmd_initial_credit_top               ;

//	Table 5: TLX to AFU Response Data Interface
   reg			afu_tlx_resp_rd_req_top               ;
   reg	[2:0]		afu_tlx_resp_rd_cnt_top               ;

//	Table 6: TLX to AFU Command Data Interface
   reg			afu_tlx_cmd_rd_req_top               ;
   reg	[2:0]		afu_tlx_cmd_rd_cnt_top               ;

//	Table 8: TLX Framer Command Interface
   reg			afu_tlx_cmd_valid_top                ;
   reg	[7:0]		afu_tlx_cmd_opcode_top               ;
   reg	[11:0]		afu_tlx_cmd_actag_top                ;
   reg	[3:0]		afu_tlx_cmd_stream_id_top            ;
   reg	[67:0]		afu_tlx_cmd_ea_or_obj_top            ;
   reg	[15:0]		afu_tlx_cmd_afutag_top               ;
   reg	[1:0]		afu_tlx_cmd_dl_top                   ;
   reg	[2:0]		afu_tlx_cmd_pl_top                   ;
   reg			afu_tlx_cmd_os_top                   ;
   reg	[63:0]		afu_tlx_cmd_be_top                   ;
   reg	[3:0]		afu_tlx_cmd_flag_top                 ;
   reg			afu_tlx_cmd_endian_top               ;
   reg	[15:0]		afu_tlx_cmd_bdf_top                  ;
   reg	[19:0]		afu_tlx_cmd_pasid_top                ;
   reg	[5:0]		afu_tlx_cmd_pg_size_top              ;
   reg	[511:0]		afu_tlx_cdata_bus_top                  ;
   reg			afu_tlx_cdata_bdi_top               ;	// TODO: TLX Ref Design doc lists this as afu_tlx_cdata_bad
   reg			afu_tlx_cdata_valid_top               ;

//	Table 9: TLX Framer Response Interface
   reg			afu_tlx_resp_valid_top               ;
   reg	[7:0]		afu_tlx_resp_opcode_top               ;
   reg	[1:0]		afu_tlx_resp_dl_top               ;
   reg	[15:0]		afu_tlx_resp_capptag_top               ;
   reg	[1:0]		afu_tlx_resp_dp_top               ;
   reg	[3:0]		afu_tlx_resp_code_top               ;
   reg			afu_tlx_rdata_valid_top               ;
   reg	[511:0]		afu_tlx_rdata_bus_top               ;
   reg			afu_tlx_rdata_bdi_top               ;

//	Table 10: TLX Framer Template Configuration
// mcp3 release of 12/Jun does not have this port  reg			afu_cfg_xmit_tmpl_config_0_top               ;
// mcp3 release of 12/Jun does not have this port  reg			afu_cfg_xmit_tmpl_config_1_top               ;
// mcp3 release of 12/Jun does not have this port  reg			afu_cfg_xmit_tmpl_config_2_top               ;
// mcp3 release of 12/Jun does not have this port  reg			afu_cfg_xmit_tmpl_config_3_top               ;
// mcp3 release of 12/Jun does not have this port  reg	[3:0]		afu_cfg_xmit_rate_config_0_top               ;
// mcp3 release of 12/Jun does not have this port  reg	[3:0]		afu_cfg_xmit_rate_config_1_top               ;
// mcp3 release of 12/Jun does not have this port  reg	[3:0]		afu_cfg_xmit_rate_config_2_top               ;
// mcp3 release of 12/Jun does not have this port  reg	[3:0]		afu_cfg_xmit_rate_config_3_top               ;
 // Wires for AFU o/p
//	Table 2: TLX Response Credit Interface
   wire			afu_tlx_resp_credit               ;
   wire	[6:0]		afu_tlx_resp_initial_credit               ;

//	Table 4: TLX Command Credit Interface
   wire			afu_tlx_cmd_credit               ;
   wire	[6:0]		afu_tlx_cmd_initial_credit               ;

//	Table 5: TLX to AFU Response Data Interface
   wire			afu_tlx_resp_rd_req               ;
   wire	[2:0]		afu_tlx_resp_rd_cnt               ;

//	Table 6: TLX to AFU Command Data Interface
   wire			afu_tlx_cmd_rd_req               ;
   wire	[2:0]		afu_tlx_cmd_rd_cnt               ;

//	Table 8: TLX Framer Command Interface
   wire			afu_tlx_cmd_valid                ;
   wire	[7:0]		afu_tlx_cmd_opcode               ;
   wire	[11:0]		afu_tlx_cmd_actag                ;
   wire	[3:0]		afu_tlx_cmd_stream_id            ;
   wire	[67:0]		afu_tlx_cmd_ea_or_obj            ;
   wire	[15:0]		afu_tlx_cmd_afutag               ;
   wire	[1:0]		afu_tlx_cmd_dl                   ;
   wire	[2:0]		afu_tlx_cmd_pl                   ;
   wire			afu_tlx_cmd_os                   ;
   wire	[63:0]		afu_tlx_cmd_be                   ;
   wire	[3:0]		afu_tlx_cmd_flag                 ;
   wire			afu_tlx_cmd_endian               ;
   wire	[15:0]		afu_tlx_cmd_bdf                  ;
   wire	[19:0]		afu_tlx_cmd_pasid                ;
   wire	[5:0]		afu_tlx_cmd_pg_size              ;
   wire	[511:0]		afu_tlx_cdata_bus                  ;
   wire			afu_tlx_cdata_bdi               ;	// TODO: TLX Ref Design doc lists this as afu_tlx_cdata_bad
   wire			afu_tlx_cdata_valid               ;

//	Table 9: TLX Framer Response Interface
   wire			afu_tlx_resp_valid               ;
   wire	[7:0]		afu_tlx_resp_opcode               ;
   wire	[1:0]		afu_tlx_resp_dl               ;
   wire	[15:0]		afu_tlx_resp_capptag               ;
   wire	[1:0]		afu_tlx_resp_dp               ;
   wire	[3:0]		afu_tlx_resp_code               ;
   wire			afu_tlx_rdata_valid               ;
   wire	[511:0]		afu_tlx_rdata_bus               ;
   wire			afu_tlx_rdata_bdi               ;

//	Table 10: TLX Framer Template Configuration
// mcp3 release of 12/Jun does not have this port wire			afu_cfg_xmit_tmpl_config_0               ;
// mcp3 release of 12/Jun does not have this port wire			afu_cfg_xmit_tmpl_config_1               ;
// mcp3 release of 12/Jun does not have this port wire			afu_cfg_xmit_tmpl_config_2               ;
// mcp3 release of 12/Jun does not have this port wire			afu_cfg_xmit_tmpl_config_3               ;
// mcp3 release of 12/Jun does not have this port wire	[3:0]		afu_cfg_xmit_rate_config_0               ;
// mcp3 release of 12/Jun does not have this port wire	[3:0]		afu_cfg_xmit_rate_config_1               ;
// mcp3 release of 12/Jun does not have this port wire	[3:0]		afu_cfg_xmit_rate_config_2               ;
// mcp3 release of 12/Jun does not have this port wire	[3:0]		afu_cfg_xmit_rate_config_3               ;

// Other wires
   wire            reset_n;
  // Table 1: TLX to AFU Response Interface
   wire             tlx_afu_resp_valid;
   wire [7:0]       tlx_afu_resp_opcode;
   wire [15:0]      tlx_afu_resp_afutag;
   wire [3:0]       tlx_afu_resp_code;
   wire [5:0]       tlx_afu_resp_pg_size;
   wire [1:0]       tlx_afu_resp_dl;
   wire [1:0]       tlx_afu_resp_dp;
   wire [23:0]      tlx_afu_resp_host_tag;
   wire [17:0]      tlx_afu_resp_addr_tag;
   wire [3:0]       tlx_afu_resp_cache_state;

//	Table 3: TLX to AFU Command Interface
   wire             tlx_afu_cmd_valid;
   wire [7:0]       tlx_afu_cmd_opcode;
   wire [15:0]      tlx_afu_cmd_capptag;
   wire [1:0]       tlx_afu_cmd_dl;
   wire [2:0]       tlx_afu_cmd_pl;
   wire [63:0]      tlx_afu_cmd_be;
   wire             tlx_afu_cmd_end;
//   wire             tlx_afu_cmd_t;
   wire [63:0]      tlx_afu_cmd_pa;
   wire [3:0]       tlx_afu_cmd_flag;
   wire             tlx_afu_cmd_os;

//	Table 5: TLX to AFU Response Data Interface
   reg             tlx_afu_resp_data_valid;
   reg [511:0]     tlx_afu_resp_data_bus;
   reg             tlx_afu_resp_data_bdi;

//	Table 6: TLX to AFU Command Data Interface
   wire             tlx_afu_cmd_data_valid;
   wire [511:0]     tlx_afu_cmd_data_bus;
   wire             tlx_afu_cmd_data_bdi;

//	Table 7: TLX Framer credit interface
   wire             tlx_afu_resp_credit;
   wire             tlx_afu_resp_data_credit;
   wire             tlx_afu_cmd_credit;
   wire             tlx_afu_cmd_data_credit;
   wire [3:0]       tlx_afu_cmd_initial_credit;
   wire [3:0]       tlx_afu_resp_initial_credit;
   wire [5:0]       tlx_afu_cmd_data_initial_credit;
   wire [5:0]       tlx_afu_resp_data_initial_credit;

  // These signals do not appear on the RefDesign Doc. However it is present
  // on the TLX spec
// mcp3 release of 12/Jun does not have this port  wire             afu_cfg_in_rcv_tmpl_capability_0;
// mcp3 release of 12/Jun does not have this port  wire             afu_cfg_in_rcv_tmpl_capability_1;
// mcp3 release of 12/Jun does not have this port  wire             afu_cfg_in_rcv_tmpl_capability_2;
// mcp3 release of 12/Jun does not have this port  wire             afu_cfg_in_rcv_tmpl_capability_3;
// mcp3 release of 12/Jun does not have this port  wire [3:0]       afu_cfg_in_rcv_rate_capability_0;
// mcp3 release of 12/Jun does not have this port  wire [3:0]       afu_cfg_in_rcv_rate_capability_1;
// mcp3 release of 12/Jun does not have this port  wire [3:0]       afu_cfg_in_rcv_rate_capability_2;
// mcp3 release of 12/Jun does not have this port  wire [3:0]       afu_cfg_in_rcv_rate_capability_3;
   wire             tlx_afu_ready;
   wire             tlx_cfg0_in_rcv_tmpl_capability_0;
   wire             tlx_cfg0_in_rcv_tmpl_capability_1;
   wire             tlx_cfg0_in_rcv_tmpl_capability_2;
   wire             tlx_cfg0_in_rcv_tmpl_capability_3;
   wire       [3:0] tlx_cfg0_in_rcv_rate_capability_0;
   wire       [3:0] tlx_cfg0_in_rcv_rate_capability_1;
   wire       [3:0] tlx_cfg0_in_rcv_rate_capability_2;
   wire       [3:0] tlx_cfg0_in_rcv_rate_capability_3;
   wire             tlx_cfg0_valid;
   wire       [7:0] tlx_cfg0_opcode;
   wire      [63:0] tlx_cfg0_pa;
   wire             tlx_cfg0_t;
   wire       [2:0] tlx_cfg0_pl;
   wire      [15:0] tlx_cfg0_capptag;
   wire      [31:0] tlx_cfg0_data_bus;
   wire             tlx_cfg0_data_bdi;
   wire             tlx_cfg0_resp_ack;
   wire	      [3:0] cfg0_tlx_initial_credit;
   wire		    cfg0_tlx_credit_return;
   wire		    cfg0_tlx_resp_valid	;
   wire	      [7:0] cfg0_tlx_resp_opcode;
   wire	     [15:0] cfg0_tlx_resp_capptag;
   wire       [3:0] cfg0_tlx_resp_code	;
   wire       [3:0] cfg0_tlx_rdata_offset;
   wire      [31:0] cfg0_tlx_rdata_bus	;
   wire		    cfg0_tlx_rdata_bdi	;
   wire      [4:0]  ro_device;
   wire     [31:0] ro_dlx0_version ;                     // -- Connect to DLX output at next level, or tie off to all 0s
   wire     [31:0] tlx0_cfg_oc4_tlx_version ;                     // -- (was ro_tlx0_version[31:0])
   wire     [31:0] flsh_cfg_rdata ;                     // -- Contains data read back from FLASH register (valid when rden=1 and 'flsh_done'=1)
   wire            flsh_cfg_done  ;                     // -- FLASH logic pulses to 1 for 1 cycle when write is complete, or when rdata contains valid results
   wire      [7:0] flsh_cfg_status;                     // -- Device Specific status information
   wire      [1:0] flsh_cfg_bresp ;                     // -- Write response from selected AXI4-Lite device
   wire      [1:0] flsh_cfg_rresp ;                     // -- Read  response from selected AXI4-Lite device
 // Integers
  integer         i;
  integer         resetCnt;
 // Sim related variables
  reg [0:63]      simulationTime ;
  reg             simulationError;

initial begin
    resetCnt = 0;
    i = 0;
    tlx_clock				<= 0;
    afu_clock				<= 0;
    reset   				<= 1;

  // Table 1: TLX to AFU Response Interface
    tlx_afu_resp_valid_top			<= 0;
    tlx_afu_resp_opcode_top			<= 8'b0;
    tlx_afu_resp_afutag_top			<= 16'b0;
    tlx_afu_resp_code_top			<= 4'b0;
    tlx_afu_resp_pg_size_top		<= 6'b0;
    tlx_afu_resp_dl_top			<= 2'b0;
    tlx_afu_resp_dp_top			<= 2'b0;
    tlx_afu_resp_host_tag_top		<= 24'b0;
    tlx_afu_resp_addr_tag_top		<= 18'b0;
    tlx_afu_resp_cache_state_top		<= 4'b0;

//	Table 3: TLX to AFU Command Interface
    tlx_afu_cmd_valid_top			<= 0;
    tlx_afu_cmd_opcode_top			<= 8'b0;
    tlx_afu_cmd_capptag_top			<= 16'b0;
    tlx_afu_cmd_dl_top			<= 2'b0;
    tlx_afu_cmd_pl_top			<= 3'b0;
    tlx_afu_cmd_be_top			<= 64'b0;
    tlx_afu_cmd_end_top			<= 0;
//    tlx_afu_cmd_t_top			<= 0;
    tlx_afu_cmd_pa_top			<= 64'b0;
    tlx_afu_cmd_flag_top			<= 4'b0;
    tlx_afu_cmd_os_top			<= 0;

//	Table 5: TLX to AFU Response Data Interface
    tlx_afu_resp_data_valid_top		<= 0;
    tlx_afu_resp_data_bus_top		<= 512'b0;
    tlx_afu_resp_data_bdi_top		<= 0;

//	Table 6: TLX to AFU Command Data Interface
    tlx_afu_cmd_data_valid_top		<= 0;
    tlx_afu_cmd_data_bus_top		<= 512'b0;
    tlx_afu_cmd_data_bdi_top		<= 0;

//	Table 7: TLX Framer credit interface
    tlx_afu_resp_credit_top			<= 0;
    tlx_afu_resp_data_credit_top		<= 0;
    tlx_afu_cmd_credit_top			<= 0;
    tlx_afu_cmd_data_credit_top		<= 0;
    tlx_afu_cmd_resp_initial_credit_top	<= 4'b1000;
    tlx_afu_data_initial_credit_top	<= 4'b0111;
    tlx_afu_cmd_data_initial_credit_top		<= 6'b100000;
    tlx_afu_resp_data_initial_credit_top	<= 6'b100000;

  // These signals do not appear on the RefDesign Doc. However it is present
  // on the TLX spec
// mcp3 release of 12/Jun does not have this port   afu_cfg_in_rcv_tmpl_capability_0_top	<= 0;
// mcp3 release of 12/Jun does not have this port   afu_cfg_in_rcv_tmpl_capability_1_top	<= 0;
// mcp3 release of 12/Jun does not have this port   afu_cfg_in_rcv_tmpl_capability_2_top	<= 0;
// mcp3 release of 12/Jun does not have this port   afu_cfg_in_rcv_tmpl_capability_3_top	<= 0;
// mcp3 release of 12/Jun does not have this port   afu_cfg_in_rcv_rate_capability_0_top	<= 4'b0;
// mcp3 release of 12/Jun does not have this port   afu_cfg_in_rcv_rate_capability_1_top	<= 4'b0;
// mcp3 release of 12/Jun does not have this port   afu_cfg_in_rcv_rate_capability_2_top	<= 4'b0;
// mcp3 release of 12/Jun does not have this port   afu_cfg_in_rcv_rate_capability_3_top	<= 4'b0;
    tlx_afu_ready_top			<= 1;
    tlx_cfg0_in_rcv_tmpl_capability_0_top	<= 0;
    tlx_cfg0_in_rcv_tmpl_capability_1_top	<= 0;
    tlx_cfg0_in_rcv_tmpl_capability_2_top	<= 0;
    tlx_cfg0_in_rcv_tmpl_capability_3_top	<= 0;
    tlx_cfg0_in_rcv_rate_capability_0_top	<= 4'b0;
    tlx_cfg0_in_rcv_rate_capability_1_top	<= 4'b0;
    tlx_cfg0_in_rcv_rate_capability_2_top	<= 4'b0;
    tlx_cfg0_in_rcv_rate_capability_3_top	<= 4'b0;
    tlx_cfg0_valid_top				<= 0;
    tlx_cfg0_opcode_top				<= 8'b0;
    tlx_cfg0_pa_top				<= 64'b0;
    tlx_cfg0_t_top				<= 0;
    tlx_cfg0_pl_top				<= 3'b0;
    tlx_cfg0_capptag_top			<= 16'b0;
    tlx_cfg0_data_bus_top			<= 32'b0;
    tlx_cfg0_data_bdi_top			<= 0;
    tlx_cfg0_resp_ack_top			<= 0;
    ro_device_top				<= 5'b0;			//Updated per Jeff R's note of 23/Jun/2017
end

  // Clock generation

  always begin
    #2 tlx_clock = !tlx_clock;
  end

  always @ (posedge tlx_clock) begin
    afu_clock = !afu_clock;
  end

  always @ ( tlx_clock ) begin
    if(resetCnt < 30)
      resetCnt = resetCnt + 1;
    else
      i = 1;
  end

  always @ ( tlx_clock ) begin
    if(resetCnt == RESET_CYCLES + 2)
      #0 tlx_bfm_init();
  end

  always @ ( tlx_clock ) begin
    if(resetCnt < RESET_CYCLES)
      reset = 1'b1;
    else
      reset = 1'b0;
  end

  always @ (posedge tlx_clock) begin
   afu_tlx_resp_credit_top               <= afu_tlx_resp_credit;
   afu_tlx_resp_initial_credit_top       <= afu_tlx_resp_initial_credit;
   afu_tlx_cmd_credit_top                <= afu_tlx_cmd_credit;
   afu_tlx_cmd_initial_credit_top        <= afu_tlx_cmd_initial_credit;
   afu_tlx_resp_rd_req_top               <= afu_tlx_resp_rd_req;
   afu_tlx_resp_rd_cnt_top               <= afu_tlx_resp_rd_cnt;
   afu_tlx_cmd_rd_req_top                <= afu_tlx_cmd_rd_req;
   afu_tlx_cmd_rd_cnt_top                <= afu_tlx_cmd_rd_cnt;
   afu_tlx_cmd_valid_top                 <= afu_tlx_cmd_valid;
   afu_tlx_cmd_opcode_top                <= afu_tlx_cmd_opcode;
   afu_tlx_cmd_actag_top                 <= afu_tlx_cmd_actag;
   afu_tlx_cmd_stream_id_top             <= afu_tlx_cmd_stream_id;
   afu_tlx_cmd_ea_or_obj_top             <= afu_tlx_cmd_ea_or_obj;
   afu_tlx_cmd_afutag_top                <= afu_tlx_cmd_afutag;
   afu_tlx_cmd_dl_top                    <= afu_tlx_cmd_dl;
   afu_tlx_cmd_pl_top                    <= afu_tlx_cmd_pl;
   afu_tlx_cmd_os_top                    <= afu_tlx_cmd_os;
   afu_tlx_cmd_be_top                    <= afu_tlx_cmd_be;
   afu_tlx_cmd_flag_top                  <= afu_tlx_cmd_flag;
   afu_tlx_cmd_endian_top                <= afu_tlx_cmd_endian;
   afu_tlx_cmd_bdf_top                   <= afu_tlx_cmd_bdf;
   afu_tlx_cmd_pasid_top                 <= afu_tlx_cmd_pasid;
   afu_tlx_cmd_pg_size_top               <= afu_tlx_cmd_pg_size;
   afu_tlx_cdata_bus_top                 <= afu_tlx_cdata_bus;
   afu_tlx_cdata_bdi_top                 <= afu_tlx_cdata_bdi;
   afu_tlx_cdata_valid_top               <= afu_tlx_cdata_valid;
   afu_tlx_resp_valid_top                <= afu_tlx_resp_valid;
   afu_tlx_resp_opcode_top               <= afu_tlx_resp_opcode;
   afu_tlx_resp_dl_top                   <= afu_tlx_resp_dl;
   afu_tlx_resp_capptag_top              <= afu_tlx_resp_capptag;
   afu_tlx_resp_dp_top                   <= afu_tlx_resp_dp;
   afu_tlx_resp_code_top                 <= afu_tlx_resp_code;
   afu_tlx_rdata_valid_top               <= afu_tlx_rdata_valid;
   afu_tlx_rdata_bus_top                 <= afu_tlx_rdata_bus;
   afu_tlx_rdata_bdi_top                 <= afu_tlx_rdata_bdi;
// mcp3 release of 12/Jun does not have this port  afu_cfg_xmit_tmpl_config_0_top        <= afu_cfg_xmit_tmpl_config_0;
// mcp3 release of 12/Jun does not have this port  afu_cfg_xmit_tmpl_config_1_top        <= afu_cfg_xmit_tmpl_config_1;
// mcp3 release of 12/Jun does not have this port  afu_cfg_xmit_tmpl_config_2_top        <= afu_cfg_xmit_tmpl_config_2;
// mcp3 release of 12/Jun does not have this port  afu_cfg_xmit_tmpl_config_3_top        <= afu_cfg_xmit_tmpl_config_3;
// mcp3 release of 12/Jun does not have this port  afu_cfg_xmit_rate_config_0_top        <= afu_cfg_xmit_rate_config_0;
// mcp3 release of 12/Jun does not have this port  afu_cfg_xmit_rate_config_1_top        <= afu_cfg_xmit_rate_config_1;
// mcp3 release of 12/Jun does not have this port  afu_cfg_xmit_rate_config_2_top        <= afu_cfg_xmit_rate_config_2;
// mcp3 release of 12/Jun does not have this port  afu_cfg_xmit_rate_config_3_top        <= afu_cfg_xmit_rate_config_3;
   cfg0_tlx_initial_credit_top		<= cfg0_tlx_initial_credit; // new
   cfg0_tlx_credit_return_top		<= cfg0_tlx_credit_return;  // new lgt
   cfg0_tlx_resp_valid_top              <= cfg0_tlx_resp_valid;
   cfg0_tlx_resp_opcode_top             <= cfg0_tlx_resp_opcode;
   cfg0_tlx_resp_capptag_top            <= cfg0_tlx_resp_capptag;
   cfg0_tlx_resp_code_top               <= cfg0_tlx_resp_code;
   cfg0_tlx_rdata_offset_top            <= cfg0_tlx_rdata_offset;
   cfg0_tlx_rdata_bus_top               <= cfg0_tlx_rdata_bus;
   cfg0_tlx_rdata_bdi_top               <= cfg0_tlx_rdata_bdi;
  end

    assign 	reset_n		= !reset;
   
// Pass Through Signals
  // Table 1: TLX to AFU Response Interface
    assign 	tlx_afu_resp_valid		= tlx_afu_resp_valid_top;
    assign 	tlx_afu_resp_opcode		= tlx_afu_resp_opcode_top;
    assign 	tlx_afu_resp_afutag		= tlx_afu_resp_afutag_top;
    assign 	tlx_afu_resp_code		= tlx_afu_resp_code_top;
    assign 	tlx_afu_resp_pg_size		= tlx_afu_resp_pg_size_top;
    assign 	tlx_afu_resp_dl			= tlx_afu_resp_dl_top;
    assign 	tlx_afu_resp_dp			= tlx_afu_resp_dp_top;
    assign 	tlx_afu_resp_host_tag		= tlx_afu_resp_host_tag_top;
    assign 	tlx_afu_resp_addr_tag		= tlx_afu_resp_addr_tag_top;
    assign 	tlx_afu_resp_cache_state	= tlx_afu_resp_cache_state_top;

//	Table 3: TLX to AFU Command Interface
    assign 	tlx_afu_cmd_valid		= tlx_afu_cmd_valid_top;
    assign 	tlx_afu_cmd_opcode		= tlx_afu_cmd_opcode_top;
    assign 	tlx_afu_cmd_capptag		= tlx_afu_cmd_capptag_top;
    assign 	tlx_afu_cmd_dl			= tlx_afu_cmd_dl_top;
    assign 	tlx_afu_cmd_pl			= tlx_afu_cmd_pl_top;
    assign 	tlx_afu_cmd_be			= tlx_afu_cmd_be_top;
    assign 	tlx_afu_cmd_end			= tlx_afu_cmd_end_top;
//    assign 	tlx_afu_cmd_t			= tlx_afu_cmd_t_top;
    assign 	tlx_afu_cmd_pa			= tlx_afu_cmd_pa_top;
    assign 	tlx_afu_cmd_flag		= tlx_afu_cmd_flag_top;
    assign 	tlx_afu_cmd_os			= tlx_afu_cmd_os_top;

//	Table 5: TLX to AFU Response Data Interface
   always @( negedge tlx_clock ) begin
      tlx_afu_resp_data_valid		<= tlx_afu_resp_data_valid_dly1;
      tlx_afu_resp_data_bus		<= tlx_afu_resp_data_bus_dly1;
      tlx_afu_resp_data_bdi		<= tlx_afu_resp_data_bdi_dly1;
   end

//	Table 6: TLX to AFU Command Data Interface
    assign 	tlx_afu_cmd_data_valid		= tlx_afu_cmd_data_valid_top;
    assign 	tlx_afu_cmd_data_bus		= tlx_afu_cmd_data_bus_top;
    assign 	tlx_afu_cmd_data_bdi		= tlx_afu_cmd_data_bdi_top;

//	Table 7: TLX Framer credit interface
    assign 	tlx_afu_resp_credit			= tlx_afu_resp_credit_top;
    assign 	tlx_afu_resp_data_credit		= tlx_afu_resp_data_credit_top;
    assign 	tlx_afu_cmd_credit			= tlx_afu_cmd_credit_top;
    assign 	tlx_afu_cmd_data_credit			= tlx_afu_cmd_data_credit_top;
    assign 	tlx_afu_cmd_initial_credit		= tlx_afu_cmd_resp_initial_credit_top;
    assign 	tlx_afu_resp_initial_credit		= tlx_afu_data_initial_credit_top;
    assign 	tlx_afu_cmd_data_initial_credit		= tlx_afu_cmd_data_initial_credit_top;
    assign 	tlx_afu_resp_data_initial_credit	= tlx_afu_resp_data_initial_credit_top;

  // These signals do not appear on the RefDesign Doc. However it is present
  // on the TLX spec
// mcp3 release of 12/Jun does not have this port   assign 	afu_cfg_in_rcv_tmpl_capability_0	= afu_cfg_in_rcv_tmpl_capability_0_top;
// mcp3 release of 12/Jun does not have this port   assign 	afu_cfg_in_rcv_tmpl_capability_1	= afu_cfg_in_rcv_tmpl_capability_1_top;
// mcp3 release of 12/Jun does not have this port   assign 	afu_cfg_in_rcv_tmpl_capability_2	= afu_cfg_in_rcv_tmpl_capability_2_top;
// mcp3 release of 12/Jun does not have this port   assign 	afu_cfg_in_rcv_tmpl_capability_3	= afu_cfg_in_rcv_tmpl_capability_3_top;
// mcp3 release of 12/Jun does not have this port   assign 	afu_cfg_in_rcv_rate_capability_0	= afu_cfg_in_rcv_rate_capability_0_top;
// mcp3 release of 12/Jun does not have this port   assign 	afu_cfg_in_rcv_rate_capability_1	= afu_cfg_in_rcv_rate_capability_1_top;
// mcp3 release of 12/Jun does not have this port   assign 	afu_cfg_in_rcv_rate_capability_2	= afu_cfg_in_rcv_rate_capability_2_top;
// mcp3 release of 12/Jun does not have this port   assign 	afu_cfg_in_rcv_rate_capability_3	= afu_cfg_in_rcv_rate_capability_3_top;
    assign 	tlx_afu_ready				= tlx_afu_ready_top;
    assign 	ro_device				= ro_device_top;
    assign 	tlx_cfg0_in_rcv_tmpl_capability_0	= tlx_cfg0_in_rcv_tmpl_capability_0_top;
    assign 	tlx_cfg0_in_rcv_tmpl_capability_1	= tlx_cfg0_in_rcv_tmpl_capability_1_top;
    assign 	tlx_cfg0_in_rcv_tmpl_capability_2	= tlx_cfg0_in_rcv_tmpl_capability_2_top;
    assign 	tlx_cfg0_in_rcv_tmpl_capability_3	= tlx_cfg0_in_rcv_tmpl_capability_3_top;
    assign 	tlx_cfg0_in_rcv_rate_capability_0	= tlx_cfg0_in_rcv_rate_capability_0_top;
    assign 	tlx_cfg0_in_rcv_rate_capability_1	= tlx_cfg0_in_rcv_rate_capability_1_top;
    assign 	tlx_cfg0_in_rcv_rate_capability_2	= tlx_cfg0_in_rcv_rate_capability_2_top;
    assign 	tlx_cfg0_in_rcv_rate_capability_3	= tlx_cfg0_in_rcv_rate_capability_3_top;
    assign	tlx_cfg0_valid				= tlx_cfg0_valid_top;
    assign 	tlx_cfg0_opcode				= tlx_cfg0_opcode_top;
    assign 	tlx_cfg0_pa				= tlx_cfg0_pa_top;
    assign 	tlx_cfg0_t				= tlx_cfg0_t_top;
    assign 	tlx_cfg0_pl				= tlx_cfg0_pl_top;
    assign 	tlx_cfg0_capptag			= tlx_cfg0_capptag_top;
    assign 	tlx_cfg0_data_bus			= tlx_cfg0_data_bus_top;
    assign 	tlx_cfg0_data_bdi			= tlx_cfg0_data_bdi_top;
    assign 	tlx_cfg0_resp_ack			= tlx_cfg0_resp_ack_top;
    assign 	ro_dlx0_version				= 32'b0;
    assign 	tlx0_cfg_oc4_tlx_version		= 32'b0;
    assign 	flsh_cfg_rdata				= 32'b0;
    assign 	flsh_cfg_done				= 1'b0;
    assign 	flsh_cfg_status				= 8'b0;
    assign 	flsh_cfg_bresp				= 2'b0;
    assign 	flsh_cfg_rresp				= 2'b0;

   // a block to delay the resp_data path 1 cycle
   // todo: variable number of cycles from 1 to n
   always @ ( negedge tlx_clock ) begin
      tlx_afu_resp_data_valid_dly1 <= tlx_afu_resp_data_valid_top;
      tlx_afu_resp_data_bus_dly1 <= tlx_afu_resp_data_bus_top;
      tlx_afu_resp_data_bdi_dly1 <= tlx_afu_resp_data_bdi_top;
   end

   always @ ( negedge tlx_clock ) begin
      tlx_afu_resp_data_valid_dly2 <= tlx_afu_resp_data_valid_dly1;
      tlx_afu_resp_data_bus_dly2 <= tlx_afu_resp_data_bus_dly1;
      tlx_afu_resp_data_bdi_dly2 <= tlx_afu_resp_data_bdi_dly1;
   end

  always @ ( tlx_clock ) begin
    simulationTime = $time;
    #0 set_simulation_time(simulationTime);
    #0 tlx_bfm( tlx_clock,
             afu_clock,
             reset,
				// Table 1: TLX to AFU Response Interface
	tlx_afu_resp_valid_top,
	tlx_afu_resp_opcode_top,
	tlx_afu_resp_afutag_top,
	tlx_afu_resp_code_top,
	tlx_afu_resp_pg_size_top,
	tlx_afu_resp_dl_top,
	tlx_afu_resp_dp_top,
	tlx_afu_resp_host_tag_top,
	tlx_afu_resp_addr_tag_top,
	tlx_afu_resp_cache_state_top,

				//	Table 2: TLX Response Credit Interface
	afu_tlx_resp_credit_top,
	afu_tlx_resp_initial_credit_top,

				//	Table 3: TLX to AFU Command Interface
	tlx_afu_cmd_valid_top,
	tlx_afu_cmd_opcode_top,
	tlx_afu_cmd_capptag_top,
	tlx_afu_cmd_dl_top,
	tlx_afu_cmd_pl_top,
	tlx_afu_cmd_be_top,
	tlx_afu_cmd_end_top,
//	tlx_afu_cmd_t_top,
	tlx_afu_cmd_pa_top,
	tlx_afu_cmd_flag_top,
	tlx_afu_cmd_os_top,

				//	Table 4: TLX Command Credit Interface
	afu_tlx_cmd_credit_top,
	afu_tlx_cmd_initial_credit_top,

				//	Table 5: TLX to AFU Response Data Interface
	tlx_afu_resp_data_valid_top,
	tlx_afu_resp_data_bus_top,
	tlx_afu_resp_data_bdi_top,
	afu_tlx_resp_rd_req_top,
	afu_tlx_resp_rd_cnt_top,

				//	Table 6: TLX to AFU Command Data Interface
	tlx_afu_cmd_data_valid_top,
	tlx_afu_cmd_data_bus_top,
	tlx_afu_cmd_data_bdi_top,
	afu_tlx_cmd_rd_req_top,
	afu_tlx_cmd_rd_cnt_top,

				//	Table 7: TLX Framer credit interface
	tlx_afu_resp_credit_top,
	tlx_afu_resp_data_credit_top,
	tlx_afu_cmd_credit_top,
	tlx_afu_cmd_data_credit_top,
	tlx_afu_cmd_resp_initial_credit_top,
	tlx_afu_data_initial_credit_top,
	tlx_afu_cmd_data_initial_credit_top,
	tlx_afu_resp_data_initial_credit_top,

				//	Table 8: TLX Framer Command Interface
	afu_tlx_cmd_valid_top,
	afu_tlx_cmd_opcode_top,
	afu_tlx_cmd_actag_top,
	afu_tlx_cmd_stream_id_top,
	afu_tlx_cmd_ea_or_obj_top,
	afu_tlx_cmd_afutag_top,
	afu_tlx_cmd_dl_top,
	afu_tlx_cmd_pl_top,
	afu_tlx_cmd_os_top,
	afu_tlx_cmd_be_top,
	afu_tlx_cmd_flag_top,
	afu_tlx_cmd_endian_top,
	afu_tlx_cmd_bdf_top,
	afu_tlx_cmd_pasid_top,
	afu_tlx_cmd_pg_size_top,
	afu_tlx_cdata_bus_top,
	afu_tlx_cdata_bdi_top,// TODO: TLX Ref Design doc lists this as afu_tlx_cdata_bad
	afu_tlx_cdata_valid_top,

				//	Table 9: TLX Framer Response Interface
	afu_tlx_resp_valid_top,
	afu_tlx_resp_opcode_top,
	afu_tlx_resp_dl_top,
	afu_tlx_resp_capptag_top,
	afu_tlx_resp_dp_top,
	afu_tlx_resp_code_top,
	afu_tlx_rdata_valid_top,
	afu_tlx_rdata_bus_top,
	afu_tlx_rdata_bdi_top,

	tlx_afu_ready_top,
        tlx_cfg0_in_rcv_tmpl_capability_0_top,
        tlx_cfg0_in_rcv_tmpl_capability_1_top,
        tlx_cfg0_in_rcv_tmpl_capability_2_top,
        tlx_cfg0_in_rcv_tmpl_capability_3_top,
        tlx_cfg0_in_rcv_rate_capability_0_top,
        tlx_cfg0_in_rcv_rate_capability_1_top,
        tlx_cfg0_in_rcv_rate_capability_2_top,
        tlx_cfg0_in_rcv_rate_capability_3_top,

        tlx_cfg0_valid_top,
        tlx_cfg0_opcode_top,
        tlx_cfg0_pa_top,
        tlx_cfg0_t_top,
        tlx_cfg0_pl_top,
        tlx_cfg0_capptag_top,
        tlx_cfg0_data_bus_top,
        tlx_cfg0_data_bdi_top,
        tlx_cfg0_resp_ack_top,

  	cfg0_tlx_initial_credit_top,
  	cfg0_tlx_credit_return_top,
   	cfg0_tlx_resp_valid_top	,
	cfg0_tlx_resp_opcode_top,
	cfg0_tlx_resp_capptag_top,
	cfg0_tlx_resp_code_top	,
	cfg0_tlx_rdata_offset_top,
	cfg0_tlx_rdata_bus_top	,
   	cfg0_tlx_rdata_bdi_top,
	ro_device_top
    );
  end

  always @ (negedge tlx_clock) begin
    #0 get_simuation_error(simulationError);
  end

  always @ (posedge tlx_clock) begin
    if(simulationError)
      $finish;
  end

//  mcp3_top a0 (		- UMA 30/Mar/2017 - the top instance name on
//  mcp changed
  mcp3_device a0 (
    .clock_tlx(tlx_clock),
    .clock_afu(afu_clock),
    .reset_n(reset_n),
  // Table 1: TLX to AFU Response Interface
    .tlx_afu_resp_valid               (tlx_afu_resp_valid),
    .tlx_afu_resp_opcode              (tlx_afu_resp_opcode),
    .tlx_afu_resp_afutag              (tlx_afu_resp_afutag),
    .tlx_afu_resp_code                (tlx_afu_resp_code),
    .tlx_afu_resp_pg_size             (tlx_afu_resp_pg_size),
    .tlx_afu_resp_dl                  (tlx_afu_resp_dl),
    .tlx_afu_resp_dp                  (tlx_afu_resp_dp),
    .tlx_afu_resp_host_tag          (tlx_afu_resp_host_tag), //    Reserved for CAPI4.0
    .tlx_afu_resp_addr_tag            (tlx_afu_resp_addr_tag),
    .tlx_afu_resp_cache_state       (tlx_afu_resp_cache_state), //    Reserved for CAPI4.0

//	Table 2: TLX Response Credit Interface
    .afu_tlx_resp_credit              (afu_tlx_resp_credit),
    .afu_tlx_resp_initial_credit      (afu_tlx_resp_initial_credit),

//	Table 3: TLX to AFU Command Interface
    .tlx_afu_cmd_valid                (tlx_afu_cmd_valid),
    .tlx_afu_cmd_opcode               (tlx_afu_cmd_opcode),
    .tlx_afu_cmd_capptag              (tlx_afu_cmd_capptag),
    .tlx_afu_cmd_dl                   (tlx_afu_cmd_dl),
    .tlx_afu_cmd_pl                   (tlx_afu_cmd_pl),
    .tlx_afu_cmd_be                   (tlx_afu_cmd_be),
    .tlx_afu_cmd_end                  (tlx_afu_cmd_end),
//    .tlx_afu_cmd_t                    (tlx_afu_cmd_t),
    .tlx_afu_cmd_pa                   (tlx_afu_cmd_pa),
    .tlx_afu_cmd_flag                 (tlx_afu_cmd_flag),
    .tlx_afu_cmd_os                   (tlx_afu_cmd_os),

//	Table 4: TLX Command Credit Interface
    .afu_tlx_cmd_credit               (afu_tlx_cmd_credit),
    .afu_tlx_cmd_initial_credit       (afu_tlx_cmd_initial_credit),

//	Table 5: TLX to AFU Response Data Interface
    .tlx_afu_resp_data_valid          (tlx_afu_resp_data_valid),
    .tlx_afu_resp_data_bus            (tlx_afu_resp_data_bus),
    .tlx_afu_resp_data_bdi            (tlx_afu_resp_data_bdi),
    .afu_tlx_resp_rd_req              (afu_tlx_resp_rd_req),
    .afu_tlx_resp_rd_cnt              (afu_tlx_resp_rd_cnt),

//	Table 6: TLX to AFU Command Data Interface
    .tlx_afu_cmd_data_valid           (tlx_afu_cmd_data_valid),
    .tlx_afu_cmd_data_bus             (tlx_afu_cmd_data_bus),
    .tlx_afu_cmd_data_bdi             (tlx_afu_cmd_data_bdi),
    .afu_tlx_cmd_rd_req               (afu_tlx_cmd_rd_req),
    .afu_tlx_cmd_rd_cnt               (afu_tlx_cmd_rd_cnt),

//	Table 7: TLX Framer credit interface
    .tlx_afu_resp_credit              (tlx_afu_resp_credit),
    .tlx_afu_resp_data_credit         (tlx_afu_resp_data_credit),
    .tlx_afu_cmd_credit               (tlx_afu_cmd_credit),
    .tlx_afu_cmd_data_credit          (tlx_afu_cmd_data_credit),
    .tlx_afu_cmd_initial_credit  (tlx_afu_cmd_initial_credit),
    .tlx_afu_resp_initial_credit  (tlx_afu_resp_initial_credit),
    .tlx_afu_cmd_data_initial_credit      (tlx_afu_cmd_data_initial_credit),
    .tlx_afu_resp_data_initial_credit      (tlx_afu_resp_data_initial_credit),

//	Table 8: TLX Framer Command Interface
    .afu_tlx_cmd_valid                (afu_tlx_cmd_valid),
    .afu_tlx_cmd_opcode               (afu_tlx_cmd_opcode),
    .afu_tlx_cmd_actag                (afu_tlx_cmd_actag),
    .afu_tlx_cmd_stream_id            (afu_tlx_cmd_stream_id),
    .afu_tlx_cmd_ea_or_obj            (afu_tlx_cmd_ea_or_obj),
    .afu_tlx_cmd_afutag               (afu_tlx_cmd_afutag),
    .afu_tlx_cmd_dl                   (afu_tlx_cmd_dl),
    .afu_tlx_cmd_pl                   (afu_tlx_cmd_pl),
    .afu_tlx_cmd_os                   (afu_tlx_cmd_os),
    .afu_tlx_cmd_be                   (afu_tlx_cmd_be),
    .afu_tlx_cmd_flag                 (afu_tlx_cmd_flag),
    .afu_tlx_cmd_endian               (afu_tlx_cmd_endian),
    .afu_tlx_cmd_bdf                  (afu_tlx_cmd_bdf),
    .afu_tlx_cmd_pasid                (afu_tlx_cmd_pasid),
    .afu_tlx_cmd_pg_size              (afu_tlx_cmd_pg_size),
    .afu_tlx_cdata_bus                (afu_tlx_cdata_bus),
    .afu_tlx_cdata_bdi                (afu_tlx_cdata_bdi),	// TODO: TLX Ref Design doc lists this as afu_tlx_cdata_bad
    .afu_tlx_cdata_valid              (afu_tlx_cdata_valid),

//	Table 9: TLX Framer Response Interface
    .afu_tlx_resp_valid               (afu_tlx_resp_valid),
    .afu_tlx_resp_opcode              (afu_tlx_resp_opcode),
    .afu_tlx_resp_dl                  (afu_tlx_resp_dl),
    .afu_tlx_resp_capptag             (afu_tlx_resp_capptag),
    .afu_tlx_resp_dp                  (afu_tlx_resp_dp),
    .afu_tlx_resp_code                (afu_tlx_resp_code),
    .afu_tlx_rdata_valid              (afu_tlx_rdata_valid),
    .afu_tlx_rdata_bus                (afu_tlx_rdata_bus),
    .afu_tlx_rdata_bdi                (afu_tlx_rdata_bdi),	// TODO: the name given in the spec for this signal is afu_tlx_rdata_bad

    .tlx_afu_ready                    (tlx_afu_ready),
 // Added newly on the mcp3 release of 30/Mar/2017
    .cfg_vpd_rden                     (),
    .cfg_vpd_wren                     (),
    .cfg_vpd_addr                     (),
    .cfg_vpd_wdata                    (),
    .vpd_cfg_done                     (1'b0),
    .vpd_cfg_rdata                    (32'b0),
// Added newly on the mcp3 release of 12/Jun/2017
  // -- Configuration Ports: Receive Capabilities (determined by TLX design)
   .tlx_cfg0_in_rcv_tmpl_capability_0	(tlx_cfg0_in_rcv_tmpl_capability_0),
   .tlx_cfg0_in_rcv_tmpl_capability_1	(tlx_cfg0_in_rcv_tmpl_capability_1),
   .tlx_cfg0_in_rcv_tmpl_capability_2	(tlx_cfg0_in_rcv_tmpl_capability_2),
   .tlx_cfg0_in_rcv_tmpl_capability_3	(tlx_cfg0_in_rcv_tmpl_capability_3),
   .tlx_cfg0_in_rcv_rate_capability_0	(tlx_cfg0_in_rcv_rate_capability_0),
   .tlx_cfg0_in_rcv_rate_capability_1	(tlx_cfg0_in_rcv_rate_capability_1),
   .tlx_cfg0_in_rcv_rate_capability_2	(tlx_cfg0_in_rcv_rate_capability_2),
   .tlx_cfg0_in_rcv_rate_capability_3	(tlx_cfg0_in_rcv_rate_capability_3),
  // -- Configuration Ports: Drive Configuration (determined by software)
   .cfg0_tlx_xmit_tmpl_config_0		(),			// For now left open
   .cfg0_tlx_xmit_tmpl_config_1		(),			// For now left open
   .cfg0_tlx_xmit_tmpl_config_2		(),			// For now left open
   .cfg0_tlx_xmit_tmpl_config_3		(),			// For now left open
   .cfg0_tlx_xmit_rate_config_0		(),			// For now left open
   .cfg0_tlx_xmit_rate_config_1		(),			// For now left open
   .cfg0_tlx_xmit_rate_config_2		(),			// For now left open
   .cfg0_tlx_xmit_rate_config_3		(),			// For now left open
  // -- Port 0: config_write/read commands from host    
   .tlx_cfg0_valid			(tlx_cfg0_valid),
   .tlx_cfg0_opcode			(tlx_cfg0_opcode),
   .tlx_cfg0_pa				(tlx_cfg0_pa),
   .tlx_cfg0_t				(tlx_cfg0_t),
   .tlx_cfg0_pl				(tlx_cfg0_pl),
   .tlx_cfg0_capptag			(tlx_cfg0_capptag),
   .tlx_cfg0_data_bus			(tlx_cfg0_data_bus),
   .tlx_cfg0_data_bdi			(tlx_cfg0_data_bdi),
   .cfg0_tlx_initial_credit		(cfg0_tlx_initial_credit),
   .cfg0_tlx_credit_return		(cfg0_tlx_credit_return),
  // -- Port 0: config_* responses back to host
   .cfg0_tlx_resp_valid			(cfg0_tlx_resp_valid),
   .cfg0_tlx_resp_opcode		(cfg0_tlx_resp_opcode),
   .cfg0_tlx_resp_capptag		(cfg0_tlx_resp_capptag),
   .cfg0_tlx_resp_code			(cfg0_tlx_resp_code),
   .cfg0_tlx_rdata_offset		(cfg0_tlx_rdata_offset),
   .cfg0_tlx_rdata_bus			(cfg0_tlx_rdata_bus),
   .cfg0_tlx_rdata_bdi			(cfg0_tlx_rdata_bdi),
   .tlx_cfg0_resp_ack			(tlx_cfg0_resp_ack),
   .ro_device				(ro_device),
   .vpd_err_unimplemented_addr          (1'b0),
   .ro_dlx0_version                     (ro_dlx0_version),	// -- Contains data read back from FLASH register (valid when rden=1 and 'flsh_done'=1) - left open
   .tlx0_cfg_oc4_tlx_version            (tlx0_cfg_oc4_tlx_version),	// -- Contains data read back from FLASH register (valid when rden=1 and 'flsh_done'=1) - left open
   .cfg_flsh_devsel                     (),                  // -- Select AXI4-Lite device to target - left open
   .cfg_flsh_addr                       (),                 // -- Read or write address to selected target - left open
   .cfg_flsh_wren                       (),                 // -- Set to 1 to write a location, hold at 1 until see 'flsh_done' = 1 then clear to 0 - left open
   .cfg_flsh_wdata                      (),                 // -- Contains data to write to FLASH register (valid while wren=1) - left open
   .cfg_flsh_rden                       (),                 // -- Set to 1 to read  a location, hold at 1 until see 'flsh_done' = 1 the clear to 0 - left open
   .flsh_cfg_rdata                      (flsh_cfg_rdata),	// -- Contains data read back from FLASH register (valid when rden=1 and 'flsh_done'=1) - left open
   .flsh_cfg_done                       (flsh_cfg_done),	// -- FLASH logic pulses to 1 for 1 cycle when write is complete, or when rdata contains valid results
   .flsh_cfg_status                     (flsh_cfg_status),	// -- Device Specific status information
   .flsh_cfg_bresp                      (flsh_cfg_bresp),	// -- Write response from selected AXI4-Lite device
   .flsh_cfg_rresp                      (flsh_cfg_rresp),	// -- Read  response from selected AXI4-Lite device
   .cfg_flsh_expand_enable              (),             // -- When 1, expand/collapse 4 bytes of data into four, 1 byte AXI operations - left open
   .cfg_flsh_expand_dir                 ()             // -- When 0, expand bytes [3:0] in order 0,1,2,3 . When 1, expand in order 3,2,1,0 . - left open
  );

endmodule
