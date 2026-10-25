//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: drv_iic
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: IIC 事务参数校验与方法分派
// Revision: 0.01
// Revision: 0.01 - File Created
// Keywords: iic-dispatch, address-width-check, tx-rx-length-check,
//   output-preserved-on-failure, optional-cancel-stats
//////////////////////////////////////////////////////////////////////////////////
#include "drv_iic.h"

/**
 * @brief 校验 IIC 总线配置。
 *
 * @param cfg 配置对象；允许为 NULL。
 * @return 1 = 合法；0 = bus_hz 为 0 或 ten_bit 非法。
 * @author Xue Weisen
 */
static int drv_iic_cfg_valid(const drv_iic_cfg_t *cfg)
{
	return ((cfg != 0) && (cfg->bus_hz != 0u) && (cfg->ten_bit <= 1u)) ? 1 : 0;
}

/**
 * @brief 校验一次 IIC 事务的指针、长度、地址宽度和标志。
 *
 * @param xfer 事务视图；允许为 NULL。
 * @return 1 = 合法；0 = 参数非法或长度相加溢出。
 * @note 这里只做公共契约检查，总线能力由后端继续验证。
 * @author Xue Weisen
 */
static int drv_iic_xfer_valid(const drv_iic_xfer_t *xfer)
{
	u32 max_done;

	/* 基础阶段：地址宽度、长度和指针必须能组成一次完整事务。 */
	if ((xfer == 0) || (xfer->ten_bit > 1u) ||
	    ((xfer->tx_len == 0u) && (xfer->rx_len == 0u))) {
		return 0;
	}
	if ((xfer->tx_len > 0u) && (xfer->tx == 0)) {
		return 0;
	}
	if ((xfer->rx_len > 0u) && (xfer->rx == 0)) {
		return 0;
	}
	if ((xfer->flags & ~DRV_IIC_XFER_FLAGS_MASK) != 0u) {
		return 0;
	}
	/* 配置阶段：per-transaction 配置若存在，地址宽度必须与 xfer 声明一致。 */
	if ((xfer->cfg != 0) && ((drv_iic_cfg_valid(xfer->cfg) == 0) ||
	    (xfer->cfg->ten_bit != xfer->ten_bit))) {
		return 0;
	}
	max_done = xfer->tx_len + xfer->rx_len;
	/* 溢出阶段：两段长度相加必须能由 u32 完整表达。 */
	if (max_done < xfer->tx_len) {
		return 0;
	}
	return (xfer->ten_bit != 0u) ? ((xfer->address <= 0x3FFu) ? 1 : 0)
	                             : ((xfer->address <= 0x7Fu) ? 1 : 0);
}

/**
 * @brief 配置 IIC 总线频率与地址宽度。
 *
 * @param self 已绑定对象。
 * @param cfg 总线配置。
 * @return IIC 域返回码。
 * @author Xue Weisen
 */
drv_iic_rc_e drv_iic_configure(drv_iic_t *self, const drv_iic_cfg_t *cfg)
{
	drv_iic_rc_e rc;

	/* 分派阶段：先确认公共对象与后端方法存在，再校验总线配置。 */
	if ((self == 0) || (self->ops == 0)) {
		return DRV_IIC_ERR_INVALID;
	}
	if (self->ops->configure == 0) {
		return DRV_IIC_ERR_UNSUPPORTED;
	}
	if (drv_iic_cfg_valid(cfg) == 0) {
		return DRV_IIC_ERR_INVALID;
	}
	rc = self->ops->configure(self, cfg);
	/* 映射阶段：后端只允许返回本域已知状态码。 */
	switch (rc) {
	case DRV_IIC_OK:
	case DRV_IIC_ERR_BUSY:
	case DRV_IIC_ERR_TIMEOUT:
	case DRV_IIC_ERR_IO:
	case DRV_IIC_ERR_NACK:
	case DRV_IIC_ERR_INVALID:
		return rc;
	default:
		return DRV_IIC_ERR_UNSUPPORTED;
	}
}

/**
 * @brief 执行一次 IIC 事务。
 *
 * 可选 per-transaction 配置先应用，再调用后端 transfer；后端实际完成字节数
 * 先写本地变量，只有成功且不超过 tx_len+rx_len 时才提交给调用方。
 *
 * @param self 已绑定对象。
 * @param xfer 事务视图。
 * @param done 可选实际完成字节数。
 * @return IIC 域返回码。
 * @author Xue Weisen
 */
drv_iic_rc_e drv_iic_transfer(drv_iic_t *self,
                              const drv_iic_xfer_t *xfer,
                              u32 *done)
{
	drv_iic_rc_e rc;
	u32 local_done = 0u;
	u32 max_done;

	/* 入口阶段：事务参数在配置或硬件状态改变前完成校验。 */
	if ((self == 0) || (self->ops == 0) || (self->ops->transfer == 0) ||
	    (drv_iic_xfer_valid(xfer) == 0)) {
		return DRV_IIC_ERR_INVALID;
	}
	if (xfer->cfg != 0) {
		/* 配置阶段：per-transaction 配置失败时不启动传输。 */
		rc = drv_iic_configure(self, xfer->cfg);
		if (rc != DRV_IIC_OK) {
			return rc;
		}
	}
	max_done = xfer->tx_len + xfer->rx_len;
	rc = self->ops->transfer(self, xfer, &local_done);
	if (rc != DRV_IIC_OK) {
		return rc;
	}
	if (local_done > max_done) {
		/* 输出阶段：后端返回越界字节数说明控制器状态不可信。 */
		return DRV_IIC_ERR_IO;
	}
	if (done != 0) {
		*done = local_done;
	}
	return DRV_IIC_OK;
}

/**
 * @brief 请求取消当前 IIC 事务。
 *
 * @param self 已绑定对象。
 * @return IIC 域返回码；后端无 cancel 时返回 UNSUPPORTED。
 * @note 取消成功不表示总线已恢复到空闲，调用方仍需检查控制器状态。
 * @author Xue Weisen
 */
drv_iic_rc_e drv_iic_cancel(drv_iic_t *self)
{
	if ((self == 0) || (self->ops == 0)) {
		return DRV_IIC_ERR_INVALID;
	}
	if (self->ops->cancel == 0) {
		return DRV_IIC_ERR_UNSUPPORTED;
	}
	return self->ops->cancel(self);
}

/**
 * @brief 读取 IIC 统计快照。
 *
 * @param self 已绑定对象。
 * @param out 统计输出。
 * @return IIC 域返回码。
 * @note 失败时不写 out。
 * @author Xue Weisen
 */
drv_iic_rc_e drv_iic_get_stats(drv_iic_t *self, drv_iic_stats_t *out)
{
	if ((self == 0) || (self->ops == 0) || (out == 0)) {
		return DRV_IIC_ERR_INVALID;
	}
	if (self->ops->get_stats == 0) {
		return DRV_IIC_ERR_UNSUPPORTED;
	}
	return self->ops->get_stats(self, out);
}
