//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: drv_port_ops
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: 通用字节端口方法表转发
// Revision: 0.01
// Revision: 0.01 - File Created
// Keywords: port-dispatch, ops-null-check, method-null-check, output-preserved-on-failure
//////////////////////////////////////////////////////////////////////////////////
#include "drv_port_ops.h"

/**
 * @brief 统一打开端口。
 *
 * @param self 端口对象。
 * @param cfg 后端配置。
 * @return DRV_PORT_OK / DRV_PORT_ERR_INVALID / DRV_PORT_ERR_UNSUPPORTED。
 * @author Xue Weisen
 */
drv_port_rc_e drv_port_open(drv_port_t *self, const void *cfg)
{
	/* 分派前检查实例、方法表和可选方法，避免空指针跳到未知地址。 */
	if ((self == 0) || (self->ops == 0)) {
		return DRV_PORT_ERR_INVALID;
	}
	if (self->ops->open == 0) {
		return DRV_PORT_ERR_UNSUPPORTED;
	}
	return self->ops->open(self, cfg);
}

/**
 * @brief 统一读端口。
 *
 * @param self 端口对象。
 * @param data 输出缓冲。
 * @param capacity 输出容量。
 * @param got 实际读取字节数。
 * @return DRV_PORT_OK / DRV_PORT_ERR_EMPTY / DRV_PORT_ERR_INVALID。
 * @author Xue Weisen
 */
drv_port_rc_e drv_port_read(drv_port_t *self,
                            u8 *data,
                            u32 capacity,
                            u32 *got)
{
	/* 输出缓冲/容量先校验；失败时不写 *got，保持调用方原值。 */
	if ((self == 0) || (self->ops == 0) || (self->ops->read == 0) ||
	    (data == 0) || (capacity == 0u)) {
		return DRV_PORT_ERR_INVALID;
	}
	return self->ops->read(self, data, capacity, got);
}

/**
 * @brief 统一写端口。
 *
 * @param self 端口对象。
 * @param data 输入数据。
 * @param len 输入长度。
 * @param put 实际写入字节数。
 * @return DRV_PORT_OK / DRV_PORT_ERR_FULL / DRV_PORT_ERR_INVALID。
 * @author Xue Weisen
 */
drv_port_rc_e drv_port_write(drv_port_t *self,
                             const u8 *data,
                             u32 len,
                             u32 *put)
{
	/* 只读输入允许 len=0；非零长度必须给数据指针，失败不写 *put。 */
	if ((self == 0) || (self->ops == 0) || (self->ops->write == 0) ||
	    ((len > 0u) && (data == 0))) {
		return DRV_PORT_ERR_INVALID;
	}
	return self->ops->write(self, data, len, put);
}

/**
 * @brief 统一关闭端口。
 *
 * @param self 端口对象。
 * @return DRV_PORT_OK / DRV_PORT_ERR_INVALID / DRV_PORT_ERR_UNSUPPORTED。
 * @author Xue Weisen
 */
drv_port_rc_e drv_port_close(drv_port_t *self)
{
	/* close 可选；未实现返回 UNSUPPORTED，由组合根决定是否视为失败。 */
	if ((self == 0) || (self->ops == 0)) {
		return DRV_PORT_ERR_INVALID;
	}
	if (self->ops->close == 0) {
		return DRV_PORT_ERR_UNSUPPORTED;
	}
	return self->ops->close(self);
}
