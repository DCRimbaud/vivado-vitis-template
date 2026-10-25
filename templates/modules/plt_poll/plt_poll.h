//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: plt_poll
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: 基于调用方时间源的超时判定
// Revision: 0.01
// Revision: 0.01 - File Created
// Keywords: platform-inline, deadline, wrap-safe-elapsed, no-blocking
//////////////////////////////////////////////////////////////////////////////////
#ifndef PLT_POLL_H
#define PLT_POLL_H

#include "common_types.h"

/** 单调时间戳起点与超时长度。 */
typedef struct {
	u32 start_ms;   /**< start timestamp */
	u32 timeout_ms; /**< timeout budget */
} plt_deadline_t;

/**
 * @brief 创建超时窗口。
 *
 * @param start_ms 当前单调毫秒时间。
 * @param timeout_ms 超时预算；0 表示立即到期。
 * @return 截止时间对象。
 * @note start_ms 必须是单调时间源的窄化值；本函数不读取系统时间。
 * @author Xue Weisen
 */
static inline plt_deadline_t plt_deadline_start(u32 start_ms, u32 timeout_ms)
{
	plt_deadline_t deadline;

	deadline.start_ms = start_ms;
	deadline.timeout_ms = timeout_ms;
	return deadline;
}

/**
 * @brief 判断超时窗口是否到期。
 *
 * 使用无符号差值比较，兼容 32 bit 单调时间自然回绕。
 *
 * @param deadline 截止时间对象；不可为 NULL。
 * @param now_ms 当前单调毫秒时间。
 * @return 1 = 已到期；0 = 未到期；deadline 为 NULL 时返回 1。
 * @author Xue Weisen
 */
static inline int plt_deadline_expired(const plt_deadline_t *deadline, u32 now_ms)
{
	if (deadline == 0) {
		return 1;
	}
	return (((u32)(now_ms - deadline->start_ms) >= deadline->timeout_ms) ? 1 : 0);
}

#endif /* PLT_POLL_H */
