//----------------------------------------------------------------------------
// Filename:			fifo.v
// Description: Standard 0-delay fifo implementation. Takes WR_DATA on WR_READY
// and WR_VALID. RD_DATA is read on RD_READY and RD_VALID
//-----------------------------------------------------------------------------
`timescale 1ns/1ns
	function integer clog2;
		input [31:0] v;
		reg [31:0] value;
		begin
			value = v;
			if (value == 1) begin
				clog2 = 0;
			end
			else begin
				value = value-1;
				for (clog2=0; value>0; clog2=clog2+1)
					value = value>>1;
			end
		end
	endfunction
	// clog2s -- calculate the ceiling log2 value, min return is 1 (safe).
	function integer clog2s;
		input [31:0] v;
		reg [31:0] value;
		begin
			value = v;
			if (value == 1) begin
				clog2s = 1;
			end
			else begin
				value = value-1;
				for (clog2s=0; value>0; clog2s=clog2s+1)
					value = value>>1;
			end
		end
	endfunction
module fifo
    #(
      parameter C_WIDTH = 32,   // Data bus width
      parameter C_DEPTH = 1024, // Depth of the FIFO
      parameter C_DELAY = 2
      )
    (
     input                CLK, // Clock
     input                RST, // Sync reset, active high

     input [C_WIDTH-1:0]  WR_DATA, // Write data input
     input                WR_VALID, // Write enable, high active
     output               WR_READY, // ~Full condition

     output [C_WIDTH-1:0] RD_DATA, // Read data output
     input                RD_READY, // Read enable, high active
     output               RD_VALID // ~Empty condition
     );

    // Local parameters
    localparam C_POW2_DEPTH = 2**clog2(C_DEPTH);
    localparam C_DEPTH_WIDTH = clog2s(C_POW2_DEPTH);

    wire [C_DELAY:0]      wDelayTaps;
    wire                  wDelayWrEn;
    wire                  wWrEn;
    wire                  wRdEn;
    wire                  wRdRdy;

    wire                  wRdEnInternal;
    wire                  wRdEnExternal;
    wire                  wEmptyNow;
    wire                  wEmptyNext;

    wire                  wOutputEmpty;

    wire                  wFullNow;
    wire                  wFullNext;

    reg                   rValid;
    reg [C_DEPTH_WIDTH:0] rWrPtr,_rWrPtr;
    reg [C_DEPTH_WIDTH:0] rWrPtrPlus1, _rWrPtrPlus1;
    reg [C_DEPTH_WIDTH:0] rRdPtr,_rRdPtr;
    reg [C_DEPTH_WIDTH:0] rRdPtrPlus1,_rRdPtrPlus1;
    reg                   rFull,_rFull;
    reg                   rEmpty,_rEmpty;

    assign wRdEnInternal = ~wEmptyNow & ~rValid; // Read enable to propogate data to the BRAM output
    assign wRdEnExternal = RD_READY & !rEmpty;   // Read enable to change data on the output
    assign wRdEn = wRdEnInternal | wRdEnExternal;
    assign wRdRdy = RD_READY & rValid; // Read Data already on the output bus

    assign wWrEn = WR_VALID & !rFull;

    assign wEmptyNow = (rRdPtr == rWrPtr);
    assign wEmptyNext = (wRdEn & ~wWrEn & (rWrPtr == rRdPtrPlus1));

    assign wFullNow = (rRdPtr[C_DEPTH_WIDTH-1:0] == rWrPtr[C_DEPTH_WIDTH-1:0]) &
                      (rWrPtr[C_DEPTH_WIDTH] != rRdPtr[C_DEPTH_WIDTH]);
    assign wFullNext = wWrEn & ~wRdEn & (rWrPtrPlus1[C_DEPTH_WIDTH-1:0] == rRdPtr[C_DEPTH_WIDTH-1:0]) &
                       (rWrPtrPlus1[C_DEPTH_WIDTH] != rRdPtr[C_DEPTH_WIDTH]);

    // Calculate empty
    assign RD_VALID = rValid;

    always @ (posedge CLK) begin
        rEmpty <= #1 (RST ? 1'd1 : _rEmpty);
    end

    always @ (*) begin
        _rEmpty = (wEmptyNow & ~wWrEn) | wEmptyNext;
    end

    always @(posedge CLK) begin
        if(RST) begin
            rValid <= #1 0;
        end else if(wRdEn | wRdRdy) begin
            rValid <= #1 ~(wEmptyNow);
        end
    end

    // Write pointer logic.
    always @ (posedge CLK) begin
        if (RST) begin
            rWrPtr <= #1 0;
            rWrPtrPlus1 <= #1 1;
        end else begin
            rWrPtr <= #1 _rWrPtr;
            rWrPtrPlus1 <= #1 _rWrPtrPlus1;
        end
    end

    always @ (*) begin
        if (wWrEn) begin
            _rWrPtr = rWrPtrPlus1;
            _rWrPtrPlus1 = rWrPtrPlus1 + 1'd1;
        end else begin
            _rWrPtr = rWrPtr;
            _rWrPtrPlus1 = rWrPtrPlus1;
        end
    end

    // Read pointer logic.
    always @ (posedge CLK) begin
        if (RST) begin
            rRdPtr <= #1 0;
            rRdPtrPlus1 <= #1 1;
        end else begin
            rRdPtr <= #1 _rRdPtr;
            rRdPtrPlus1 <= #1 _rRdPtrPlus1;
        end
    end

    always @ (*) begin
        if (wRdEn) begin
            _rRdPtr = rRdPtrPlus1;
            _rRdPtrPlus1 = rRdPtrPlus1 + 1'd1;
        end else begin
            _rRdPtr = rRdPtr;
            _rRdPtrPlus1 = rRdPtrPlus1;
        end
    end


    // Calculate full
    assign WR_READY = ~rFull;

    always @ (posedge CLK) begin
        rFull <= #1 (RST ? 1'd0 : _rFull);
    end

    always @ (*) begin
        _rFull = wFullNow | wFullNext;
    end

    // Memory block (synthesis attributes applied to this module will
    // determine the memory option).
    scsdpram
        #(
          .C_WIDTH(C_WIDTH),
          .C_DEPTH(C_POW2_DEPTH)
          /*AUTOINSTPARAM*/)
    mem
        (
         .WR1_EN                        (wWrEn),
         .WR1_ADDR                      (rWrPtr[C_DEPTH_WIDTH-1:0]),
         .WR1_DATA                      (WR_DATA),

         .RD1_EN                        (wRdEn),
         .RD1_ADDR                      (rRdPtr[C_DEPTH_WIDTH-1:0]),
         .RD1_DATA                      (RD_DATA),
         /*AUTOINST*/
         // Inputs
         .CLK                           (CLK));

    shiftreg
        #(
          // Parameters
          .C_DEPTH                      (C_DELAY),
          .C_WIDTH                      (1'b1),
          .C_VALUE                      (0)
          /*AUTOINSTPARAM*/)
    shiftreg_wr_delay_inst
        (
         // Outputs
         .RD_DATA                       (wDelayTaps),
         // Inputs
         .RST_IN                        (RST),
         .WR_DATA                       (wWrEn),
         /*AUTOINST*/
         // Inputs
         .CLK                           (CLK));


endmodule
