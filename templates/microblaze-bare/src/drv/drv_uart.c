//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: drv_uart
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: 控制台串口驱动实现：XUartLite 字节收发
// Revision: 0.02
// Revision 0.01 - File Created
// Revision 0.02 - 初始化失败显式映射为驱动域返回码
// Keywords: xuartlite-instance, driver-rc, device-id-from-xparameters, rx-nonblocking/tx-blocking, intr-id-query, receive-interrupt-enabled
//////////////////////////////////////////////////////////////////////////////////
#include "xparameters.h"
#include "xuartlite.h"
#include "xuartlite_l.h"

#include "drv_uart.h"
#include "cfg_board.h"

static XUartLite Uart; /* 串口实例（系统控制台） */

/**
 * @brief  初始化 AXI UARTLite 控制台实例并使能接收中断。
 *
 * 设备 ID 取自 BSP 的 xparameters.h；只负责实例初始化与中断使能，
 * ISR 由使用方（mw_console）经 plt_int_connect 注册到中断控制器。
 *
 * @return DRV_UART_OK / DRV_UART_ERR_IO。
 * @pre 若使用中断，调用方须先完成中断控制器初始化（init_intr）。
 * @author Xue Weisen
 */
drv_uart_rc_e drv_uart_init(void)
{
	int status;

	/* 实例阶段：设备 ID 或配置失败时不能继续打开中断。 */
	/* Vitis 2025.2 SDT 用基地址，classic BSP 用 DeviceId；差异集中在板级配置。 */
#ifdef SDT
	status = XUartLite_Initialize(&Uart, CFG_CONSOLE_UART_BASEADDR);
#else
	status = XUartLite_Initialize(&Uart, CFG_CONSOLE_UART_DEVICE_ID);
#endif
	if (status != XST_SUCCESS) {
		return DRV_UART_ERR_IO;
	}
	XUartLite_EnableInterrupt(&Uart); /* 使能接收中断 */
	return DRV_UART_OK;
}

/**
 * @brief  非阻塞读 1 字节。
 *
 * 先查 RX FIFO 有效位，有数据才读；无数据立即返回 -1，不等待。
 *
 * @return 0~255 = 读到的字节；-1 = 当前无数据。
 * @note 可在 ISR 中调用；不阻塞。
 * @author Xue Weisen
 */
int drv_uart_read_byte(void)
{
	u32 isr_status = XUartLite_ReadReg(Uart.RegBaseAddress, XUL_STATUS_REG_OFFSET);
	if (isr_status & XUL_SR_RX_FIFO_VALID_DATA)
	{
		return (int)XUartLite_ReadReg(Uart.RegBaseAddress, XUL_RX_FIFO_OFFSET);
	}
	return -1;
}

/**
 * @brief  发送 1 字节（阻塞直到 TX FIFO 可写）。
 *
 * 转调 XUartLite_SendByte；FIFO 满时在驱动内自旋等待，因此不可在 ISR 中调用。
 *
 * @param b 待发送字节。
 * @author Xue Weisen
 */
void drv_uart_send_byte(u8 b)
{
	XUartLite_SendByte(Uart.RegBaseAddress, b);
}

/**
 * @brief  返回 UART 中断号，供 plt_int_connect/enable 使用。
 *
 * 数值来自 BSP 的 xparameters.h；换平台或改 BD 后编号可能变化，须核对。
 *
 * @return UART 接收中断号。
 * @author Xue Weisen
 */
u32 drv_uart_intr_id(void)
{
	return CFG_CONSOLE_UART_IRQ;
}
