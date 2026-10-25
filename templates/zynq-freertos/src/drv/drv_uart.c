//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: drv_uart
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: 控制台串口驱动实现：XUartPs 字节收发
// Revision: 0.03
// Revision 0.01 - File Created
// Revision 0.02 - 按 xuartps 实际头文件修正 normal mode 与 TOUT 中断宏
// Revision 0.03 - 初始化失败显式映射为驱动域返回码
// Keywords: xuartps-instance, driver-rc, normal-mode-rxovr/tout, write-1-to-clear-isr, rx-nonblocking/tx-blocking, intr-id-query
//////////////////////////////////////////////////////////////////////////////////
#include "xparameters.h"
#include "xuartps.h"

#include "drv_uart.h"
#include "cfg_board.h"

static XUartPs Uart; /* 串口实例（系统控制台；xil_printf 后端按 BSP stdout 配置） */

/**
 * @brief  初始化 XUartPs 控制台：查配置、CfgInitialize、使能接收中断源。
 *
 * ISR 由使用方（mw_console）经 plt_int_connect 注册；本函数只负责实例与中断配置。
 * RXOVR/RXTOUT 都需使能，否则 FIFO 溢出或接收超时无法触发中断。
 *
 * @return DRV_UART_OK / DRV_UART_ERR_IO。
 * @pre 使用中断时须先完成 init_intr（GIC 已 CfgInitialize）。
 * @author Xue Weisen
 */
drv_uart_rc_e drv_uart_init(void)
{
	XUartPs_Config *Config;
	int status;

	/* Vitis 2025.2 SDT 用基地址，classic BSP 用 DeviceId。 */
#ifdef SDT
	Config = XUartPs_LookupConfig(CFG_CONSOLE_UART_BASEADDR);
#else
	Config = XUartPs_LookupConfig(CFG_CONSOLE_UART_DEVICE_ID);
#endif

	/* 配置阶段：设备 ID 缺失时不访问任何寄存器。 */
	if (Config == NULL)
		return DRV_UART_ERR_IO;

	status = XUartPs_CfgInitialize(&Uart, Config, Config->BaseAddress);
	if (status != XST_SUCCESS) {
		return DRV_UART_ERR_IO;
	}

	/* 普通工作模式 + 使能接收中断源（溢出/超时）；ISR 经 plt_int_connect 注册。 */
	XUartPs_SetOperMode(&Uart, XUARTPS_OPER_MODE_NORMAL);
	XUartPs_SetInterruptMask(&Uart, XUARTPS_IXR_RXOVR | XUARTPS_IXR_TOUT);
	return DRV_UART_OK;
}

/**
 * @brief  清除 UART 中断状态（XUartPs 的 RXTOUT/RXOVR 需“写 1 清”）。
 *
 * 只读 FIFO 不会清标志，会导致中断反复触发；必须在 ISR 开头调用。
 *
 * @note 可在 ISR 中调用；不阻塞。
 * @author Xue Weisen
 */
void drv_uart_isr_clear(void)
{
	u32 isr = XUartPs_ReadReg(Uart.Config.BaseAddress, XUARTPS_ISR_OFFSET);
	XUartPs_WriteReg(Uart.Config.BaseAddress, XUARTPS_ISR_OFFSET, isr);
}

/**
 * @brief  非阻塞读 1 字节。
 *
 * @return 0~255 = 读到的字节；-1 = 当前无数据。
 * @note 可在 ISR 中调用；不阻塞。
 * @author Xue Weisen
 */
int drv_uart_read_byte(void)
{
	if (XUartPs_IsReceiveData(Uart.Config.BaseAddress))
	{
		return (int)XUartPs_ReadReg(Uart.Config.BaseAddress, XUARTPS_FIFO_OFFSET);
	}
	return -1;
}

/**
 * @brief  发送 1 字节（阻塞直到 TX FIFO 可写）。
 *
 * @param b 待发送字节。
 * @note 不可在 ISR 中调用。
 * @author Xue Weisen
 */
void drv_uart_send_byte(u8 b)
{
	XUartPs_SendByte(Uart.Config.BaseAddress, b);
}

/**
 * @brief  返回 UART 中断号，供 plt_int_connect/enable 使用。
 *
 * @return 来自 xparameters.h 的 UART 中断号。
 * @author Xue Weisen
 */
u32 drv_uart_intr_id(void)
{
	return CFG_CONSOLE_UART_IRQ;
}
