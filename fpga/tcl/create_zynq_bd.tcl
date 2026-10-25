# SPDX-License-Identifier: MIT
# Author: Xue Weisen <xueweisen96@gmail.com>
# GitHub: https://github.com/DCRimbaud
#
# Create a minimal Zynq-7000 Block Design for the Vitis platform template.
#
# Run from the Vivado 2025.2 Tcl console or:
#   vivado -mode batch -source fpga/tcl/create_zynq_bd.tcl
#
# Environment variables:
#   TEMPLATE_PROJECT_DIR    default: build/vivado-zynq
#   TEMPLATE_PROJECT_NAME   default: zynq_platform
#   TEMPLATE_PART           default: xc7z020clg400-1
#   TEMPLATE_BOARD_PART     recommended: choose the actual board part
#   TEMPLATE_BD_NAME        default: system
#
# The board preset configures DDR, MIO, and PS peripherals. Verify the PS UART,
# FCLK, M_AXI_GP0, and reset settings against the real board before exporting XSA.

set project_dir [expr {[info exists ::env(TEMPLATE_PROJECT_DIR)] ? $::env(TEMPLATE_PROJECT_DIR) : "build/vivado-zynq"}]
set project_name [expr {[info exists ::env(TEMPLATE_PROJECT_NAME)] ? $::env(TEMPLATE_PROJECT_NAME) : "zynq_platform"}]
set part_name [expr {[info exists ::env(TEMPLATE_PART)] ? $::env(TEMPLATE_PART) : "xc7z020clg400-1"}]
set board_part [expr {[info exists ::env(TEMPLATE_BOARD_PART)] ? $::env(TEMPLATE_BOARD_PART) : ""}]
set bd_name [expr {[info exists ::env(TEMPLATE_BD_NAME)] ? $::env(TEMPLATE_BD_NAME) : "system"}]

puts "INFO: creating Zynq platform project: $project_name"
puts "INFO: part=$part_name board_part=$board_part"

create_project -force $project_name $project_dir -part $part_name
if {$board_part ne ""} {
    set_property board_part $board_part [current_project]
}
create_bd_design $bd_name

create_bd_cell -type ip -vlnv [get_ipdefs xilinx.com:ip:processing_system7:*] processing_system7_0
apply_bd_automation -rule xilinx.com:bd_rule:processing_system7 -config [list \
    make_external "FIXED_IO, DDR" \
    apply_board_preset "1" \
    Master "Disable" \
    Slave "Disable"] [get_bd_cells processing_system7_0]

# AXI GPIO example. The AXI4 automation creates or extends the PS-to-PL
# interconnect and connects the peripheral clock/reset where the PS topology
# exposes the expected pins.
create_bd_cell -type ip -vlnv [get_ipdefs xilinx.com:ip:axi_gpio:*] axi_gpio_0
set_property -dict [list CONFIG.C_GPIO_WIDTH {4} CONFIG.C_ALL_OUTPUTS {1}] [get_bd_cells axi_gpio_0]
apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config [list \
    Master "/processing_system7_0/M_AXI_GP0" Clk "Auto"] [get_bd_intf_pins axi_gpio_0/S_AXI]
make_bd_pins_external [get_bd_pins axi_gpio_0/gpio_io_o]
catch {set_property name gpio_led [get_bd_ports gpio_io_o_0]}

# 32-bit AXI Timer example for a PL-side time base when PS private timers are
# not used by the application.
create_bd_cell -type ip -vlnv [get_ipdefs xilinx.com:ip:axi_timer:*] axi_timer_0
apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config [list \
    Master "/processing_system7_0/M_AXI_GP0" Clk "Auto"] [get_bd_intf_pins axi_timer_0/S_AXI]

assign_bd_address
validate_bd_design
save_bd_design

foreach seg [get_bd_addr_segs -of [get_bd_addr_spaces processing_system7_0/Data]] {
    puts "ADDR_SEG: [get_property NAME $seg] OFFSET [format 0x%08X [get_property OFFSET $seg]]"
}

set wrapper_files [make_wrapper -files [get_files ${bd_name}.bd] -top]
foreach wrapper_file $wrapper_files {
    add_files -norecurse $wrapper_file
}
update_compile_order -fileset sources_1
puts "BD_CREATE_OK: ${project_dir}/${project_name}.xpr"
