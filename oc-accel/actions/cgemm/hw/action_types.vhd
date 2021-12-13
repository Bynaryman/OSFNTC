LIBRARY ieee;
USE ieee.std_logic_1164.all;
USE ieee.std_logic_misc.all;
USE ieee.std_logic_unsigned.all;
USE ieee.numeric_std.all;

PACKAGE action_types IS
CONSTANT INT_BITS                : integer :=  64;  -- number of bits required to represent the seven interrupt
CONSTANT CONTEXT_BITS            : integer :=  9;   -- number of bits required to represent the supported contexts as integer

CONSTANT    C_M_AXI_CARD_MEM0_ID_WIDTH       : integer   := 4;
CONSTANT    C_M_AXI_CARD_MEM0_ADDR_WIDTH     : integer   := 33;
CONSTANT    C_M_AXI_CARD_MEM0_DATA_WIDTH     : integer   := 512;
CONSTANT    C_M_AXI_CARD_MEM0_AWUSER_WIDTH   : integer   := 1;
CONSTANT    C_M_AXI_CARD_MEM0_ARUSER_WIDTH   : integer   := 1;
CONSTANT    C_M_AXI_CARD_MEM0_WUSER_WIDTH    : integer   := 1;
CONSTANT    C_M_AXI_CARD_MEM0_RUSER_WIDTH    : integer   := 1;
CONSTANT    C_M_AXI_CARD_MEM0_BUSER_WIDTH    : integer   := 1;

-- Parameters for Axi Slave Bus Interface AXI_CTRL_REG
CONSTANT    C_S_AXI_CTRL_REG_DATA_WIDTH      : integer   := 32;
CONSTANT    C_S_AXI_CTRL_REG_ADDR_WIDTH      : integer   := 32;

-- Parameters for Axi Master Bus Interface AXI_HOST_MEM : to Host memory

-- CAPI2
-- CONSTANT    C_M_AXI_HOST_MEM_ID_WIDTH        : integer   := 5;
-- CONSTANT    C_M_AXI_HOST_MEM_ADDR_WIDTH      : integer   := 64;
-- CONSTANT    C_M_AXI_HOST_MEM_DATA_WIDTH      : integer   := 512;
-- CONSTANT    C_M_AXI_HOST_MEM_AWUSER_WIDTH    : integer   := 9;
-- CONSTANT    C_M_AXI_HOST_MEM_ARUSER_WIDTH    : integer   := 9;
-- CONSTANT    C_M_AXI_HOST_MEM_WUSER_WIDTH     : integer   := 1;
-- CONSTANT    C_M_AXI_HOST_MEM_RUSER_WIDTH     : integer   := 1;
-- CONSTANT    C_M_AXI_HOST_MEM_BUSER_WIDTH     : integer   := 1;
-- CAPI3
CONSTANT    C_M_AXI_HOST_MEM_ID_WIDTH        : integer   := 5;
CONSTANT    C_M_AXI_HOST_MEM_ADDR_WIDTH      : integer   := 64;
CONSTANT    C_M_AXI_HOST_MEM_DATA_WIDTH      : integer   := 1024;
CONSTANT    C_M_AXI_HOST_MEM_AWUSER_WIDTH    : integer   := 9;
CONSTANT    C_M_AXI_HOST_MEM_ARUSER_WIDTH    : integer   := 9;
CONSTANT    C_M_AXI_HOST_MEM_WUSER_WIDTH     : integer   := 9;
CONSTANT    C_M_AXI_HOST_MEM_RUSER_WIDTH     : integer   := 9;
CONSTANT    C_M_AXI_HOST_MEM_BUSER_WIDTH     : integer   := 9;

-- Parameters for Axi Master Bus Interface AXI_NVME : to NVMe controller
CONSTANT    C_M_AXI_NVME_ID_WIDTH            : integer  :=  1;
CONSTANT    C_M_AXI_NVME_ADDR_WIDTH          : integer  :=  32;
CONSTANT    C_M_AXI_NVME_DATA_WIDTH          : integer  :=  32;
CONSTANT    C_M_AXI_NVME_AWUSER_WIDTH        : integer  :=  1;
CONSTANT    C_M_AXI_NVME_ARUSER_WIDTH        : integer  :=  1;
CONSTANT    C_M_AXI_NVME_WUSER_WIDTH         : integer  :=  1;
CONSTANT    C_M_AXI_NVME_RUSER_WIDTH         : integer  :=  1;
CONSTANT    C_M_AXI_NVME_BUSER_WIDTH         : integer  :=  1;

END action_types;


PACKAGE BODY action_types IS


END action_types;
