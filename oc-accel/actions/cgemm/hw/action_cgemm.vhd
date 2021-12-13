-----------------------------------------------------------------------------------
-- Company: BSC
-- Engineer: LEDOUX Louis
--
-- Create Date: 2021
-- Design Name: action_cgemm
-- Module Name: action_cgemm
-- Project Name:
-- Target Devices:
-- Tool Versions:
-- Description:
--
--
-- Dependencies:
--
-- Revision:
-- Revision 0.01 - File Created
-- Additional Comments:
--
-----------------------------------------------------------------------------------

library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_misc.all;
use ieee.STD_LOGIC_UNSIGNED.all;
use ieee.numeric_std.all;


entity action_cgemm is
    generic (
        -- Parameters of Axi Master Bus Interface AXI_CARD_MEM0 ; to DDR memory
        C_AXI_CARD_MEM0_ID_WIDTH     : integer   := 2;
        C_AXI_CARD_MEM0_ADDR_WIDTH   : integer   := 33;
        C_AXI_CARD_MEM0_DATA_WIDTH   : integer   := 512;
        C_AXI_CARD_MEM0_AWUSER_WIDTH : integer   := 1;
        C_AXI_CARD_MEM0_ARUSER_WIDTH : integer   := 1;
        C_AXI_CARD_MEM0_WUSER_WIDTH  : integer   := 1;
        C_AXI_CARD_MEM0_RUSER_WIDTH  : integer   := 1;
        C_AXI_CARD_MEM0_BUSER_WIDTH  : integer   := 1;

        -- Parameters of Axi Slave Bus Interface AXI_CTRL_REG
        C_AXI_CTRL_REG_DATA_WIDTH    : integer   := 32;
        C_AXI_CTRL_REG_ADDR_WIDTH    : integer   := 32;

        -- Parameters of Axi Master Bus Interface AXI_HOST_MEM ; to Host memory
        C_AXI_HOST_MEM_ID_WIDTH      : integer   := 2;
        C_AXI_HOST_MEM_ADDR_WIDTH    : integer   := 64;
        C_AXI_HOST_MEM_DATA_WIDTH    : integer   := 1024;
        C_AXI_HOST_MEM_AWUSER_WIDTH  : integer   := 9;
        C_AXI_HOST_MEM_ARUSER_WIDTH  : integer   := 9;
        C_AXI_HOST_MEM_WUSER_WIDTH   : integer   := 9;
        C_AXI_HOST_MEM_RUSER_WIDTH   : integer   := 9;
        C_AXI_HOST_MEM_BUSER_WIDTH   : integer   := 9;
        INT_BITS                     : integer   := 64;
        CONTEXT_BITS                 : integer   := 8
    );
    port (
        action_clk              : in STD_LOGIC;
        action_rst_n            : in STD_LOGIC;
        int_req_ack             : in STD_LOGIC;
        int_req                 : out std_logic;
        int_src                 : out std_logic_vector(INT_BITS-1 DOWNTO 0);
        int_ctx                 : out std_logic_vector(CONTEXT_BITS-1 DOWNTO 0);

        --
        --
        --
        -- Ports of Axi Slave Bus Interface AXI_CTRL_REG
        axi_ctrl_reg_awaddr     : in std_logic_vector(C_AXI_CTRL_REG_ADDR_WIDTH-1 downto 0);
        -- axi_ctrl_reg_awprot : in std_logic_vector(2 downto 0);
        axi_ctrl_reg_awvalid    : in std_logic;
        axi_ctrl_reg_awready    : out std_logic;
        axi_ctrl_reg_wdata      : in std_logic_vector(C_AXI_CTRL_REG_DATA_WIDTH-1 downto 0);
        axi_ctrl_reg_wstrb      : in std_logic_vector((C_AXI_CTRL_REG_DATA_WIDTH/8)-1 downto 0);
        axi_ctrl_reg_wvalid     : in std_logic;
        axi_ctrl_reg_wready     : out std_logic;
        axi_ctrl_reg_bresp      : out std_logic_vector(1 downto 0);
        axi_ctrl_reg_bvalid     : out std_logic;
        axi_ctrl_reg_bready     : in std_logic;
        axi_ctrl_reg_araddr     : in std_logic_vector(C_AXI_CTRL_REG_ADDR_WIDTH-1 downto 0);
        -- axi_ctrl_reg_arprot  : in std_logic_vector(2 downto 0);
        axi_ctrl_reg_arvalid    : in std_logic;
        axi_ctrl_reg_arready    : out std_logic;
        axi_ctrl_reg_rdata      : out std_logic_vector(C_AXI_CTRL_REG_DATA_WIDTH-1 downto 0);
        axi_ctrl_reg_rresp      : out std_logic_vector(1 downto 0);
        axi_ctrl_reg_rvalid     : out std_logic;
        axi_ctrl_reg_rready     : in std_logic;

        -- Ports of Axi Master Bus Interface AXI_HOST_MEM
        -- to HOST memory
        axi_host_mem_awaddr   : out std_logic_vector(C_AXI_HOST_MEM_ADDR_WIDTH-1 downto 0);
        axi_host_mem_awlen    : out std_logic_vector(7 downto 0);
        axi_host_mem_awsize   : out std_logic_vector(2 downto 0);
        axi_host_mem_awburst  : out std_logic_vector(1 downto 0);
        axi_host_mem_awlock   : out std_logic_vector(1 downto 0);
        axi_host_mem_awcache  : out std_logic_vector(3 downto 0);
        axi_host_mem_awprot   : out std_logic_vector(2 downto 0);
        axi_host_mem_awregion : out std_logic_vector(3 downto 0);
        axi_host_mem_awqos    : out std_logic_vector(3 downto 0);
        axi_host_mem_awvalid  : out std_logic;
        axi_host_mem_awready  : in std_logic;
        axi_host_mem_wdata    : out std_logic_vector(C_AXI_HOST_MEM_DATA_WIDTH-1 downto 0);
        axi_host_mem_wstrb    : out std_logic_vector(C_AXI_HOST_MEM_DATA_WIDTH/8-1 downto 0);
        axi_host_mem_wlast    : out std_logic;
        axi_host_mem_wvalid   : out std_logic;
        axi_host_mem_wready   : in std_logic;
        axi_host_mem_bresp    : in std_logic_vector(1 downto 0);
        axi_host_mem_bvalid   : in std_logic;
        axi_host_mem_bready   : out std_logic;
        axi_host_mem_araddr   : out std_logic_vector(C_AXI_HOST_MEM_ADDR_WIDTH-1 downto 0);
        axi_host_mem_arlen    : out std_logic_vector(7 downto 0);
        axi_host_mem_arsize   : out std_logic_vector(2 downto 0);
        axi_host_mem_arburst  : out std_logic_vector(1 downto 0);
        axi_host_mem_arlock   : out std_logic_vector(1 downto 0);
        axi_host_mem_arcache  : out std_logic_vector(3 downto 0);
        axi_host_mem_arprot   : out std_logic_vector(2 downto 0);
        axi_host_mem_arregion : out std_logic_vector(3 downto 0);
        axi_host_mem_arqos    : out std_logic_vector(3 downto 0);
        axi_host_mem_arvalid  : out std_logic;
        axi_host_mem_arready  : in std_logic;
        axi_host_mem_rdata    : in std_logic_vector(C_AXI_HOST_MEM_DATA_WIDTH-1 downto 0);
        axi_host_mem_rresp    : in std_logic_vector(1 downto 0);
        axi_host_mem_rlast    : in std_logic;
        axi_host_mem_rvalid   : in std_logic;
        axi_host_mem_rready   : out std_logic;
--      axi_host_mem_error    : out std_logic;
        axi_host_mem_arid     : out std_logic_vector(C_AXI_HOST_MEM_ID_WIDTH-1 downto 0);
        axi_host_mem_aruser   : out std_logic_vector(C_AXI_HOST_MEM_ARUSER_WIDTH-1 downto 0);
        axi_host_mem_awid     : out std_logic_vector(C_AXI_HOST_MEM_ID_WIDTH-1 downto 0);
        axi_host_mem_awuser   : out std_logic_vector(C_AXI_HOST_MEM_AWUSER_WIDTH-1 downto 0);
        axi_host_mem_bid      : in std_logic_vector(C_AXI_HOST_MEM_ID_WIDTH-1 downto 0);
        axi_host_mem_buser    : in std_logic_vector(C_AXI_HOST_MEM_BUSER_WIDTH-1 downto 0);
        axi_host_mem_rid      : in std_logic_vector(C_AXI_HOST_MEM_ID_WIDTH-1 downto 0);
        axi_host_mem_ruser    : in std_logic_vector(C_AXI_HOST_MEM_RUSER_WIDTH-1 downto 0);
        axi_host_mem_wuser    : out std_logic_vector(C_AXI_HOST_MEM_WUSER_WIDTH-1 downto 0)
);
end action_cgemm;

architecture action_cgemm of action_cgemm is

    component my_sv_wrapper
    generic (
        DATA_WIDTH : INTEGER := 1024
    );
    port (
        --  System signals
        clk    : in std_logic;
        rst_n  : in std_logic;

        -- SLAVE SIDE
        rtr_o  : out std_logic;
        rts_i  : in std_logic;
        sow_i  : in std_logic;
        eow_dma_i  : in std_logic;
        data_i : in std_logic_vector(DATA_WIDTH-1 downto 0);

        -- MASTER SIDE
        rtr_i  : in std_logic;
        rts_o  : out std_logic;
        data_o : out std_logic_vector(DATA_WIDTH-1 downto 0)
    );
    end component my_sv_wrapper;


        type   fsm_app_t    is (IDLE, JUST_COUNT_DOWN, WAIT_FOR_MEMCOPY_DONE);
        type   fsm_copy_t   is (IDLE, PROCESS_COPY, WAIT_FOR_WRITE_DONE);

        signal fsm_app_q        : fsm_app_t;
        signal fsm_copy_q       : fsm_copy_t;

        -- AXI Lite SLAVE control
        signal s_Context_ID     : std_logic_vector(31 downto 0);
        signal s_src_addr_h     : std_logic_vector(31 downto 0);
        signal s_src_addr_l     : std_logic_vector(31 downto 0);
        signal s_src_data_size  : std_logic_vector(31 downto 0);
        signal s_dst_data_size  : std_logic_vector(31 downto 0);
        signal s_dst_addr_h     : std_logic_vector(31 downto 0);
        signal s_dst_addr_l     : std_logic_vector(31 downto 0);
        signal s_rd_burst_num   : std_logic_vector(31 downto 0);
        signal s_wr_burst_num   : std_logic_vector(31 downto 0);
        signal s_transfer_type  : std_logic_vector(31 downto 0);
        signal int_enable       : std_logic;
        signal app_start        : std_logic;
        signal app_done         : std_logic;
        signal app_ready        : std_logic;
        signal app_idle         : std_logic;

        -- AXI MM MASTER / DMA
        signal dma_rd_req         : std_logic;
        signal dma_rd_req_ack     : std_logic;
        signal rd_addr            : std_logic_vector( 63 downto 0);
        signal rd_len             : std_logic_vector(  7 downto 0);
        signal dma_rd_data        : std_logic_vector(511 downto 0);
        signal dma_rd_data_valid  : std_logic;
        signal dma_rd_data_last   : std_logic;
        signal dma_rd_data_taken  : std_logic;

        signal dma_wr_req         : std_logic;
        signal dma_wr_req_ack     : std_logic;
        signal wr_addr            : std_logic_vector( 63 downto 0);
        signal wr_len             : std_logic_vector(  7 downto 0);
        signal wr_data            : std_logic_vector(511 downto 0);
        signal dma_wr_data_strobe : std_logic_vector( 63  downto 0);
        signal dma_wr_data_valid  : std_logic;
        signal dma_wr_data_last   : std_logic;
        signal dma_wr_ready       : std_logic;
        signal dma_wr_bready      : std_logic;
        signal dma_wr_done        : std_logic;
        signal dma_wr_eow_o       : std_logic;

        signal memcopy            : boolean;
        signal start_copy         : std_logic;
        signal last_write_done    : std_logic;
        signal src_host           : std_logic;
        signal dest_host          : std_logic;

        signal blocks_to_write    : std_logic_vector(31 downto 0);
        signal blocks_expected    : std_logic_vector(31 downto 0);
        signal blocks_to_read     : std_logic_vector(31 downto 0);
        signal dma_blocks_to_read : std_logic_vector(31 downto 0); -- similar to blocks to read but for the read bus and not the AR bus
        signal first_max_blk_r    : std_logic_vector(31 downto 0);
        signal first_blk_r        : std_logic_vector(31 downto 0);
        signal first_max_blk_w    : std_logic_vector(31 downto 0);
        signal first_blk_w        : std_logic_vector(31 downto 0);
        signal write_counter_up   : std_logic_vector(31 downto 0);
        signal total_write_count  : std_logic_vector(31 downto 0);
        signal write_counter_dn   : std_logic_vector(25 downto 0);
        signal wr_req_count       : integer;
        signal wr_done_count      : integer;
        signal rd_data_taken      : std_logic;
        signal rd_req_ack         : std_logic;
        signal wr_req_ack         : std_logic;
        signal rd_requests_done   : std_logic;
        signal wr_requests_done   : std_logic;
        signal rd_addr_adder      : std_logic_vector(15 downto 0);
        signal wr_addr_adder      : std_logic_vector(15 downto 0);
        signal block_diff         : std_logic_vector(63 downto 0);
        signal last_write         : std_logic;
        signal last_write_q       : std_logic;
        signal first_write_q      : std_logic;
        signal wr_gate            : std_logic;
        signal eow_dma            : std_logic;

        -- custom pipeline signals
        signal my_sv_wrapper_rtr_o  : std_logic;
        signal my_sv_wrapper_sow_o  : std_logic;
        signal my_sv_wrapper_data_o  : std_logic_vector(C_AXI_HOST_MEM_DATA_WIDTH-1 downto 0);
        signal my_sv_wrapper_rts_o  : std_logic;


        function or_reduce (signal arg : std_logic_vector) return std_logic is
          variable result : std_logic;

        begin
          result := '0';
          for i in arg'low to arg'high loop
            result := result or arg(i);
          end loop;  -- i
          return result;
        end or_reduce;
begin

    int_ctx <= s_Context_ID(CONTEXT_BITS - 1 downto 0);
    int_src <= "00";
    int_req <= '0'; -- no interuption for the moment

-- Instantiation of Axi Bus Interface AXI_CTRL_REG
action_axi_slave_inst : entity work.action_axi_slave
    generic map (
        C_S_AXI_DATA_WIDTH  => C_AXI_CTRL_REG_DATA_WIDTH,
        C_S_AXI_ADDR_WIDTH  => C_AXI_CTRL_REG_ADDR_WIDTH
    )
    port map (
        -- User ports begin
        o_int_enable            => int_enable,
        i_Action_Type           => x"8686_8604",  -- action type
        i_Action_VER            => x"0000_0002",  -- 2nd version (OpenCAPI)
        o_Context_ID            => s_Context_ID,

        o_app_start             => app_start,
        i_app_done              => app_done,
        i_app_ready             => '0',           -- app_ready,
        i_app_idle              => app_idle,

        o_src_addr_h            => s_src_addr_h,
        o_src_addr_l            => s_src_addr_l,
        o_src_data_size         => s_src_data_size,
        o_dst_addr_h            => s_dst_addr_h,
        o_dst_addr_l            => s_dst_addr_l,
        o_dst_data_size         => s_dst_data_size,
        o_rd_burst_num          => s_rd_burst_num,
        o_wr_burst_num          => s_wr_burst_num,
        o_transfer_type         => s_transfer_type,

        -- User ports ends
        S_AXI_ACLK  => action_clk,
        S_AXI_ARESETN   => action_rst_n,
        S_AXI_AWADDR    => axi_ctrl_reg_awaddr,
        -- S_AXI_AWPROT    => axi_ctrl_reg_awprot,
        S_AXI_AWVALID   => axi_ctrl_reg_awvalid,
        S_AXI_AWREADY   => axi_ctrl_reg_awready,
        S_AXI_WDATA => axi_ctrl_reg_wdata,
        S_AXI_WSTRB => axi_ctrl_reg_wstrb,
        S_AXI_WVALID    => axi_ctrl_reg_wvalid,
        S_AXI_WREADY    => axi_ctrl_reg_wready,
        S_AXI_BRESP => axi_ctrl_reg_bresp,
        S_AXI_BVALID    => axi_ctrl_reg_bvalid,
        S_AXI_BREADY    => axi_ctrl_reg_bready,
        S_AXI_ARADDR    => axi_ctrl_reg_araddr,
        -- S_AXI_ARPROT    => axi_ctrl_reg_arprot,
        S_AXI_ARVALID   => axi_ctrl_reg_arvalid,
        S_AXI_ARREADY   => axi_ctrl_reg_arready,
        S_AXI_RDATA => axi_ctrl_reg_rdata,
        S_AXI_RRESP => axi_ctrl_reg_rresp,
        S_AXI_RVALID    => axi_ctrl_reg_rvalid,
        S_AXI_RREADY    => axi_ctrl_reg_rready
    );

-- Instantiation of Axi Bus Interface AXI_HOST_MEM
action_dma_axi_master_inst : entity work.action_axi_master
    generic map (

        C_M_AXI_ID_WIDTH    => C_AXI_HOST_MEM_ID_WIDTH,
        C_M_AXI_ADDR_WIDTH  => C_AXI_HOST_MEM_ADDR_WIDTH,
        C_M_AXI_DATA_WIDTH  => C_AXI_HOST_MEM_DATA_WIDTH,
        C_M_AXI_AWUSER_WIDTH    => C_AXI_HOST_MEM_AWUSER_WIDTH,
        C_M_AXI_ARUSER_WIDTH    => C_AXI_HOST_MEM_ARUSER_WIDTH,
        C_M_AXI_WUSER_WIDTH => C_AXI_HOST_MEM_WUSER_WIDTH,
        C_M_AXI_RUSER_WIDTH => C_AXI_HOST_MEM_RUSER_WIDTH,
        C_M_AXI_BUSER_WIDTH => C_AXI_HOST_MEM_BUSER_WIDTH
    )
    port map (

        dma_rd_req_i            => dma_rd_req,
        dma_rd_addr_i           => rd_addr,
        dma_rd_len_i            => rd_len,
        dma_rd_req_ack_o        => dma_rd_req_ack,
        dma_rd_data_o           => dma_rd_data,
        dma_rd_data_last_o      => dma_rd_data_last,
        dma_rd_data_valid_o     => dma_rd_data_valid,
        dma_rd_data_taken_i     => dma_rd_data_taken,
        dma_rd_context_id       => s_Context_ID(C_AXI_HOST_MEM_ARUSER_WIDTH - 1 downto 0),

        dma_wr_req_i            => dma_wr_req,
        dma_wr_addr_i           => wr_addr,
        dma_wr_len_i            => wr_len,
        dma_wr_req_ack_o        => dma_wr_req_ack,
        dma_wr_data_i           => wr_data,
        dma_wr_data_strobe_i    => dma_wr_data_strobe,
        dma_wr_data_last_i      => dma_wr_data_last,
        dma_wr_ready_o          => dma_wr_ready,
        dma_wr_bready_i         => dma_wr_bready,
        dma_wr_done_o           => dma_wr_done,
        dma_wr_context_id       => s_Context_ID(C_AXI_HOST_MEM_AWUSER_WIDTH - 1 downto 0),


        M_AXI_ACLK  => action_clk,
        M_AXI_ARESETN   => action_rst_n,
        M_AXI_AWID  => axi_host_mem_awid,
        M_AXI_AWADDR    => axi_host_mem_awaddr,
        M_AXI_AWLEN => axi_host_mem_awlen,
        M_AXI_AWSIZE    => axi_host_mem_awsize,
        M_AXI_AWBURST   => axi_host_mem_awburst,
        M_AXI_AWLOCK    => axi_host_mem_awlock,
        M_AXI_AWCACHE   => axi_host_mem_awcache,
        M_AXI_AWPROT    => axi_host_mem_awprot,
        M_AXI_AWQOS => axi_host_mem_awqos,
        M_AXI_AWUSER    => axi_host_mem_awuser,
        M_AXI_AWVALID   => axi_host_mem_awvalid,
        M_AXI_AWREADY   => axi_host_mem_awready,
        M_AXI_WDATA => axi_host_mem_wdata,
        M_AXI_WSTRB => axi_host_mem_wstrb,
        M_AXI_WLAST => axi_host_mem_wlast,
        M_AXI_WUSER => axi_host_mem_wuser,
        M_AXI_WVALID    => axi_host_mem_wvalid,
        M_AXI_WREADY    => axi_host_mem_wready,
        M_AXI_BID   => axi_host_mem_bid,
        M_AXI_BRESP => axi_host_mem_bresp,
        M_AXI_BUSER => axi_host_mem_buser,
        M_AXI_BVALID    => axi_host_mem_bvalid,
        M_AXI_BREADY    => axi_host_mem_bready,
        M_AXI_ARID  => axi_host_mem_arid,
        M_AXI_ARADDR    => axi_host_mem_araddr,
        M_AXI_ARLEN => axi_host_mem_arlen,
        M_AXI_ARSIZE    => axi_host_mem_arsize,
        M_AXI_ARBURST   => axi_host_mem_arburst,
        M_AXI_ARLOCK    => axi_host_mem_arlock,
        M_AXI_ARCACHE   => axi_host_mem_arcache,
        M_AXI_ARPROT    => axi_host_mem_arprot,
        M_AXI_ARQOS => axi_host_mem_arqos,
        M_AXI_ARUSER    => axi_host_mem_aruser,
        M_AXI_ARVALID   => axi_host_mem_arvalid,
        M_AXI_ARREADY   => axi_host_mem_arready,
        M_AXI_RID   => axi_host_mem_rid,
        M_AXI_RDATA => axi_host_mem_rdata,
        M_AXI_RRESP => axi_host_mem_rresp,
        M_AXI_RLAST => axi_host_mem_rlast,
        M_AXI_RUSER => axi_host_mem_ruser,
        M_AXI_RVALID    => axi_host_mem_rvalid,
        M_AXI_RREADY    => axi_host_mem_rready
    );

    process(action_clk ) is
    begin
      if (rising_edge (action_clk)) then
            start_copy          <= '0';
            -- start_fill          <= '0';
            if s_transfer_type(3 downto 0) = x"4" then -- HOST to HOST
              memcopy <= true;
            else
              memcopy <= false;
            end if;
        if ( action_rst_n = '0' ) then
              fsm_app_q         <= IDLE;
              app_ready         <= '0';
              app_idle          <= '0';
            else
              app_done          <= '0';
              app_idle          <= '0';
              app_ready         <= '1';
              case fsm_app_q is
                when IDLE  =>
                  app_idle <= '1';

                  if app_start = '1' then
                    src_host  <= '0';
                    dest_host <= '0';
                    case s_transfer_type(3 downto 0) is

                       when x"4" =>
                        -- memcopy host to host memory
                        fsm_app_q  <= WAIT_FOR_MEMCOPY_DONE;
                        src_host   <= '1';
                        dest_host  <= '1';
                        start_copy <= '1';

                       when others =>
                         app_done   <= '1';

                    end case;
                  end if ;

                when WAIT_FOR_MEMCOPY_DONE =>
                  if last_write_done = '1'
                  then
                    app_done   <= '1';
                    fsm_app_q  <= IDLE;
                  end if;

                when others => null;
              end case;
        end if;
      end if;
    end process;

  rd_req_ack <= dma_rd_req_ack;
  wr_req_ack <= dma_wr_req_ack;

    -- signal active high when is the last TLAST
    -- decrease the dma block to read, active high when is 0 and lst of chunk
    eow_dma <= '1' when ((dma_rd_data_last = '1') and (unsigned(dma_blocks_to_read) = 0)) else '0';

    block_diff <= "000000" & ((s_dst_addr_h & s_dst_addr_l(31 downto 6)) - (s_src_addr_h & s_src_addr_l(31 downto 6)));

    process(action_clk ) is
     variable temp64 : std_logic_vector(63 downto 0);
    begin
      if (rising_edge (action_clk)) then
        last_write_done   <= '0';
        if ( action_rst_n = '0' ) then
              fsm_copy_q         <= IDLE;
              dma_rd_req         <= '0';
              dma_wr_req         <= '0';
              dma_wr_bready      <= '0';
              wr_gate            <= '0';
              dma_blocks_to_read <= (others => '0');
            else
              case fsm_copy_q is
                when IDLE =>
                  wr_req_count       <= 0;
                  wr_done_count      <= 0;
                  wr_gate            <= '0';
                  blocks_to_read     <= "000000"   & s_src_data_size(31 downto 6);
                  dma_blocks_to_read <= "000000"   & s_src_data_size(31 downto 6);
                  blocks_to_write    <= "000000"   & s_dst_data_size(31 downto 6);
                  blocks_expected    <= "000000"   & s_dst_data_size(31 downto 6);
                  first_max_blk_w    <= x"0000_00" & (x"40" - s_dst_addr_l(11 downto 6));
                  first_max_blk_r    <= x"0000_00" & (x"40" - s_src_addr_l(11 downto 6));

                  if first_max_blk_r < blocks_to_read then
                    first_blk_r      <= first_max_blk_r;
                  else
                    first_blk_r      <= blocks_to_read;
                  end if;

                  if first_max_blk_w < blocks_to_write then
                    first_blk_w      <= first_max_blk_w;
                  else
                    first_blk_w      <= blocks_to_write ;
                  end if;

                  rd_addr            <= s_src_addr_h & s_src_addr_l(31 downto 6) & "000000";
                  wr_addr            <= s_dst_addr_h & s_dst_addr_l(31 downto 6) & "000000";

                  rd_requests_done <= '0';
                  wr_requests_done <= '0';
                  rd_len          <= first_blk_r (7 downto 0) - '1';
                  wr_len          <= first_blk_w (7 downto 0) - '1';

                  rd_addr_adder   <= x"1000" - (s_src_addr_l(11 downto 6) & (5 downto 0 =>'0'));

                  if start_copy = '1' then
                    wr_addr_adder   <= x"1000" - (s_dst_addr_l(11 downto 6) & (5 downto 0 =>'0'));
                    blocks_to_read  <= blocks_to_read  -first_blk_r (7 downto 0) ;
                    dma_blocks_to_read <= dma_blocks_to_read  - first_blk_r (7 downto 0) ;
                    blocks_to_write <= blocks_to_write -first_blk_w (7 downto 0) ;
                    dma_rd_req    <= src_host;
                    dma_wr_req    <= dest_host;
                    dma_wr_bready <= dest_host;
                    wr_gate       <= '1';
                    fsm_copy_q    <= PROCESS_COPY;
                  end if;

                when PROCESS_COPY =>

                  if rd_req_ack = '1' and or_reduce(blocks_to_read) = '1' then
                    rd_addr          <= rd_addr + rd_addr_adder;
                    rd_addr_adder    <= x"1000";
                    dma_rd_req       <= src_host;
                    if blocks_to_read >  x"0000_0040" then
                      rd_len         <= x"3f";
                      blocks_to_read <= blocks_to_read - x"40";
                    else
                      rd_len         <= blocks_to_read(7 downto 0) - '1';
                      blocks_to_read <= (others => '0');
                    end if;
                  end if;

                  if rd_req_ack = '1' and or_reduce(blocks_to_read) = '0' then
                    dma_rd_req       <= '0';
                    rd_requests_done <= '1';
                  end if;

                  if wr_req_ack = '1' and or_reduce(blocks_to_write) = '1' then
                    wr_addr         <= wr_addr + wr_addr_adder;
                    wr_addr_adder   <= x"1000";
                    dma_wr_req      <= dest_host;
                    if blocks_to_write >  x"0000_0040" then
                      wr_len     <= x"3f";
                      blocks_to_write <= blocks_to_write - x"40";
                    else
                      wr_len         <= blocks_to_write(7 downto 0) - '1';
                      blocks_to_write <= (others => '0');
                    end if;
                  end if;

                  if wr_req_ack = '1' and or_reduce(blocks_to_write) = '0' then
                    dma_wr_req       <= '0';
                    wr_requests_done <= '1';
                  end if;

                  if rd_requests_done = '1' and wr_requests_done = '1' then
                    fsm_copy_q      <= WAIT_FOR_WRITE_DONE;
                  end if;

                 when WAIT_FOR_WRITE_DONE =>
                   if or_reduce(write_counter_dn) = '0' and wr_req_count = wr_done_count then
                     last_write_done <= '1';
                     fsm_copy_q      <= IDLE;
                     dma_wr_bready   <= '0';
                   end if;

               end case;

               if dma_rd_data_last = '1' and dma_rd_data_taken = '1' and dma_rd_data_valid = '1' then
                   if dma_blocks_to_read >  x"0000_0040" then
                       dma_blocks_to_read <= dma_blocks_to_read - x"40";
                   else
                       dma_blocks_to_read <= (others => '0');
                   end if;
               end if;
               if (dma_wr_done = '1' and dest_host = '1')
               then
                 wr_done_count <= wr_done_count + 1;
               end if;
               if (dma_wr_req = '1' and dma_wr_req_ack = '1' and dest_host = '1')
               then
                 wr_req_count <= wr_req_count + 1;
               end if;
            end if;
          end if;

        end process;
    dma_rd_data_taken <= src_host and my_sv_wrapper_rtr_o;

    my_sv_wrapper_inst : my_sv_wrapper
        generic map (
            DATA_WIDTH => C_AXI_HOST_MEM_DATA_WIDTH
        )
        port map (
            --  System signals
            clk    => action_clk,
            rst_n  => action_rst_n,

            -- SLAVE SIDE
            rtr_o  => my_sv_wrapper_rtr_o,
            rts_i  => dma_rd_data_valid,
            sow_i  => '0',
            eow_dma_i => eow_dma,
            data_i => dma_rd_data,

            -- MASTER SIDE
            rtr_i => dma_wr_ready,
            rts_o => dma_wr_data_valid,
            data_o => wr_data
    );


read_write_process:
      process(action_clk ) is
    begin
      if (rising_edge (action_clk)) then
            if start_copy = '1' or action_rst_n = '0' then
              total_write_count <=(31 downto 1 => '0') & '1';
              write_counter_up  <=(31 downto 0 => '0' ) + s_dst_addr_l(11 downto 6) + 1;
              write_counter_dn  <= blocks_to_write(25 downto 0);
              last_write_q      <= '0';
              first_write_q     <= '1';
            else
              last_write_q     <= (last_write and dma_wr_ready and dma_wr_data_valid) or last_write_q;                              -- only for DDRI_USED!=TRUE
            end if;
            if (dma_wr_data_valid = '1' and dma_wr_ready = '1' and dest_host = '1')
            then
              first_write_q <= '0';
              total_write_count <= total_write_count + '1';
              write_counter_up  <= write_counter_up  + '1';
              write_counter_dn  <= write_counter_dn  - '1';
            end if;

          end if;                       -- rising edge
        end process;

write_data_process:
  process( write_counter_up, total_write_count, last_write_q,
          dest_host, blocks_expected, wr_gate, memcopy  ) is
    begin

      if total_write_count = blocks_expected or
          or_reduce(write_counter_up(5 downto 0)) = '0'                 then
              dma_wr_data_last <= '1' and dest_host; --true for each 4096 or for last
      else
        dma_wr_data_last <= '0';
      end if;
      if total_write_count >= blocks_expected then
        last_write <= '1';
      else
        last_write <= '0';
      end if;
    end process;

wr_strobes: process(dma_wr_data_valid, memcopy )
  begin
    dma_wr_data_strobe <= (63 downto 0 => '0');
    if (dma_wr_data_valid = '1' and memcopy) then
        dma_wr_data_strobe <= (63 downto 0 => '1');
    end if;

  end process;

end action_cgemm;
