# AXI-Lite Firmware Adapter

The files here show the C side of the generic AXI-Lite register block:

- `axi_lite_regs_map.h` is the shared offset contract.
- `axi_lite_client.h` documents the borrowed base address and access rules.
- `axi_lite_client.c` performs volatile 32-bit reads and writes through
  `embedded_c/mmio.h`.

Add the `embedded-c-kit` include directory to the Vitis application, then pass a
base address from the current `xparameters.h` or project-owned `cfg_board.h`.

The client rejects offsets outside `0x00..0x1C` and rejects unaligned addresses.
It does not cache values, allocate memory, or manage the IP clock/reset domain.
Those responsibilities remain in the platform and application layers.
