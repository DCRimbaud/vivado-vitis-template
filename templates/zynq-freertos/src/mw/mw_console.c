//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: mw_console
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: 系统控制台实现：UART ISR 批量入队、任务读取与丢字节计数
// Revision: 0.04
// Revision 0.01 - File Created
// Revision 0.02 - 移除 FreeRTOS 类型，改走 plt_rtos
// Revision 0.03 - 返回码按域定义，初始化事务化，ISR 有界排空 FIFO 并统计队列满丢弃
// Revision 0.04 - 打印出口改走 plt_console
// Keywords: console-impl, isr-to-queue, bounded-fifo-drain, drop-counter, transactional-init, deinit, status-plus-output, no-freertos-include
//////////////////////////////////////////////////////////////////////////////////
#include <stdarg.h>

#include "mw_console.h"
#include "drv_uart.h"
#include "plt_int.h"
#include "plt_rtos.h"
#include "plt_console.h"
#include "cfg_rtos.h"

/* 单次 ISR 最多排空的硬件字节数：覆盖 16 B FIFO 并留一倍余量，防止异常源占满中断。 */
#define MW_CONSOLE_RX_DRAIN_LIMIT 32u

static plt_rtos_queue_t s_rx_queue;
static volatile u32 s_rx_drop_count;
static volatile u8 s_console_ready;

/**
 * @brief UART ISR：清中断源、有限排空 FIFO 并投递字节。
 *
 * @param ctx 注册上下文；本实现未使用。
 * @note 运行在 ISR 上下文。队列满的字节会计数并丢弃，但硬件 FIFO 仍继续
 *       排空以避免溢出；不得打印或阻塞。
 * @author Xue Weisen
 */
static void mw_console_isr(void *ctx)
{
	plt_rtos_woken_t woken = 0u;
	plt_rtos_status_e st;
	u32 i;
	u8 b;
	int ch;

	(void)ctx;

	/* 未完成初始化时只清中断源，不能向尚未提交的队列投递。 */
	drv_uart_isr_clear();
	if ((s_console_ready == 0u) || (s_rx_queue == 0)) {
		plt_rtos_isr_exit(0u);
		return;
	}

	/* 排空阶段：有界循环覆盖 FIFO 上限，队列满时继续排空但累计软丢字节。 */
	for (i = 0u; i < MW_CONSOLE_RX_DRAIN_LIMIT; i++) {
		ch = drv_uart_read_byte();
		if (ch < 0) {
			break;
		}
		b = (u8)ch;
		st = plt_rtos_queue_send_from_isr(s_rx_queue, &b, &woken);
		if (st != PLT_RTOS_OK) {
			s_rx_drop_count++;
		}
	}

	/* 退出阶段：所有字节处理完只提交一次上下文切换请求。 */
	plt_rtos_isr_exit(woken);
}

/**
 * @brief 以事务方式初始化控制台硬件和 OS 资源。
 *
 * @return 控制台域返回码。
 * @pre 平台和 OSAL 已初始化。
 * @note 只有中断使能成功后才发布队列；失败路径删除队列并保持模块未初始化。
 * @author Xue Weisen
 */
mw_console_rc_e mw_console_init(void)
{
	plt_rtos_queue_t queue = 0;
	plt_rtos_status_e st;
	u32 irq;

	/* 重复初始化会制造两个队列所有权，直接拒绝。 */
	if ((s_console_ready != 0u) || (s_rx_queue != 0)) {
		return MW_CONSOLE_ERR_STATE;
	}

	/* 硬件阶段：串口配置失败时不创建任何 OS 资源。 */
	if (drv_uart_init() != 0) {
		return MW_CONSOLE_ERR_STATE;
	}
	irq = drv_uart_intr_id();

	/* 资源阶段：队列先放局部句柄，任何后续失败都可回收。 */
	st = plt_rtos_queue_create(RTOS_CONSOLE_RXQ_LEN, sizeof(u8), &queue);
	if (st != PLT_RTOS_OK) {
		return MW_CONSOLE_ERR_OS;
	}
	s_rx_queue = queue;
	s_rx_drop_count = 0u;

	/* 中断阶段：Connect/Enable 任一失败都撤销队列，避免半初始化控制台。 */
	st = plt_int_connect(irq, mw_console_isr, 0);
	if (st != PLT_RTOS_OK) {
		s_rx_queue = 0;
		(void)plt_rtos_queue_delete(queue);
		return MW_CONSOLE_ERR_OS;
	}
	st = plt_int_enable(irq);
	if (st != PLT_RTOS_OK) {
		s_rx_queue = 0;
		(void)plt_rtos_queue_delete(queue);
		return MW_CONSOLE_ERR_OS;
	}

	/* 提交阶段：IRQ 已可用后才允许 ISR 观察 ready。 */
	s_console_ready = 1u;
	return MW_CONSOLE_OK;
}

/**
 * @brief 禁用控制台中断并释放接收队列。
 *
 * @return 控制台域返回码。
 * @pre 控制台任务已停止，且没有调用方与 getc/deinit 并发。
 * @note 若禁用中断失败则保留队列；中断仍可能触发时删除队列会造成 UAF。
 * @author Xue Weisen
 */
mw_console_rc_e mw_console_deinit(void)
{
	plt_rtos_status_e st;
	u32 irq;

	/* 未初始化对象按已释放处理，便于组合根统一清理。 */
	if (s_rx_queue == 0) {
		s_console_ready = 0u;
		return MW_CONSOLE_OK;
	}
	irq = drv_uart_intr_id();

	/* 先断中断源，再失活 ready，最后删除队列。 */
	st = plt_int_disable(irq);
	if (st != PLT_RTOS_OK) {
		return MW_CONSOLE_ERR_OS;
	}
	s_console_ready = 0u;
	st = plt_rtos_queue_delete(s_rx_queue);
	if (st != PLT_RTOS_OK) {
		return MW_CONSOLE_ERR_OS;
	}
	s_rx_queue = 0;
	return MW_CONSOLE_OK;
}

/**
 * @brief 通过 BSP UART 后端格式化并输出。
 *
 * @param fmt printf 风格格式串。
 * @note 阻塞且不可重入；不得从 ISR 调用。
 * @author Xue Weisen
 */
void uart_print(const char *fmt, ...)
{
	va_list ap;

	/* va_list 只消费一次；输出阻塞特性由 BSP xil_printf 决定。 */
	va_start(ap, fmt);
	plt_console_vprintf(fmt, ap);
	va_end(ap);
}

/**
 * @brief 从控制台队列读取一个字节。
 *
 * @param out 输出字节。
 * @param timeout_ms 有限毫秒等待。
 * @return 控制台域返回码。
 * @note 仅限任务上下文；所有失败路径都保持输出不变。
 * @author Xue Weisen
 */
mw_console_rc_e mw_console_getc(u8 *out, u32 timeout_ms)
{
	plt_rtos_status_e st;
	u8 value;

	/* 参数与初始化状态必须先于 OS 调用检查。 */
	if (out == 0) {
		return MW_CONSOLE_ERR_INVALID;
	}
	if ((s_console_ready == 0u) || (s_rx_queue == 0)) {
		return MW_CONSOLE_ERR_STATE;
	}

	/* 读取阶段：先写局部值，成功后才提交给调用方输出。 */
	st = plt_rtos_queue_receive(s_rx_queue, &value, timeout_ms);
	if (st == PLT_RTOS_OK) {
		*out = value;
		return MW_CONSOLE_OK;
	}
	if (st == PLT_RTOS_ERR_TIMEOUT) {
		return MW_CONSOLE_TIMEOUT;
	}
	return MW_CONSOLE_ERR_OS;
}

/**
 * @brief 返回软件接收队列的累计丢弃计数。
 *
 * @return OS 队列满导致的累计丢弃字节数。
 * @note 只由单一 UART ISR 生产者更新。
 * @author Xue Weisen
 */
u32 mw_console_rx_drop_count(void)
{
	return s_rx_drop_count;
}
