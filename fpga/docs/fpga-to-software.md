# FPGA-to-Software Handoff

## Contract

The XSA is the handoff artifact. It carries the hardware handoff metadata and,
when exported correctly, the matching bitstream. The Vitis platform and BSP must
be generated from the same XSA used by the FPGA build.

```text
BD address/IRQ map
  -> completed implementation
  -> XSA with bitstream
  -> Vitis platform
  -> domain/BSP
  -> xparameters.h and driver configuration
  -> firmware cfg/drv/plt layers
  -> application
```

## Mapping generated symbols

Do not assume the device ID, base address, or interrupt number. After BSP
generation:

1. Open `xparameters.h`.
2. Find the actual UART, GPIO, timer, interrupt-controller, and custom AXI-Lite
   symbols.
3. Map them through project-owned aliases in `cfg_board.h`.
4. Rebuild after any XSA or BSP regeneration.
5. Record the XSA path and build ID in the diagnostic package.

## C access pattern

The generic AXI-Lite register map is:

| Offset | Name | Access |
|---:|---|---|
| `0x00` | `CONTROL` | R/W |
| `0x04` | `STATUS` | RO |
| `0x08` | `SCRATCH0` | R/W |
| `0x0C` | `SCRATCH1` | R/W |
| `0x10` | `SCRATCH2` | R/W |
| `0x14` | `SCRATCH3` | R/W |
| `0x18` | `IRQ_ENABLE` | R/W |
| `0x1C` | `IRQ_STATUS` | RO |

The firmware adapter in `fpga/firmware/axi_lite_client.*` uses
`embedded_c/mmio.h` from [embedded-c-kit](https://github.com/DCRimbaud/embedded-c-kit):

```c
axi_lite_client_t client;
axi_lite_client_init(&client, AXI_LITE_BASEADDR);
axi_lite_client_write(&client, AXI_LITE_REG_CONTROL, 0x00000003u);
uint32_t status = axi_lite_client_read(&client, AXI_LITE_REG_STATUS);
```

Use 32-bit aligned offsets. Keep the base address in a configuration or
generated-board mapping layer, not in a product algorithm.

## Interrupt path

- The PL IP asserts its interrupt line only after its local status is valid.
- The BD routes the line through the selected controller.
- `xparameters.h` supplies the controller/input mapping.
- The firmware `plt` layer enables and acknowledges the controller.
- The ISR clears the hardware source, queues fixed-size evidence, and exits
  through the OSAL wake path.
- A missing interrupt is not fixed by adding a long delay; inspect the BD route,
  polarity, enable mask, clear sequence, and BSP vector table.

## Clocks, reset, and DMA

- AXI-Lite control registers are normally accessed in a slow peripheral clock
  domain; do not move them to a high-speed data clock without a CDC contract.
- Treat reset polarity as part of the BD/board contract.
- For DMA or shared-memory buffers, define cacheability, alignment, ownership,
  invalidation, and flush points in the platform layer.
- Never place a DMA buffer on the stack or in a region whose lifetime is shorter
  than the transfer.
