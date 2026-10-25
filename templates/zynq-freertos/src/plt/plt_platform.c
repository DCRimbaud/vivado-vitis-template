//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: plt_platform
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: 平台层实现：cache 使能与平台初始化、清理
// Revision: 0.01
// Revision 0.01 - File Created
// Keywords: platform-init/cleanup, a9-cache-enable/disable-unconditional
//////////////////////////////////////////////////////////////////////////////////
#include "xparameters.h"
#include "xil_cache.h"

#include "plt_platform.h"

/**
 * @brief  平台初始化：使能 A9 指令/数据 cache。
 *
 * @note 在 main 中最早调用；寄存器访问走 volatile，不受 cache 影响。
 * @author Xue Weisen
 */
void init_platform()
{
	enable_caches();
}

/**
 * @brief  平台清理：关闭 A9 指令/数据 cache，与 init_platform 对称。
 *
 * @author Xue Weisen
 */
void cleanup_platform()
{
	disable_caches();
}

/**
 * @brief  使能 Cortex-A9 指令/数据 cache。
 *
 * @author Xue Weisen
 */
void enable_caches()
{
	Xil_ICacheEnable();
	Xil_DCacheEnable();
}

/**
 * @brief  关闭 Cortex-A9 指令/数据 cache，与 enable_caches 对称。
 *
 * @author Xue Weisen
 */
void disable_caches()
{
	Xil_DCacheDisable();
	Xil_ICacheDisable();
}
