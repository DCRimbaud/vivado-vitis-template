# SPDX-License-Identifier: MIT
# Author: Xue Weisen <xueweisen96@gmail.com>
# GitHub: https://github.com/DCRimbaud
#
# Create a minimal MicroBlaze Block Design for the Vitis platform template.
#
# Run from the Vivado 2025.2 Tcl console or:
#   vivado -mode batch -source fpga/tcl/create_microblaze_bd.tcl
#
# Optional environment variables:
#   TEMPLATE_PROJECT_DIR   default: build/vivado-microblaze
#   TEMPLATE_PROJECT_NAME  default: microblaze_platform
#   TEMPLATE_PART          required unless the default part is acceptable
#   TEMPLATE_BOARD_PART    optional board part name
#   TEMPLATE_BD_NAME       default: system
#
# This script creates and validates the BD and wrapper. It deliberately does
# not launch synthesis, implementation, or write a formal bitstream. Use
# Vivado Project Mode Design Runs for the delivery build.

set project_dir [expr {[info exists ::env(TEMPLATE_PROJECT_DIR)] ? $::env(TEMPLATE_PROJECT_DIR) : "build/vivado-microblaze"}]
set project_name [expr {[info exists ::env(TEMPLATE_PROJECT_NAME)] ? $::env(TEMPLATE_PROJECT_NAME) : "microblaze_platform"}]
set part_name [expr {[info exists ::env(TEMPLATE_PART)] ? $::env(TEMPLATE_PART) : "xc7a100tcsg324-1"}]
set board_part [expr {[info exists ::env(TEMPLATE_BOARD_PART)] ? $::env(TEMPLATE_BOARD_PART) : ""}]
set bd_name [expr {[info exists ::env(TEMPLATE_BD_NAME)] ? $::env(TEMPLATE_BD_NAME) : "system"}]

puts "INFO: creating MicroBlaze platform project: $project_name"
puts "INFO: part=$part_name board_part=$board_part"

create_project -force $project_name $project_dir -part $part_name
if {$board_part ne ""} {
    set_property board_part $board_part [current_project]
}

create_bd_design $bd_name

# External clock/reset boundary. The board-level XDC owns the real pin mapping.
create_bd_port -dir I -type clk -freq_hz 100000000 sys_clock
create_bd_port -dir I -type rst reset
set_property CONFIG.POLARITY ACTIVE_LOW [get_bd_ports reset]

create_bd_cell -type ip -vlnv [get_ipdefs xilinx.com:ip:clk_wiz:*] clk_wiz_0
set_property -dict [list \
    CONFIG.PRIM_IN_FREQ {100.000} \
    CONFIG.CLKOUT1_REQUESTED_OUT_FREQ {100.000} \
    CONFIG.USE_RESET {true} \
    CONFIG.RESET_TYPE {ACTIVE_LOW} \
    CONFIG.RESET_PORT {resetn} \
] [get_bd_cells clk_wiz_0]
connect_bd_net [get_bd_ports sys_clock] [get_bd_pins clk_wiz_0/clk_in1]
connect_bd_net [get_bd_ports reset] [get_bd_pins clk_wiz_0/resetn]

# Block automation creates the MicroBlaze, local memory, debug, AXI interconnect,
# clock/reset support, and optional interrupt controller. Adjust local memory and
# cache only after measuring the real application image size.
create_bd_cell -type ip -vlnv [get_ipdefs xilinx.com:ip:microblaze:*] microblaze_0
apply_bd_automation -rule xilinx.com:bd_rule:microblaze -config [list \
    local_mem "32KB" \
    ecc "None" \
    cache "None" \
    debug_module "Debug Only" \
    axi_periph "Enabled" \
    axi_intc "1" \
    preset "None" \
    clk "/clk_wiz_0/clk_out1 (100 MHz)"] [get_bd_cells microblaze_0]


# UARTLite console. The official Xilinx driver remains the implementation base;
# the C template's plt/drv layer owns the application-facing contract.
create_bd_cell -type ip -vlnv [get_ipdefs xilinx.com:ip:axi_uartlite:*] axi_uartlite_0
set_property CONFIG.C_BAUDRATE {115200} [get_bd_cells axi_uartlite_0]
apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config [list \
    Master "/microblaze_0 (Periph)" Clk "Auto"] [get_bd_intf_pins axi_uartlite_0/S_AXI]
make_bd_intf_pins_external [get_bd_intf_pins axi_uartlite_0/UART]
catch {set_property name uart [get_bd_intf_ports UART_0]}

# Four-bit AXI GPIO example for board LEDs or enables.
create_bd_cell -type ip -vlnv [get_ipdefs xilinx.com:ip:axi_gpio:*] axi_gpio_0
set_property -dict [list CONFIG.C_GPIO_WIDTH {4} CONFIG.C_ALL_OUTPUTS {1}] [get_bd_cells axi_gpio_0]
apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config [list \
    Master "/microblaze_0 (Periph)" Clk "Auto"] [get_bd_intf_pins axi_gpio_0/S_AXI]
make_bd_pins_external [get_bd_pins axi_gpio_0/gpio_io_o]
catch {set_property name gpio_led [get_bd_ports gpio_io_o_0]}

# 32-bit AXI Timer example for the bare-metal 1 ms time base.
create_bd_cell -type ip -vlnv [get_ipdefs xilinx.com:ip:axi_timer:*] axi_timer_0
apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config [list \
    Master "/microblaze_0 (Periph)" Clk "Auto"] [get_bd_intf_pins axi_timer_0/S_AXI]

# MicroBlaze block automation creates microblaze_0_xlconcat and connects its
# dout to the AXI INTC. In0 is normally reserved for the debug module, so add
# UARTLite and timer sources on In1/In2.
set concat_cells [get_bd_cells -hier -filter {NAME =~ "*microblaze_0_xlconcat"}]
if {[llength $concat_cells] == 0} {
    error "microblaze_0_xlconcat was not created; rerun MicroBlaze block automation."
}
set concat_cell [lindex $concat_cells 0]
set_property CONFIG.NUM_PORTS {3} $concat_cell
connect_bd_net [get_bd_pins axi_uartlite_0/interrupt] [get_bd_pins ${concat_cell}/In1]
connect_bd_net [get_bd_pins axi_timer_0/interrupt] [get_bd_pins ${concat_cell}/In2]

assign_bd_address
validate_bd_design
save_bd_design

# Print the generated address map for the firmware handoff.
foreach seg [get_bd_addr_segs -of [get_bd_addr_spaces microblaze_0/Data]] {
    puts "ADDR_SEG: [get_property NAME $seg] OFFSET [format 0x%08X [get_property OFFSET $seg]]"
}

set wrapper_files [make_wrapper -files [get_files ${bd_name}.bd] -top]
foreach wrapper_file $wrapper_files {
    add_files -norecurse $wrapper_file
}
update_compile_order -fileset sources_1
puts "BD_CREATE_OK: ${project_dir}/${project_name}.xpr"
