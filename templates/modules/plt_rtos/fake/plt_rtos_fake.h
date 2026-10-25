//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: plt_rtos_fake
// Target Devices: host only
// Tool Versions: host compiler
// Description: host 测试后端的显式时钟控制接口
// Revision: 0.01
// Revision 0.01 - File Created
// Keywords: host-test-only, explicit-clock, deterministic-timeout, no-scheduler
//////////////////////////////////////////////////////////////////////////////////
#ifndef PLT_RTOS_FAKE_H
#define PLT_RTOS_FAKE_H

#include "plt_rtos.h"

/**
 * @brief 设置 fake 单调毫秒时钟。
 *
 * @param now_ms 新的逻辑时间。
 * @note 仅 host 测试使用；不得在目标固件构建中包含本头。
 * @author Xue Weisen
 */
void plt_rtos_fake_set_now_ms(u32 now_ms);

/**
 * @brief 推进 fake 单调毫秒时钟。
 *
 * @param delta_ms 增量；按 u32 自然回绕。
 * @note 仅 host 测试使用；不得在目标固件构建中包含本头。
 * @author Xue Weisen
 */
void plt_rtos_fake_advance_ms(u32 delta_ms);

#endif /* PLT_RTOS_FAKE_H */
