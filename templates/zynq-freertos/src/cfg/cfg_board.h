//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: cfg_board
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: 板级参数配置：计时器槽位与工程规格宏
// Revision: 0.02
// Revision 0.01 - File Created
// Revision 0.02 - 增加 Vitis 2025.2 SDT/classic 板级别名，消除驱动内宏硬编码
// Keywords: board-constants, Vitis-2025.2, SDT-and-classic, uart-base/irq, freertos-bsp, timer-slots, project-spec-macro-placeholders
//////////////////////////////////////////////////////////////////////////////////
#ifndef CFG_BOARD_H
#define CFG_BOARD_H

#include "xparameters.h"

/* ---- 计时器槽位 ---- */
#define TIC_NUM             8    /* 计时器槽位总数 */
#define TIC_GLOBAL          0    /* 全局计时器 */

/* ---- 控制台 UART（Vitis 2025.2：SDT 用 BASEADDR，classic 用 DEVICE_ID） ---- */
#ifdef SDT
#define CFG_CONSOLE_UART_BASEADDR   XPAR_XUARTPS_0_BASEADDR
#else
#define CFG_CONSOLE_UART_DEVICE_ID  XPAR_XUARTPS_0_DEVICE_ID
#endif

#if defined(XPAR_XUARTPS_0_INTERRUPT_INTR)
#define CFG_CONSOLE_UART_IRQ        XPAR_XUARTPS_0_INTERRUPT_INTR
#else
/* FreeRTOS 项目的 GIC 由 BSP port 管理；UART IRQ 号仍必须与 BD 一致。 */
#error "Set CFG_CONSOLE_UART_IRQ to the GIC IRQ used by the console UART"
#endif

/* ---- 工程规格宏（按需补充，示例）----
 * #define FMC_NUM         5     // FMC 插槽数量
 * #define ADC_NUM_ACTIVE  10    // 实际使用的 ADC 芯片数量
 * #define USER_BRAM_BYTES (32 * 512)          // PL 共享内存字节数
 * #define USER_BRAM_SAMPLES (USER_BRAM_BYTES / 2)
 */

#endif /* CFG_BOARD_H */
