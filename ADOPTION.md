# Adoption Guide

## Choose a template

| Situation | Template | Notes |
|---|---|---|
| MicroBlaze, no OS, polling/ISR console | `templates/microblaze-bare` | AXI INTC/Timer/UARTLite expected by the skeleton |
| Zynq-7000, no OS, polling/ISR console | `templates/zynq-bare` | PS UART and SCU timer path |
| Zynq-7000 with Xilinx FreeRTOS BSP | `templates/zynq-freertos` | uses `freertos10_xilinx`, OSAL keeps FreeRTOS types private |
| Generic register/control IP | `fpga/rtl/axi_lite_regs.v` | 32-bit AXI4-Lite register map |
| Need a reusable library | [embedded-c-kit](https://github.com/DCRimbaud/embedded-c-kit) | link or copy individual modules |

## Platform sequence

1. Decide CPU, OS, console UART, timer, interrupt controller, and custom AXI-Lite peripherals.
2. Create the Vivado project and BD using the scripts in `fpga/tcl`.
3. Run BD validation, assign addresses, generate the wrapper, and verify the address report.
4. Run synthesis and implementation through Vivado Project Mode Design Runs. Do not substitute direct
   `synth_design`/`route_design` commands for a formal delivery flow.
5. Export the XSA from the completed implementation with the bitstream included.
6. Create the Vitis platform and BSP from that XSA. For FreeRTOS, select the official Xilinx BSP backend.
7. Generate the application source skeleton.
8. Reconcile `cfg_board.h` with the generated `xparameters.h`.
9. Build, run on hardware, and capture timing/DRC/resource evidence.

## Reuse rules

- Use the generator for project identity and path setup; manually verify all board-specific values.
- Keep the original template under version control only after reviewing generated diffs.
- Add product registers and board pins in the consuming project.
- Do not copy generated BSP/workspace directories into this repository.
- If the project needs more generic modules, contribute them with tests and English public contracts.
