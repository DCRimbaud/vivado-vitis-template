//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: plt_rtos_freertos
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: FreeRTOS 编译期后端
// Revision: 0.02
// Revision: 0.01 - File Created
// Revision 0.02 - 增加可移植优先级/字节栈换算、有限 timeout 饱和、资源删除与队列观测
// Keywords: freertos-backend, only-layer-allowed-freertos-headers, priority-map, stack-bytes, timeout-clamp, lifecycle, queue-count, isr-woken
//////////////////////////////////////////////////////////////////////////////////
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "portmacro.h"

#include "plt_rtos.h"

/* ARM_CA9 的 classic GIC 路径使用 8 bit 中断号；SDT/XilTimer 路径使用 16 bit。 */
#if !defined(XPAR_XILTIMER_ENABLED) && !defined(SDT)
#define PLT_RTOS_IRQ_MAX 0xFFu
#define PLT_RTOS_IRQ_CAST(irq) ((uint8_t)(irq))
#else
#define PLT_RTOS_IRQ_MAX 0xFFFFu
#define PLT_RTOS_IRQ_CAST(irq) ((uint16_t)(irq))
#endif

/* 当前薄适配层明确依赖动态创建；若 BSP 只允许静态分配，必须另加静态工厂。 */
#if (configSUPPORT_DYNAMIC_ALLOCATION != 1)
#error "plt_rtos_freertos currently requires configSUPPORT_DYNAMIC_ALLOCATION=1"
#endif
#if (INCLUDE_vTaskDelete != 1) || (INCLUDE_vTaskDelay != 1) || \
    (INCLUDE_xTaskGetSchedulerState != 1)
#error "enable vTaskDelete, vTaskDelay and xTaskGetSchedulerState in FreeRTOSConfig.h"
#endif
#if (configTICK_RATE_HZ == 0) || (configMAX_PRIORITIES == 0)
#error "FreeRTOS tick rate and priority count must be positive"
#endif

/**
 * @brief 把毫秒超时换算为有限 FreeRTOS tick。
 *
 * 使用 64 bit 中间值避免毫秒乘以 tick 频率时在 32 bit 上溢出；正毫秒至少
 * 换算为 1 tick。结果饱和到 portMAX_DELAY - 1，确保不会表达成无限等待。
 *
 * @param timeout_ms 有限毫秒数；0 表示不等待。
 * @return 0 或 1..(portMAX_DELAY - 1) 的有限 tick 数。
 * @author Xue Weisen
 */
static TickType_t plt_rtos_ms_to_ticks(u32 timeout_ms)
{
	u64 ticks;
	u64 max_finite;

	/* 零超时保持非阻塞；正超时至少 1 tick，避免小于一个 tick 时退化成轮询。 */
	if (timeout_ms == 0u) {
		return 0;
	}
	ticks = (((u64)timeout_ms * (u64)configTICK_RATE_HZ) + 999u) / 1000u;
	if (ticks == 0u) {
		ticks = 1u;
	}

	/* portMAX_DELAY 可能被后端解释为无限等待，因此有限等待最多使用其前一档。 */
	max_finite = (u64)((TickType_t)~((TickType_t)0)) - 1u;
	if (ticks > max_finite) {
		ticks = max_finite;
	}
	return (TickType_t)ticks;
}

/**
 * @brief 把可移植优先级映射为 FreeRTOS 原生优先级。
 *
 * @param priority 可移植优先级枚举。
 * @return 受 configMAX_PRIORITIES 限制的原生优先级；非法输入按 IDLE 处理。
 * @note 映射保持相对顺序，调用方应使用枚举而不是硬编码数值。
 * @author Xue Weisen
 */
static UBaseType_t plt_rtos_priority_to_native(plt_rtos_priority_e priority)
{
	UBaseType_t native = (UBaseType_t)priority;

	/* 任何枚举值都不能越过 BSP 配置的优先级上限。 */
	if (native >= (UBaseType_t)configMAX_PRIORITIES) {
		native = (UBaseType_t)(configMAX_PRIORITIES - 1);
	}
	return native;
}

/**
 * @brief 初始化 FreeRTOS 后端。
 *
 * 内核对象由 BSP 和后续 create 接口初始化，本函数不重复配置 tick。
 *
 * @return PLT_RTOS_OK。
 * @author Xue Weisen
 */
plt_rtos_status_e plt_rtos_init(void)
{
	/* FreeRTOS 的 tick/GIC 由 BSP port 初始化；应用层不重复灌入。 */
	return PLT_RTOS_OK;
}

/**
 * @brief 启动 FreeRTOS 调度器。
 *
 * @return 仅在启动失败时返回 PLT_RTOS_ERR_STATE。
 * @warning 成功启动后本函数不返回。
 * @author Xue Weisen
 */
plt_rtos_status_e plt_rtos_start(void)
{
	/* 调度器成功接管后不会返回；返回即视为启动失败。 */
	vTaskStartScheduler();
	return PLT_RTOS_ERR_STATE;
}

/**
 * @brief 创建 FreeRTOS 任务。
 *
 * @param fn 任务入口。
 * @param name 任务名。
 * @param ctx 上下文。
 * @param stack_bytes 栈字节数。
 * @param priority 可移植优先级枚举。
 * @param out_task 输出句柄。
 * @return PLT_RTOS_OK / PLT_RTOS_ERR_INVALID / PLT_RTOS_ERR_NOMEM。
 * @author Xue Weisen
 */
plt_rtos_status_e plt_rtos_task_create(plt_rtos_task_fn fn,
                                       const char *name,
                                       void *ctx,
                                       u32 stack_bytes,
                                       plt_rtos_priority_e priority,
                                       plt_rtos_task_t *out_task)
{
	TaskHandle_t handle = NULL;
	BaseType_t rc;
	u32 stack_words;
	const char *task_name = (name != 0) ? name : "task";

	/* 参数阶段：入口与字节栈必须有效，避免后端自行猜一个不可控默认栈。 */
	if ((fn == 0) || (stack_bytes == 0u)) {
		return PLT_RTOS_ERR_INVALID;
	}

	/* 换算阶段：OSAL 使用字节，FreeRTOS 按 StackType_t 元素数向上取整。 */
	stack_words = (stack_bytes + (u32)sizeof(StackType_t) - 1u) /
	              (u32)sizeof(StackType_t);
	if (stack_words == 0u) {
		return PLT_RTOS_ERR_INVALID;
	}

	/* 创建阶段：原生失败统一映射为 NOMEM，避免业务层认识 FreeRTOS 返回码。 */
	rc = xTaskCreate(fn, task_name, (configSTACK_DEPTH_TYPE)stack_words,
	                 ctx, plt_rtos_priority_to_native(priority), &handle);
	if (rc != pdPASS) {
		return PLT_RTOS_ERR_NOMEM;
	}
	if (out_task != 0) {
		*out_task = (plt_rtos_task_t)handle;
	}
	return PLT_RTOS_OK;
}

/**
 * @brief 删除 FreeRTOS 任务。
 *
 * @param task 任务句柄。
 * @return PLT_RTOS_OK / PLT_RTOS_ERR_INVALID。
 * @note 只允许删除其他任务或按 FreeRTOS 规则删除自身；组合根通常只删启动失败的任务。
 * @author Xue Weisen
 */
plt_rtos_status_e plt_rtos_task_delete(plt_rtos_task_t task)
{
	/* 空句柄无法确定目标任务，直接拒绝，避免误删当前任务。 */
	if (task == 0) {
		return PLT_RTOS_ERR_INVALID;
	}
	vTaskDelete((TaskHandle_t)task);
	return PLT_RTOS_OK;
}

/**
 * @brief 让当前 FreeRTOS 任务按毫秒延时。
 *
 * @param delay_ms 延时毫秒；0 表示不阻塞。
 * @return PLT_RTOS_OK / PLT_RTOS_ERR_STATE。
 * @note 调度器未运行时 FreeRTOS 会触发断言或返回，本包装统一返回 STATE。
 * @author Xue Weisen
 */
plt_rtos_status_e plt_rtos_task_delay_ms(u32 delay_ms)
{
	TickType_t ticks;

	/* 调度器未运行时没有任务上下文，不能用忙等伪装成任务延时。 */
	if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) {
		return PLT_RTOS_ERR_STATE;
	}
	ticks = plt_rtos_ms_to_ticks(delay_ms);
	if ((delay_ms > 0u) && (ticks == 0u)) {
		ticks = 1;
	}
	vTaskDelay(ticks);
	return PLT_RTOS_OK;
}

/**
 * @brief 创建 FreeRTOS 队列。
 *
 * @param depth 队列深度。
 * @param item_size 单项字节数。
 * @param out_queue 输出句柄。
 * @return PLT_RTOS_OK / PLT_RTOS_ERR_INVALID / PLT_RTOS_ERR_NOMEM。
 * @author Xue Weisen
 */
plt_rtos_status_e plt_rtos_queue_create(u32 depth,
                                        u32 item_size,
                                        plt_rtos_queue_t *out_queue)
{
	QueueHandle_t queue;

	/* 队列必须同时有深度、项宽和输出句柄，避免创建后无人持有。 */
	if ((depth == 0u) || (item_size == 0u) || (out_queue == 0)) {
		return PLT_RTOS_ERR_INVALID;
	}
	queue = xQueueCreate((UBaseType_t)depth, (UBaseType_t)item_size);
	if (queue == NULL) {
		return PLT_RTOS_ERR_NOMEM;
	}
	*out_queue = (plt_rtos_queue_t)queue;
	return PLT_RTOS_OK;
}

/**
 * @brief 删除 FreeRTOS 队列。
 *
 * @param queue 队列句柄。
 * @return PLT_RTOS_OK / PLT_RTOS_ERR_INVALID。
 * @pre 所有等待该队列的任务已停止。
 * @author Xue Weisen
 */
plt_rtos_status_e plt_rtos_queue_delete(plt_rtos_queue_t queue)
{
	/* 空句柄不能交给 vQueueDelete，否则会在内核断言处才暴露。 */
	if (queue == 0) {
		return PLT_RTOS_ERR_INVALID;
	}
	vQueueDelete((QueueHandle_t)queue);
	return PLT_RTOS_OK;
}

/**
 * @brief 查询 FreeRTOS 队列当前消息数。
 *
 * @param queue 队列句柄。
 * @param out_count 输出消息数。
 * @return PLT_RTOS_OK / PLT_RTOS_ERR_INVALID。
 * @note 结果只是快照，不得用于替代发送/接收返回码。
 * @author Xue Weisen
 */
plt_rtos_status_e plt_rtos_queue_count(plt_rtos_queue_t queue,
                                       u32 *out_count)
{
	u32 count;

	/* 查询只读句柄；失败不写调用方输出，保持输出参数契约。 */
	if ((queue == 0) || (out_count == 0)) {
		return PLT_RTOS_ERR_INVALID;
	}
	count = (u32)uxQueueMessagesWaiting((QueueHandle_t)queue);
	*out_count = count;
	return PLT_RTOS_OK;
}

/**
 * @brief 从 FreeRTOS 队列发送一项。
 *
 * @param queue 队列句柄。
 * @param item 输入项。
 * @param timeout_ms 有限毫秒超时。
 * @return PLT_RTOS_OK / PLT_RTOS_ERR_TIMEOUT / PLT_RTOS_ERR_FULL / PLT_RTOS_ERR_INVALID。
 * @author Xue Weisen
 */
plt_rtos_status_e plt_rtos_queue_send(plt_rtos_queue_t queue,
                                      const void *item,
                                      u32 timeout_ms)
{
	TickType_t ticks;
	BaseType_t rc;

	/* 任务上下文按有限 tick 等待；正超时至少 1 tick，避免被截断成不等待。 */
	if ((queue == 0) || (item == 0)) {
		return PLT_RTOS_ERR_INVALID;
	}
	ticks = plt_rtos_ms_to_ticks(timeout_ms);
	rc = xQueueSend((QueueHandle_t)queue, item, ticks);
	if (rc == errQUEUE_FULL) {
		return (timeout_ms == 0u) ? PLT_RTOS_ERR_FULL : PLT_RTOS_ERR_TIMEOUT;
	}
	if (rc != pdTRUE) {
		return PLT_RTOS_ERR_TIMEOUT;
	}
	return PLT_RTOS_OK;
}

/**
 * @brief 从 FreeRTOS 队列接收一项。
 *
 * @param queue 队列句柄。
 * @param item 输出项。
 * @param timeout_ms 有限毫秒超时。
 * @return PLT_RTOS_OK / PLT_RTOS_ERR_TIMEOUT / PLT_RTOS_ERR_INVALID。
 * @author Xue Weisen
 */
plt_rtos_status_e plt_rtos_queue_receive(plt_rtos_queue_t queue,
                                         void *item,
                                         u32 timeout_ms)
{
	TickType_t ticks;

	/* 有限等待：timeout_ms>0 时至少换算为 1 tick，避免被截断成“不等待”。 */
	if ((queue == 0) || (item == 0)) {
		return PLT_RTOS_ERR_INVALID;
	}
	ticks = plt_rtos_ms_to_ticks(timeout_ms);
	if (xQueueReceive((QueueHandle_t)queue, item, ticks) != pdTRUE) {
		return PLT_RTOS_ERR_TIMEOUT;
	}
	return PLT_RTOS_OK;
}

/**
 * @brief 从 ISR 发送一项。
 *
 * @param queue 队列句柄。
 * @param item 输入项。
 * @param woken 上下文切换 token。
 * @return PLT_RTOS_OK / PLT_RTOS_ERR_FULL / PLT_RTOS_ERR_INVALID。
 * @author Xue Weisen
 */
plt_rtos_status_e plt_rtos_queue_send_from_isr(plt_rtos_queue_t queue,
                                               const void *item,
                                               plt_rtos_woken_t *woken)
{
	BaseType_t woke = pdFALSE;

	/* FromISR 不阻塞：队列满时返回 FULL，由上层协议重同步；woken 必须回传。 */
	if ((queue == 0) || (item == 0) || (woken == 0)) {
		return PLT_RTOS_ERR_INVALID;
	}
	*woken = 0u;
	if (xQueueSendFromISR((QueueHandle_t)queue, item, &woke) != pdTRUE) {
		return PLT_RTOS_ERR_FULL;
	}
	*woken = (woke == pdTRUE) ? 1u : 0u;
	return PLT_RTOS_OK;
}

/**
 * @brief 创建 FreeRTOS 互斥量。
 *
 * @param out_mutex 输出句柄。
 * @return PLT_RTOS_OK / PLT_RTOS_ERR_INVALID / PLT_RTOS_ERR_NOMEM。
 * @author Xue Weisen
 */
plt_rtos_status_e plt_rtos_mutex_create(plt_rtos_mutex_t *out_mutex)
{
	SemaphoreHandle_t mutex;

	/* 句柄输出必须由调用方提供，创建失败统一映射为 NOMEM。 */
	if (out_mutex == 0) {
		return PLT_RTOS_ERR_INVALID;
	}
	mutex = xSemaphoreCreateMutex();
	if (mutex == NULL) {
		return PLT_RTOS_ERR_NOMEM;
	}
	*out_mutex = (plt_rtos_mutex_t)mutex;
	return PLT_RTOS_OK;
}

/**
 * @brief 删除 FreeRTOS 互斥量。
 *
 * @param mutex 互斥量句柄。
 * @return PLT_RTOS_OK / PLT_RTOS_ERR_INVALID / PLT_RTOS_ERR_STATE。
 * @pre 互斥量未锁定，且没有等待者。
 * @author Xue Weisen
 */
plt_rtos_status_e plt_rtos_mutex_delete(plt_rtos_mutex_t mutex)
{
	/* 删除由 vSemaphoreDelete 完成；无效句柄必须先被 OSAL 拦截。 */
	if (mutex == 0) {
		return PLT_RTOS_ERR_INVALID;
	}
	vSemaphoreDelete((SemaphoreHandle_t)mutex);
	return PLT_RTOS_OK;
}

/**
 * @brief 获取 FreeRTOS 互斥量。
 *
 * @param mutex 互斥量句柄。
 * @param timeout_ms 有限毫秒超时。
 * @return PLT_RTOS_OK / PLT_RTOS_ERR_TIMEOUT / PLT_RTOS_ERR_INVALID。
 * @author Xue Weisen
 */
plt_rtos_status_e plt_rtos_mutex_lock(plt_rtos_mutex_t mutex, u32 timeout_ms)
{
	TickType_t ticks;

	/* 与队列一致：正超时至少等待 1 tick，0 表示不等待。 */
	if (mutex == 0) {
		return PLT_RTOS_ERR_INVALID;
	}
	ticks = plt_rtos_ms_to_ticks(timeout_ms);
	if (xSemaphoreTake((SemaphoreHandle_t)mutex, ticks) != pdTRUE) {
		return PLT_RTOS_ERR_TIMEOUT;
	}
	return PLT_RTOS_OK;
}

/**
 * @brief 释放 FreeRTOS 互斥量。
 *
 * @param mutex 互斥量句柄。
 * @return PLT_RTOS_OK / PLT_RTOS_ERR_INVALID / PLT_RTOS_ERR_STATE。
 * @author Xue Weisen
 */
plt_rtos_status_e plt_rtos_mutex_unlock(plt_rtos_mutex_t mutex)
{
	/* 互斥量只能由持有者释放；失败返回状态，不静默吞。 */
	if (mutex == 0) {
		return PLT_RTOS_ERR_INVALID;
	}
	if (xSemaphoreGive((SemaphoreHandle_t)mutex) != pdTRUE) {
		return PLT_RTOS_ERR_STATE;
	}
	return PLT_RTOS_OK;
}

/**
 * @brief 读取 FreeRTOS tick 并换算毫秒。
 *
 * @return 32 bit 单调毫秒值。
 * @author Xue Weisen
 */
u32 plt_rtos_now_ms(void)
{
	u64 ticks = (u64)xTaskGetTickCount();

	/* 先扩大再换算，减小时中间乘法在 32 bit 上溢出的概率。 */
	return (u32)((ticks * 1000u) / (u64)configTICK_RATE_HZ);
}

/**
 * @brief 安装 FreeRTOS/Zynq 中断处理函数。
 *
 * @param irq 中断号。
 * @param handler ISR 函数。
 * @param ctx 上下文。
 * @return PLT_RTOS_OK / PLT_RTOS_ERR_INVALID / PLT_RTOS_ERR_STATE。
 * @author Xue Weisen
 */
plt_rtos_status_e plt_rtos_isr_connect(u32 irq,
                                       plt_rtos_isr_fn handler,
                                       void *ctx)
{
	/* FreeRTOS Zynq port 使用自己的 GIC 实例，应用层不再重复 CfgInitialize。 */
	if ((handler == 0) || (irq > PLT_RTOS_IRQ_MAX)) {
		return PLT_RTOS_ERR_INVALID;
	}
	if (xPortInstallInterruptHandler(PLT_RTOS_IRQ_CAST(irq),
	                                 (XInterruptHandler)handler,
	                                 ctx) != pdPASS) {
		return PLT_RTOS_ERR_STATE;
	}
	return PLT_RTOS_OK;
}

/**
 * @brief 使能 FreeRTOS/Zynq 中断。
 *
 * @param irq 中断号。
 * @return PLT_RTOS_OK / PLT_RTOS_ERR_INVALID。
 * @author Xue Weisen
 */
plt_rtos_status_e plt_rtos_isr_enable(u32 irq)
{
	if (irq > PLT_RTOS_IRQ_MAX) {
		return PLT_RTOS_ERR_INVALID;
	}
	vPortEnableInterrupt(PLT_RTOS_IRQ_CAST(irq));
	return PLT_RTOS_OK;
}

/**
 * @brief 禁用 FreeRTOS/Zynq 中断。
 *
 * @param irq 中断号。
 * @return PLT_RTOS_OK / PLT_RTOS_ERR_INVALID。
 * @author Xue Weisen
 */
plt_rtos_status_e plt_rtos_isr_disable(u32 irq)
{
	if (irq > PLT_RTOS_IRQ_MAX) {
		return PLT_RTOS_ERR_INVALID;
	}
	vPortDisableInterrupt(PLT_RTOS_IRQ_CAST(irq));
	return PLT_RTOS_OK;
}

/**
 * @brief 在 ISR 末尾提交 FreeRTOS 上下文切换。
 *
 * @param woken FromISR 发送产生的 token。
 * @author Xue Weisen
 */
void plt_rtos_isr_exit(plt_rtos_woken_t woken)
{
	/* 只有发送方报告需要切换时才让出；避免无意义抖动。 */
	portYIELD_FROM_ISR((woken != 0u) ? pdTRUE : pdFALSE);
}
