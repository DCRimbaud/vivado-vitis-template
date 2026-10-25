//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: drv_gpio
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: 逻辑 GPIO 端口：存在掩码、方向、整字读写与位操作
// Revision: 0.01
// Revision 0.01 - File Created
// Keywords: logical-gpio-port, const-ops-self-priv, present/output-mask, direction,
//   read-write-set-clear, masked-operation, vendor-type-isolation
//////////////////////////////////////////////////////////////////////////////////
#ifndef DRV_GPIO_H
#define DRV_GPIO_H

#include "common_types.h"

/** GPIO 端口返回码。 */
typedef enum {
	DRV_GPIO_OK = 0,
	DRV_GPIO_ERR_INVALID = -1,
	DRV_GPIO_ERR_IO = -2,
	DRV_GPIO_ERR_UNSUPPORTED = -3
} drv_gpio_rc_e;

typedef struct drv_gpio drv_gpio_t;

/** 可替换的 GPIO 行为；每个方法都收到所属逻辑端口。 */
typedef struct {
	drv_gpio_rc_e (*read)(drv_gpio_t *self, u32 *value);
	drv_gpio_rc_e (*write)(drv_gpio_t *self, u32 value);
	drv_gpio_rc_e (*set)(drv_gpio_t *self, u32 mask);
	drv_gpio_rc_e (*clear)(drv_gpio_t *self, u32 mask);
	drv_gpio_rc_e (*set_output_mask)(drv_gpio_t *self, u32 output_mask);
} drv_gpio_ops_t;

/** 一个 32 bit 逻辑 GPIO 端口；priv 由适配器解释，调用方不得解引用。 */
struct drv_gpio {
	const drv_gpio_ops_t *ops;
	void *priv;
	u32 present_mask;
	u32 input_mask;
	u32 output_mask;
};

/**
 * @brief 绑定并配置一个逻辑 GPIO 端口。
 *
 * @param self 调用方提供的端口对象；不可为 NULL。
 * @param ops 方法表；read/write/set/clear 必须非 NULL。
 * @param priv 适配器私有状态。
 * @param present_mask 实际存在的 GPIO 位；不得为 0。
 * @param output_mask 初始输出位；必须包含于 present_mask。
 * @return DRV_GPIO_OK / DRV_GPIO_ERR_INVALID / DRV_GPIO_ERR_IO /
 *         DRV_GPIO_ERR_UNSUPPORTED。
 * @note 失败时对象被清空；成功后再把 self 交给上层。
 * @author Xue Weisen
 */
drv_gpio_rc_e drv_gpio_init(drv_gpio_t *self,
                            const drv_gpio_ops_t *ops,
                            void *priv,
                            u32 present_mask,
                            u32 output_mask);

/**
 * @brief 读取端口当前原始电平。
 *
 * @param self 已初始化端口。
 * @param value 输出值；只在成功时写入。
 * @return DRV_GPIO_OK / DRV_GPIO_ERR_INVALID / DRV_GPIO_ERR_IO。
 * @note 返回值包含未实现的位，调用方按 present_mask 解释。
 * @author Xue Weisen
 */
drv_gpio_rc_e drv_gpio_read(drv_gpio_t *self, u32 *value);

/**
 * @brief 整字写输出端口。
 *
 * @param self 已初始化端口。
 * @param value 写入值；不允许包含 output_mask 之外的位。
 * @return GPIO 域返回码。
 * @author Xue Weisen
 */
drv_gpio_rc_e drv_gpio_write(drv_gpio_t *self, u32 value);

/**
 * @brief 将指定输出位同时置 1。
 *
 * @param self 已初始化端口。
 * @param mask 输出位掩码；不得为 0。
 * @return GPIO 域返回码。
 * @author Xue Weisen
 */
drv_gpio_rc_e drv_gpio_set(drv_gpio_t *self, u32 mask);

/**
 * @brief 将指定输出位同时清 0。
 *
 * @param self 已初始化端口。
 * @param mask 输出位掩码；不得为 0。
 * @return GPIO 域返回码。
 * @author Xue Weisen
 */
drv_gpio_rc_e drv_gpio_clear(drv_gpio_t *self, u32 mask);

/**
 * @brief 修改端口的输出位集合与方向。
 *
 * @param self 已初始化端口。
 * @param output_mask 新输出位集合；必须包含于 present_mask。
 * @return GPIO 域返回码。
 * @note 成功后才更新对象中的 input_mask/output_mask。
 * @author Xue Weisen
 */
drv_gpio_rc_e drv_gpio_set_output_mask(drv_gpio_t *self, u32 output_mask);

/**
 * @brief 读取端口的输入/输出掩码快照。
 *
 * @param self 已初始化端口。
 * @param present_mask 可选存在掩码输出。
 * @param input_mask 可选输入掩码输出。
 * @param output_mask 可选输出掩码输出。
 * @return DRV_GPIO_OK / DRV_GPIO_ERR_INVALID。
 * @note 至少提供一个输出参数；失败时不写任何输出。
 * @author Xue Weisen
 */
drv_gpio_rc_e drv_gpio_get_masks(const drv_gpio_t *self,
                                 u32 *present_mask,
                                 u32 *input_mask,
                                 u32 *output_mask);

#endif /* DRV_GPIO_H */
