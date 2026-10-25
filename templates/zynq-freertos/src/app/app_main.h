//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: app_main
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: 应用入口接口：启动引导与任务创建
// Revision: 0.02
// Revision 0.01 - File Created
// Revision 0.02 - 启动接口改为不依赖具体 RTOS
// Keywords: app-entry, app-start, osal-task-create, scheduler-owned-by-main
//////////////////////////////////////////////////////////////////////////////////
#ifndef APP_MAIN_H
#define APP_MAIN_H

#include "common_types.h"

/**
 * @brief 应用启动：控制台初始化和任务创建。
 *
 * @return 0 = 引导成功，由 main 调用 plt_rtos_start()；-1 = 初始化或任务创建失败。
 * @pre 调用方已完成 plt_platform_init() 和 plt_rtos_init()。
 * @author Xue Weisen
 */
int app_start(void);

#endif /* APP_MAIN_H */
