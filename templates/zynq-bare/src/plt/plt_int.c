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
// Keywords: scugic-scutimer-init, explicit-rc, 1ms-timer-flags, second-counter, external-event-count, soft-timer-tic/toc, no-silent-unsupported
//////////////////////////////////////////////////////////////////////////////////
#include "xparameters.h"

#include "plt_int.h"
#include "cfg_board.h"

#include "xscugic.h"
#include "xscutimer.h"
#include "xil_exception.h"

#if !defined(SDT) && (!defined(XPAR_XSCUGIC_0_DEVICE_ID) || \
    !defined(XPAR_XSCUTIMER_0_DEVICE_ID))
#error "Zynq classic BSP requires ScuGic and ScuTimer device IDs"
#endif

#if !defined(XPAR_XSCUTIMER_0_INTERRUPT_INTR) || \
    !defined(XPAR_CPU_CORTEXA9_CORE_CLOCK_FREQ_HZ)
#error "Zynq interrupt template requires ScuGic, ScuTimer and CPU clock parameters from xparameters.h"
#endif

/* 中断/定时器实例 ID 与中断号必须来自当前 BSP，不从旧工程复制。 */
#ifdef SDT
#define INTC_SELECTOR    CFG_INTC_BASEADDR
#define TIMER_SELECTOR   CFG_TIMER_BASEADDR
#else
#define INTC_SELECTOR    CFG_INTC_DEVICE_ID
#define TIMER_SELECTOR   CFG_TIMER_DEVICE_ID
#endif
#define TIMER_IRPT_INTR  CFG_TIMER_IRQ

/* 1ms 装载值：ScuTimer 计数时钟为 CPU 频率的一半。 */
#define TIMER_LOAD_VALUE (XPAR_CPU_CORTEXA9_CORE_CLOCK_FREQ_HZ / 2000u)

static XScuGic s_intc;
static XScuTimer s_timer;
static volatile u32 s_tick_sec;
static volatile u32 s_ext_event_count;
static volatile u32 s_flag_1ms;
static volatile u32 s_flag_10ms;
static volatile u32 s_flag_100ms;
static volatile u32 s_flag_1s;
static u32 s_tick_start[TIC_NUM];
static u8 s_initialized;
static volatile u32 s_cnt_10ms;
static volatile u32 s_cnt_100ms;
static volatile u32 s_cnt_1s;

/**
 * @brief 记录一次外部中断事件。
 *
 * @note 由外部 ISR 调用；只递增计数，不打印、不等待，也不读清共享标志。
 * @author Xue Weisen
 */
void plt_int_note_ext_event(void)
{
	s_ext_event_count++;
}

/**
 * @brief ScuTimer 1ms ISR：清中断源并产生分级节拍。
 *
 * @param ctx 注册上下文；本示例未使用。
 * @note 必须先清 ScuTimer 中断状态，否则中断会重复触发。
 * @author Xue Weisen
 */
static void plt_int_timer_1ms_isr(void *ctx)
{
	(void)ctx;

	/* 清源阶段：先清硬件状态，再更新软件节拍。 */
	XScuTimer_ClearInterruptStatus(&s_timer);
	s_flag_1ms = 1u;

	/* 分频阶段：独立计数器生成 10ms/100ms/1s 节拍。 */
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
 * @brief 初始化 ScuGic 与 1ms ScuTimer。
 *
 * @return PLT_INT_OK / PLT_INT_ERR_STATE / PLT_INT_ERR_IO。
 * @pre 平台和 cache 初始化已完成。
 * @note 所有 LookupConfig/CfgInitialize 失败都会阻止后续中断使能。
 * @author Xue Weisen
 */
plt_int_rc_e plt_int_init(void)
{
	XScuGic_Config *gic_cfg;
	XScuTimer_Config *timer_cfg;
	s32 status;

	/* 重复初始化会重置正在运行的节拍，直接拒绝。 */
	if (s_initialized != 0u) {
		return PLT_INT_ERR_STATE;
	}

	/* 配置阶段：两个设备配置必须在访问寄存器前全部查找到。 */
	gic_cfg = XScuGic_LookupConfig(INTC_SELECTOR);
	timer_cfg = XScuTimer_LookupConfig(TIMER_SELECTOR);
	if ((gic_cfg == NULL) || (timer_cfg == NULL)) {
		return PLT_INT_ERR_IO;
	}

	/* 初始化阶段：GIC 与 Timer 任一失败都返回 IO，不进入部分使能。 */
	status = XScuGic_CfgInitialize(&s_intc, gic_cfg, gic_cfg->CpuBaseAddress);
	if (status != XST_SUCCESS) {
		return PLT_INT_ERR_IO;
	}
	status = XScuTimer_CfgInitialize(&s_timer, timer_cfg, timer_cfg->BaseAddr);
	if (status != XST_SUCCESS) {
		return PLT_INT_ERR_IO;
	}

	/* 连接阶段：定时器中断先接入 GIC，再注册处理器异常向量。 */
	XScuGic_SetPriorityTriggerType(&s_intc, TIMER_IRPT_INTR, 0xA0u, 0x3u);
	status = XScuGic_Connect(&s_intc, TIMER_IRPT_INTR,
	                         (Xil_ExceptionHandler)plt_int_timer_1ms_isr,
	                         &s_timer);
	if (status != XST_SUCCESS) {
		return PLT_INT_ERR_IO;
	}
	Xil_ExceptionInit();
	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
	                             (Xil_ExceptionHandler)XScuGic_InterruptHandler,
	                             &s_intc);
	Xil_ExceptionEnable();
	XScuGic_Enable(&s_intc, TIMER_IRPT_INTR);

	/* 启动阶段：自动重载并启动计数器；void API 后无法再探测失败。 */
	XScuTimer_EnableAutoReload(&s_timer);
	XScuTimer_LoadTimer(&s_timer, TIMER_LOAD_VALUE);
	XScuTimer_EnableInterrupt(&s_timer);
	XScuTimer_Start(&s_timer);
	s_initialized = 1u;
	return PLT_INT_OK;
}

/**
 * @brief 连接一个应用中断处理函数。
 *
 * @param intr_id GIC 中断号。
 * @param handler ISR 函数。
 * @param ctx 回调上下文。
 * @return PLT_INT 结果码。
 * @pre plt_int_init() 已成功。
 * @author Xue Weisen
 */
plt_int_rc_e plt_int_connect(u32 intr_id,
                             plt_int_handler_fn handler,
                             void *ctx)
{
	s32 status;

	/* 参数和初始化状态先检查，避免连接到未初始化 GIC。 */
	if ((handler == 0) || (intr_id > 0xFFu)) {
		return PLT_INT_ERR_INVALID;
	}
	if (s_initialized == 0u) {
		return PLT_INT_ERR_STATE;
	}
	status = XScuGic_Connect(&s_intc, intr_id,
	                         (Xil_ExceptionHandler)handler, ctx);
	return (status == XST_SUCCESS) ? PLT_INT_OK : PLT_INT_ERR_IO;
}

/**
 * @brief 使能指定 GIC 中断号。
 *
 * @param intr_id GIC 中断号。
 * @return PLT_INT 结果码。
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
	XScuGic_Enable(&s_intc, intr_id);
	return PLT_INT_OK;
}

/**
 * @brief 禁用指定 GIC 中断号。
 *
 * @param intr_id GIC 中断号。
 * @return PLT_INT 结果码。
 * @author Xue Weisen
 */
plt_int_rc_e plt_int_disable(u32 intr_id)
{
	/* 未初始化时不得操作未知 GIC 实例。 */
	if (intr_id > 0xFFu) {
		return PLT_INT_ERR_INVALID;
	}
	if (s_initialized == 0u) {
		return PLT_INT_ERR_STATE;
	}
	XScuGic_Disable(&s_intc, intr_id);
	return PLT_INT_OK;
}

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
