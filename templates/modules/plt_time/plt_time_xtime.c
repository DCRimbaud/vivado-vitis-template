//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: plt_time_xtime
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: XTime_GetTime 到统一微秒时间源的适配
// Revision: 0.01
// Revision: 0.01 - File Created
// Keywords: xtime-gettime, counts-to-us, vendor-header-in-plt-only
//////////////////////////////////////////////////////////////////////////////////
#include "xtime_l.h"

#include "plt_time_xtime.h"

/**
 * @brief 读取 XTime 全局计数器并换算为微秒。
 *
 * @param ctx 未使用的上下文。
 * @param now_us 输出微秒时间。
 * @return PLT_TIME 域返回码。
 * @note 依赖 BSP 的 COUNTS_PER_SECOND；换算只做一次 64 bit 乘除。
 * @author Xue Weisen
 */
static plt_time_rc_e plt_time_xtime_read_us(void *ctx, u64 *now_us)
{
	XTime count;

	(void)ctx;
	if (now_us == 0) {
		return PLT_TIME_ERR_INVALID;
	}
	XTime_GetTime(&count);
	*now_us = ((u64)count * 1000000u) / (u64)COUNTS_PER_SECOND;
	return PLT_TIME_OK;
}

/**
 * @brief 把 XTime 全局计数器绑定到统一时间对象。
 *
 * @param self 调用方拥有的时间对象。
 * @return PLT_TIME 域返回码。
 * @author Xue Weisen
 */
int plt_time_xtime_bind(plt_time_t *self)
{
	return (int)plt_time_init(self, plt_time_xtime_read_us, 0);
}
