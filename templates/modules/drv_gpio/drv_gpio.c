//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: drv_gpio
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: 逻辑 GPIO 端口参数校验与方法分派
// Revision: 0.01
// Revision 0.01 - File Created
// Keywords: gpio-dispatch, mask-subset-check, direction-mask, output-preserved-on-failure
//////////////////////////////////////////////////////////////////////////////////
#include "drv_gpio.h"

/**
 * @brief 把逻辑端口恢复为未绑定状态。
 *
 * @param self 端口对象；允许为 NULL。
 * @return 无返回值。
 * @note 只清本地字段，不改变已经写入硬件的方向或电平。
 * @author Xue Weisen
 */
static void drv_gpio_reset(drv_gpio_t *self)
{
	if (self != 0) {
		self->ops = 0;
		self->priv = 0;
		self->present_mask = 0u;
		self->input_mask = 0u;
		self->output_mask = 0u;
	}
}

/**
 * @brief 判断逻辑 GPIO 对象是否已经完成绑定。
 *
 * @param self 端口对象；允许为 NULL。
 * @return 1 = 方法表和存在掩码有效；0 = 尚未初始化。
 * @note 只做本地字段检查，不访问硬件。
 * @author Xue Weisen
 */
static int drv_gpio_ready(const drv_gpio_t *self)
{
	return ((self != 0) && (self->ops != 0) && (self->present_mask != 0u)) ? 1 : 0;
}

/**
 * @brief 判断位掩码是否非空并完全落在存在位内。
 *
 * @param self 已初始化端口。
 * @param mask 待检查掩码。
 * @return 1 = 合法；0 = 空掩码或含不存在位。
 * @note 调用方必须先保证 self 有效。
 * @author Xue Weisen
 */
static int drv_gpio_present_mask_valid(const drv_gpio_t *self, u32 mask)
{
	return ((mask != 0u) && ((mask & ~self->present_mask) == 0u)) ? 1 : 0;
}

/**
 * @brief 绑定并配置一个逻辑 GPIO 端口。
 *
 * 先校验方法表与掩码，再把对象字段置为后端可观察的状态；若存在输出位，
 * 必须通过 set_output_mask 真正配置方向。任一步失败都会清空对象，避免上层
 * 把半初始化端口当成可用对象。
 *
 * @param self 调用方提供的端口对象。
 * @param ops 方法表。
 * @param priv 适配器私有状态。
 * @param present_mask 实际存在的位。
 * @param output_mask 初始输出位。
 * @return GPIO 域返回码。
 * @author Xue Weisen
 */
drv_gpio_rc_e drv_gpio_init(drv_gpio_t *self,
                            const drv_gpio_ops_t *ops,
                            void *priv,
                            u32 present_mask,
                            u32 output_mask)
{
	drv_gpio_rc_e rc;

	/* 参数阶段：方法表、存在位和输出位必须构成可安全分派的端口。 */
	if (self == 0) {
		return DRV_GPIO_ERR_INVALID;
	}
	if ((ops == 0) || (ops->read == 0) || (ops->write == 0) ||
	    (ops->set == 0) || (ops->clear == 0) || (present_mask == 0u) ||
	    ((output_mask & ~present_mask) != 0u)) {
		drv_gpio_reset(self);
		return DRV_GPIO_ERR_INVALID;
	}

	/* 绑定阶段：先暴露完整字段，后端方向方法才能看到 present/output 掩码。 */
	self->ops = ops;
	self->priv = priv;
	self->present_mask = present_mask;
	self->output_mask = output_mask;
	self->input_mask = present_mask & ~output_mask;

	/* 能力阶段：有输出位却没有方向方法时拒绝初始化，避免伪成功。 */
	if ((output_mask != 0u) && (ops->set_output_mask == 0)) {
		drv_gpio_reset(self);
		return DRV_GPIO_ERR_UNSUPPORTED;
	}
	/* 生效阶段：方向配置失败则清空对象，调用方不得继续使用半初始化端口。 */
	if (ops->set_output_mask != 0) {
		rc = ops->set_output_mask(self, output_mask);
		if (rc != DRV_GPIO_OK) {
			drv_gpio_reset(self);
			return rc;
		}
	}
	return DRV_GPIO_OK;
}

/**
 * @brief 读取端口当前原始电平。
 *
 * 后端先写入本地变量，只有成功时才提交给调用方，失败保持原输出值。
 *
 * @param self 已初始化端口。
 * @param value 输出值。
 * @return GPIO 域返回码。
 * @author Xue Weisen
 */
drv_gpio_rc_e drv_gpio_read(drv_gpio_t *self, u32 *value)
{
	u32 local_value = 0u;
	drv_gpio_rc_e rc;

	/* 输出保护：先确认目标指针有效，再由后端写本地临时值。 */
	if ((drv_gpio_ready(self) == 0) || (value == 0)) {
		return DRV_GPIO_ERR_INVALID;
	}
	rc = self->ops->read(self, &local_value);
	/* 提交阶段：只有完整成功才覆盖调用方原值。 */
	if (rc == DRV_GPIO_OK) {
		*value = local_value;
	}
	return rc;
}

/**
 * @brief 整字写输出端口。
 *
 * 调用前拒绝未实现位和输入位，避免一次写操作驱动错误引脚。
 *
 * @param self 已初始化端口。
 * @param value 写入值。
 * @return GPIO 域返回码。
 * @author Xue Weisen
 */
drv_gpio_rc_e drv_gpio_write(drv_gpio_t *self, u32 value)
{
	/* 整字写只允许落在已配置的输出位，防止误驱动输入或未实现引脚。 */
	if ((drv_gpio_ready(self) == 0) || ((value & ~self->output_mask) != 0u)) {
		return DRV_GPIO_ERR_INVALID;
	}
	return self->ops->write(self, value);
}

/**
 * @brief 将指定输出位同时置 1。
 *
 * @param self 已初始化端口。
 * @param mask 必须为非空输出掩码。
 * @return GPIO 域返回码。
 * @author Xue Weisen
 */
drv_gpio_rc_e drv_gpio_set(drv_gpio_t *self, u32 mask)
{
	/* 位操作阶段：非空、存在且为输出位，三个条件缺一不可。 */
	if ((drv_gpio_ready(self) == 0) || (drv_gpio_present_mask_valid(self, mask) == 0) ||
	    ((mask & ~self->output_mask) != 0u)) {
		return DRV_GPIO_ERR_INVALID;
	}
	return self->ops->set(self, mask);
}

/**
 * @brief 将指定输出位同时清 0。
 *
 * @param self 已初始化端口。
 * @param mask 必须为非空输出掩码。
 * @return GPIO 域返回码。
 * @author Xue Weisen
 */
drv_gpio_rc_e drv_gpio_clear(drv_gpio_t *self, u32 mask)
{
	/* 位操作阶段：与 set 使用同一掩码契约，避免读写路径规则漂移。 */
	if ((drv_gpio_ready(self) == 0) || (drv_gpio_present_mask_valid(self, mask) == 0) ||
	    ((mask & ~self->output_mask) != 0u)) {
		return DRV_GPIO_ERR_INVALID;
	}
	return self->ops->clear(self, mask);
}

/**
 * @brief 修改端口的输出位集合与方向。
 *
 * 先让后端完成硬件方向切换，成功后才更新本对象的方向掩码；失败时保留旧对象状态。
 *
 * @param self 已初始化端口。
 * @param output_mask 新输出位集合。
 * @return GPIO 域返回码。
 * @author Xue Weisen
 */
drv_gpio_rc_e drv_gpio_set_output_mask(drv_gpio_t *self, u32 output_mask)
{
	drv_gpio_rc_e rc;

	/* 方向切换先交给后端；对象字段只在硬件配置成功后同步。 */
	if ((drv_gpio_ready(self) == 0) || ((output_mask & ~self->present_mask) != 0u)) {
		return DRV_GPIO_ERR_INVALID;
	}
	if (self->ops->set_output_mask == 0) {
		return DRV_GPIO_ERR_UNSUPPORTED;
	}
	rc = self->ops->set_output_mask(self, output_mask);
	/* 提交阶段：后端成功后再更新输出/输入掩码，失败保持旧方向视图。 */
	if (rc == DRV_GPIO_OK) {
		self->output_mask = output_mask;
		self->input_mask = self->present_mask & ~output_mask;
	}
	return rc;
}

/**
 * @brief 读取端口的输入/输出掩码快照。
 *
 * @param self 已初始化端口。
 * @param present_mask 可选存在掩码输出。
 * @param input_mask 可选输入掩码输出。
 * @param output_mask 可选输出掩码输出。
 * @return GPIO 域返回码。
 * @note 失败时三个输出参数均保持原值。
 * @author Xue Weisen
 */
drv_gpio_rc_e drv_gpio_get_masks(const drv_gpio_t *self,
                                 u32 *present_mask,
                                 u32 *input_mask,
                                 u32 *output_mask)
{
	/* 快照阶段：至少一个输出参数存在，避免空调用。 */
	if ((drv_gpio_ready(self) == 0) ||
	    ((present_mask == 0) && (input_mask == 0) && (output_mask == 0))) {
		return DRV_GPIO_ERR_INVALID;
	}
	/* 写入阶段：三个输出均为可选，存在才写。 */
	if (present_mask != 0) {
		*present_mask = self->present_mask;
	}
	if (input_mask != 0) {
		*input_mask = self->input_mask;
	}
	if (output_mask != 0) {
		*output_mask = self->output_mask;
	}
	return DRV_GPIO_OK;
}
