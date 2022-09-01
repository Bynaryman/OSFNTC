`timescale 1ns/1ns
module shiftreg
    #(parameter C_DEPTH=10,
      parameter C_WIDTH=32,
      parameter C_VALUE=0
      )
    (input                            CLK,
     input                            RST_IN,
     input [C_WIDTH-1:0]              WR_DATA,
     output [(C_DEPTH+1)*C_WIDTH-1:0] RD_DATA);

    // Start Flag Shift Register. Data enables are derived from the
    // taps on this shift register.

    wire [(C_DEPTH+1)*C_WIDTH-1:0]    wDataShift;
    reg [C_WIDTH-1:0]                 rDataShift[C_DEPTH:0];

    assign wDataShift[(C_WIDTH*0)+:C_WIDTH] = WR_DATA;
    always @(posedge CLK) begin
        rDataShift[0] <= WR_DATA;
    end

    genvar                                     i;
    generate
        for (i = 1 ; i <= C_DEPTH; i = i + 1) begin : gen_sr_registers
            assign wDataShift[(C_WIDTH*i)+:C_WIDTH] = rDataShift[i-1];
            always @(posedge CLK) begin
                if(RST_IN)
                    rDataShift[i] <= C_VALUE;
                else
                    rDataShift[i] <= rDataShift[i-1];
            end
        end
    endgenerate
    assign RD_DATA = wDataShift;

endmodule
