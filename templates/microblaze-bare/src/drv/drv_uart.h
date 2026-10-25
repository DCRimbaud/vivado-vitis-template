//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: drv_uart
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: 控制台串口驱动接口：初始化、字节收发与中断号查询
// Revision: 0.02
// Revision 0.01 - File Created
// Revision 0.02 - 初始化改为驱动域返回码，避免裸露 XST 状态
// Keywords: uart-api-header, driver-rc, init/read_byte/send_byte/intr_id-decls, axi-uartlite
//////////////////////////////////////////////////////////////////////////////////
#ifndef DRV_UART_H
#define DRV_UART_H

#include "common_types.h"

/** UART driver result domain. */
typedef enum {
	DRV_UART_OK = 0,          /**< success */
	DRV_UART_ERR_IO = -1,     /**< BSP/device initialization failed */
	DRV_UART_ERR_STATE = -2   /**< invalid BSP configuration */
} drv_uart_rc_e;

/**
 * @brief  初始化 XUartLite 并使能接收中断。
 *
 * @return DRV_UART_OK / DRV_UART_ERR_IO / DRV_UART_ERR_STATE。
 * @pre 使用中断时须先初始化中断控制器。
 * @author Xue Weisen
 */
drv_uart_rc_e drv_uart_init(void);

/**
 * @brief  非阻塞读 1 字节。
 *
 * @return 0~255 = 读到的字节；-1 = 无数据。
 * @author Xue Weisen
 */
int  drv_uart_read_byte(void);

/**
 * @brief  阻塞发送 1 字节。
 *
 * @param b 待发送字节。
 * @note 不可在 ISR 中调用。
 * @author Xue Weisen
 */
void drv_uart_send_byte(u8 b);

/**
 * @brief  返回 UART 中断号（来自 xparameters.h）。
 *
 * @return 供 plt_int_connect/enable 使用的中断号。
 * @author Xue Weisen
 */
u32  drv_uart_intr_id(void);

#endif /* DRV_UART_H */
