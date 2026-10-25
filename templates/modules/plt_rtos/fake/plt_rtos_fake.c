//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: plt_rtos_fake
// Target Devices: host only
// Tool Versions: host compiler
// Description: host 测试用最小 RTOS 后端
// Revision: 0.02
// Revision: 0.01 - File Created
// Revision 0.02 - 时间改为显式注入/推进，补齐资源删除、队列观测与阻塞超时仿真
// Keywords: host-fake-osal, explicit-clock, queue-ring, no-scheduler, lifecycle, timeout-simulation, explicit-host-only
//////////////////////////////////////////////////////////////////////////////////
#if !defined(PLT_RTOS_FAKE_HOST)
#error "plt_rtos_fake is host-test only; define PLT_RTOS_FAKE_HOST=1"
#endif

#include <stdlib.h>
#include <string.h>

#include "plt_rtos.h"
#include "plt_rtos_fake.h"

struct plt_rtos_task_token {
	u32 id;
};

struct plt_rtos_queue_token {
	u32 depth;
	u32 item_size;
	u32 head;
	u32 count;
	u8 *storage;
};

struct plt_rtos_mutex_token {
	u32 locked;
};

static u32 s_fake_now_ms;

/**
 * @brief 初始化 fake 后端。
 *
 * @return PLT_RTOS_OK。
 * @note 仅 host 测试编译；通过 PLT_RTOS_FAKE_HOST 显式启用。
 * @author Xue Weisen
 */
plt_rtos_status_e plt_rtos_init(void)
{
	s_fake_now_ms = 0u;
	return PLT_RTOS_OK;
}

/**
 * @brief fake 后端不启动真实调度器。
 *
 * @return PLT_RTOS_OK。
 * @note 测试直接调用组件 step/handle_event，不依赖调度。
 * @author Xue Weisen
 */
plt_rtos_status_e plt_rtos_start(void)
{
	/* fake 后端不创建调度器；返回 UNSUPPORTED，禁止测试把空操作误当启动成功。 */
	return PLT_RTOS_ERR_UNSUPPORTED;
}

/**
 * @brief 创建 fake 任务 token。
 *
 * @param fn 任务入口；仅用于参数契约检查。
 * @param name 任务名；未使用。
 * @param ctx 上下文；未使用。
 * @param stack_bytes 栈字节数；未使用。
 * @param priority 可移植优先级；未使用。
 * @param out_task 输出 token。
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
	plt_rtos_task_t token;

	(void)name; (void)ctx; (void)stack_bytes; (void)priority;
	if ((fn == 0) || (out_task == 0)) {
		return PLT_RTOS_ERR_INVALID;
	}
	token = (plt_rtos_task_t)calloc(1u, sizeof(*token));
	if (token == 0) {
		return PLT_RTOS_ERR_NOMEM;
	}
	*out_task = token;
	return PLT_RTOS_OK;
}

/**
 * @brief 释放 fake 任务 token。
 *
 * @param task 任务 token。
 * @return PLT_RTOS_OK / PLT_RTOS_ERR_INVALID。
 * @note fake 不执行任务体，只验证组合根的资源生命周期。
 * @author Xue Weisen
 */
plt_rtos_status_e plt_rtos_task_delete(plt_rtos_task_t task)
{
	/* 空 token 无人持有，直接拒绝；有效 token 释放后调用方不得继续使用。 */
	if (task == 0) {
		return PLT_RTOS_ERR_INVALID;
	}
	free(task);
	return PLT_RTOS_OK;
}

/**
 * @brief 在 fake 时间线上同步推进一次任务延时。
 *
 * @param delay_ms 延时毫秒。
 * @return PLT_RTOS_OK。
 * @note 只用于单线程测试；不模拟任务切换或真实阻塞。
 * @author Xue Weisen
 */
plt_rtos_status_e plt_rtos_task_delay_ms(u32 delay_ms)
{
	/* fake 直接推进逻辑时钟，使超时分支可重复测试。 */
	s_fake_now_ms += delay_ms;
	return PLT_RTOS_OK;
}

/**
 * @brief 创建 fake 队列并分配存储。
 *
 * @param depth 队列深度。
 * @param item_size 单项字节数。
 * @param out_queue 输出队列。
 * @return PLT_RTOS_OK / PLT_RTOS_ERR_INVALID / PLT_RTOS_ERR_NOMEM。
 * @author Xue Weisen
 */
plt_rtos_status_e plt_rtos_queue_create(u32 depth,
                                        u32 item_size,
                                        plt_rtos_queue_t *out_queue)
{
	plt_rtos_queue_t queue;

	/* 参数校验：深度、项宽和输出指针缺一不可，失败时不分配任何资源。 */
	if ((depth == 0u) || (item_size == 0u) || (out_queue == 0)) {
		return PLT_RTOS_ERR_INVALID;
	}
	/* 控制块分配：先创建队列 token，再按深度和项宽分配独立存储。 */
	queue = (plt_rtos_queue_t)calloc(1u, sizeof(*queue));
	if (queue == 0) {
		return PLT_RTOS_ERR_NOMEM;
	}
	queue->storage = (u8 *)calloc((size_t)depth, (size_t)item_size);
	if (queue->storage == 0) {
		/* 失败回滚：存储分配失败时释放刚创建的控制块，保持调用方输出不变。 */
		free(queue);
		return PLT_RTOS_ERR_NOMEM;
	}
	/* 提交阶段：容量字段和存储都就绪后才发布队列句柄。 */
	queue->depth = depth;
	queue->item_size = item_size;
	*out_queue = queue;
	return PLT_RTOS_OK;
}

/**
 * @brief 释放 fake 队列及其存储。
 *
 * @param queue 队列 token。
 * @return PLT_RTOS_OK / PLT_RTOS_ERR_INVALID。
 * @note 仅 host 测试使用；删除后所有句柄失效。
 * @author Xue Weisen
 */
plt_rtos_status_e plt_rtos_queue_delete(plt_rtos_queue_t queue)
{
	/* 先释放 payload 存储，再释放控制块，避免部分释放后指针不可达。 */
	if (queue == 0) {
		return PLT_RTOS_ERR_INVALID;
	}
	free(queue->storage);
	free(queue);
	return PLT_RTOS_OK;
}

/**
 * @brief 查询 fake 队列当前消息数。
 *
 * @param queue 队列 token。
 * @param out_count 输出消息数。
 * @return PLT_RTOS_OK / PLT_RTOS_ERR_INVALID。
 * @note 结果用于测试断言；不模拟并发快照语义。
 * @author Xue Weisen
 */
plt_rtos_status_e plt_rtos_queue_count(plt_rtos_queue_t queue,
                                       u32 *out_count)
{
	/* 输出参数只在完整校验通过后写入，保持失败不写约定。 */
	if ((queue == 0) || (out_count == 0)) {
		return PLT_RTOS_ERR_INVALID;
	}
	*out_count = queue->count;
	return PLT_RTOS_OK;
}

/**
 * @brief 向 fake 队列发送一项。
 *
 * @param queue 队列。
 * @param item 输入项。
 * @param timeout_ms 队列满时的逻辑等待预算；fake 会推进时间模拟超时。
 * @return PLT_RTOS_OK / PLT_RTOS_ERR_FULL / PLT_RTOS_ERR_INVALID。
 * @author Xue Weisen
 */
plt_rtos_status_e plt_rtos_queue_send(plt_rtos_queue_t queue,
                                      const void *item,
                                      u32 timeout_ms)
{
	plt_rtos_woken_t woken = 0u;
	plt_rtos_status_e st;

	/* fake 后端用一次性逻辑时间推进模拟有限等待，再按等待预算区分 FULL/TIMEOUT。 */
	st = plt_rtos_queue_send_from_isr(queue, item, &woken);
	if ((st == PLT_RTOS_ERR_FULL) && (timeout_ms > 0u)) {
		s_fake_now_ms += timeout_ms;
		return PLT_RTOS_ERR_TIMEOUT;
	}
	return st;
}

/**
 * @brief 从 fake 队列非阻塞取一项。
 *
 * @param queue 队列。
 * @param item 输出项。
 * @param timeout_ms 空队列时的逻辑等待预算；fake 会推进时间模拟超时。
 * @return PLT_RTOS_OK / PLT_RTOS_ERR_TIMEOUT / PLT_RTOS_ERR_INVALID。
 * @author Xue Weisen
 */
plt_rtos_status_e plt_rtos_queue_receive(plt_rtos_queue_t queue,
                                         void *item,
                                         u32 timeout_ms)
{
	u32 index;

	if ((queue == 0) || (item == 0)) {
		return PLT_RTOS_ERR_INVALID;
	}
	if (queue->count == 0u) {
		s_fake_now_ms += timeout_ms;
		return PLT_RTOS_ERR_TIMEOUT;
	}
	index = queue->head;
	(void)memcpy(item, queue->storage + (index * queue->item_size), queue->item_size);
	queue->head = (queue->head + 1u) % queue->depth;
	queue->count--;
	return PLT_RTOS_OK;
}

/**
 * @brief 向 fake 队列发送一项。
 *
 * @param queue 队列。
 * @param item 输入项。
 * @param woken 输出 token；fake 永远为 0。
 * @return PLT_RTOS_OK / PLT_RTOS_ERR_FULL / PLT_RTOS_ERR_INVALID。
 * @author Xue Weisen
 */
plt_rtos_status_e plt_rtos_queue_send_from_isr(plt_rtos_queue_t queue,
                                               const void *item,
                                               plt_rtos_woken_t *woken)
{
	u32 index;

	if ((queue == 0) || (item == 0) || (woken == 0)) {
		return PLT_RTOS_ERR_INVALID;
	}
	*woken = 0u;
	if (queue->count >= queue->depth) {
		return PLT_RTOS_ERR_FULL;
	}
	index = (queue->head + queue->count) % queue->depth;
	(void)memcpy(queue->storage + (index * queue->item_size), item, queue->item_size);
	queue->count++;
	*woken = 0u;
	return PLT_RTOS_OK;
}

/**
 * @brief 释放 fake 互斥量。
 *
 * @param mutex 互斥量 token。
 * @return PLT_RTOS_OK / PLT_RTOS_ERR_INVALID / PLT_RTOS_ERR_STATE。
 * @note 锁定中的互斥量拒绝删除，避免测试掩盖泄漏。
 * @author Xue Weisen
 */
plt_rtos_status_e plt_rtos_mutex_delete(plt_rtos_mutex_t mutex)
{
	/* 锁定状态说明持有者尚未配对释放，不能把泄漏当成成功删除。 */
	if (mutex == 0) {
		return PLT_RTOS_ERR_INVALID;
	}
	if (mutex->locked != 0u) {
		return PLT_RTOS_ERR_STATE;
	}
	free(mutex);
	return PLT_RTOS_OK;
}

/**
 * @brief 创建 fake 互斥量。
 *
 * @param out_mutex 输出互斥量。
 * @return PLT_RTOS_OK / PLT_RTOS_ERR_INVALID / PLT_RTOS_ERR_NOMEM。
 * @author Xue Weisen
 */
plt_rtos_status_e plt_rtos_mutex_create(plt_rtos_mutex_t *out_mutex)
{
	plt_rtos_mutex_t mutex;

	if (out_mutex == 0) {
		return PLT_RTOS_ERR_INVALID;
	}
	mutex = (plt_rtos_mutex_t)calloc(1u, sizeof(*mutex));
	if (mutex == 0) {
		return PLT_RTOS_ERR_NOMEM;
	}
	*out_mutex = mutex;
	return PLT_RTOS_OK;
}

/**
 * @brief 获取 fake 互斥量。
 *
 * @param mutex 互斥量。
 * @param timeout_ms 锁定时的逻辑等待预算；fake 会推进时间模拟超时。
 * @return PLT_RTOS_OK / PLT_RTOS_ERR_TIMEOUT / PLT_RTOS_ERR_INVALID。
 * @author Xue Weisen
 */
plt_rtos_status_e plt_rtos_mutex_lock(plt_rtos_mutex_t mutex, u32 timeout_ms)
{
	if (mutex == 0) {
		return PLT_RTOS_ERR_INVALID;
	}
	if (mutex->locked != 0u) {
		s_fake_now_ms += timeout_ms;
		return PLT_RTOS_ERR_TIMEOUT;
	}
	mutex->locked = 1u;
	return PLT_RTOS_OK;
}

/**
 * @brief 释放 fake 互斥量。
 *
 * @param mutex 互斥量。
 * @return PLT_RTOS_OK / PLT_RTOS_ERR_INVALID / PLT_RTOS_ERR_STATE。
 * @author Xue Weisen
 */
plt_rtos_status_e plt_rtos_mutex_unlock(plt_rtos_mutex_t mutex)
{
	if (mutex == 0) {
		return PLT_RTOS_ERR_INVALID;
	}
	if (mutex->locked == 0u) {
		return PLT_RTOS_ERR_STATE;
	}
	mutex->locked = 0u;
	return PLT_RTOS_OK;
}

/**
 * @brief 返回 fake 单调毫秒值。
 *
 * @return 当前逻辑毫秒。
 * @note 时间只由显式 fake API 或模拟超时推进，读取不产生副作用。
 * @author Xue Weisen
 */
u32 plt_rtos_now_ms(void)
{
	return s_fake_now_ms;
}

/**
 * @brief 设置 fake 单调毫秒时钟。
 *
 * @param now_ms 新的逻辑时间。
 * @note 仅 host 测试使用。
 * @author Xue Weisen
 */
void plt_rtos_fake_set_now_ms(u32 now_ms)
{
	s_fake_now_ms = now_ms;
}

/**
 * @brief 推进 fake 单调毫秒时钟。
 *
 * @param delta_ms 增量，按 u32 自然回绕。
 * @note 仅 host 测试使用。
 * @author Xue Weisen
 */
void plt_rtos_fake_advance_ms(u32 delta_ms)
{
	s_fake_now_ms += delta_ms;
}

/**
 * @brief fake 中断安装空操作。
 *
 * @param irq 中断号。
 * @param handler ISR。
 * @param ctx 上下文。
 * @return PLT_RTOS_OK / PLT_RTOS_ERR_INVALID。
 * @note 测试应直接调用 ISR 或注入事件，不依赖中断控制器。
 * @author Xue Weisen
 */
plt_rtos_status_e plt_rtos_isr_connect(u32 irq,
                                       plt_rtos_isr_fn handler,
                                       void *ctx)
{
	(void)irq; (void)ctx;
	return ((handler != 0) ? PLT_RTOS_OK : PLT_RTOS_ERR_INVALID);
}

/**
 * @brief fake 中断使能空操作。
 *
 * @param irq 中断号。
 * @return PLT_RTOS_OK。
 * @author Xue Weisen
 */
plt_rtos_status_e plt_rtos_isr_enable(u32 irq)
{
	(void)irq;
	return PLT_RTOS_OK;
}

/**
 * @brief fake 中断禁用空操作。
 *
 * @param irq 中断号。
 * @return PLT_RTOS_OK。
 * @author Xue Weisen
 */
plt_rtos_status_e plt_rtos_isr_disable(u32 irq)
{
	(void)irq;
	return PLT_RTOS_OK;
}

/**
 * @brief fake ISR 退出空操作。
 *
 * @param woken 忽略。
 * @author Xue Weisen
 */
void plt_rtos_isr_exit(plt_rtos_woken_t woken)
{
	(void)woken;
}
