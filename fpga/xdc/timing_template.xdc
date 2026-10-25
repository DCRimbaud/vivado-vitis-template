# SPDX-License-Identifier: MIT
# Author: Xue Weisen <xueweisen96@gmail.com>
# GitHub: https://github.com/DCRimbaud
#
# Board and timing constraint template.
#
# These lines are intentionally commented. Replace port names, clock periods,
# package pins, and I/O standards only after checking the generated BD wrapper,
# the board schematic, and the external device datasheet.
#
# Primary clock example:
# create_clock -period 10.000 -name sys_clk [get_ports sys_clock]
#
# Input delay example:
# set_input_delay -clock [get_clocks sys_clk] -max 2.000 [get_ports data_in*]
# set_input_delay -clock [get_clocks sys_clk] -min 0.500 [get_ports data_in*]
#
# Output delay example:
# set_output_delay -clock [get_clocks sys_clk] -max 2.000 [get_ports data_out*]
# set_output_delay -clock [get_clocks sys_clk] -min -0.500 [get_ports data_out*]
#
# Board pin examples:
# set_property PACKAGE_PIN <pin> [get_ports sys_clock]
# set_property IOSTANDARD <standard> [get_ports sys_clock]
#
# Asynchronous clock group example, only after CDC review:
# set_clock_groups -asynchronous -group [get_clocks -of_objects [get_pins ...]] \
#     -group [get_clocks -of_objects [get_pins ...]]
#
# Verification:
# check_timing
# report_clock_interaction -file clock_interaction.rpt
# report_cdc -file cdc.rpt
