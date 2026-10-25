# SPDX-License-Identifier: MIT
# Author: Xue Weisen <xueweisen96@gmail.com>
# GitHub: https://github.com/DCRimbaud
#
# Add the packaged AXI-Lite register IP to an existing Block Design and connect
# it to a processor AXI master through Vivado's AXI4 automation.
#
# Environment variables:
#   TEMPLATE_PROJECT    path to an existing .xpr (required)
#   TEMPLATE_BD         Block Design name, default system
#   TEMPLATE_IP_REPO    IP repository path, default <repo>/fpga/ip_repo
#   TEMPLATE_IP_VLNV    default user.org:user:axi_lite_regs:1.0
#   TEMPLATE_MASTER     default /microblaze_0 (Periph)
#   TEMPLATE_INSTANCE   default axi_lite_regs_0
#   TEMPLATE_OFFSET     optional AXI offset such as 0x42000000

if {![info exists ::env(TEMPLATE_PROJECT)]} {
    error "TEMPLATE_PROJECT must point to an existing Vivado .xpr"
}

set script_dir [file normalize [file dirname [info script]]]
set repo_root [file normalize [file join $script_dir .. ..]]
set project_file [file normalize $::env(TEMPLATE_PROJECT)]
set bd_name [expr {[info exists ::env(TEMPLATE_BD)] ? $::env(TEMPLATE_BD) : "system"}]
set ip_repo [expr {[info exists ::env(TEMPLATE_IP_REPO)] ? $::env(TEMPLATE_IP_REPO) : [file join $repo_root fpga ip_repo]}]
set ip_vlnv [expr {[info exists ::env(TEMPLATE_IP_VLNV)] ? $::env(TEMPLATE_IP_VLNV) : "user.org:user:axi_lite_regs:1.0"}]
set master_path [expr {[info exists ::env(TEMPLATE_MASTER)] ? $::env(TEMPLATE_MASTER) : "/microblaze_0 (Periph)"}]
set instance_name [expr {[info exists ::env(TEMPLATE_INSTANCE)] ? $::env(TEMPLATE_INSTANCE) : "axi_lite_regs_0"}]
set offset_value [expr {[info exists ::env(TEMPLATE_OFFSET)] ? $::env(TEMPLATE_OFFSET) : ""}]

open_project $project_file
set_property ip_repo_paths [list $ip_repo] [current_project]
update_ip_catalog
open_bd_design [get_bd_files $bd_name]

if {[llength [get_bd_cells -quiet $instance_name]] != 0} {
    error "BD cell already exists: $instance_name"
}

create_bd_cell -type ip -vlnv $ip_vlnv $instance_name
set slave_interface [get_bd_intf_pins ${instance_name}/s_axi]
if {$slave_interface eq ""} {
    error "Packaged IP does not expose s_axi: $ip_vlnv"
}

apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config [list \
    Master $master_path Clk "Auto"] $slave_interface

# Tie the generic read-only inputs low for the first bring-up. A real product
# can replace these constants with status/IRQ sources or external ports.
create_bd_cell -type ip -vlnv [get_ipdefs xilinx.com:ip:xlconstant:*] ${instance_name}_status_const
set_property -dict [list CONFIG.CONST_WIDTH {32} CONFIG.CONST_VAL {0}] [get_bd_cells ${instance_name}_status_const]
connect_bd_net [get_bd_pins ${instance_name}_status_const/dout] \
    [get_bd_pins ${instance_name}/status_in] [get_bd_pins ${instance_name}/irq_status_in]

assign_bd_address

set addr_space [get_bd_addr_spaces -quiet microblaze_0/Data]
if {$addr_space eq ""} {
    set addr_space [get_bd_addr_spaces -quiet processing_system7_0/Data]
}
set mapped_seg ""
foreach seg [get_bd_addr_segs -of_objects $addr_space] {
    set seg_name [get_property NAME $seg]
    if {[string match "*${instance_name}*" $seg_name]} {
        set mapped_seg $seg
        break
    }
}
if {$mapped_seg eq ""} {
    error "No mapped address segment was created for $instance_name"
}
if {$offset_value ne ""} {
    set_property OFFSET $offset_value $mapped_seg
}
puts "CUSTOM_IP_ADDR: [get_property NAME $mapped_seg] OFFSET [format 0x%08X [get_property OFFSET $mapped_seg]] RANGE [format 0x%08X [get_property RANGE $mapped_seg]]"

validate_bd_design
save_bd_design
puts "BD_ADD_IP_OK: $instance_name"
close_project
