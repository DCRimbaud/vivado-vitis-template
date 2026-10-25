//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: app_main
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: 应用入口实现：控制台初始化与任务创建
// Revision: 0.02
// Revision 0.01 - File Created
// Revision 0.02 - 改用 plt_rtos 句柄和毫秒等待，移除 FreeRTOS 类型
// Keywords: app-startup-linear, console-init-status, osal-task-create, now-ms-heartbeat, no-freertos-include
//////////////////////////////////////////////////////////////////////////////////
#include "app_main.h"
#include "mw_console.h"
#include "plt_rtos.h"
#include "cfg_rtos.h"

static plt_rtos_task_t s_console_task;
static const char *s_boot_fail;

/**
 * @brief 控制台任务示例：回显输入并周期输出心跳。
 *
 * @param ctx 任务参数；本示例未使用。
 * @warning 本函数是无限循环，正常不返回。
 * @author Xue Weisen
 */
static void app_task_console(void *ctx)
{
	u32 last_ms;
	u32 now_ms;
	u8 ch;
	mw_console_rc_e console_rc;

	(void)ctx;
	last_ms = plt_rtos_now_ms();
	uart_print("console task started.\r\n");

	/* 任务阶段：有字符就回显，没有字符则按 100 ms 超时继续运行。 */
	for (;;) {
		console_rc = mw_console_getc(&ch, 100u);
		if (console_rc == MW_CONSOLE_OK) {
			uart_print("%c", (char)ch);
		}

		/* 心跳阶段：用无符号差值兼容 32 bit 时间回绕。 */
		now_ms = plt_rtos_now_ms();
		if ((u32)(now_ms - last_ms) >= 1000u) {
			last_ms = now_ms;
			uart_print("alive %u ms\r\n", (unsigned)now_ms);
		}
	}
}

/**
 * @brief 应用启动：初始化控制台并创建控制台任务。
 *
 * 每一步失败都停止后续初始化并返回 -1；调度器由 main 统一启动。
 *
 * @return 0 = 引导完成；-1 = 控制台或任务创建失败。
 * @pre 调用方已完成平台初始化和 plt_rtos_init()。
 * @author Xue Weisen
 */
int app_start(void)
{
	mw_console_rc_e console_rc;
	plt_rtos_status_e st;

	/* 控制台阶段：队列和 UART ISR 必须成功，否则任务无法可靠收字节。 */
	console_rc = mw_console_init();
	if (console_rc != MW_CONSOLE_OK) {
		s_boot_fail = "console";
		return -1;
	}

	/* 任务阶段：只创建一个控制台任务，失败把原因留给调试器观测。 */
	st = plt_rtos_task_create(app_task_console,
	                          "console",
	                          0,
	                          RTOS_TASK_CONSOLE_STACK_BYTES,
	                          PLT_RTOS_PRIO_NORMAL,
	                          &s_console_task);
	if (st != PLT_RTOS_OK) {
		s_boot_fail = "console_task";
		return -1;
	}

	return 0;
}
