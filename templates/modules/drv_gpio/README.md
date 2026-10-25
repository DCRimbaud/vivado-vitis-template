# drv_gpio

## 适用场景

- 真正的 AXI GPIO 通道：复制 `drv_gpio.c/h` 与 `drv_gpio_xgpio.c/h`；
- PS GPIO：新增 `drv_gpio_xgpiops` 适配器，公共层仍只看见逻辑端口；
- PL 自定义控制字窗口：不要伪装成 `XGpio`，使用 `plt_mmio + drv_reg_shadow + cfg_board`。

## 边界

`drv_gpio.h` 不包含任何 Xilinx 头。`drv_gpio_xgpio.h` 是唯一允许暴露 `XGpio` 的适配层头文件，
且只能被 `drv/` 或静态组合根包含，不得进入 `app/mw` 公共接口。

## 掩码语义

- `present_mask`：硬件实际存在的位；
- `output_mask`：方向为输出的位；
- `input_mask = present_mask & ~output_mask`；
- `set/clear` 只接受输出位，`write` 不允许写入未实现位。

## PL 控制字窗口

如果地址在 BD 中由 AXI GPIO 承载，但语义是产品控制字寄存器，不要把它当作通用 GPIO API。
优先使用领域驱动的 `plt_mmio + drv_reg_shadow + cfg_board` 路径；只有确实需要通用 pin/通道语义时，
才引入本模块做薄适配。