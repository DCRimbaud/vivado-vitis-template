//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: plt_int
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: 平台中断实现，转发到编译期 OSAL
// Revision: 0.02
// Revision 0.01 - File Created
// Revision 0.02 - FreeRTOS/GIC 细节下沉到 plt_rtos_freertos
// Keywords: interrupt-forward, osal-backend, no-freertos-include
//////////////////////////////////////////////////////////////////////////////////
#include "plt_int.h"

/**
 * @brief 安装应用中断并返回失败可见性。
 *
 * @param intr_id 中断号。
 * @param handler ISR 函数。
 * @param ctx 上下文。
 * @return OSAL 状态码。
 * @author Xue Weisen
 */
plt_rtos_status_e plt_int_connect(u32 intr_id,
                                  plt_int_handler_fn handler,
                                  void *ctx)
{
	/* 中断控制器差异全部由 OSAL 后端处理，本层只保留稳定接口。 */
	return plt_rtos_isr_connect(intr_id, handler, ctx);
}

/**
 * @brief 使能应用中断。
 *
 * @param intr_id 中断号。
 * @return OSAL 状态码。
 * @author Xue Weisen
 */
plt_rtos_status_e plt_int_enable(u32 intr_id)
{
	/* 使能失败必须上抛，调用方不能把未使能中断当成已工作。 */
	return plt_rtos_isr_enable(intr_id);
}

/**
 * @brief 禁用应用中断。
 *
 * @param intr_id 中断号。
 * @return OSAL 状态码。
 * @author Xue Weisen
 */
plt_rtos_status_e plt_int_disable(u32 intr_id)
{
	/* 禁用是幂等操作，后端决定具体返回语义。 */
	return plt_rtos_isr_disable(intr_id);
}
