library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity action_axi_slave is
	generic (
		-- Users to add parameters here

		-- User parameters ends
		-- Do not modify the parameters beyond this line

		-- Width of S_AXI data bus
		C_S_AXI_DATA_WIDTH	: integer	:= 32;
		-- Width of S_AXI address bus
		C_S_AXI_ADDR_WIDTH	: integer	:= 32
	);
	port (
		-- Users to add ports here
                i_Action_Type    : in  std_logic_vector(31 downto 0);
                i_Action_VER     : in  std_logic_vector(31 downto 0);
                o_Context_ID     : buffer std_logic_vector(31 downto 0);

                o_src_addr_h     : buffer std_logic_vector(31 downto 0);
                o_src_addr_l     : buffer std_logic_vector(31 downto 0);
                o_src_data_size  : buffer std_logic_vector(31 downto 0);

                o_dst_addr_h     : buffer std_logic_vector(31 downto 0);
                o_dst_addr_l     : buffer std_logic_vector(31 downto 0);
                o_dst_data_size  : buffer std_logic_vector(31 downto 0);

                o_rd_burst_num   : buffer std_logic_vector(31 downto 0);
                o_wr_burst_num   : buffer std_logic_vector(31 downto 0);
                o_transfer_type  : buffer std_logic_vector(31 downto 0);

                o_int_enable     : out std_logic;
                o_app_start      : out std_logic;
                i_app_done       : in  std_logic;
                i_app_ready      : in  std_logic;
                i_app_idle       : in  std_logic;


		-- User ports ends
		-- Do not modify the ports beyond this line

		-- Global Clock Signal
		S_AXI_ACLK	: in std_logic;
		-- Global Reset Signal. This Signal is Active LOW
		S_AXI_ARESETN	: in std_logic;
		-- Write address (issued by master, acceped by Slave)
		S_AXI_AWADDR	: in std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0);
--		-- Write channel Protection type. This signal indicates the
--    		-- privilege and security level of the transaction, and whether
--    		-- the transaction is a data access or an instruction access.
--		S_AXI_AWPROT	: in std_logic_vector(2 downto 0);
		-- Write address valid. This signal indicates that the master signaling
    		-- valid write address and control information.
		S_AXI_AWVALID	: in std_logic;
		-- Write address ready. This signal indicates that the slave is ready
    		-- to accept an address and associated control signals.
		S_AXI_AWREADY	: out std_logic;
		-- Write data (issued by master, acceped by Slave)
		S_AXI_WDATA	: in std_logic_vector(C_S_AXI_DATA_WIDTH-1 downto 0);
		-- Write strobes. This signal indicates which byte lanes hold
    		-- valid data. There is one write strobe bit for each eight
    		-- bits of the write data bus.
		S_AXI_WSTRB	: in std_logic_vector((C_S_AXI_DATA_WIDTH/8)-1 downto 0);
		-- Write valid. This signal indicates that valid write
    		-- data and strobes are available.
		S_AXI_WVALID	: in std_logic;
		-- Write ready. This signal indicates that the slave
    		-- can accept the write data.
		S_AXI_WREADY	: out std_logic;
		-- Write response. This signal indicates the status
    		-- of the write transaction.
		S_AXI_BRESP	: out std_logic_vector(1 downto 0);
		-- Write response valid. This signal indicates that the channel
    		-- is signaling a valid write response.
		S_AXI_BVALID	: out std_logic;
		-- Response ready. This signal indicates that the master
    		-- can accept a write response.
		S_AXI_BREADY	: in std_logic;
		-- Read address (issued by master, acceped by Slave)
		S_AXI_ARADDR	: in std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0);
--		-- Protection type. This signal indicates the privilege
--    		-- and security level of the transaction, and whether the
--    		-- transaction is a data access or an instruction access.
--		S_AXI_ARPROT	: in std_logic_vector(2 downto 0);
		-- Read address valid. This signal indicates that the channel
    		-- is signaling valid read address and control information.
		S_AXI_ARVALID	: in std_logic;
		-- Read address ready. This signal indicates that the slave is
    		-- ready to accept an address and associated control signals.
		S_AXI_ARREADY	: out std_logic;
		-- Read data (issued by slave)
		S_AXI_RDATA	: out std_logic_vector(C_S_AXI_DATA_WIDTH-1 downto 0);
		-- Read response. This signal indicates the status of the
    		-- read transfer.
		S_AXI_RRESP	: out std_logic_vector(1 downto 0);
		-- Read valid. This signal indicates that the channel is
    		-- signaling the required read data.
		S_AXI_RVALID	: out std_logic;
		-- Read ready. This signal indicates that the master can
    		-- accept the read data and response information.
		S_AXI_RREADY	: in std_logic
	);
end action_axi_slave;

architecture action_axi_slave of action_axi_slave is

	-- AXI4LITE signals
	signal axi_awaddr	: std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0);
	signal axi_awready	: std_logic;
	signal axi_wready	: std_logic;
	signal axi_bresp	: std_logic_vector(1 downto 0);
	signal axi_bvalid	: std_logic;
	signal axi_araddr	: std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0);
	signal axi_arready	: std_logic;
	signal axi_rdata	: std_logic_vector(C_S_AXI_DATA_WIDTH-1 downto 0);
	signal axi_rresp	: std_logic_vector(1 downto 0);
	signal axi_rvalid	: std_logic;

	-- Example-specific design signals
	-- local parameter for addressing 32 bit / 64 bit C_S_AXI_DATA_WIDTH
	-- ADDR_LSB is used for addressing 32/64 bit registers/memories
	-- ADDR_LSB = 2 for 32 bits (n downto 2)
	-- ADDR_LSB = 3 for 64 bits (n downto 3)
	constant ADDR_LSB          : integer := (C_S_AXI_DATA_WIDTH/32)+ 1;
	constant OPT_MEM_ADDR_BITS : integer := 5;
	------------------------------------------------
	---- Signals for user logic register space example
	--------------------------------------------------
	---- Number of Slave Registers 16

-- 	signal slv_reg1         : std_logic_vector(C_S_AXI_DATA_WIDTH-1 downto 0);
-- 	signal slv_reg2         : std_logic_vector(C_S_AXI_DATA_WIDTH-1 downto 0);
-- 	signal slv_reg3         : std_logic_vector(C_S_AXI_DATA_WIDTH-1 downto 0);
-- 	signal slv_reg8         : std_logic_vector(C_S_AXI_DATA_WIDTH-1 downto 0);
-- 	signal slv_reg12	: std_logic_vector(C_S_AXI_DATA_WIDTH-1 downto 0);
-- 	signal slv_reg13	: std_logic_vector(C_S_AXI_DATA_WIDTH-1 downto 0);
-- 	signal slv_reg14	: std_logic_vector(C_S_AXI_DATA_WIDTH-1 downto 0);
-- 	signal slv_reg15	: std_logic_vector(C_S_AXI_DATA_WIDTH-1 downto 0);
-- 	signal slv_reg16	: std_logic_vector(C_S_AXI_DATA_WIDTH-1 downto 0);
-- 	signal slv_reg17	: std_logic_vector(C_S_AXI_DATA_WIDTH-1 downto 0);
-- 	signal slv_reg18	: std_logic_vector(C_S_AXI_DATA_WIDTH-1 downto 0);
-- 	signal slv_reg19	: std_logic_vector(C_S_AXI_DATA_WIDTH-1 downto 0);
	signal slv_reg_rden	: std_logic;
	signal slv_reg_wren	: std_logic;
-- 	signal reg_data_out	: std_logic_vector(C_S_AXI_DATA_WIDTH-1 downto 0);
	signal byte_index	: integer;

   signal idle_q           : std_logic;
   signal app_start_q      : std_logic;
   signal app_done_q       : std_logic;
   signal s_action_ctrl_W_bit0_q  : std_logic;


   -- Write Addresses
   constant C_Src_Addr_L_WR_Addr    : std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0) := X"00_00_01_10";
   constant C_Src_Addr_H_WR_Addr    : std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0) := X"00_00_01_14";
   constant C_Src_Data_Size_WR_Addr : std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0) := X"00_00_01_18";
   constant C_Src_Flag_Type_WR_Addr : std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0) := X"00_00_01_1C";

   constant C_Dst_Addr_L_WR_Addr    : std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0) := X"00_00_01_20";
   constant C_Dst_Addr_H_WR_Addr    : std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0) := X"00_00_01_24";
   constant C_Dst_Data_Size_WR_Addr : std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0) := X"00_00_01_28";
   constant C_Dst_Flag_Type_WR_Addr : std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0) := X"00_00_01_2C";

   constant C_RD_BURST_NUM_WR_Addr  : std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0) := X"00_00_01_30";
   constant C_WR_BURST_NUM_WR_Addr  : std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0) := X"00_00_01_34";
   constant C_TRANSFER_TYPE_WR_Addr : std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0) := X"00_00_01_38";

   constant C_CTRL_RETC_WR_Addr     : std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0) := X"00_00_01_04";

   -- Read Addresses
   constant C_Src_Addr_L_RD_Addr    : std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0) := X"00_00_01_90";
   constant C_Src_Addr_H_RD_Addr    : std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0) := X"00_00_01_94";
   constant C_Src_Data_Size_RD_Addr : std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0) := X"00_00_01_98";
   constant C_Src_Flag_Type_RD_Addr : std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0) := X"00_00_01_9C";

   constant C_Dst_Addr_L_RD_Addr    : std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0) := X"00_00_01_A0";
   constant C_Dst_Addr_H_RD_Addr    : std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0) := X"00_00_01_A4";
   constant C_Dst_Data_Size_RD_Addr : std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0) := X"00_00_01_A8";
   constant C_Dst_Flag_Type_RD_Addr : std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0) := X"00_00_01_AC";

   constant C_RD_BURST_NUM_RD_Addr  : std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0) := X"00_00_01_B0";
   constant C_WR_BURST_NUM_RD_Addr  : std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0) := X"00_00_01_B4";
   constant C_TRANSFER_TYPE_RD_Addr : std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0) := X"00_00_01_B8";

   constant C_CTRL_RETC_RD_Addr     : std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0) := X"00_00_01_84";

   --Control MMIO
   constant C_Action_Control_Addr   : std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0) := X"00_00_00_00";
   constant C_IRQ_Enable_Addr       : std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0) := X"00_00_00_04";
   constant C_Action_Type_Addr      : std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0) := X"00_00_00_10";
   constant C_Action_VER_Addr       : std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0) := X"00_00_00_14";
   constant C_CONTEXT_ID_Addr       : std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0) := X"00_00_00_20";

   signal s_action_ctrl_W           : std_logic_vector(C_S_AXI_DATA_WIDTH-1 downto 0);
   signal s_action_ctrl_R           : std_logic_vector(C_S_AXI_DATA_WIDTH-1 downto 0);
   signal s_IRQ_EN                  : std_logic_vector(C_S_AXI_DATA_WIDTH-1 downto 0);
   signal s_flag_type_src           : std_logic_vector(C_S_AXI_DATA_WIDTH-1 downto 0);
   signal s_flag_type_dst           : std_logic_vector(C_S_AXI_DATA_WIDTH-1 downto 0);
   signal s_retc                    : std_logic_vector(C_S_AXI_DATA_WIDTH-1 downto 0);





begin
	-- I/O Connections assignments

    o_int_enable    <= s_IRQ_EN(0);
	S_AXI_AWREADY	<= axi_awready;
	S_AXI_WREADY	<= axi_wready;
	S_AXI_BRESP	<= axi_bresp;
	S_AXI_BVALID	<= axi_bvalid;
	S_AXI_ARREADY	<= axi_arready;
	S_AXI_RDATA	<= axi_rdata;
	S_AXI_RRESP	<= axi_rresp;
	S_AXI_RVALID	<= axi_rvalid;
	-- Implement axi_awready generation
	-- axi_awready is asserted for one S_AXI_ACLK clock cycle when both
	-- S_AXI_AWVALID and S_AXI_WVALID are asserted. axi_awready is
	-- de-asserted when reset is low.

	process (S_AXI_ACLK)
	begin
	  if rising_edge(S_AXI_ACLK) then
	    if S_AXI_ARESETN = '0' then
	      axi_awready <= '0';
	    else
	      if (axi_awready = '0' and S_AXI_AWVALID = '1' and S_AXI_WVALID = '1') then
	        -- slave is ready to accept write address when
	        -- there is a valid write address and write data
	        -- on the write address and data bus. This design
	        -- expects no outstanding transactions.
	        axi_awready <= '1';
	      else
	        axi_awready <= '0';
	      end if;
	    end if;
	  end if;
	end process;

	-- Implement axi_awaddr latching
	-- This process is used to latch the address when both
	-- S_AXI_AWVALID and S_AXI_WVALID are valid.

	process (S_AXI_ACLK)
	begin
	  if rising_edge(S_AXI_ACLK) then
	    if S_AXI_ARESETN = '0' then
	      axi_awaddr <= (others => '0');
	    else
	      if (axi_awready = '0' and S_AXI_AWVALID = '1' and S_AXI_WVALID = '1') then
	        -- Write Address latching
	        axi_awaddr <= S_AXI_AWADDR;
	      end if;
	    end if;
	  end if;
	end process;

	-- Implement axi_wready generation
	-- axi_wready is asserted for one S_AXI_ACLK clock cycle when both
	-- S_AXI_AWVALID and S_AXI_WVALID are asserted. axi_wready is
	-- de-asserted when reset is low.

	process (S_AXI_ACLK)
	begin
	  if rising_edge(S_AXI_ACLK) then
	    if S_AXI_ARESETN = '0' then
	      axi_wready <= '0';
	    else
	      if (axi_wready = '0' and S_AXI_WVALID = '1' and S_AXI_AWVALID = '1') then
	          -- slave is ready to accept write data when
	          -- there is a valid write address and write data
	          -- on the write address and data bus. This design
	          -- expects no outstanding transactions.
	          axi_wready <= '1';
	      else
	        axi_wready <= '0';
	      end if;
	    end if;
	  end if;
	end process;

	-- Implement memory mapped register select and write logic generation
	-- The write data is accepted and written to memory mapped registers when
	-- axi_awready, S_AXI_WVALID, axi_wready and S_AXI_WVALID are asserted. Write strobes are used to
	-- select byte enables of slave registers while writing.
	-- These registers are cleared when reset (active low) is applied.
	-- Slave register write enable is asserted when valid address and data are available
	-- and the slave is ready to accept the write address and write data.
	slv_reg_wren <= axi_wready and S_AXI_WVALID and axi_awready and S_AXI_AWVALID ;

	process (S_AXI_ACLK)
	variable loc_addr :std_logic_vector(OPT_MEM_ADDR_BITS-1 downto 0);
	begin
	  if rising_edge(S_AXI_ACLK) then
	    if S_AXI_ARESETN = '0' then
	      s_action_ctrl_W  <= (others => '0');
	      s_IRQ_EN         <= (others => '0');
	      o_Context_ID     <= (others => '0');
	      o_src_addr_h     <= (others => '0');
	      o_src_addr_l     <= (others => '0');
	      o_src_data_size  <= (others => '0');
	      s_flag_type_src  <= (others => '0');
	      o_dst_addr_l     <= (others => '0');
	      o_dst_addr_h     <= (others => '0');
	      o_dst_data_size  <= (others => '0');
	      s_flag_type_dst  <= (others => '0');
	      s_retc           <= (others => '0');
	      o_rd_burst_num   <= (others => '0');
	      o_wr_burst_num   <= (others => '0');
	      o_transfer_type  <= (others => '0');


	    else
	      --loc_addr := axi_awaddr(ADDR_LSB + OPT_MEM_ADDR_BITS-1 downto ADDR_LSB);
	      if (slv_reg_wren = '1') then
	        case axi_awaddr is
	          when C_Action_Control_Addr =>
	            for byte_index in 0 to (C_S_AXI_DATA_WIDTH/8-1) loop
	              if ( S_AXI_WSTRB(byte_index) = '1' ) then
	                -- Respective byte enables are asserted as per write strobes
	                -- slave registor 0
	                s_action_ctrl_W(byte_index*8+7 downto byte_index*8) <= S_AXI_WDATA(byte_index*8+7 downto byte_index*8);
	              end if;
	            end loop;
	          when C_IRQ_Enable_Addr =>
	            for byte_index in 0 to (C_S_AXI_DATA_WIDTH/8-1) loop
	              if ( S_AXI_WSTRB(byte_index) = '1' ) then
	                -- Respective byte enables are asserted as per write strobes
	                -- slave registor 1
	                s_IRQ_EN(byte_index*8+7 downto byte_index*8) <= S_AXI_WDATA(byte_index*8+7 downto byte_index*8);
	              end if;
	            end loop;
	          when C_CONTEXT_ID_Addr =>
	            for byte_index in 0 to (C_S_AXI_DATA_WIDTH/8-1) loop
	              if ( S_AXI_WSTRB(byte_index) = '1' ) then
	                -- Respective byte enables are asserted as per write strobes
	                -- slave registor 2
	                o_Context_ID(byte_index*8+7 downto byte_index*8) <= S_AXI_WDATA(byte_index*8+7 downto byte_index*8);
	              end if;
	            end loop;
	          when C_Src_Addr_L_WR_Addr =>
	            for byte_index in 0 to (C_S_AXI_DATA_WIDTH/8-1) loop
	              if ( S_AXI_WSTRB(byte_index) = '1' ) then
	                -- Respective byte enables are asserted as per write strobes
	                -- slave registor 3
	                o_src_addr_l(byte_index*8+7 downto byte_index*8) <= S_AXI_WDATA(byte_index*8+7 downto byte_index*8);
	              end if;
	            end loop;
	          when C_Src_Addr_H_WR_Addr =>
	            for byte_index in 0 to (C_S_AXI_DATA_WIDTH/8-1) loop
	              if ( S_AXI_WSTRB(byte_index) = '1' ) then
	                -- Respective byte enables are asserted as per write strobes
	                -- slave registor 8
	                o_src_addr_h(byte_index*8+7 downto byte_index*8) <= S_AXI_WDATA(byte_index*8+7 downto byte_index*8);
	              end if;
	            end loop;
	          when C_Src_Data_Size_WR_Addr =>
	            for byte_index in 0 to (C_S_AXI_DATA_WIDTH/8-1) loop
	              if ( S_AXI_WSTRB(byte_index) = '1' ) then
	                -- Respective byte enables are asserted as per write strobes
	                -- slave registor 12
	                o_src_data_size(byte_index*8+7 downto byte_index*8) <= S_AXI_WDATA(byte_index*8+7 downto byte_index*8);
	              end if;
	            end loop;
	          when C_Src_Flag_Type_WR_Addr =>
	            for byte_index in 0 to (C_S_AXI_DATA_WIDTH/8-1) loop
	              if ( S_AXI_WSTRB(byte_index) = '1' ) then
	                -- Respective byte enables are asserted as per write strobes
	                -- slave registor 13
	                s_flag_type_src(byte_index*8+7 downto byte_index*8) <= S_AXI_WDATA(byte_index*8+7 downto byte_index*8);
	              end if;
	            end loop;
	          when C_Dst_Addr_L_WR_Addr =>
	            for byte_index in 0 to (C_S_AXI_DATA_WIDTH/8-1) loop
	              if ( S_AXI_WSTRB(byte_index) = '1' ) then
	                -- Respective byte enables are asserted as per write strobes
	                -- slave registor 14
	                o_dst_addr_l(byte_index*8+7 downto byte_index*8) <= S_AXI_WDATA(byte_index*8+7 downto byte_index*8);
	              end if;
	            end loop;
	          when C_Dst_Addr_H_WR_Addr =>
	            for byte_index in 0 to (C_S_AXI_DATA_WIDTH/8-1) loop
	              if ( S_AXI_WSTRB(byte_index) = '1' ) then
	                -- Respective byte enables are asserted as per write strobes
	                -- slave registor 15
	                o_dst_addr_h(byte_index*8+7 downto byte_index*8) <= S_AXI_WDATA(byte_index*8+7 downto byte_index*8);
	              end if;
	            end loop;
	          when C_Dst_Data_Size_WR_Addr =>
	            for byte_index in 0 to (C_S_AXI_DATA_WIDTH/8-1) loop
	              if ( S_AXI_WSTRB(byte_index) = '1' ) then
	                -- Respective byte enables are asserted as per write strobes
	                -- slave registor 16
	                o_dst_data_size(byte_index*8+7 downto byte_index*8) <= S_AXI_WDATA(byte_index*8+7 downto byte_index*8);
	              end if;
	            end loop;
	          when C_Dst_Flag_Type_WR_Addr =>
	            for byte_index in 0 to (C_S_AXI_DATA_WIDTH/8-1) loop
	              if ( S_AXI_WSTRB(byte_index) = '1' ) then
	                -- Respective byte enables are asserted as per write strobes
	                -- slave registor 17
	                s_flag_type_dst(byte_index*8+7 downto byte_index*8) <= S_AXI_WDATA(byte_index*8+7 downto byte_index*8);
	              end if;
	            end loop;
	          when C_CTRL_RETC_WR_Addr =>
	            for byte_index in 0 to (C_S_AXI_DATA_WIDTH/8-1) loop
	              if ( S_AXI_WSTRB(byte_index) = '1' ) then
	                -- Respective byte enables are asserted as per write strobes
	                -- slave registor 18
	                s_retc(byte_index*8+7 downto byte_index*8) <= S_AXI_WDATA(byte_index*8+7 downto byte_index*8);
	              end if;
	            end loop;
	          when C_RD_BURST_NUM_WR_Addr =>
	            for byte_index in 0 to (C_S_AXI_DATA_WIDTH/8-1) loop
	              if ( S_AXI_WSTRB(byte_index) = '1' ) then
	                -- Respective byte enables are asserted as per write strobes
	                -- slave registor 18
	                o_rd_burst_num(byte_index*8+7 downto byte_index*8) <= S_AXI_WDATA(byte_index*8+7 downto byte_index*8);
	              end if;
	            end loop;
	          when C_WR_BURST_NUM_WR_Addr =>
	            for byte_index in 0 to (C_S_AXI_DATA_WIDTH/8-1) loop
	              if ( S_AXI_WSTRB(byte_index) = '1' ) then
	                -- Respective byte enables are asserted as per write strobes
	                -- slave registor 18
	                o_wr_burst_num(byte_index*8+7 downto byte_index*8) <= S_AXI_WDATA(byte_index*8+7 downto byte_index*8);
	              end if;
	            end loop;
	          when C_TRANSFER_TYPE_WR_Addr =>
	            for byte_index in 0 to (C_S_AXI_DATA_WIDTH/8-1) loop
	              if ( S_AXI_WSTRB(byte_index) = '1' ) then
	                -- Respective byte enables are asserted as per write strobes
	                -- slave registor 18
	                o_transfer_type(byte_index*8+7 downto byte_index*8) <= S_AXI_WDATA(byte_index*8+7 downto byte_index*8);
	              end if;
	            end loop;

	          when others =>
	            s_action_ctrl_W  <= s_action_ctrl_W;
	            s_IRQ_EN         <= s_IRQ_EN       ;
	            o_Context_ID     <= o_Context_ID   ;
	            o_src_addr_h     <= o_src_addr_h   ;
	            o_src_addr_l     <= o_src_addr_l   ;
	            o_src_data_size  <= o_src_data_size;
	            s_flag_type_src  <= s_flag_type_src;
	            o_dst_addr_l     <= o_dst_addr_l   ;
	            o_dst_addr_h     <= o_dst_addr_h   ;
	            o_dst_data_size  <= o_dst_data_size;
	            s_flag_type_dst  <= s_flag_type_dst;
	            s_retc           <= s_retc         ;
	            o_rd_burst_num   <= o_rd_burst_num ;
	            o_wr_burst_num   <= o_wr_burst_num ;
	            o_transfer_type  <= o_transfer_type;

	        end case;
	      end if;
              if app_start_q = '1' then
                s_action_ctrl_W(0) <= '0';
              end if;
	    end if;
	  end if;
	end process;

	-- Implement write response logic generation
	-- The write response and response valid signals are asserted by the slave
	-- when axi_wready, S_AXI_WVALID, axi_wready and S_AXI_WVALID are asserted.
	-- This marks the acceptance of address and indicates the status of
	-- write transaction.

	process (S_AXI_ACLK)
	begin
	  if rising_edge(S_AXI_ACLK) then
	    if S_AXI_ARESETN = '0' then
	      axi_bvalid  <= '0';
	      axi_bresp   <= "00"; --need to work more on the responses
	    else
	      if (axi_awready = '1' and S_AXI_AWVALID = '1' and axi_wready = '1' and S_AXI_WVALID = '1' and axi_bvalid = '0'  ) then
	        axi_bvalid <= '1';
	        axi_bresp  <= "00";
	      elsif (S_AXI_BREADY = '1' and axi_bvalid = '1') then   --check if bready is asserted while bvalid is high)
	        axi_bvalid <= '0';                                 -- (there is a possibility that bready is always asserted high)
	      end if;
	    end if;
	  end if;
	end process;

	-- Implement axi_arready generation
	-- axi_arready is asserted for one S_AXI_ACLK clock cycle when
	-- S_AXI_ARVALID is asserted. axi_awready is
	-- de-asserted when reset (active low) is asserted.
	-- The read address is also latched when S_AXI_ARVALID is
	-- asserted. axi_araddr is reset to zero on reset assertion.

	process (S_AXI_ACLK)
	begin
	  if rising_edge(S_AXI_ACLK) then
	    if S_AXI_ARESETN = '0' then
	      axi_arready <= '0';
	      axi_araddr  <= (others => '1');
	    else
	      if (axi_arready = '0' and S_AXI_ARVALID = '1') then
	        -- indicates that the slave has acceped the valid read address
	        axi_arready <= '1';
	        -- Read Address latching
	        axi_araddr  <= S_AXI_ARADDR;
	      else
	        axi_arready <= '0';
	      end if;
	    end if;
	  end if;
	end process;

	-- Implement axi_arvalid generation
	-- axi_rvalid is asserted for one S_AXI_ACLK clock cycle when both
	-- S_AXI_ARVALID and axi_arready are asserted. The slave registers
	-- data are available on the axi_rdata bus at this instance. The
	-- assertion of axi_rvalid marks the validity of read data on the
	-- bus and axi_rresp indicates the status of read transaction.axi_rvalid
	-- is deasserted on reset (active low). axi_rresp and axi_rdata are
	-- cleared to zero on reset (active low).
	process (S_AXI_ACLK)
	begin
	  if rising_edge(S_AXI_ACLK) then
	    if S_AXI_ARESETN = '0' then
	      axi_rvalid <= '0';
	      axi_rresp  <= "00";
	    else
	      if (axi_arready = '1' and S_AXI_ARVALID = '1' and axi_rvalid = '0') then
	        -- Valid read data is available at the read data bus
	        axi_rvalid <= '1';
	        axi_rresp  <= "00"; -- 'OKAY' response
	      elsif (axi_rvalid = '1' and S_AXI_RREADY = '1') then
	        -- Read data is accepted by the master
	        axi_rvalid <= '0';
	      end if;
	    end if;
	  end if;
	end process;

	-- Implement memory mapped register select and read logic generation
	-- Slave register read enable is asserted when valid address is available
	-- and the slave is ready to accept the read address.
	slv_reg_rden <= axi_arready and S_AXI_ARVALID and (not axi_rvalid) ;

-- 	process (s_action_ctrl_R, slv_reg1, slv_reg2, slv_reg3, reg_0x10_i, reg_0x14_i, slv_reg8, slv_reg12, slv_reg13, slv_reg14, slv_reg15, slv_reg16, slv_reg17, slv_reg18, slv_reg19, axi_araddr)
-- 	variable loc_addr :std_logic_vector(OPT_MEM_ADDR_BITS-1 downto 0);
-- 	begin
-- 	    -- Address decoding for reading registers
-- 	    loc_addr := axi_araddr(ADDR_LSB + OPT_MEM_ADDR_BITS-1 downto ADDR_LSB);
-- 	    case loc_addr is
-- 	      when b"00000" =>
-- 	        reg_data_out <= s_action_ctrl_R;  -- 0x00
-- 	      when b"00001" =>
-- 	        reg_data_out <= X"00_00_01_02";--slv_reg1;      -- 0x04
-- 	      when b"00010" =>
-- 	        reg_data_out <= slv_reg2;      -- 0x08
-- 	      when b"00011" =>
-- 	        reg_data_out <= slv_reg3;      -- 0x0c
-- 	      when b"00100" =>
-- 	        reg_data_out <= reg_0x10_i;    -- 0x10
-- 	      when b"00101" =>
-- 	        reg_data_out <= reg_0x14_i;    -- 0x14
-- 	      when b"01000" =>
-- 	        reg_data_out <= slv_reg8;      -- 0x20
-- 	      when b"01100" =>
-- 	        reg_data_out <= slv_reg12;     -- 0x30
-- 	      when b"01101" =>
-- 	        reg_data_out <= slv_reg13;     -- 0x34
-- 	      when b"01110" =>
-- 	        reg_data_out <= slv_reg14;     -- 0x38
-- 	      when b"01111" =>
-- 	        reg_data_out <= slv_reg15;     -- 0x3c
-- 	      when b"10000" =>
-- 	        reg_data_out <= slv_reg16;     -- 0x40
-- 	      when b"10001" =>
-- 	        reg_data_out <= slv_reg17;     -- 0x44
-- 	      when b"10010" =>
-- 	        reg_data_out <= slv_reg18;     -- 0x48
-- 	      when b"10011" =>
-- 	        reg_data_out <= slv_reg19;     -- 0x4c
-- 	      when others =>
-- 	        reg_data_out  <= X"00_00_01_02";--(others => '0'); X"184" REC
-- 	    end case;
-- 	end process;

	-- Output register or memory read data
	process( S_AXI_ACLK ) is
	begin
	  if (rising_edge (S_AXI_ACLK)) then
	    if ( S_AXI_ARESETN = '0' ) then
	      axi_rdata  <= (others => '0');
	    else
	      if (slv_reg_rden = '1') then
            case axi_araddr is
               when C_Action_Control_Addr =>
                  axi_rdata <= s_action_ctrl_R;          -- 0x00
               when C_IRQ_Enable_Addr =>
                  axi_rdata <= s_IRQ_EN;                 -- 0x04
               when C_Action_Type_Addr =>
                  axi_rdata <= i_Action_Type;            -- 0x10
               when C_Action_VER_Addr =>
                  axi_rdata <= i_Action_VER;             -- 0x14
               when C_CONTEXT_ID_Addr =>
                  axi_rdata <= o_Context_ID;             -- 0x20
               when C_Src_Addr_L_RD_Addr =>
                  axi_rdata <= o_src_addr_l;             -- 0x190
               when C_Src_Addr_H_RD_Addr =>
                  axi_rdata <= o_src_addr_h;             -- 0x194
               when C_Src_Data_Size_RD_Addr =>
                  axi_rdata <= o_src_data_size;          -- 0x198
               when C_Src_Flag_Type_RD_Addr =>
                  axi_rdata <= s_flag_type_src;          -- 0x19c
               when C_Dst_Addr_L_RD_Addr =>
                  axi_rdata <= o_dst_addr_l;             -- 0x1a0
               when C_Dst_Addr_H_RD_Addr =>
                  axi_rdata <= o_dst_addr_h;             -- 0x1a4
               when C_Dst_Data_Size_RD_Addr =>
                  axi_rdata <= o_dst_data_size;          -- 0x1a8
               when C_Dst_Flag_Type_RD_Addr =>
                  axi_rdata <= s_flag_type_dst;          -- 0x1ac
               when C_RD_BURST_NUM_RD_Addr  =>
                  axi_rdata <= o_rd_burst_num;
               when C_WR_BURST_NUM_RD_Addr  =>
                  axi_rdata <= o_wr_burst_num;
               when C_TRANSFER_TYPE_RD_Addr =>
                  axi_rdata <= o_transfer_type;
               when C_CTRL_RETC_RD_Addr =>
                  axi_rdata <= X"00_00_01_02";           -- 0x184
               when others =>
                  axi_rdata  <= (others => '0');
            end case;
	      end if;
	    end if;
	  end if;
	end process;



	-- Add user logic here
        -- Reiner

        o_app_start     <= app_start_q;

        process( S_AXI_ACLK ) is
          variable app_done_i_q    : std_logic;

          --variable loc_addr        :std_logic_vector(OPT_MEM_ADDR_BITS-1 downto 0);
	begin
	  if (rising_edge (S_AXI_ACLK)) then
	    if ( S_AXI_ARESETN = '0' ) then
	      app_start_q     <=    '0';
	      app_done_q      <=    '0';
              app_done_i_q    :=    '0';
              s_action_ctrl_W_bit0_q <=    '0';
              idle_q          <=    '0';
     	    else
              idle_q          <= i_app_idle;
              s_action_ctrl_W_bit0_q <= s_action_ctrl_W(0);
              app_done_i_q    := i_app_done;
              --loc_addr        := axi_awaddr(ADDR_LSB + OPT_MEM_ADDR_BITS-1 downto ADDR_LSB);
              -- clear ap_done bit when register is read
              if slv_reg_rden = '1' and axi_araddr = C_Action_Control_Addr  then
                app_done_q     <= '0';
              end if;
	      if (app_done_i_q = '0' and i_app_done = '1') then
                app_done_q     <= '1';
	      end if;
              if s_action_ctrl_W_bit0_q = '0' and s_action_ctrl_W(0) = '1' then
                app_start_q <= '1';
              end if;
              if idle_q = '1' and i_app_idle = '0' then
                app_start_q <= '0';
              end if;

	    end if;
	  end if;
	end process;
        s_action_ctrl_R <= s_action_ctrl_W (31 downto 4) & i_app_ready & idle_q & app_done_q & app_start_q ;


	-- User logic ends

end action_axi_slave;
