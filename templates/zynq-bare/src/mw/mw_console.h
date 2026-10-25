//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: mw_console
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: 系统控制台接口：初始化、打印与 ISR 回调注册
// Revision: 0.03
// Revision 0.01 - File Created
// Revision 0.02 - 接收回调增加 ctx 参数
// Revision 0.03 - 初始化改为显式返回码，硬件连接/使能失败不再被吞
// Keywords: console-api, explicit-rc, init-status, isr-callback-ctx, bounded-isr, read-only-registration, no-mutable-extern
//////////////////////////////////////////////////////////////////////////////////
#ifndef MW_CONSOLE_H
#define MW_CONSOLE_H

#include "common_types.h"

/** 控制台域返回码。 */
typedef enum {
	MW_CONSOLE_OK = 0,           /**< success */
	MW_CONSOLE_ERR_INVALID = -1, /**< invalid argument */
	MW_CONSOLE_ERR_OS = -2,      /**< reserved for RTOS-backed variants */
	MW_CONSOLE_ERR_STATE = -3    /**< driver/IRQ initialization failed */
} mw_console_rc_e;

/** 在 ISR 上下文执行的接收回调；必须短小且不得阻塞。 */
typedef void (*mw_console_rx_fn)(u8 ch, void *ctx);

/**
 * @brief 初始化 UART、连接 ISR 并使能中断。
 *
 * @return MW_CONSOLE_OK / MW_CONSOLE_ERR_STATE。
 * @pre plt_int_init() 已成功。
 * @note 初始化具备事务语义：Connect/Enable 失败会报告给组合根，不会留下
 *       半可用控制台。
 * @author Xue Weisen
 */
mw_console_rc_e mw_console_init(void);

/**
 * @brief 通过 BSP UART 后端格式化并输出。
 *
 * @param fmt printf 风格格式串；必须非 NULL。
 * @note 阻塞且不可重入；不得从 ISR 调用。
 * @author Xue Weisen
 */
void uart_print(const char *fmt, ...);

/**
 * @brief 注册接收字节回调。
 *
 * @param fn 回调；NULL 表示丢弃收到的字节。
 * @param ctx 回调上下文；生命周期覆盖注册期。
 * @note 回调运行在 ISR 上下文。注册是命令处理的放行门，必须在所有依赖服务
 *       就绪后再注册。
 * @author Xue Weisen
 */
void mw_console_set_rx_handler(mw_console_rx_fn fn, void *ctx);

#endif /* MW_CONSOLE_H */
