//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: drv_gpio_xgpio
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: AXI GPIO 官方驱动到逻辑 GPIO 端口的适配器
// Revision: 0.01
// Revision: 0.01 - File Created
// Keywords: xgpio-adapter, two-channel, direction-mask, set-clear-write-read,
//   vendor-type-boundary, drv-only-header
//////////////////////////////////////////////////////////////////////////////////
#ifndef DRV_GPIO_XGPIO_H
#define DRV_GPIO_XGPIO_H

#include "xgpio.h"
#include "drv_gpio.h"

/** AXI GPIO 适配器；instance 与 channel 只允许驱动/组合根访问。 */
typedef struct {
	drv_gpio_t port;
	XGpio instance;
	u32 channel;
	u8 initialized;
} drv_gpio_xgpio_t;

/**
 * @brief 用 BSP 提供的 XGpio_Config 初始化一个 AXI GPIO 通道。
 *
 * @param self 调用方提供的适配器对象；不可为 NULL。
 * @param config BSP 配置对象；SDT/classic 的 LookupConfig 差异由调用方处理。
 * @param channel AXI GPIO 通道号，只允许 1 或 2。
 * @param present_mask 实际连接的逻辑位，不得为 0。
 * @param output_mask 初始输出位，必须包含于 present_mask。
 * @param initial_value 初始输出值，只能包含 output_mask 内的位。
 * @return DRV_GPIO_OK / DRV_GPIO_ERR_INVALID / DRV_GPIO_ERR_IO。
 * @note XGpio 类型不得越过本头文件进入 app/mw 公共接口。
 * @author Xue Weisen
 */
drv_gpio_rc_e drv_gpio_xgpio_init(drv_gpio_xgpio_t *self,
                                  const XGpio_Config *config,
                                  u32 channel,
                                  u32 present_mask,
                                  u32 output_mask,
                                  u32 initial_value);

/**
 * @brief 取得适配器内部的标准逻辑 GPIO 端口。
 *
 * @param self 适配器对象；不可为 NULL。
 * @return 端口指针；self 为 NULL 时返回 NULL。
 * @author Xue Weisen
 */
drv_gpio_t *drv_gpio_xgpio_port(drv_gpio_xgpio_t *self);

#endif /* DRV_GPIO_XGPIO_H */
