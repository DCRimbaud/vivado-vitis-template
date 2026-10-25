//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: plt_int
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: 平台中断公共接口
// Revision: 0.02
// Revision 0.01 - File Created
// Revision 0.02 - 改为转发编译期 OSAL，不再向业务层暴露 FreeRTOS 类型
// Keywords: interrupt-api, osal-forward, handler-ctx, no-freertos-type
//////////////////////////////////////////////////////////////////////////////////
#ifndef PLT_INT_H
#define PLT_INT_H

#include "common_types.h"
#include "plt_rtos.h"

typedef plt_rtos_isr_fn plt_int_handler_fn;

/**
 * @brief 安装应用中断。
 *
 * @param intr_id 中断号。
 * @param handler ISR 函数；不可为 NULL。
 * @param ctx 上下文；生命周期覆盖注册期。
 * @return PLT_RTOS_OK / PLT_RTOS_ERR_INVALID / PLT_RTOS_ERR_STATE。
 * @pre 调用方已完成平台和 OSAL 初始化。
 * @author Xue Weisen
 */
plt_rtos_status_e plt_int_connect(u32 intr_id,
                                  plt_int_handler_fn handler,
                                  void *ctx);

/**
 * @brief 使能应用中断。
 *
 * @param intr_id 中断号。
 * @return PLT_RTOS_OK / PLT_RTOS_ERR_INVALID。
 * @author Xue Weisen
 */
plt_rtos_status_e plt_int_enable(u32 intr_id);

/**
 * @brief 禁用应用中断。
 *
 * @param intr_id 中断号。
 * @return PLT_RTOS_OK / PLT_RTOS_ERR_INVALID。
 * @author Xue Weisen
 */
plt_rtos_status_e plt_int_disable(u32 intr_id);

#endif /* PLT_INT_H */
