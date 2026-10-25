# FPGA Integration Kit

This directory contains the hardware-side reference for the platform template.

| Path | Purpose |
|---|---|
| `rtl/axi_lite_regs.v` | Generic 32-bit AXI4-Lite register block |
| `rtl/tb_axi_lite_regs.sv` | Self-checking simulation testbench |
| `tcl/create_microblaze_bd.tcl` | Create MicroBlaze + UARTLite + GPIO + Timer BD |
| `tcl/create_zynq_bd.tcl` | Create Zynq-7000 PS + GPIO + Timer BD scaffold |
| `tcl/package_axi_lite_ip.tcl` | Package the generic register RTL as a Vivado IP |
| `tcl/add_axi_lite_to_bd.tcl` | Add and address-map the packaged IP in an existing BD |
| `tcl/export_xsa_after_runs.tcl` | Export an XSA only after completed Design Runs |
| `xdc/timing_template.xdc` | Commented timing/pin constraint skeleton |
| `docs/` | BD, XSA, Vitis, and software handoff guidance |

## Validated flow

The MicroBlaze BD script and custom-IP insertion flow were executed with Vivado
2025.2 on an available Kintex-7 part. The RTL testbench passed. Zynq-7000 BD
creation depends on a board preset and PS7 device library; run it against the
actual board part and verify DDR, MIO, UART, FCLK, and reset configuration.

## Formal build rule

The scripts create and validate the Block Design and wrapper. They do not launch
synthesis or implementation. For a deliverable bitstream, use Vivado
Project Mode Design Runs with `synth_1` and `impl_1`, then export the XSA from
the completed implementation run. Direct `synth_design`/`route_design` commands
are diagnostic only and are not a formal delivery flow.
