//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: drv_spi
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: SPI 完整事务端口：配置、全双工传输、取消与统计
// Revision: 0.01
// Revision 0.01 - File Created
// Keywords: spi-transaction, const-ops-self-priv, full-duplex, cs-mode-clock,
//   finite-timeout, cancel, stats, xlnx-adapter-boundary
//////////////////////////////////////////////////////////////////////////////////
#ifndef DRV_SPI_H
#define DRV_SPI_H

#include "common_types.h"

/** SPI 端口返回码。 */
typedef enum {
	DRV_SPI_OK = 0,             /**< success */
	DRV_SPI_ERR_INVALID = -1,   /**< invalid argument or configuration */
	DRV_SPI_ERR_BUSY = -2,      /**< an earlier transaction is still active */
	DRV_SPI_ERR_TIMEOUT = -3,   /**< finite transaction deadline expired */
	DRV_SPI_ERR_IO = -4,        /**< controller or bus error */
	DRV_SPI_ERR_UNSUPPORTED = -5 /**< backend cannot provide the request */
} drv_spi_rc_e;

/** 标准 SPI 时钟模式。 */
typedef enum {
	DRV_SPI_MODE_0 = 0, /**< CPOL=0, CPHA=0 */
	DRV_SPI_MODE_1 = 1, /**< CPOL=0, CPHA=1 */
	DRV_SPI_MODE_2 = 2, /**< CPOL=1, CPHA=0 */
	DRV_SPI_MODE_3 = 3  /**< CPOL=1, CPHA=1 */
} drv_spi_mode_e;

/** 单次事务配置；各字段由后端解释。 */
typedef struct {
	u32 clock_hz;   /**< requested SCLK; must be > 0 */
	u8 mode;        /**< drv_spi_mode_e */
	u8 data_bits;   /**< 8, 16, or 32 */
	u8 lsb_first;   /**< 0 = MSB first, 1 = LSB first */
	u8 cs;          /**< backend chip-select index */
} drv_spi_cfg_t;

/** 一次全双工事务；len > 0 时 tx/rx 都必须有效。 */
typedef struct {
	const u8 *tx;   /**< bytes clocked out */
	u8 *rx;         /**< bytes clocked in; dummy storage for write-only */
	u32 len;        /**< transaction bytes */
	u32 timeout_ms; /**< finite deadline; 0 = backend-specific immediate check */
	const drv_spi_cfg_t *cfg; /**< optional per-transaction configuration */
} drv_spi_xfer_t;

/** 统计快照；除后端明确说明可复位外，计数保持单调。 */
typedef struct {
	u32 transfers;       /**< started transactions */
	u32 completed;       /**< completed transactions */
	u32 io_errors;       /**< controller/bus errors */
	u32 timeouts;        /**< finite deadline expirations */
	u32 bytes_transferred; /**< bytes clocked in either direction */
} drv_spi_stats_t;

typedef struct drv_spi drv_spi_t;

/** 可替换的 SPI 行为；每个方法都收到所属实例。 */
typedef struct {
	drv_spi_rc_e (*configure)(drv_spi_t *self, const drv_spi_cfg_t *cfg);
	drv_spi_rc_e (*transfer)(drv_spi_t *self,
	                         const drv_spi_xfer_t *xfer,
	                         u32 *done);
	drv_spi_rc_e (*cancel)(drv_spi_t *self);
	drv_spi_rc_e (*get_stats)(drv_spi_t *self, drv_spi_stats_t *out);
} drv_spi_ops_t;

/** SPI 对象：只读方法表 + 实现方私有状态。 */
struct drv_spi {
	const drv_spi_ops_t *ops; /**< shared method table */
	void *priv;               /**< backend state; never interpreted by callers */
};

/**
 * @brief 通过方法表绑定并配置一个 SPI 实例。
 *
 * @param self 已绑定对象；ops 和 configure 必须非 NULL。
 * @param cfg 配置参数；必须非 NULL，并在分派前完成公共校验。
 * @return DRV_SPI_OK / DRV_SPI_ERR_INVALID / DRV_SPI_ERR_IO /
 *         DRV_SPI_ERR_UNSUPPORTED。
 * @note 本包装层不分配内存，返回后也不保留 cfg 指针。
 * @author Xue Weisen
 */
drv_spi_rc_e drv_spi_configure(drv_spi_t *self, const drv_spi_cfg_t *cfg);

/**
 * @brief 执行一次全双工 SPI 事务。
 *
 * 包装层先校验事务，再分派给后端。成功且 done 非 NULL 时写入实际字节数；
 * 失败时不写 done。后端必须执行有限超时；timeout_ms=0 只表示立即/非阻塞
 * 检查，绝不表示无限等待。
 *
 * @param self 已绑定 SPI 对象。
 * @param xfer 事务视图；len > 0 时 tx/rx 都必须非 NULL。
 * @param done 可选的实际字节数输出；取值范围必须为 0..xfer->len。
 * @return DRV_SPI_OK / DRV_SPI_ERR_INVALID / DRV_SPI_ERR_BUSY /
 *         DRV_SPI_ERR_TIMEOUT / DRV_SPI_ERR_IO / DRV_SPI_ERR_UNSUPPORTED。
 * @note 除非后端明确声明可重入，否则不得从 ISR 调用。
 * @author Xue Weisen
 */
drv_spi_rc_e drv_spi_transfer(drv_spi_t *self,
                              const drv_spi_xfer_t *xfer,
                              u32 *done);

/**
 * @brief 在后端支持取消时中止当前事务。
 *
 * @param self 已绑定 SPI 对象。
 * @return DRV_SPI_OK / DRV_SPI_ERR_INVALID / DRV_SPI_ERR_BUSY /
 *         DRV_SPI_ERR_UNSUPPORTED。
 * @note 取消是尽力而为；调用方仍必须排空/停止后端状态。
 * @author Xue Weisen
 */
drv_spi_rc_e drv_spi_cancel(drv_spi_t *self);

/**
 * @brief 读取后端统计的一致性快照。
 *
 * @param self 已绑定 SPI 对象。
 * @param out 统计输出；只在成功时写入。
 * @return DRV_SPI_OK / DRV_SPI_ERR_INVALID / DRV_SPI_ERR_UNSUPPORTED。
 * @author Xue Weisen
 */
drv_spi_rc_e drv_spi_get_stats(drv_spi_t *self, drv_spi_stats_t *out);

#endif /* DRV_SPI_H */
