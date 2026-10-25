//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: drv_iic
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: IIC 事务端口：7/10 bit 地址、写后读、重复起始、超时与统计
// Revision: 0.01
// Revision: 0.01 - File Created
// Keywords: iic-transaction-port, const-ops-self-priv, 7bit-10bit-address,
//   tx-rx-transfer, repeated-start, finite-timeout, cancel-stats
//////////////////////////////////////////////////////////////////////////////////
#ifndef DRV_IIC_H
#define DRV_IIC_H

#include "common_types.h"

/** IIC 端口返回码。 */
typedef enum {
	DRV_IIC_OK = 0,
	DRV_IIC_ERR_INVALID = -1,
	DRV_IIC_ERR_BUSY = -2,
	DRV_IIC_ERR_TIMEOUT = -3,
	DRV_IIC_ERR_IO = -4,
	DRV_IIC_ERR_NACK = -5,
	DRV_IIC_ERR_UNSUPPORTED = -6
} drv_iic_rc_e;

/** 总线配置；由后端解释并保证实际频率不高于请求值。 */
typedef struct {
	u32 bus_hz;
	u8 ten_bit;
	u8 reserved[3];
} drv_iic_cfg_t;

/** 一次 IIC 事务；tx/rx 可同时存在，形成写后读或重复起始。 */
typedef struct {
	const u8 *tx;
	u8 *rx;
	u32 tx_len;
	u32 rx_len;
	u32 timeout_ms;
	u32 flags;
	const drv_iic_cfg_t *cfg;
	u16 address;
	u8 ten_bit;
	u8 reserved;
} drv_iic_xfer_t;

#define DRV_IIC_XFER_REPEATED_START (1u << 0)
#define DRV_IIC_XFER_NO_STOP        (1u << 1)
#define DRV_IIC_XFER_FLAGS_MASK     (DRV_IIC_XFER_REPEATED_START | DRV_IIC_XFER_NO_STOP)

/** IIC 统计快照。 */
typedef struct {
	u32 transfers;
	u32 completed;
	u32 nacks;
	u32 io_errors;
	u32 timeouts;
	u32 bytes_transferred;
} drv_iic_stats_t;

typedef struct drv_iic drv_iic_t;

/** 可替换的 IIC 行为；每个方法都收到所属实例。 */
typedef struct {
	drv_iic_rc_e (*configure)(drv_iic_t *self, const drv_iic_cfg_t *cfg);
	drv_iic_rc_e (*transfer)(drv_iic_t *self,
	                         const drv_iic_xfer_t *xfer,
	                         u32 *done);
	drv_iic_rc_e (*cancel)(drv_iic_t *self);
	drv_iic_rc_e (*get_stats)(drv_iic_t *self, drv_iic_stats_t *out);
} drv_iic_ops_t;

/** IIC 对象：共享方法表 + 后端私有状态。 */
struct drv_iic {
	const drv_iic_ops_t *ops;
	void *priv;
};

/**
 * @brief 配置 IIC 总线频率与地址宽度。
 *
 * @param self 已绑定对象；ops->configure 必须存在。
 * @param cfg 总线配置；bus_hz 必须大于 0，ten_bit 只能为 0/1。
 * @return IIC 域返回码。
 * @note 后端负责把 bus_hz 向下取整到可实现的时钟。
 * @author Xue Weisen
 */
drv_iic_rc_e drv_iic_configure(drv_iic_t *self, const drv_iic_cfg_t *cfg);

/**
 * @brief 执行一次 IIC 事务。
 *
 * @param self 已绑定对象。
 * @param xfer 事务视图；tx_len 与 rx_len 至少一个大于 0。
 * @param done 可选实际完成字节数；失败时不写。
 * @return IIC 域返回码。
 * @note 写后读必须显式设置 repeated-start；timeout_ms=0 表示立即检查；
 *       cfg 只在本次调用期间使用，函数返回后不保留指针。
 * @author Xue Weisen
 */
drv_iic_rc_e drv_iic_transfer(drv_iic_t *self,
                              const drv_iic_xfer_t *xfer,
                              u32 *done);

/**
 * @brief 请求取消当前事务。
 *
 * @param self 已绑定对象。
 * @return IIC 域返回码；后端不支持取消时返回 UNSUPPORTED。
 * @note 取消是尽力而为，调用方仍须按事务状态恢复总线。
 * @author Xue Weisen
 */
drv_iic_rc_e drv_iic_cancel(drv_iic_t *self);

/**
 * @brief 读取 IIC 统计快照。
 *
 * @param self 已绑定对象。
 * @param out 统计输出；只在成功时写入。
 * @return IIC 域返回码。
 * @author Xue Weisen
 */
drv_iic_rc_e drv_iic_get_stats(drv_iic_t *self, drv_iic_stats_t *out);

#endif /* DRV_IIC_H */
