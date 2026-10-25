//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: mw_console
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: 系统控制台接口：接收队列、任务读取与丢字节观测
// Revision: 0.03
// Revision 0.01 - File Created
// Revision 0.02 - 公共接口改用毫秒 timeout 和不透明 OSAL 队列
// Revision 0.03 - 控制台返回码按域定义，读取改为状态码+输出参数，补充丢字节计数
// Keywords: console-api, queue-backed-rx, explicit-rc, timeout-ms, status-plus-output, drop-counter, lifecycle, no-freertos-type
//////////////////////////////////////////////////////////////////////////////////
#ifndef MW_CONSOLE_H
#define MW_CONSOLE_H

#include "common_types.h"

/** 控制台域返回码；不泄漏 OSAL/FreeRTOS 状态值。 */
typedef enum {
	MW_CONSOLE_OK = 0,           /**< success */
	MW_CONSOLE_TIMEOUT = 1,      /**< no byte within the bounded wait */
	MW_CONSOLE_ERR_INVALID = -1, /**< invalid argument */
	MW_CONSOLE_ERR_OS = -2,      /**< queue/OS resource failure */
	MW_CONSOLE_ERR_STATE = -3    /**< module not initialized or hardware failure */
} mw_console_rc_e;

/**
 * @brief 以事务方式初始化 UART、接收队列和中断路径。
 *
 * @return MW_CONSOLE_OK / MW_CONSOLE_ERR_OS / MW_CONSOLE_ERR_STATE。
 * @pre 平台和 OSAL 已初始化。
 * @note ISR 只排空硬件 FIFO 并投递字节，不执行打印。
 * @author Xue Weisen
 */
mw_console_rc_e mw_console_init(void);

/**
 * @brief 停止控制台并释放队列资源。
 *
 * @return MW_CONSOLE_OK / MW_CONSOLE_ERR_OS / MW_CONSOLE_ERR_STATE。
 * @pre 控制台任务已停止，且没有生产者并发投递或读取。
 * @note 删除队列前先禁用 UART 中断，避免 ISR 访问已释放队列。
 * @author Xue Weisen
 */
mw_console_rc_e mw_console_deinit(void);

/**
 * @brief 通过 BSP UART 后端格式化并输出字符串。
 *
 * @param fmt printf 风格格式串；必须非 NULL。
 * @note 阻塞且不可重入；不得从 ISR 调用。专用日志服务引入前，多任务调用
 *       方必须外部串行化。
 * @author Xue Weisen
 */
void uart_print(const char *fmt, ...);

/**
 * @brief 从接收队列读取一个字节。
 *
 * @param out 输出字节；必须非 NULL。
 * @param timeout_ms 有限毫秒等待；0 表示非阻塞。
 * @return MW_CONSOLE_OK / MW_CONSOLE_TIMEOUT / MW_CONSOLE_ERR_INVALID /
 *         MW_CONSOLE_ERR_OS / MW_CONSOLE_ERR_STATE。
 * @note 仅限任务上下文；只在 MW_CONSOLE_OK 时写输出。
 * @author Xue Weisen
 */
mw_console_rc_e mw_console_getc(u8 *out, u32 timeout_ms);

/**
 * @brief 返回软件接收队列满导致的累计丢字节数。
 *
 * @return 累计丢弃字节数。
 * @note 只由单一 UART ISR 生产者递增。
 * @author Xue Weisen
 */
u32 mw_console_rx_drop_count(void);

#endif /* MW_CONSOLE_H */
