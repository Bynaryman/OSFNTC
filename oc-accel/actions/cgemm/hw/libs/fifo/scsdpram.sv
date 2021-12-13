/*
 Description: A simple, single clock, simple dual port (SCSDP) ram
 */
`timescale 1ns/1ns
	
module scsdpram
    #(
      parameter C_WIDTH = 32,
      parameter C_DEPTH = 1024
      )
    (
     input                       CLK,

     input                       RD1_EN,
     input [clog2s(C_DEPTH)-1:0] RD1_ADDR,
     output [C_WIDTH-1:0]        RD1_DATA,

     input                       WR1_EN,
     input [clog2s(C_DEPTH)-1:0] WR1_ADDR,
     input [C_WIDTH-1:0]         WR1_DATA
     );

    reg [C_WIDTH-1:0]            rMemory [C_DEPTH-1:0];
    reg [C_WIDTH-1:0]            rDataOut;

    assign RD1_DATA = rDataOut;

    always @(posedge CLK) begin
        if (WR1_EN) begin
            rMemory[WR1_ADDR] <= #1 WR1_DATA;
        end
        if(RD1_EN) begin
            rDataOut <= #1 rMemory[RD1_ADDR];
        end
    end
endmodule
