# Firmware Templates

The templates are layered Vitis application skeletons. They are intentionally
complete enough to show startup ordering, console/UART integration, interrupt or
OSAL boundaries, and board-configuration ownership.

| Directory | CPU | OS | Entry point | Main adaptation points |
|---|---|---|---|---|
| `microblaze-bare` | MicroBlaze | bare metal | `src/main.c` | AXI INTC, AXI Timer, UARTLite |
| `zynq-bare` | Zynq-7000 PS | bare metal | `src/main.c` | SCU timer, GIC, PS UART |
| `zynq-freertos` | Zynq-7000 PS | `freertos10_xilinx` | `src/main.c` | OSAL queue/task, PS UART, GIC |

## Placeholder policy

The skeleton files use `__PROJECT__`, `__DEVICE__`, `__DATE__`, and `__TOOL_VERSIONS__` placeholders.
Replace them only after choosing
the target BSP and reviewing the generated `xparameters.h`.

Use the generator:

```powershell
.\scripts\new_app_scaffold.ps1 -Platform mb -Dest C:\work\app\src `
  -Project MyApp -Device xc7a100tcsg324-1 -ToolVersions "Vitis 2025.2"
```

## Verification after generation

1. Confirm all include directories are added to the Vitis application.
2. Compare `cfg_board.h` against the generated `xparameters.h`.
3. Confirm the console UART and interrupt parent/IRQ numbers.
4. Confirm heap, stack, DMA, cache, and linker-script budgets.
5. Run `check_syntax`, `check_headers`, `check_layers`, `check_api_surface`, and
   `check_build` where applicable.
6. Run on the target and capture the console boot path.
