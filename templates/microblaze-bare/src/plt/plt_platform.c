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
// Keywords: platform-init/cleanup, conditional-cache-enable/disable-on-xpar-microblaze-use
//////////////////////////////////////////////////////////////////////////////////
#include "xparameters.h"
#include "xil_cache.h"

#include "plt_platform.h"

/**
 * @brief  平台初始化：使能指令/数据 cache。
 *
 * 当前只调用 enable_caches；后续时钟、外设复位等平台初始化在此扩展。
 *
 * @note 在 main 中最早调用；寄存器访问走 volatile，不受 cache 影响。
 * @author Xue Weisen
 */
void init_platform()
{
	enable_caches();
}

/**
 * @brief  平台清理：关闭指令/数据 cache，与 init_platform 对称。
 *
 * @note 正常 main 循环不可达；保留以表达资源释放意图，也便于异常路径调用。
 * @author Xue Weisen
 */
void cleanup_platform()
{
	disable_caches();
}

/**
 * @brief  按 BSP 条件编译使能指令/数据 cache。
 *
 * 只有 XPAR_MICROBLAZE_USE_ICACHE / XPAR_MICROBLAZE_USE_DCACHE 存在时才使能；
 * PPC 走 region 使能路径。BSP 未开 cache 时本函数等效为空操作。
 *
 * @author Xue Weisen
 */
void enable_caches()
{
#ifdef __PPC__
	Xil_ICacheEnableRegion(CACHEABLE_REGION_MASK);
	Xil_DCacheEnableRegion(CACHEABLE_REGION_MASK);
#elif __MICROBLAZE__
#ifdef XPAR_MICROBLAZE_USE_ICACHE
	Xil_ICacheEnable();
#endif
#ifdef XPAR_MICROBLAZE_USE_DCACHE
	Xil_DCacheEnable();
#endif
#endif
}

/**
 * @brief  按 BSP 条件编译关闭指令/数据 cache，与 enable_caches 对称。
 *
 * @author Xue Weisen
 */
void disable_caches()
{
#ifdef __MICROBLAZE__
#ifdef XPAR_MICROBLAZE_USE_DCACHE
	Xil_DCacheDisable();
#endif
#ifdef XPAR_MICROBLAZE_USE_ICACHE
	Xil_ICacheDisable();
#endif
#endif
}
