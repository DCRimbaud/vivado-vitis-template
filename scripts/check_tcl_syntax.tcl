# SPDX-License-Identifier: MIT
# Author: Xue Weisen <xueweisen96@gmail.com>
# GitHub: https://github.com/DCRimbaud
#
# Parse-only Tcl validation. It checks quoting and brace completeness without
# requiring Vivado. Vivado command semantics still require a Vivado smoke run.

set script_dir [file normalize [file dirname [info script]]]
set root [file normalize [file join $script_dir ..]]
set files [glob -nocomplain [file join $root fpga tcl *.tcl]]
set failed 0

foreach file $files {
    set stream [open $file r]
    set text [read $stream]
    close $stream
    if {![info complete $text]} {
        puts "TCL_SYNTAX_FAIL: $file"
        set failed 1
    } else {
        puts "TCL_SYNTAX_OK: $file"
    }
}

if {$failed != 0} {
    exit 1
}
puts "TCL_SYNTAX_PASS"
