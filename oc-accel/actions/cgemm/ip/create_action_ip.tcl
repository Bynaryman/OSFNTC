set action_root [lindex $argv 0]
set fpga_part   [lindex $argv 1]

set aip_dir  $action_root/ip
set log_dir  $action_root/../../hardware/logs
set log_file $log_dir/create_action_ip.log
set src_dir  $aip_dir/action_ip_prj/action_ip_prj.srcs/sources_1/ip

## Create a new Vivado IP Project
puts "\[CREATE_ACTION_IPs..........\] start [clock format [clock seconds] -format {%T %a %b %d/ %Y}]"

puts "                        FPGACHIP = $fpga_part"
puts "                        ACTION_ROOT = $action_root"
puts "                        Creating IP in $src_dir"
puts "                        NO VIVADO XILINX IP TO CREATE FOR THIS PROJECT"

puts "\[CREATE_ACTION_IPs..........\] done  [clock format [clock seconds] -format {%T %a %b %d %Y}]"
