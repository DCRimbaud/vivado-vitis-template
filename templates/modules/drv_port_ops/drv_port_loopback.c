//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: drv_port_loopback
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: 回环端口实现，用于 host 测试和协议零硬件验证
// Revision: 0.01
// Revision: 0.01 - File Created
// Keywords: loopback-impl, ring-buffer, self-priv, read-nonblocking, write-reject-when-full
//////////////////////////////////////////////////////////////////////////////////
#include "drv_port_loopback.h"

static drv_port_rc_e drv_port_loopback_open(drv_port_t *self, const void *cfg);
static drv_port_rc_e drv_port_loopback_read(drv_port_t *self,
                                             u8 *data,
                                             u32 capacity,
                                             u32 *got);
static drv_port_rc_e drv_port_loopback_write(drv_port_t *self,
                                              const u8 *data,
                                              u32 len,
                                              u32 *put);
static drv_port_rc_e drv_port_loopback_close(drv_port_t *self);

static const drv_port_ops_t s_loopback_ops = {
	.open = drv_port_loopback_open,
	.read = drv_port_loopback_read,
	.write = drv_port_loopback_write,
	.close = drv_port_loopback_close,
};

/**
 * @brief 绑定回环端口实例。
 *
 * @param port 输出端口对象。
 * @param ctx 调用方提供的回环上下文。
 * @param storage 调用方提供的环形存储。
 * @param capacity 存储容量。
 * @return DRV_PORT_OK / DRV_PORT_ERR_INVALID。
 * @author Xue Weisen
 */
drv_port_rc_e drv_port_loopback_init(drv_port_t *port,
                                     drv_port_loopback_t *ctx,
                                     u8 *storage,
                                     u32 capacity)
{
	/* 绑定阶段只保存借用指针，不复制存储；调用方必须保证生命周期。 */
	if ((port == 0) || (ctx == 0) || (storage == 0) || (capacity == 0u)) {
		return DRV_PORT_ERR_INVALID;
	}
	ctx->storage = storage;
	ctx->capacity = capacity;
	ctx->head = 0u;
	ctx->count = 0u;
	port->ops = &s_loopback_ops;
	port->priv = ctx;
	return DRV_PORT_OK;
}

/**
 * @brief 回环端口打开。
 *
 * @param self 端口对象。
 * @param cfg 未使用。
 * @return DRV_PORT_OK / DRV_PORT_ERR_INVALID。
 * @author Xue Weisen
 */
static drv_port_rc_e drv_port_loopback_open(drv_port_t *self, const void *cfg)
{
	(void)cfg;
	/* 操作前把 priv 恢复为强类型；这是 void * 的受控出口。 */
	if ((self == 0) || (self->priv == 0)) {
		return DRV_PORT_ERR_INVALID;
	}
	return DRV_PORT_OK;
}

/**
 * @brief 从回环环形缓冲读取。
 *
 * @param self 端口对象。
 * @param data 输出缓冲。
 * @param capacity 输出容量。
 * @param got 实际读取字节数。
 * @return DRV_PORT_OK / DRV_PORT_ERR_EMPTY / DRV_PORT_ERR_INVALID。
 * @author Xue Weisen
 */
static drv_port_rc_e drv_port_loopback_read(drv_port_t *self,
                                             u8 *data,
                                             u32 capacity,
                                             u32 *got)
{
	drv_port_loopback_t *ctx;
	u32 n;
	u32 i;
	u32 index;

	/* 校验输出与实例状态；读失败不写 *got。 */
	if ((self == 0) || (data == 0) || (capacity == 0u) || (self->priv == 0)) {
		return DRV_PORT_ERR_INVALID;
	}
	ctx = (drv_port_loopback_t *)self->priv;
	if (ctx->count == 0u) {
		return DRV_PORT_ERR_EMPTY;
	}

	/* 读取阶段按环尾回绕逐字节复制，最多读取调用方容量。 */
	n = (capacity < ctx->count) ? capacity : ctx->count;
	index = ctx->head;
	for (i = 0u; i < n; i++) {
		data[i] = ctx->storage[index];
		index = (index + 1u) % ctx->capacity;
	}
	ctx->head = index;
	ctx->count -= n;
	if (got != 0) {
		*got = n;
	}
	return DRV_PORT_OK;
}

/**
 * @brief 写入回环环形缓冲。
 *
 * @param self 端口对象。
 * @param data 输入数据。
 * @param len 输入长度。
 * @param put 实际写入字节数。
 * @return DRV_PORT_OK / DRV_PORT_ERR_FULL / DRV_PORT_ERR_INVALID。
 * @author Xue Weisen
 */
static drv_port_rc_e drv_port_loopback_write(drv_port_t *self,
                                              const u8 *data,
                                              u32 len,
                                              u32 *put)
{
	drv_port_loopback_t *ctx;
	u32 free_bytes;
	u32 i;
	u32 index;

	/* 校验实例和输入；len=0 合法，不要求 data 非 NULL。 */
	if ((self == 0) || (self->priv == 0) || ((len > 0u) && (data == 0))) {
		return DRV_PORT_ERR_INVALID;
	}
	ctx = (drv_port_loopback_t *)self->priv;
	free_bytes = ctx->capacity - ctx->count;
	if (len > free_bytes) {
		return DRV_PORT_ERR_FULL;
	}

	/* 写入阶段使用 tail = head + count，始终不覆盖未读字节。 */
	index = (ctx->head + ctx->count) % ctx->capacity;
	for (i = 0u; i < len; i++) {
		ctx->storage[index] = data[i];
		index = (index + 1u) % ctx->capacity;
	}
	ctx->count += len;
	if (put != 0) {
		*put = len;
	}
	return DRV_PORT_OK;
}

/**
 * @brief 回环端口关闭。
 *
 * @param self 端口对象。
 * @return DRV_PORT_OK / DRV_PORT_ERR_INVALID。
 * @author Xue Weisen
 */
static drv_port_rc_e drv_port_loopback_close(drv_port_t *self)
{
	/* 关闭只清逻辑状态，不释放调用方存储。 */
	if ((self == 0) || (self->priv == 0)) {
		return DRV_PORT_ERR_INVALID;
	}
	((drv_port_loopback_t *)self->priv)->head = 0u;
	((drv_port_loopback_t *)self->priv)->count = 0u;
	return DRV_PORT_OK;
}
