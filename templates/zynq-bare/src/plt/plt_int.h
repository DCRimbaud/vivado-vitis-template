//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: plt_int
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: 平台中断、时基与外部事件计数接口
// Revision: 0.02
// Revision 0.01 - File Created
// Revision 0.02 - 初始化/连接/使能显式返回错误码，删除公共可变全局，改为只读访问器
// Keywords: interrupt-api, explicit-rc, handler-ctx, time-tick, monotonic-second, external-event-count, no-mutable-extern, scugic-scutimer
//////////////////////////////////////////////////////////////////////////////////
#ifndef PLT_INT_H
#define PLT_INT_H

#include "common_types.h"

/** 中断/时基平台返回码。 */
typedef enum {
	PLT_INT_OK = 0,           /**< success */
	PLT_INT_ERR_INVALID = -1, /**< invalid argument/configuration */
	PLT_INT_ERR_STATE = -2,   /**< wrong initialization order */
	PLT_INT_ERR_IO = -3,      /**< BSP call failed */
	PLT_INT_UNSUPPORTED = -4  /**< BSP has no required interrupt/time block */
} plt_int_rc_e;

/** 应用 ISR 签名；ctx 生命周期覆盖注册期。 */
typedef void (*plt_int_handler_fn)(void *ctx);

/**
 * @brief 初始化 ScuGic 和 1 ms ScuTimer。
 *
 * @return PLT_INT_OK / PLT_INT_ERR_STATE / PLT_INT_ERR_IO。
 * @pre 平台/cache 已完成初始化。
 * @note 必须在控制台/UART 中断注册前完成。
 * @author Xue Weisen
 */
plt_int_rc_e plt_int_init(void);

/**
 * @brief 连接一个应用 ISR。
 *
 * @param intr_id BSP 中断号。
 * @param handler ISR 回调；必须非 NULL。
 * @param ctx 回调上下文；生命周期覆盖注册期。
 * @return PLT_INT_OK / PLT_INT_ERR_INVALID / PLT_INT_ERR_STATE / PLT_INT_ERR_IO。
 * @note ISR 运行在中断上下文，执行时间必须有界。
 * @author Xue Weisen
 */
plt_int_rc_e plt_int_connect(u32 intr_id,
                             plt_int_handler_fn handler,
                             void *ctx);

/**
 * @brief 使能一个已连接的 GIC 中断。
 *
 * @param intr_id BSP 中断号。
 * @return PLT_INT_OK / PLT_INT_ERR_INVALID / PLT_INT_ERR_STATE。
 * @author Xue Weisen
 */
plt_int_rc_e plt_int_enable(u32 intr_id);

/**
 * @brief 禁用一个 GIC 中断。
 *
 * @param intr_id BSP 中断号。
 * @return PLT_INT_OK / PLT_INT_ERR_INVALID / PLT_INT_ERR_STATE。
 * @author Xue Weisen
 */
plt_int_rc_e plt_int_disable(u32 intr_id);

/**
 * @brief 查询并清除 1 ms 标志。
 *
 * @return 1 = due; 0 = not due.
 * @author Xue Weisen
 */
u32 is_timer_1ms(void);

/**
 * @brief 查询并清除 10 ms 标志。
 *
 * @return 1 = due; 0 = not due.
 * @author Xue Weisen
 */
u32 is_timer_10ms(void);

/**
 * @brief 查询并清除 100 ms 标志。
 *
 * @return 1 = due; 0 = not due.
 * @author Xue Weisen
 */
u32 is_timer_100ms(void);

/**
 * @brief 查询并清除 1 s 标志。
 *
 * @return 1 = due; 0 = not due.
 * @author Xue Weisen
 */
u32 is_timer_1s(void);

/**
 * @brief 读取单调秒计数。
 *
 * @return 启动后的秒数；32 bit 自然回绕。
 * @note 只读访问器；调用方使用无符号减法计算差值。
 * @author Xue Weisen
 */
u32 plt_int_now_s(void);

/**
 * @brief 读取累计外部中断事件数。
 *
 * @return 事件计数；32 bit 自然回绕。
 * @note 使用累计计数可避免 ISR 与主循环之间的读清竞态。
 * @author Xue Weisen
 */
u32 plt_int_ext_event_count(void);

/**
 * @brief 从应用 ISR 记录一次外部中断事件。
 *
 * @note ISR 安全、有界且不分配内存。
 * @author Xue Weisen
 */
void plt_int_note_ext_event(void);

/**
 * @brief 捕获一个软定时器槽位。
 *
 * @param index 槽位索引；非法返回 0。
 * @return 当前秒数。
 * @author Xue Weisen
 */
u32 tic(u32 index);

/**
 * @brief 返回自 tic() 以来的经过秒数。
 *
 * @param index 槽位索引；非法返回 0。
 * @return 经过秒数；32 bit 自然回绕。
 * @author Xue Weisen
 */
u32 toc(u32 index);

#endif /* PLT_INT_H */
