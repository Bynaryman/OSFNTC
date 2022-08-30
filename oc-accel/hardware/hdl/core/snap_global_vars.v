`timescale 1ns/1ps
`define DEBUG
`define FRAMEWORK




`define AD9V3
  `define IMP_VERSION_DAT 64'h04_05_12FF_394C_0518
  `define BUILD_DATE_DAT 64'h0000_2022_0830_1550
  `define CARD_TYPE 8'h31
  `define USERCODE 64'h0



  `define HLS_ACTION_TYPE 32'h0
  `define HLS_RELEASE_LEVEL 32'h0



  `define NUM_OF_ACTIONS 16'h0
  `define DMA_XFER_SIZE 4'h0
  `define DMA_ALIGNMENT 4'h0
  `define SDRAM_SIZE 16'h0




`define IDW 1

  `define CTXW 9
  `define TAGW 7





  `define AXI_MM_DW 1024
  `define AXI_ST_DW 1024


  `define AXI_MM_AW 64
  `define AXI_ST_AW 64
  `define AXI_LITE_DW 32
  `define AXI_LITE_AW 32
  `define AXI_AWUSER 9
  `define AXI_ARUSER 9
  `define AXI_WUSER 9
  `define AXI_RUSER 9
  `define AXI_BUSER 9
  `define AXI_ST_USER 9

  `define AXI_CARD_MEM_ID_WIDTH 4
  `define AXI_CARD_MEM_DATA_WIDTH 512
  `define AXI_CARD_MEM_ADDR_WIDTH 33
  `define AXI_CARD_MEM_USER_WIDTH 1

  `define AXI_CARD_HBM_ID_WIDTH 4
  `define AXI_CARD_HBM_DATA_WIDTH 256
  `define AXI_CARD_HBM_ADDR_WIDTH 34
  `define AXI_CARD_HBM_USER_WIDTH 1

  `define INT_BITS 64

`ifdef ACTION_HALF_WIDTH
  `define AXI_ACT_DW 512
`else
  `define AXI_ACT_DW 1024
`endif
