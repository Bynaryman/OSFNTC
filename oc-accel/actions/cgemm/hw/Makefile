vhd_srcs=$(wildcard *.vhd_source)
vhd_files=$(vhd_srcs:.vhd_source=.vhd)

.PHONY: all config clean vhdl

all: config

config:
	$(info Call action_config.sh (creating action IPs))
	./action_config.sh

clean:
#	$(info Also clean up action ip project...)
	@$(RM) $(ACTION_ROOT)/ip/*.log
	@$(RM) $(ACTION_ROOT)/ip/*.jou
	@$(RM) -fr $(ACTION_ROOT)/ip/action_ip_prj
	@$(RM) -fr $(ACTION_ROOT)/ip/ip_user_files
	@$(RM) $(vhd_files)

vhdl:

#
