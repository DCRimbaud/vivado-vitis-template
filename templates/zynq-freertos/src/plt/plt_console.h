//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: plt_console
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: 平台控制台输出公共接口，隔离 BSP printf 细节
// Revision: 0.01
// Revision 0.01 - File Created
// Keywords: platform-console, varargs, xil-printf-isolation, no-bsp-type-in-mw
//////////////////////////////////////////////////////////////////////////////////
#ifndef PLT_CONSOLE_H
#define PLT_CONSOLE_H

#include <stdarg.h>

/**
 * @brief 通过平台控制台输出格式化字符串。
 *
 * @param fmt printf 风格格式串；必须非 NULL。
 * @note 阻塞且不可重入；调用方负责串行化，禁止在 ISR 中调用。
 * @author Xue Weisen
 */
void plt_console_printf(const char *fmt, ...);

/**
 * @brief 通过平台控制台输出已展开的 varargs。
 *
 * @param fmt printf 风格格式串；必须非 NULL。
 * @param ap 已由调用方 va_start 展开的参数列表。
 * @note 阻塞且不可重入；禁止在 ISR 中调用。
 * @author Xue Weisen
 */
void plt_console_vprintf(const char *fmt, va_list ap);

#endif /* PLT_CONSOLE_H */
