//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: drv_snapshot_window_mmio
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: 快照窗口的 plt_mmio 只读方法表
// Revision: 0.01
// Revision: 0.01 - File Created
// Keywords: snapshot-mmio-adapter, plt-mmio-read32, const-ops-object
//////////////////////////////////////////////////////////////////////////////////
#include "plt_mmio.h"
#include "drv_snapshot_window.h"

/**
 * @brief 用 plt_mmio_read32 读取一个窗口字。
 *
 * @param ctx 未使用的上下文；保留统一回调签名。
 * @param addr 目标地址；不得为 0。
 * @param value 输出值；只在成功时写入。
 * @return SNAPSHOT 域返回码。
 * @note 可在任务或 ISR 中调用；本函数只做一次 volatile 读。
 * @author Xue Weisen
 */
static drv_snapshot_rc_e drv_snapshot_mmio_read32(void *ctx,
                                                  uintptr_t addr,
                                                  u32 *value)
{
	(void)ctx;
	if ((addr == 0u) || (value == 0)) {
		return DRV_SNAPSHOT_ERR_INVALID;
	}
	*value = plt_mmio_read32(addr);
	return DRV_SNAPSHOT_OK;
}

const drv_snapshot_io_ops_t drv_snapshot_mmio_ops = {
	drv_snapshot_mmio_read32
};
