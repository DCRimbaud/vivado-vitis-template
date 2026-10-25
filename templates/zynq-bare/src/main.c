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
// Keywords: compositional-root, fsbl-handoff->init_platform->app_start->while(app_poll), zynq-7000-ps-bare-metal, a9-cache-unconditional, non-returning
//////////////////////////////////////////////////////////////////////////////////
#include "plt_console.h"

#include "plt_platform.h"
#include "app_main.h"

/**
 * @brief  Zynq-7000 PS 裸机组合根：平台初始化、应用启动与主轮询。
 *
 * 由 FSBL/BOOT.bin 引导，PS 时钟/MIO/DDR 已由 FSBL 配置，应用运行在 CPU0；
 * init_platform 使能 A9 I/D cache，随后线性执行 app_start，成功则进入
 * while(1) app_poll（1s 节拍源为 ScuTimer）。
 *
 * @warning 正常路径不返回；只有 app_start 失败时才 cleanup_platform 并返回 1。
 * @author Xue Weisen
 */
int main(void)
{
	init_platform();

	plt_console_printf("\r\nZynq-7000 PS bare-metal boot.\r\n");

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
