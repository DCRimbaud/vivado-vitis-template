//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: main
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: 程序入口：平台初始化、应用启动与轮询循环
// Revision: 0.02
// Revision 0.01 - File Created
// Revision 0.02 - 通过 plt_console 隔离 xil_printf
// Keywords: compositional-root, init_platform->app_start->while(app_poll), microblaze-bare-metal-entry, cache-conditional-on-xpar-microblaze-use, non-returning
//////////////////////////////////////////////////////////////////////////////////
#include "plt_console.h"

#include "plt_platform.h"
#include "app_main.h"

/**
 * @brief  MicroBlaze 裸机组合根：平台初始化、应用启动与主轮询。
 *
 * 无 FSBL 阶段，BSP 的 _start 完成 BSS/栈初始化后直接进入本函数；
 * init_platform 按 XPAR_MICROBLAZE_USE_* 条件使能 cache，随后线性执行
 * app_start，成功则进入 while(1) app_poll。
 *
 * @warning 正常路径不返回；只有在 app_start 失败时才 cleanup_platform 并返回 1。
 * @author Xue Weisen
 */
int main(void)
{
	init_platform();

	plt_console_printf("\r\nMicroBlaze bare-metal boot.\r\n");

	if (app_start() != APP_BOOT_OK)
	{
		cleanup_platform();
		return 1;
	}

	while (1)
	{
		app_poll();
	}

	cleanup_platform();   /* 正常不可达；保留以对称表达资源释放意图 */
	return 0;
}
