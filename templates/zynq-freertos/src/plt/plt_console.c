//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: plt_console
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: 平台控制台实现，封装 BSP xil_printf
// Revision: 0.01
// Revision 0.01 - File Created
// Keywords: platform-console-impl, xil-printf, varargs, output-adapter
//////////////////////////////////////////////////////////////////////////////////
#include <stdarg.h>

#include "xil_printf.h"
#include "plt_console.h"

/**
 * @brief 通过平台控制台输出格式化字符串。
 *
 * @param fmt printf 风格格式串；NULL 时直接返回。
 * @note xil_printf 的输出设备由 BSP stdout 配置决定；本函数不初始化 UART。
 * @author Xue Weisen
 */
void plt_console_printf(const char *fmt, ...)
{
	va_list ap;

	if (fmt == 0) {
		return;
	}
	va_start(ap, fmt);
	xil_vprintf(fmt, ap);
	va_end(ap);
}

/**
 * @brief 通过平台控制台输出已展开的 varargs。
 *
 * @param fmt printf 风格格式串；NULL 时直接返回。
 * @param ap 已由调用方展开的参数列表。
 * @note 本函数不接管 ap 的生命周期，调用方仍负责 va_end。
 * @author Xue Weisen
 */
void plt_console_vprintf(const char *fmt, va_list ap)
{
	if (fmt == 0) {
		return;
	}
	xil_vprintf(fmt, ap);
}
