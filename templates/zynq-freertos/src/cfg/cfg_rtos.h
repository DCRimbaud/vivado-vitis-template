//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: cfg_rtos
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: RTOS 参数：tick 频率、任务优先级与栈、队列深度
// Revision: 0.02
// Revision 0.01 - File Created
// Revision 0.02 - 删除 BSP tick 的重复事实源，栈改字节并由 OSAL 映射
// Keywords: freertos-bsp-tuning, no-tick-duplicate, task-stack-bytes, queue-depths, no-portmax-delay
//////////////////////////////////////////////////////////////////////////////////
#ifndef CFG_RTOS_H
#define CFG_RTOS_H

/* 控制台任务：栈按字节配置，由 OSAL 后端按 StackType_t 向上取整；
 * 依据：xil_printf 格式化 + 队列接收的调用深度，留 2 倍余量（估）。 */
#define RTOS_TASK_CONSOLE_STACK_BYTES 4096u

/* 控制台接收队列深度（字节）：容纳一次连续输入/粘贴的突发量（来源：骨架估算） */
#define RTOS_CONSOLE_RXQ_LEN    32u

#endif /* CFG_RTOS_H */
