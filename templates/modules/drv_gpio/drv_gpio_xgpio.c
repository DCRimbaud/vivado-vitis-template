//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: drv_gpio_xgpio
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: AXI GPIO 适配器：方向、整字读写与原子置位/清位
// Revision: 0.01
// Revision: 0.01 - File Created
// Keywords: xgpio-lookup-by-caller, cfg-initialize, data-direction, discrete-read-write,
//   discrete-set-clear, generic-port-handoff
//////////////////////////////////////////////////////////////////////////////////
#include "drv_gpio_xgpio.h"

/**
 * @brief 从逻辑端口取回 AXI GPIO 适配器对象。
 *
 * @param port 已绑定的逻辑端口；其 priv 必须是 drv_gpio_xgpio_t。
 * @return 适配器指针。
 * @note 仅在适配器内部使用；调用方不得把 priv 当成通用容器解释。
 * @author Xue Weisen
 */
static drv_gpio_xgpio_t *drv_gpio_xgpio_self(drv_gpio_t *port)
{
	return (port == 0) ? 0 : (drv_gpio_xgpio_t *)port->priv;
}

/**
 * @brief AXI GPIO 读取方法。
 *
 * @param port 逻辑端口。
 * @param value 输出原始通道值。
 * @return GPIO 域返回码。
 * @note 读取不会改变方向或输出值，可在任务或轮询上下文调用。
 * @author Xue Weisen
 */
static drv_gpio_rc_e drv_gpio_xgpio_read(drv_gpio_t *port, u32 *value)
{
	drv_gpio_xgpio_t *self = drv_gpio_xgpio_self(port);

	if ((self == 0) || (self->initialized == 0u) || (value == 0)) {
		return DRV_GPIO_ERR_INVALID;
	}
	*value = XGpio_DiscreteRead(&self->instance, self->channel);
	return DRV_GPIO_OK;
}

/**
 * @brief AXI GPIO 整字写方法。
 *
 * @param port 逻辑端口。
 * @param value 已由公共层校验的输出值。
 * @return GPIO 域返回码。
 * @author Xue Weisen
 */
static drv_gpio_rc_e drv_gpio_xgpio_write(drv_gpio_t *port, u32 value)
{
	drv_gpio_xgpio_t *self = drv_gpio_xgpio_self(port);

	if ((self == 0) || (self->initialized == 0u)) {
		return DRV_GPIO_ERR_INVALID;
	}
	XGpio_DiscreteWrite(&self->instance, self->channel, value);
	return DRV_GPIO_OK;
}

/**
 * @brief AXI GPIO 原子置位方法。
 *
 * @param port 逻辑端口。
 * @param mask 已校验的输出位。
 * @return GPIO 域返回码。
 * @author Xue Weisen
 */
static drv_gpio_rc_e drv_gpio_xgpio_set(drv_gpio_t *port, u32 mask)
{
	drv_gpio_xgpio_t *self = drv_gpio_xgpio_self(port);

	if ((self == 0) || (self->initialized == 0u)) {
		return DRV_GPIO_ERR_INVALID;
	}
	XGpio_DiscreteSet(&self->instance, self->channel, mask);
	return DRV_GPIO_OK;
}

/**
 * @brief AXI GPIO 原子清位方法。
 *
 * @param port 逻辑端口。
 * @param mask 已校验的输出位。
 * @return GPIO 域返回码。
 * @author Xue Weisen
 */
static drv_gpio_rc_e drv_gpio_xgpio_clear(drv_gpio_t *port, u32 mask)
{
	drv_gpio_xgpio_t *self = drv_gpio_xgpio_self(port);

	if ((self == 0) || (self->initialized == 0u)) {
		return DRV_GPIO_ERR_INVALID;
	}
	XGpio_DiscreteClear(&self->instance, self->channel, mask);
	return DRV_GPIO_OK;
}

/**
 * @brief AXI GPIO 方向配置方法。
 *
 * XGpio 方向位 1 表示输入、0 表示输出；未实现位保持输入，避免误驱动。
 *
 * @param port 逻辑端口。
 * @param output_mask 输出位集合。
 * @return GPIO 域返回码。
 * @author Xue Weisen
 */
static drv_gpio_rc_e drv_gpio_xgpio_set_output_mask(drv_gpio_t *port, u32 output_mask)
{
	drv_gpio_xgpio_t *self = drv_gpio_xgpio_self(port);

	if ((self == 0) || (self->initialized == 0u) || (port == 0)) {
		return DRV_GPIO_ERR_INVALID;
	}
	/* XGpio 方向位 1=input、0=output；未实现的位保持输入，避免误驱动。 */
	XGpio_SetDataDirection(&self->instance, self->channel, ~output_mask);
	return DRV_GPIO_OK;
}

static const drv_gpio_ops_t s_drv_gpio_xgpio_ops = {
	drv_gpio_xgpio_read,
	drv_gpio_xgpio_write,
	drv_gpio_xgpio_set,
	drv_gpio_xgpio_clear,
	drv_gpio_xgpio_set_output_mask
};

/**
 * @brief 用 BSP XGpio_Config 初始化一个 AXI GPIO 通道。
 *
 * 配置查询留在组合根，适配器只接收 config；初始化顺序为 CfgInitialize、
 * 方向配置、初始输出写入。任一失败都不会留下可用的逻辑端口。
 *
 * @param self 适配器对象。
 * @param config BSP 配置对象。
 * @param channel AXI GPIO 通道号 1/2。
 * @param present_mask 实际连接位。
 * @param output_mask 初始输出位。
 * @param initial_value 初始输出值。
 * @return GPIO 域返回码。
 * @author Xue Weisen
 */
drv_gpio_rc_e drv_gpio_xgpio_init(drv_gpio_xgpio_t *self,
                                  const XGpio_Config *config,
                                  u32 channel,
                                  u32 present_mask,
                                  u32 output_mask,
                                  u32 initial_value)
{
	drv_gpio_rc_e rc;
	int status;

	/* 参数阶段：通道、存在位、方向位和初始值必须在进入硬件前全部合法。 */
	if ((self == 0) || (config == 0) || ((channel != 1u) && (channel != 2u)) ||
	    (present_mask == 0u) || ((output_mask & ~present_mask) != 0u) ||
	    ((initial_value & ~output_mask) != 0u)) {
		return DRV_GPIO_ERR_INVALID;
	}

	/* 初始化阶段：先关闭旧端口视图，再完成官方 XGpio 实例绑定。 */
	self->initialized = 0u;
	self->channel = channel;
	self->port.ops = 0;
	self->port.priv = 0;
	self->port.present_mask = 0u;
	self->port.input_mask = 0u;
	self->port.output_mask = 0u;
	status = XGpio_CfgInitialize(&self->instance, (XGpio_Config *)config,
	                             config->BaseAddress);
	if (status != XST_SUCCESS) {
		return DRV_GPIO_ERR_IO;
	}
	self->initialized = 1u;

	/* 绑定阶段：公共层设置方向时会回调本适配器，因此 initialized 必须先置位。 */
	rc = drv_gpio_init(&self->port, &s_drv_gpio_xgpio_ops, self,
	                   present_mask, output_mask);
	if (rc != DRV_GPIO_OK) {
		self->initialized = 0u;
		return rc;
	}
	/* 输出阶段：方向已稳定后写初始值，避免先驱动后改方向。 */
	XGpio_DiscreteWrite(&self->instance, self->channel, initial_value);
	return DRV_GPIO_OK;
}

/**
 * @brief 取得适配器内部的标准逻辑端口。
 *
 * @param self 适配器对象；允许为 NULL。
 * @return 端口指针；self 为 NULL 时返回 NULL。
 * @author Xue Weisen
 */
drv_gpio_t *drv_gpio_xgpio_port(drv_gpio_xgpio_t *self)
{
	return (self == 0) ? 0 : &self->port;
}
