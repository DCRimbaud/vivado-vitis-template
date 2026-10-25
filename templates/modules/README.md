# Generic Modules

These modules are selected from the maintained firmware template set. They are
copy-ready references, not a replacement for the package/target-oriented
[embedded-c-kit](https://github.com/DCRimbaud/embedded-c-kit). Use the kit for
shared primitives when possible; copy a module only when project integration or
board-specific adaptation requires it.

| Module | Purpose | Typical dependency |
|---|---|---|
| `app_cmd` | command table and line dispatch | `mw_console` |
| `app_component` | component event-loop scaffold | `plt_rtos` |
| `cfg_memory.h` | stack/heap/pool/DMA budget entry point | none |
| `cfg_project.h` | project version/build identity | none |
| `common_bits.h` | bit get/set helpers | `common_types` |
| `common_parse` | integer parsing | `common_types` |
| `common_span.h` | borrowed byte views | `common_types` |
| `common_xfer.h` | payload descriptor | `common_types` |
| `drv_gpio` | logical GPIO port + AXI GPIO adapter | `common_types`, `xgpio` |
| `drv_iic` | IIC transaction port | `common_types` |
| `drv_port_ops` | replaceable byte-port example | `common_types` |
| `drv_reg_shadow` | shadow/readback/busy wait helper | `common_types`, `plt_mmio` |
| `drv_snapshot_window` | PL double-buffer snapshot helper | `common_types`, `plt_mmio` |
| `drv_spi` | SPI full-transaction port | `common_types` |
| `mw_command` | opcode/command lookup | `common_types` |
| `mw_xfer_rtos` | ISR/task descriptor queue | `common_xfer`, `plt_rtos` |
| `plt_fault` | caller-owned fault ring | `common_types` |
| `plt_mmio` | caller-owned MMIO access contract | none |
| `plt_poll` | wrap-safe deadlines | `common_types` |
| `plt_rtos` | compile-time OSAL contract and backends | `common_types` |
| `plt_time` | OS-independent monotonic time | `common_types`, XTime adapter |

## Copy rules

1. Keep the layer prefix and include path.
2. Review the module's README and failure contract.
3. Do not copy a duplicate already provided by `embedded-c-kit`.
4. Keep FreeRTOS types inside `plt/rtos/freertos/`.
5. Keep vendor headers inside `drv/` or `plt/`.
6. Run the template quality and API-surface checks after adapting names.
