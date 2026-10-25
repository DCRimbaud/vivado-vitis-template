//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: plt_int
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: 平台中断与软计时器实现
// Revision: 0.02
// Revision 0.01 - File Created
// Revision 0.02 - 中断初始化/连接/使能返回错误码，时间与外部事件改为只读访问器
// Keywords: axi-intc-init, explicit-rc, 1ms-timer-flags, second-counter, external-event-count, soft-timer-tic/toc, unsupported-without-intc
//////////////////////////////////////////////////////////////////////////////////
#include "xparameters.h"

#include "plt_int.h"
#include "cfg_board.h"

/* 全文件共享的软计时状态；只由 1ms ISR 推进秒计数。 */
static volatile u32 s_tick_sec;
static volatile u32 s_ext_event_count;
static volatile u32 s_flag_1ms;
static volatile u32 s_flag_10ms;
static volatile u32 s_flag_100ms;
static volatile u32 s_flag_1s;
static u32 s_tick_start[TIC_NUM];
static u8 s_initialized;

#if defined(SDT) || defined(XPAR_INTC_0_DEVICE_ID)

#include "xintc.h"
#include "xil_exception.h"

#define INTC_DEVICE_INTR_ID_TIMER CFG_TIMER_1MS_IRQ

static XIntc InterruptController;
static volatile u32 s_cnt_10ms;
static volatile u32 s_cnt_100ms;
static volatile u32 s_cnt_1s;

/**
 * @brief 外部中断 ISR：累计事件次数并立即返回。
 *
 * @param ctx 注册上下文；本示例未使用。
 * @note 只在 ISR 上下文运行；主循环读取计数器，不存在读清竞态。
 * @author Xue Weisen
 */
static void plt_int_ext_isr(void *ctx)
{
	(void)ctx;
	plt_int_note_ext_event();
}

/**
 * @brief 1ms 定时 ISR：产生分级节拍并推进秒计数。
 *
 * @param ctx 注册上下文；本示例未使用。
 * @note 只做计数和置标志，不打印、不等待、不调用业务状态机。
 * @author Xue Weisen
 */
static void plt_int_timer_1ms_isr(void *ctx)
{
	(void)ctx;

	/* 1ms 阶段：每拍置位，供主循环消费一次。 */
	s_flag_1ms = 1u;

	/* 分频阶段：独立计数器避免一个计数器边界影响其他节拍。 */
	s_cnt_10ms++;
	if (s_cnt_10ms >= 10u) {
		s_cnt_10ms = 0u;
		s_flag_10ms = 1u;
	}
	s_cnt_100ms++;
	if (s_cnt_100ms >= 100u) {
		s_cnt_100ms = 0u;
		s_flag_100ms = 1u;
	}
	s_cnt_1s++;
	if (s_cnt_1s >= 1000u) {
		s_cnt_1s = 0u;
		s_flag_1s = 1u;
		s_tick_sec++;
	}
}

/**
 * @brief 初始化 AXI INTC、1ms 定时器和可选外部中断。
 *
 * @return PLT_INT_OK / PLT_INT_ERR_STATE / PLT_INT_ERR_IO。
 * @pre 平台和 cache 初始化已完成。
 * @note 初始化失败时停止控制器并保持 s_initialized=0，防止后续连接假成功。
 * @author Xue Weisen
 */
plt_int_rc_e plt_int_init(void)
{
	int status;

	/* 重复初始化会重置正在运行的节拍，直接拒绝。 */
	if (s_initialized != 0u) {
		return PLT_INT_ERR_STATE;
	}

	/* 控制器阶段：Initialize 必须成功后才能 Connect。 */
	/* Vitis 2025.2 SDT 传基地址，classic BSP 传 DeviceId。 */
#ifdef SDT
	status = XIntc_Initialize(&InterruptController, CFG_INTC_BASEADDR);
#else
	status = XIntc_Initialize(&InterruptController, CFG_INTC_DEVICE_ID);
#endif
	if (status != XST_SUCCESS) {
		return PLT_INT_ERR_IO;
	}
	status = XIntc_Connect(&InterruptController, INTC_DEVICE_INTR_ID_TIMER,
	                       (XInterruptHandler)plt_int_timer_1ms_isr, 0);
	if (status != XST_SUCCESS) {
		return PLT_INT_ERR_IO;
	}
#if defined(XPAR_AXI_INTC_0_SYSTEM_INT_CPU_INTR)
	status = XIntc_Connect(&InterruptController, XPAR_AXI_INTC_0_SYSTEM_INT_CPU_INTR,
	                       (XInterruptHandler)plt_int_ext_isr, 0);
	if (status != XST_SUCCESS) {
		return PLT_INT_ERR_IO;
	}
#endif

	/* 启动阶段：任何一步失败都撤销控制器，避免部分中断源继续运行。 */
	status = XIntc_Start(&InterruptController, XIN_REAL_MODE);
	if (status != XST_SUCCESS) {
		(void)XIntc_Stop(&InterruptController);
		return PLT_INT_ERR_IO;
	}
	XIntc_Enable(&InterruptController, INTC_DEVICE_INTR_ID_TIMER);
#if defined(XPAR_AXI_INTC_0_SYSTEM_INT_CPU_INTR)
	XIntc_Enable(&InterruptController, XPAR_AXI_INTC_0_SYSTEM_INT_CPU_INTR);
#endif

	/* 异常阶段：最后挂接处理器异常向量，成功后才公开 ready 状态。 */
	Xil_ExceptionInit();
	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
	                             (Xil_ExceptionHandler)XIntc_InterruptHandler,
	                             &InterruptController);
	Xil_ExceptionEnable();
	s_initialized = 1u;
	return PLT_INT_OK;
}

/**
 * @brief 连接一个应用中断处理函数。
 *
 * @param intr_id 中断号。
 * @param handler ISR 函数。
 * @param ctx 回调上下文。
 * @return PLT_INT 结果码。
 * @pre plt_int_init() 已成功。
 * @note 失败不改变既有连接，调用方必须停止后续使能步骤。
 * @author Xue Weisen
 */
plt_int_rc_e plt_int_connect(u32 intr_id,
                             plt_int_handler_fn handler,
                             void *ctx)
{
	int status;

	/* 参数与初始化状态先检查，避免 BSP 在无效控制器上断言。 */
	if ((handler == 0) || (intr_id > 0xFFu)) {
		return PLT_INT_ERR_INVALID;
	}
	if (s_initialized == 0u) {
		return PLT_INT_ERR_STATE;
	}

	/* 连接阶段：BSP 失败原样映射为 IO 错误。 */
	status = XIntc_Connect(&InterruptController, (u8)intr_id,
	                       (XInterruptHandler)handler, ctx);
	return (status == XST_SUCCESS) ? PLT_INT_OK : PLT_INT_ERR_IO;
}

/**
 * @brief 使能指定中断号。
 *
 * @param intr_id 中断号。
 * @return PLT_INT 结果码。
 * @note XIntc_Enable 没有返回值；初始化状态和编号是唯一可验证前置条件。
 * @author Xue Weisen
 */
plt_int_rc_e plt_int_enable(u32 intr_id)
{
	/* 未初始化或编号越界时不能把空操作当成功。 */
	if (intr_id > 0xFFu) {
		return PLT_INT_ERR_INVALID;
	}
	if (s_initialized == 0u) {
		return PLT_INT_ERR_STATE;
	}
	XIntc_Enable(&InterruptController, (u8)intr_id);
	return PLT_INT_OK;
}

/**
 * @brief 禁用指定中断号。
 *
 * @param intr_id 中断号。
 * @return PLT_INT 结果码。
 * @author Xue Weisen
 */
plt_int_rc_e plt_int_disable(u32 intr_id)
{
	/* 未初始化时不得操作未知控制器实例。 */
	if (intr_id > 0xFFu) {
		return PLT_INT_ERR_INVALID;
	}
	if (s_initialized == 0u) {
		return PLT_INT_ERR_STATE;
	}
	XIntc_Disable(&InterruptController, (u8)intr_id);
	return PLT_INT_OK;
}

#else

/**
 * @brief 无中断控制器的 BSP：明确返回不支持。
 *
 * @return PLT_INT_UNSUPPORTED。
 * @author Xue Weisen
 */
plt_int_rc_e plt_int_init(void)
{
	return PLT_INT_UNSUPPORTED;
}

/**
 * @brief 无中断控制器：拒绝连接请求。
 *
 * @param intr_id 中断号（未使用）。
 * @param handler ISR（未使用）。
 * @param ctx 上下文（未使用）。
 * @return PLT_INT_UNSUPPORTED。
 * @author Xue Weisen
 */
plt_int_rc_e plt_int_connect(u32 intr_id,
                             plt_int_handler_fn handler,
                             void *ctx)
{
	(void)intr_id;
	(void)handler;
	(void)ctx;
	return PLT_INT_UNSUPPORTED;
}

/**
 * @brief 无中断控制器：拒绝使能请求。
 *
 * @param intr_id 中断号（未使用）。
 * @return PLT_INT_UNSUPPORTED。
 * @author Xue Weisen
 */
plt_int_rc_e plt_int_enable(u32 intr_id)
{
	(void)intr_id;
	return PLT_INT_UNSUPPORTED;
}

/**
 * @brief 无中断控制器：拒绝禁用请求。
 *
 * @param intr_id 中断号（未使用）。
 * @return PLT_INT_UNSUPPORTED。
 * @author Xue Weisen
 */
plt_int_rc_e plt_int_disable(u32 intr_id)
{
	(void)intr_id;
	return PLT_INT_UNSUPPORTED;
}

#endif

/**
 * @brief 查询并清除 1ms 标志。
 *
 * @return 1 = 到期；0 = 未到期。
 * @author Xue Weisen
 */
u32 is_timer_1ms(void)
{
	/* 读取后清零，保证同一节拍只有一个消费点。 */
	if (s_flag_1ms != 0u) {
		s_flag_1ms = 0u;
		return 1u;
	}
	return 0u;
}

/**
 * @brief 查询并清除 10ms 标志。
 *
 * @return 1 = 到期；0 = 未到期。
 * @author Xue Weisen
 */
u32 is_timer_10ms(void)
{
	/* 读取后清零，保证同一节拍只有一个消费点。 */
	if (s_flag_10ms != 0u) {
		s_flag_10ms = 0u;
		return 1u;
	}
	return 0u;
}

/**
 * @brief 查询并清除 100ms 标志。
 *
 * @return 1 = 到期；0 = 未到期。
 * @author Xue Weisen
 */
u32 is_timer_100ms(void)
{
	/* 读取后清零，保证同一节拍只有一个消费点。 */
	if (s_flag_100ms != 0u) {
		s_flag_100ms = 0u;
		return 1u;
	}
	return 0u;
}

/**
 * @brief 查询并清除 1s 标志。
 *
 * @return 1 = 到期；0 = 未到期。
 * @author Xue Weisen
 */
u32 is_timer_1s(void)
{
	/* 读取后清零，保证同一节拍只有一个消费点。 */
	if (s_flag_1s != 0u) {
		s_flag_1s = 0u;
		return 1u;
	}
	return 0u;
}

/**
 * @brief 读取上电以来的秒计数。
 *
 * @return 秒计数；32 bit 自然回绕。
 * @author Xue Weisen
 */
u32 plt_int_now_s(void)
{
	return s_tick_sec;
}

/**
 * @brief 读取外部中断累计事件数。
 *
 * @return 事件计数；32 bit 自然回绕。
 * @author Xue Weisen
 */
u32 plt_int_ext_event_count(void)
{
	return s_ext_event_count;
}

/**
 * @brief 记录一次外部中断事件。
 *
 * @note 由外部 ISR 调用；只递增计数，不打印、不等待。
 * @author Xue Weisen
 */
void plt_int_note_ext_event(void)
{
	s_ext_event_count++;
}

/**
 * @brief 记录软计时槽位起点。
 *
 * @param index 槽位号。
 * @return 当前秒值；越界返回 0。
 * @author Xue Weisen
 */
u32 tic(u32 index)
{
	if (index >= (u32)TIC_NUM) {
		return 0u;
	}
	s_tick_start[index] = s_tick_sec;
	return s_tick_start[index];
}

/**
 * @brief 返回软计时槽位经过的秒数。
 *
 * @param index 槽位号。
 * @return 经过秒数；越界返回 0。
 * @author Xue Weisen
 */
u32 toc(u32 index)
{
	if (index >= (u32)TIC_NUM) {
		return 0u;
	}
	return s_tick_sec - s_tick_start[index];
}
