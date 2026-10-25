//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: drv_spi
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: SPI 事务端口转发与公共参数校验
// Revision: 0.01
// Revision 0.01 - File Created
// Keywords: spi-dispatch, const-ops-null-check, full-duplex-validation,
//   output-preserved-on-failure, optional-cancel-stats
//////////////////////////////////////////////////////////////////////////////////
#include "drv_spi.h"

/**
 * @brief 校验调用方提供的 SPI 配置。
 *
 * @param cfg 配置指针；NULL 视为非法。
 * @return 1 表示有效；0 表示非法。
 * @author Xue Weisen
 */
static int drv_spi_cfg_valid(const drv_spi_cfg_t *cfg)
{
	/* 参数校验：模式、位宽和位序必须是后端可明确实现的取值，禁止靠驱动猜配置。 */
	if (cfg == 0) {
		return 0;
	}
	if ((cfg->clock_hz == 0u) || ((u32)cfg->mode > (u32)DRV_SPI_MODE_3)) {
		return 0;
	}
	if ((cfg->data_bits != 8u) && (cfg->data_bits != 16u) && (cfg->data_bits != 32u)) {
		return 0;
	}
	if (cfg->lsb_first > 1u) {
		return 0;
	}
	return 1;
}

/**
 * @brief 通过方法表绑定并配置 SPI 实例。
 *
 * @param self SPI 对象。
 * @param cfg 配置参数。
 * @return SPI 域返回码。
 * @author Xue Weisen
 */
drv_spi_rc_e drv_spi_configure(drv_spi_t *self, const drv_spi_cfg_t *cfg)
{
	drv_spi_rc_e rc;

	/* 对象校验：实例或方法表缺失属于参数错误，可选 configure 未实现则明确返回不支持。 */
	if ((self == 0) || (self->ops == 0)) {
		return DRV_SPI_ERR_INVALID;
	}
	if (self->ops->configure == 0) {
		return DRV_SPI_ERR_UNSUPPORTED;
	}
	if (drv_spi_cfg_valid(cfg) == 0) {
		return DRV_SPI_ERR_INVALID;
	}

	/* 执行阶段：公共层先完成统一参数校验，后端返回码仅接受本域已定义集合。 */
	rc = self->ops->configure(self, cfg);
	if ((rc != DRV_SPI_OK) && (rc != DRV_SPI_ERR_TIMEOUT) &&
	    (rc != DRV_SPI_ERR_BUSY) && (rc != DRV_SPI_ERR_IO) &&
	    (rc != DRV_SPI_ERR_INVALID)) {
		return DRV_SPI_ERR_UNSUPPORTED;
	}
	return rc;
}

/**
 * @brief 执行一次全双工 SPI 事务。
 *
 * @param self SPI 对象。
 * @param xfer 事务视图。
 * @param done 可选的实际字节数输出。
 * @return SPI 域返回码。
 * @author Xue Weisen
 */
drv_spi_rc_e drv_spi_transfer(drv_spi_t *self,
                              const drv_spi_xfer_t *xfer,
                              u32 *done)
{
	drv_spi_rc_e rc;
	u32 local_done = 0u;

	/* 入参校验：全双工视图必须自洽，任何失败都发生在后端状态改变之前。 */
	if ((self == 0) || (self->ops == 0) || (xfer == 0)) {
		return DRV_SPI_ERR_INVALID;
	}
	if (self->ops->transfer == 0) {
		return DRV_SPI_ERR_UNSUPPORTED;
	}
	if ((xfer->len > 0u) && ((xfer->tx == 0) || (xfer->rx == 0))) {
		return DRV_SPI_ERR_INVALID;
	}
	if ((xfer->cfg != 0) && (drv_spi_cfg_valid(xfer->cfg) == 0)) {
		return DRV_SPI_ERR_INVALID;
	}

	/* 配置阶段：事务级配置可选；若提供则必须先成功，再用同一实例执行传输。 */
	if (xfer->cfg != 0) {
		rc = drv_spi_configure(self, xfer->cfg);
		if (rc != DRV_SPI_OK) {
			return rc;
		}
	}

	/* 传输阶段：后端只写局部计数，失败时不污染调用方输出；成功后校验计数上界。 */
	rc = self->ops->transfer(self, xfer, &local_done);
	if (rc != DRV_SPI_OK) {
		return rc;
	}
	if (local_done > xfer->len) {
		return DRV_SPI_ERR_IO;
	}
	if (done != 0) {
		/* 提交阶段：只有后端成功且计数合法时才发布 done，保证失败输出保持原值。 */
		*done = local_done;
	}
	return DRV_SPI_OK;
}

/**
 * @brief 在后端支持时取消当前事务。
 *
 * @param self SPI 对象。
 * @return SPI 域返回码。
 * @author Xue Weisen
 */
drv_spi_rc_e drv_spi_cancel(drv_spi_t *self)
{
	/* Cancellation is optional; absence is explicit, not a silent success. */
	if ((self == 0) || (self->ops == 0)) {
		return DRV_SPI_ERR_INVALID;
	}
	if (self->ops->cancel == 0) {
		return DRV_SPI_ERR_UNSUPPORTED;
	}
	return self->ops->cancel(self);
}

/**
 * @brief 读取 SPI 统计的一致性快照。
 *
 * @param self SPI 对象。
 * @param out 统计输出。
 * @return SPI 域返回码。
 * @author Xue Weisen
 */
drv_spi_rc_e drv_spi_get_stats(drv_spi_t *self, drv_spi_stats_t *out)
{
	/* Output storage and backend method are both required before dispatch. */
	if ((self == 0) || (self->ops == 0) || (out == 0)) {
		return DRV_SPI_ERR_INVALID;
	}
	if (self->ops->get_stats == 0) {
		return DRV_SPI_ERR_UNSUPPORTED;
	}
	return self->ops->get_stats(self, out);
}
