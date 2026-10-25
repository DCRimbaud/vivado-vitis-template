//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: main
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: 程序入口：平台初始化、OSAL 初始化、应用启动与调度启动
// Revision: 0.03
// Revision 0.01 - File Created
// Revision 0.02 - 移除直接 FreeRTOS 调用，改走 plt_rtos
// Revision 0.03 - 通过 plt_console 隔离 xil_printf
// Keywords: composition-root, platform->osal->app->start, scheduler-backend, failure-retry-guard
//////////////////////////////////////////////////////////////////////////////////
#include "plt_console.h"

#include "plt_platform.h"
#include "plt_rtos.h"
#include "app_main.h"

/**
 * @brief Zynq-7000 PS 组合根：初始化平台、OSAL 和应用，然后启动调度器。
 *
 * 平台和 OSAL 初始化失败时直接返回；应用启动失败时清理平台并返回。
 * 调度器成功启动后本函数不返回。
 *
 * @warning 调度器成功接管后本函数不会返回；only failure path returns.
 * @author Xue Weisen
 */
int main(void)
{
	plt_rtos_status_e st;

	/* 平台阶段：cache 和外设基础配置先就绪。 */
	plt_platform_init();

	/* OSAL 阶段：选择编译期后端，失败不能继续创建任务/队列。 */
	st = plt_rtos_init();
	if (st != PLT_RTOS_OK) {
		plt_console_printf("plt_rtos_init failed: %d\r\n", (int)st);
		return 1;
	}

	plt_console_printf("\r\nZynq-7000 PS FreeRTOS boot.\r\n");

	/* 应用阶段：控制台和任务创建失败直接回收平台资源。 */
	if (app_start() != 0) {
		plt_platform_cleanup();
		return 1;
	}

	/* 调度阶段：成功不返回；返回即说明调度器启动失败。 */
	st = plt_rtos_start();
	plt_console_printf("plt_rtos_start failed: %d\r\n", (int)st);
	plt_platform_cleanup();
	return 1;
}
