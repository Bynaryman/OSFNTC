`timescale 1ns / 1ps
`default_nettype none
//////////////////////////////////////////////////////////////////////////////////
//
// Company: BSC
// Engineer: LEDOUX Louis
//
// Create Date: 30/08/2022
// Description: axi stream sv wrapper +
//  assuming N + M arithmetic dense words fit in the 512b bus or 1024. Depending CAPI2 or CAPI3
//
//////////////////////////////////////////////////////////////////////////////////


module my_sv_wrapper #
(
    parameter integer DATA_WIDTH = 1024
)
(
    // System signals
    input  wire clk,
    input  wire rst_n,

    // SLAVE SIDE

    // control signals
    output logic rtr_o,
    input  wire rts_i,
    input  wire sow_i,
    input  wire eow_dma_i,
    // data in
    input wire [DATA_WIDTH-1:0] data_i,

    // MASTER SIDE

    // control signals
    input  wire rtr_i,
    output logic rts_o,
    // data out
    output logic [DATA_WIDTH-1:0] data_o
);

// localparams
localparam integer ARITH_IN_WIDTH = 4;
localparam integer ARITH_OUT_WIDTH = 4;
localparam integer N = 32;
localparam integer M = 31;
// this number exists only after a flopoco run
localparam integer S3FDP_PP_DEPTH = 1;
localparam integer L2A_PP_DEPTH = 1;
localparam integer FIFO_DEPTH = 1024;
localparam integer OUT_WIDTH = M*ARITH_OUT_WIDTH;
localparam integer FIFO_WIDTH = OUT_WIDTH;

// signals

// SA
logic [DATA_WIDTH-1:0] sa_data_i;
logic [OUT_WIDTH-1:0] sa_data_o;
logic sa_eob;
logic sa_eob_q;
logic sa_valid_o;
logic sa_sob;

// FIFO
logic [OUT_WIDTH-1:0] fifo_data_o;
logic fifo_valid_o;

//    _____ __
//   / ___// /___ __   _____
//   \__ \/ / __ `/ | / / _ \
//  ___/ / / /_/ /| |/ /  __/
// /____/_/\__,_/ |___/\___/
assign rtr_o = rtr_i;
assign sa_data_i = (rts_i & rtr_o) ? data_i : {DATA_WIDTH{1'b0}};
assign sa_eob = sa_data_i[DATA_WIDTH-1];
assign sa_sob = sa_data_i[DATA_WIDTH-2];

//    _____ ___
//   / ___//   |
//   \__ \/ /| |
//  ___/ / ___ |
// /____/_/  |_|
SystolicArray sa_inst (

    // System
    .clk     ( clk                                            ),
    .rst     ( ~rst_n                                         ),

    // IOs
    .rowsA   ( sa_data_i[(N*ARITH_IN_WIDTH)-1:0]                 ),
    .colsB   ( sa_data_i[((M+N)*ARITH_IN_WIDTH)-1:N*ARITH_IN_WIDTH] ),
    .SOB     ( sa_sob                                         ),
    .EOB     ( sa_eob                                         ),
    .colsC   ( sa_data_o                                      ),
    .EOB_Q_o ( sa_eob_q                                       )

);

// valid_o from systolic array logic
// we will create a shift register of size N+2+PP_DEPTH(S3FDP)-1+PP_DEPTH(L2A)
// we connect the eob_q of PE(N-1,M-1) as the input bit
// the OR reduction from the N MSB bits are the valid signal
localparam integer size = N+2+S3FDP_PP_DEPTH+L2A_PP_DEPTH-1;
logic [size-1:0] shift_register;
always_ff @(posedge clk or negedge rst_n) begin
	if ( ~rst_n ) begin
		shift_register <= 0;
	end
	else begin
		shift_register <= {shift_register[size-2:0],sa_eob_q};
	end
end
assign sa_valid_o = |shift_register[(size-2) -: N];
//     ____________________
//    / ____/  _/ ____/ __ \
//   / /_   / // /_  / / / /
//  / __/ _/ // __/ / /_/ /
// /_/   /___/_/    \____/
// this fifo acts as backpressure FIFO that accepts data from the systolic array
// in case the host ready is de-acked. Should never be full, the depth is ~the execution time
// of the SA
fifo #
(
    .C_WIDTH ( FIFO_WIDTH ),
    .C_DEPTH ( FIFO_DEPTH ),
    .C_DELAY ( 2 )
) fifo_inst (
     .CLK      ( clk                  ), // Clock
     .RST      ( ~rst_n               ), // Sync reset, active high

     .WR_DATA  ( sa_data_o            ), // Write data input
     .WR_VALID ( sa_valid_o           ), // Write enable, high active
     .WR_READY ( ), // ~Full condition

     .RD_DATA  ( fifo_data_o          ), // Read data output
     .RD_READY ( rtr_i & fifo_valid_o ), // Read enable, high active
     .RD_VALID ( fifo_valid_o         )  // ~Empty condition
);

//     __  ___           __
//    /  |/  /___ ______/ /____  _____
//   / /|_/ / __ `/ ___/ __/ _ \/ ___/
//  / /  / / /_/ (__  ) /_/  __/ /
// /_/  /_/\__,_/____/\__/\___/_/
assign rts_o  = fifo_valid_o;
assign data_o = {{DATA_WIDTH-OUT_WIDTH{1'b0}},fifo_data_o};

endmodule
`default_nettype wire
