# Platform Architecture

## Purpose

This repository is a platform reference, not a product firmware repository. It defines the boundary between
the programmable logic, the CPU BSP, and the application software so a new project does not re-invent the
same Vivado/Vitis integration work.

## End-to-end data path

```text
Board pin / IP core
  -> PL AXI-Lite or stream interface
  -> Vivado Block Design address/interrupt map
  -> XSA with bitstream and hardware metadata
  -> Vitis platform and BSP
  -> xparameters.h and official driver instances
  -> firmware drv/plt adapters
  -> application policy
```

The address map is the contract. A register offset, IRQ number, clock domain, or reset domain must exist in
the BD and in the firmware configuration for the same generated XSA. Never copy `xparameters.h` values from a
different project.

## Hardware platform responsibilities

- Create the processing system or MicroBlaze subsystem and its reset/clock topology.
- Add only the IP needed by the current product slice.
- Assign AXI base addresses deterministically and document them in the BD handoff.
- Connect interrupt sources through the intended controller and verify interrupt numbers.
- Expose board ports and pin constraints in XDC; keep board-specific pins out of generic RTL.
- Generate the wrapper only after BD validation.
- Produce the XSA from the completed implementation run that contains the bitstream.

## Firmware responsibilities

- Keep `cfg_board.h` as the single mapping from generated BSP symbols to project aliases.
- Keep `drv/` responsible for protocol/register semantics and `plt/` responsible for BSP/OS adaptation.
- Keep application code independent of vendor types and task handles.
- Verify `xparameters.h` names, base addresses, and IRQ numbers after every XSA/BSP regeneration.
- Budget stack, heap, DMA memory, and cache maintenance in the project configuration.

## FreeRTOS boundary

The FreeRTOS template targets the Vitis BSP `freertos10_xilinx` backend. Only `plt/rtos/freertos/` may include
FreeRTOS headers. Public contracts use milliseconds and explicit status codes. The ISR path only acknowledges
hardware, queues fixed-size work, and propagates a wake request.

## Repository boundaries

The public repository contains generic templates and generic AXI-Lite RTL. Product protocols, board maps,
lane layouts, calibration, production bitstreams, and customer configuration remain in the consuming project.
