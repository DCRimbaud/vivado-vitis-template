# Vitis Project Integration

## Platform creation

1. Build the Vivado design through `synth_1` and `impl_1` Design Runs.
2. Confirm timing, DRC, CDC, and bitstream completion.
3. Run `export_xsa_after_runs.tcl` or export the XSA from the Vivado GUI.
4. Create a Vitis platform from that XSA.
5. Create a domain/BSP for the selected processor.
6. Choose `standalone` for bare metal or the official `freertos10_xilinx` BSP
   for FreeRTOS.

## Application creation

1. Generate a skeleton with `scripts/new_app_scaffold.ps1`.
2. Add the generated source tree to the Vitis application.
3. Add include paths for `src` and all subdirectories, including `src/plt/rtos`
   for the FreeRTOS OSAL backend.
4. Add every `.c` file to the application source list.
5. Set the linker script and heap/stack budget to project values.
6. Review `cfg_board.h` against `xparameters.h`.
7. Build the application and inspect map/ELF output.

## FreeRTOS

The allowed backend is the Xilinx Vitis FreeRTOS BSP. Do not add a second kernel.
Only `src/plt/rtos/freertos/` may include FreeRTOS headers. Keep task and queue
handles out of application-facing headers. Use millisecond timeouts and explicit
status codes at the OSAL boundary.

## Bare metal

Keep the main loop bounded. A blocking UART or polling loop must have a timeout
and a visible error path. Do not convert a missing interrupt into an infinite
wait. The MicroBlaze template demonstrates the platform → app → poll ordering.

## Debug checklist

- XSA path and Vivado build version
- BSP library versions and OS selection
- `xparameters.h` mapping
- linker script memory regions
- heap/stack/BSS/map totals
- UART console pin and baud rate
- interrupt controller input numbers
- reset polarity and clock frequency
- application ELF and first boot log
