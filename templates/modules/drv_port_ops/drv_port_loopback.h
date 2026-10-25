//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: drv_port_loopback
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: host 可测试的回环端口实现
// Revision: 0.01
// Revision: 0.01 - File Created
// Keywords: loopback-port, multi-instance, caller-owned-ring, no-malloc
//////////////////////////////////////////////////////////////////////////////////
#ifndef DRV_PORT_LOOPBACK_H
#define DRV_PORT_LOOPBACK_H

#include "drv_port_ops.h"

/** 回环端口私有上下文；调用方提供并保持生命周期。 */
typedef struct {
	u8 *storage; /**< caller-owned ring storage */
	u32 capacity;/**< storage capacity */
	u32 head;    /**< read index */
	u32 count;   /**< bytes currently stored */
} drv_port_loopback_t;

/**
 * @brief 绑定回环端口实例。
 *
 * @param port 输出端口对象；不可为 NULL。
 * @param ctx 回环上下文；不可为 NULL，生命周期覆盖端口。
 * @param storage 调用方存储；不可为 NULL，容量必须 > 0。
 * @param capacity 存储容量。
 * @return DRV_PORT_OK / DRV_PORT_ERR_INVALID。
 * @author Xue Weisen
 */
drv_port_rc_e drv_port_loopback_init(drv_port_t *port,
                                     drv_port_loopback_t *ctx,
                                     u8 *storage,
                                     u32 capacity);

#endif /* DRV_PORT_LOOPBACK_H */
