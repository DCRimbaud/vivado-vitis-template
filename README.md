# Vivado / Vitis Platform Template

A reusable reference for creating a clean FPGA-to-software platform with AMD Vivado and Vitis 2025.2.
It complements the [Embedded C Kit](https://github.com/DCRimbaud/embedded-c-kit) and
[.NET Serial Toolkit](https://github.com/DCRimbaud/dotnet-serial-toolkit).

## What is included

- MicroBlaze bare-metal application skeleton
- Zynq-7000 bare-metal application skeleton
- Zynq-7000 FreeRTOS application skeleton using the official Vitis BSP
- selected generic firmware modules from the maintained template set
- reusable AXI4-Lite register RTL and a self-checking testbench
- Vivado Block Design creation and AXI address-assignment Tcl
- Vitis platform, XSA, BSP, and application integration guidance
- XDC baseline and pin-integration checklist
- PowerShell scaffold and quality scripts

## Recommended flow

```text
choose CPU and OS
  -> create Vivado project and Block Design
  -> add UART/GPIO/custom AXI-Lite register IP
  -> assign addresses and interrupts
  -> validate BD and generate the wrapper
  -> run synthesis and implementation through Design Runs
  -> export XSA with bitstream
  -> create the Vitis platform/BSP from that XSA
  -> instantiate one firmware template
  -> align cfg_board.h with xparameters.h
  -> build, run, and inspect timing/DRC
```

See [ADOPTION.md](ADOPTION.md) for the decision table and
[ARCHITECTURE.md](ARCHITECTURE.md) for the platform contract.

## Quick start

Create a MicroBlaze bare-metal application skeleton:

```powershell
.\scripts\new_app_scaffold.ps1 -Platform mb -Dest C:\work\my_app\src `
  -Project MyApp -Device xc7a100tcsg324-1 -ToolVersions "Vitis 2025.2"
```

Create a Zynq FreeRTOS skeleton:

```powershell
.\scripts\new_app_scaffold.ps1 -Platform zynq_freertos -Dest C:\work\my_app\src `
  -Project MyApp -Device xc7z020clg400-1 -ToolVersions "Vitis 2025.2"
```

The generator substitutes project/device/date/tool identity placeholders. Review `cfg_board.h`, the BSP
device names, interrupt numbers, and memory budget after generation; no script can infer board-specific
addresses safely.

## Repository layout

| Path | Purpose |
|---|---|
| `templates/microblaze-bare` | MicroBlaze bare-metal application source skeleton |
| `templates/zynq-bare` | Zynq-7000 bare-metal application source skeleton |
| `templates/zynq-freertos` | Zynq-7000 FreeRTOS application source skeleton |
| `templates/modules` | Selected generic firmware modules ready for project adaptation |
| `fpga/rtl` | Generic AXI4-Lite register RTL and testbench |
| `fpga/tcl` | Vivado BD creation, IP packaging, and XSA export helpers |
| `fpga/docs` | Block Design, FPGA-to-software, Vitis, and XDC integration guides |
| `scripts` | Scaffold, syntax, layer, API, and template quality checks |

## Template language

The firmware files under `templates/` are engineering source snapshots and intentionally preserve the
original contract comments and placeholders. Public repository documentation and newly added platform
integration code are written in English. Replace every `__PROJECT__`, `__DEVICE__`, `__DATE__`,
`__TOOL_VERSIONS__`, and identity placeholder before committing generated project code.

## Safety boundary

This repository does not contain product register maps, board pinouts, training logic, customer data,
production bitstreams, or generated Vitis workspaces. The included AXI-Lite register map is a generic
demonstration contract, not a product register map.

## License

MIT. See [LICENSE](LICENSE).

## Author

Xue Weisen  
Email: xueweisen96@gmail.com  
GitHub: https://github.com/DCRimbaud
