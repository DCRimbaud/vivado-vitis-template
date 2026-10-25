//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: drv_port_ops
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: 通用字节端口方法表
// Revision: 0.01
// Revision: 0.01 - File Created
// Keywords: byte-port, const-ops, self-priv, open/read/write/close, multi-instance
//////////////////////////////////////////////////////////////////////////////////
#ifndef DRV_PORT_OPS_H
#define DRV_PORT_OPS_H

#include "common_types.h"

/** 端口返回码。 */
typedef enum {
	DRV_PORT_OK = 0,           /**< success */
	DRV_PORT_ERR_INVALID = -1, /**< invalid argument */
	DRV_PORT_ERR_TIMEOUT = -2, /**< operation timed out */
	DRV_PORT_ERR_FULL = -3,    /**< write buffer full */
	DRV_PORT_ERR_EMPTY = -4,   /**< no bytes available */
	DRV_PORT_ERR_UNSUPPORTED = -5 /**< operation not implemented */
} drv_port_rc_e;

typedef struct drv_port drv_port_t;

/** 共享行为表；每个方法都收到所属实例。 */
typedef struct {
	drv_port_rc_e (*open)(drv_port_t *self, const void *cfg);
	drv_port_rc_e (*read)(drv_port_t *self, u8 *data, u32 capacity, u32 *got);
	drv_port_rc_e (*write)(drv_port_t *self, const u8 *data, u32 len, u32 *put);
	drv_port_rc_e (*close)(drv_port_t *self);
} drv_port_ops_t;

/** 端口对象：只读方法表 + 实现方私有指针。 */
struct drv_port {
	const drv_port_ops_t *ops; /**< shared const methods */
	void *priv;                /**< implementation state */
};

/**
 * @brief 统一打开端口。
 *
 * @param self 端口对象；ops 必须已由工厂绑定。
 * @param cfg 后端配置；允许为 NULL，由后端解释。
 * @return DRV_PORT_OK / DRV_PORT_ERR_INVALID / DRV_PORT_ERR_UNSUPPORTED。
 * @author Xue Weisen
 */
drv_port_rc_e drv_port_open(drv_port_t *self, const void *cfg);

/**
 * @brief 统一读端口。
 *
 * @param self 端口对象。
 * @param data 输出缓冲；capacity > 0。
 * @param capacity 输出容量。
 * @param got 实际读取字节数；允许为 NULL。
 * @return DRV_PORT_OK / DRV_PORT_ERR_EMPTY / DRV_PORT_ERR_INVALID。
 * @author Xue Weisen
 */
drv_port_rc_e drv_port_read(drv_port_t *self,
                            u8 *data,
                            u32 capacity,
                            u32 *got);

/**
 * @brief 统一写端口。
 *
 * @param self 端口对象。
 * @param data 输入字节。
 * @param len 输入长度。
 * @param put 实际写入字节数；允许为 NULL。
 * @return DRV_PORT_OK / DRV_PORT_ERR_FULL / DRV_PORT_ERR_INVALID。
 * @author Xue Weisen
 */
drv_port_rc_e drv_port_write(drv_port_t *self,
                             const u8 *data,
                             u32 len,
                             u32 *put);

/**
 * @brief 统一关闭端口。
 *
 * @param self 端口对象。
 * @return DRV_PORT_OK / DRV_PORT_ERR_INVALID / DRV_PORT_ERR_UNSUPPORTED。
 * @author Xue Weisen
 */
drv_port_rc_e drv_port_close(drv_port_t *self);

#endif /* DRV_PORT_OPS_H */
