# SPDX-License-Identifier: MIT
# Author: Xue Weisen <xueweisen96@gmail.com>
# GitHub: https://github.com/DCRimbaud
#
# Export an XSA from an already completed Vivado Project Mode implementation.
# This script intentionally does not launch synthesis or implementation. Use
# visible Design Runs (synth_1 and impl_1) for the formal build and wait for
# both runs to reach Complete before running this script.
#
# Environment variables:
#   TEMPLATE_PROJECT    existing .xpr (required)
#   TEMPLATE_XSA        output XSA path (required)
#   TEMPLATE_IMPL_RUN   implementation run, default impl_1

if {![info exists ::env(TEMPLATE_PROJECT)] || ![info exists ::env(TEMPLATE_XSA)]} {
    error "TEMPLATE_PROJECT and TEMPLATE_XSA are required"
}

set project_file [file normalize $::env(TEMPLATE_PROJECT)]
set xsa_file [file normalize $::env(TEMPLATE_XSA)]
set impl_name [expr {[info exists ::env(TEMPLATE_IMPL_RUN)] ? $::env(TEMPLATE_IMPL_RUN) : "impl_1"}]

open_project $project_file
set impl_run [get_runs -quiet $impl_name]
if {$impl_run eq ""} {
    error "Implementation run not found: $impl_name"
}
set run_status [get_property STATUS $impl_run]
if {[string first "Complete" $run_status] < 0} {
    error "Implementation run '$impl_name' is not complete: $run_status"
}
if {[llength [get_runs -quiet synth_1]] == 0} {
    error "Required Project Mode synthesis run synth_1 is missing"
}

file mkdir [file dirname $xsa_file]
write_hw_platform -fixed -include_bit -force -file $xsa_file
if {![file exists $xsa_file]} {
    error "XSA was not created: $xsa_file"
}
puts "XSA_EXPORT_OK: $xsa_file"
close_project
