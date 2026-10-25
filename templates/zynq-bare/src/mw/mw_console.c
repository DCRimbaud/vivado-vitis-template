//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: mw_console
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: 系统控制台实现：UART ISR 有界排空、回调分发与打印
// Revision: 0.04
// Revision 0.01 - File Created
// Revision 0.02 - 接收回调增加 ctx 参数
// Revision 0.03 - 初始化返回码与 ISR 有界排空，硬件失败不再静默
// Revision 0.04 - 打印出口改走 plt_console
// Keywords: console-impl, bounded-fifo-drain, isr-callback-ctx, explicit-init-rc, xuartps, no-mutable-extern
//////////////////////////////////////////////////////////////////////////////////
#include <stdarg.h>

#include "mw_console.h"
#include "drv_uart.h"
#include "plt_int.h"
#include "plt_console.h"

/* 单次 ISR 最多处理的字节数；覆盖 64 B FIFO 以外的异常突发仍需重进中断。 */
#define MW_CONSOLE_RX_DRAIN_LIMIT 64u

static mw_console_rx_fn volatile s_rx_handler = NULL;
static void *volatile s_rx_ctx = NULL;

/**
 * @brief UART ISR：清中断源、有界排空 FIFO 并分发每个字节。
 *
 * @param ctx 注册上下文；本模块未使用。
 * @note 只清源、读 FIFO 和调用短回调，不打印、不等待、不分配内存。
 * @author Xue Weisen
 */
static void mw_console_isr(void *ctx)
{
	mw_console_rx_fn handler;
	void *handler_ctx;
	u32 i;
	int ch;

	(void)ctx;

	/* 清源阶段：XUartPs 的 RX timeout/overrun 必须写 1 清。 */
	drv_uart_isr_clear();

	/* 快照阶段：先复制回调与上下文，避免分发期间注册变化。 */
	handler = s_rx_handler;
	handler_ctx = s_rx_ctx;

	/* 排空阶段：有界处理当前 FIFO；回调缺失时仍读空硬件。 */
	for (i = 0u; i < MW_CONSOLE_RX_DRAIN_LIMIT; i++) {
		ch = drv_uart_read_byte();
		if (ch < 0) {
			break;
		}
		if (handler != NULL) {
			handler((u8)ch, handler_ctx);
		}
	}
}

/**
 * @brief 初始化控制台并完成 ISR 连接与使能。
 *
 * @return MW_CONSOLE_OK / MW_CONSOLE_ERR_STATE。
 * @pre plt_int_init() 已成功。
 * @note 任一阶段失败都返回 STATE，组合根不得继续宣布控制台可用。
 * @author Xue Weisen
 */
mw_console_rc_e mw_console_init(void)
{
	plt_int_rc_e int_rc;
	drv_uart_rc_e uart_rc;
	u32 irq;

	/* 硬件阶段：串口实例失败时没有可用的中断号或寄存器。 */
	uart_rc = drv_uart_init();
	if (uart_rc != DRV_UART_OK) {
		return MW_CONSOLE_ERR_STATE;
	}
	irq = drv_uart_intr_id();

	/* 连接阶段：ISR 必须成功挂入 GIC。 */
	int_rc = plt_int_connect(irq, mw_console_isr, NULL);
	if (int_rc != PLT_INT_OK) {
		return MW_CONSOLE_ERR_STATE;
	}

	/* 使能阶段：最后打开 IRQ；失败时组合根保留失败阶段并停止启动。 */
	int_rc = plt_int_enable(irq);
	if (int_rc != PLT_INT_OK) {
		return MW_CONSOLE_ERR_STATE;
	}
	return MW_CONSOLE_OK;
}

/**
 * @brief 控制台格式化打印。
 *
 * @param fmt printf 风格格式串。
 * @note 阻塞且非线程安全；严禁在 ISR 中调用。
 * @author Xue Weisen
 */
void uart_print(const char *fmt, ...)
{
	va_list ap;

	/* va_list 只消费一次；格式与参数类型由调用方保证匹配。 */
	va_start(ap, fmt);
	plt_console_vprintf(fmt, ap);
	va_end(ap);
}

/**
 * @brief 注册接收字节回调。
 *
 * @param fn 回调；NULL 表示丢弃接收字节。
 * @param ctx 回调上下文；生命周期覆盖注册期。
 * @note 回调在 ISR 上下文执行，必须极短；注册时机就是命令处理的放行闸门。
 * @author Xue Weisen
 */
void mw_console_set_rx_handler(mw_console_rx_fn fn, void *ctx)
{
	/* 先写上下文再发布函数指针，避免 ISR 看见新函数配旧上下文。 */
	s_rx_ctx = ctx;
	s_rx_handler = fn;
}
