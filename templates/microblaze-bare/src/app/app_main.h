//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: app_main
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: 应用启动与主轮询接口
// Revision: 0.02
// Revision 0.01 - File Created
// Revision 0.02 - 启动改为分阶段返回码，删除可变初始化全局
// Keywords: app-boot-api, boot-stage-rc, fail-step-readonly, poll-entry, no-mutable-extern
//////////////////////////////////////////////////////////////////////////////////
#ifndef APP_MAIN_H
#define APP_MAIN_H

#include "common_types.h"

/** 应用启动返回码。 */
typedef enum {
	APP_BOOT_OK = 0,           /**< ready to poll */
	APP_BOOT_ERR_INT = -1,     /**< interrupt/time platform failed */
	APP_BOOT_ERR_CONSOLE = -2, /**< UART/console failed */
	APP_BOOT_ERR_BUSINESS = -3 /**< business initialization failed */
} app_boot_rc_e;

/**
 * @brief 初始化中断平台、控制台和业务服务。
 *
 * @return APP_BOOT_OK 或具体阶段的失败码。
 * @pre 平台/cache 已完成初始化。
 * @note 本模板没有调度器；成功启动后进入主轮询。
 * @author Xue Weisen
 */
app_boot_rc_e app_start(void);

/**
 * @brief 返回最近一次启动失败阶段名称。
 *
 * @return 静态只读字符串；NULL 表示没有失败。
 * @note 返回指针不得释放或修改。
 * @author Xue Weisen
 */
const char *app_boot_fail_step(void);

/**
 * @brief 执行一次非阻塞应用轮询。
 *
 * @note 阻塞时间不得超过一个定时周期。
 * @author Xue Weisen
 */
void app_poll(void);

#endif /* APP_MAIN_H */
