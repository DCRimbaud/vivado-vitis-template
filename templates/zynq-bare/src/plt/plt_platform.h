//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: plt_platform
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: 平台层接口：cache 使能与平台初始化、清理
// Revision: 0.01
// Revision 0.01 - File Created
// Keywords: platform-api-header, init_platform/cleanup_platform/enable_caches/disable_caches
//////////////////////////////////////////////////////////////////////////////////
#ifndef PLT_PLATFORM_H
#define PLT_PLATFORM_H

#include "common_types.h"

/**
 * @brief  平台初始化（当前仅使能 cache；时钟/外设复位在此扩展）。
 *
 * @author Xue Weisen
 */
void init_platform(void);

/**
 * @brief  平台清理（关闭 cache，与 init_platform 对称）。
 *
 * @author Xue Weisen
 */
void cleanup_platform(void);

/**
 * @brief  按 BSP 条件编译使能 cache。
 *
 * @author Xue Weisen
 */
void enable_caches(void);

/**
 * @brief  按 BSP 条件编译关闭 cache。
 *
 * @author Xue Weisen
 */
void disable_caches(void);

#endif /* PLT_PLATFORM_H */
