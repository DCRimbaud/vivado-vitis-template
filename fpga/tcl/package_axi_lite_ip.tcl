# SPDX-License-Identifier: MIT
# Author: Xue Weisen <xueweisen96@gmail.com>
# GitHub: https://github.com/DCRimbaud
#
# Package fpga/rtl/axi_lite_regs.v as a reusable Vivado IP core.
#
# The script creates a temporary packaging project, infers the AXI4-Lite,
# clock, and reset interfaces, and writes the IP to fpga/ip_repo.
#
# Environment variables:
#   TEMPLATE_PART       Vivado part used for packaging, default xc7k325tffg900-2
#   TEMPLATE_IP_VENDOR  default user.org
#   TEMPLATE_IP_LIBRARY default user
#
# The packaged repository is a reference. Inspect the IP GUI and address block
# after packaging, then commit the generated component.xml only when intended.

set script_dir [file normalize [file dirname [info script]]]
set repo_root [file normalize [file join $script_dir .. ..]]
set rtl_file [file join $repo_root fpga rtl axi_lite_regs.v]
set project_dir [file join $repo_root build package_axi_lite]
set ip_repo [file join $repo_root fpga ip_repo]
set part_name [expr {[info exists ::env(TEMPLATE_PART)] ? $::env(TEMPLATE_PART) : "xc7k325tffg900-2"}]
set ip_vendor [expr {[info exists ::env(TEMPLATE_IP_VENDOR)] ? $::env(TEMPLATE_IP_VENDOR) : "user.org"}]
set ip_library [expr {[info exists ::env(TEMPLATE_IP_LIBRARY)] ? $::env(TEMPLATE_IP_LIBRARY) : "user"}]

if {![file exists $rtl_file]} {
    error "AXI-Lite RTL not found: $rtl_file"
}

puts "INFO: packaging $rtl_file"
create_project -force axi_lite_pkg $project_dir -part $part_name
add_files -norecurse $rtl_file
set_property top axi_lite_regs [current_fileset]
update_compile_order -fileset sources_1

ipx::package_project -root_dir $ip_repo -vendor $ip_vendor -library $ip_library \
    -taxonomy /UserIP -import_files -set_current true

set core [ipx::current_core]
set_property name axi_lite_regs $core
set_property display_name "AXI4-Lite Register Block" $core
set_property description "Generic 32-bit AXI4-Lite register block for platform bring-up." $core
set_property vendor_display_name "DCRimbaud" $core
set_property version 1.0 $core

# package_project infers the s_axi, s_axi_aclk, and s_axi_aresetn interfaces
# from the RTL port names. Verify the inference instead of adding a duplicate
# interface definition.
set aximm_interface [ipx::get_bus_interfaces s_axi -of_objects $core -quiet]
if {$aximm_interface eq ""} {
    error "AXI4-Lite interface inference failed."
}
set aximm_name [get_property NAME $aximm_interface]
ipx::associate_bus_interfaces -busif $aximm_name -clock s_axi_aclk $core
set clock_interface [ipx::get_bus_interfaces s_axi_aclk -of_objects $core]
set freq_param [ipx::get_bus_parameters FREQ_HZ -of_objects $clock_interface -quiet]
if {$freq_param ne ""} {
    set_property value 100000000 $freq_param
}

ipx::create_xgui_files $core
ipx::update_checksums $core
ipx::check_integrity -quiet $core
ipx::save_core $core

puts "IP_PACKAGE_OK: [file join $ip_repo ${ip_vendor}_${ip_library}_axi_lite_regs_1.0]"
puts "IP_REPO: $ip_repo"
close_project
