# Generates Transceive IP with buffer-bypass at 25.78125 Gbps Using lane 4 as the master lane
create_ip -name gtwizard_ultrascale -vendor xilinx.com -library ip -version 1.7 -module_name DLx_phy
set_property -dict [list CONFIG.CHANNEL_ENABLE {X0Y23 X0Y22 X0Y21 X0Y20 X0Y19 X0Y18 X0Y17 X0Y16} \
                         CONFIG.TX_MASTER_CHANNEL {X0Y20} \
                         CONFIG.RX_MASTER_CHANNEL {X0Y20} \
                         CONFIG.TX_LINE_RATE {25.78125} \
                         CONFIG.TX_REFCLK_FREQUENCY {156.25} \
                         CONFIG.TX_DATA_ENCODING {64B66B} \
                         CONFIG.TX_USER_DATA_WIDTH {64} \
                         CONFIG.TX_INT_DATA_WIDTH {64} \
                         CONFIG.TX_BUFFER_MODE {0} \
                         CONFIG.TX_QPLL_FRACN_NUMERATOR {8388608} \
                         CONFIG.TX_OUTCLK_SOURCE {TXPROGDIVCLK} \
                         CONFIG.RX_LINE_RATE {25.78125} \
                         CONFIG.RX_REFCLK_FREQUENCY {156.25} \
                         CONFIG.RX_DATA_DECODING {64B66B} \
                         CONFIG.RX_USER_DATA_WIDTH {64} \
                         CONFIG.RX_INT_DATA_WIDTH {64} \
                         CONFIG.RX_BUFFER_MODE {0} \
                         CONFIG.RX_QPLL_FRACN_NUMERATOR {8388608} \
                         CONFIG.RX_JTOL_FC {10} \
                         CONFIG.RX_CB_MAX_LEVEL {4} \
                         CONFIG.RX_REFCLK_SOURCE {X0Y23 clk0-1 X0Y22 clk0-1 X0Y21 clk0-1 X0Y20 clk0-1} \
                         CONFIG.TX_REFCLK_SOURCE {X0Y23 clk0-1 X0Y22 clk0-1 X0Y21 clk0-1 X0Y20 clk0-1} \
                         CONFIG.LOCATE_RESET_CONTROLLER {EXAMPLE_DESIGN} \
                         CONFIG.LOCATE_TX_BUFFER_BYPASS_CONTROLLER {EXAMPLE_DESIGN} \
                         CONFIG.LOCATE_RX_BUFFER_BYPASS_CONTROLLER {EXAMPLE_DESIGN} \
                         CONFIG.LOCATE_IN_SYSTEM_IBERT_CORE {EXAMPLE_DESIGN} \
                         CONFIG.TXPROGDIV_FREQ_VAL {402.8320312} \
                         CONFIG.FREERUN_FREQUENCY {150} \
                         CONFIG.Component_Name {DLx_phy}] [get_ips DLx_phy]
generate_target {all} [get_ips DLx_phy]

