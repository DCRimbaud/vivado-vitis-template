//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: app_component
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: 主动组件实现
// Revision: 0.02
// Revision: 0.01 - File Created
// Revision 0.02 - 初始化改为事务式提交并补充去初始化
// Keywords: active-component-impl, transactional-init, queue-lifecycle, event-dispatch, bounded-wait, step-each-cycle
//////////////////////////////////////////////////////////////////////////////////
#include "app_component.h"

/**
 * @brief 初始化组件和事件队列。
 *
 * @param self 组件对象。
 * @param name 调试名。
 * @param ops 行为表。
 * @param impl 私有状态。
 * @param inbox_depth 队列深度。
 * @return PLT_RTOS_OK / PLT_RTOS_ERR_INVALID / PLT_RTOS_ERR_NOMEM。
 * @author Xue Weisen
 */
plt_rtos_status_e app_component_init(app_component_t *self,
                                     const char *name,
                                     const app_component_ops_t *ops,
                                     void *impl,
                                     u32 inbox_depth)
{
	plt_rtos_status_e st;
	plt_rtos_queue_t inbox = 0;

	/* 校验阶段：组件行为和队列深度是运行时必需条件，缺失直接拒绝。 */
	if ((self == 0) || (ops == 0) || (ops->step == 0) || (inbox_depth == 0u)) {
		return PLT_RTOS_ERR_INVALID;
	}
	/* 创建阶段：先在局部句柄完成 OS 分配，失败时对象保持调用前状态。 */
	st = plt_rtos_queue_create(inbox_depth, sizeof(app_event_t), &inbox);
	if (st != PLT_RTOS_OK) {
		return st;
	}

	/* 提交阶段：队列创建成功后才公开行为表、上下文与句柄，避免半初始化。 */
	self->ops = ops;
	self->name = (name != 0) ? name : "component";
	self->impl = impl;
	self->inbox = inbox;
	return PLT_RTOS_OK;
}

/**
 * @brief 停止主动组件并释放事件队列。
 *
 * @param self 组件对象。
 * @return PLT_RTOS_OK / PLT_RTOS_ERR_INVALID。
 * @note impl 由调用方拥有，本函数不释放。
 * @author Xue Weisen
 */
plt_rtos_status_e app_component_deinit(app_component_t *self)
{
	plt_rtos_status_e st;

	/* 空对象视为已经去初始化，便于组合根统一清理失败路径。 */
	if (self == 0) {
		return PLT_RTOS_OK;
	}

	/* 释放阶段：先删队列，成功后才清空公开字段，失败时保留现场供重试。 */
	if (self->inbox != 0) {
		st = plt_rtos_queue_delete(self->inbox);
		if (st != PLT_RTOS_OK) {
			return st;
		}
	}
	self->ops = 0;
	self->inbox = 0;
	self->name = 0;
	self->impl = 0;
	return PLT_RTOS_OK;
}

/**
 * @brief 从任务上下文投递事件。
 *
 * @param self 组件对象。
 * @param event 事件。
 * @param timeout_ms 有限超时。
 * @return PLT_RTOS_OK / PLT_RTOS_ERR_TIMEOUT / PLT_RTOS_ERR_INVALID。
 * @author Xue Weisen
 */
plt_rtos_status_e app_component_post(app_component_t *self,
                                     const app_event_t *event,
                                     u32 timeout_ms)
{
	/* 任务上下文用普通发送路径；当前 OSAL 先用“非 ISR 发送”由后端提供。 */
	if ((self == 0) || (event == 0) || (self->inbox == 0)) {
		return PLT_RTOS_ERR_INVALID;
	}
	return plt_rtos_queue_send(self->inbox, event, timeout_ms);
}

/**
 * @brief 从 ISR 上下文投递事件。
 *
 * @param self 组件对象。
 * @param event 事件。
 * @param woken 上下文切换 token。
 * @return PLT_RTOS_OK / PLT_RTOS_ERR_FULL / PLT_RTOS_ERR_INVALID。
 * @author Xue Weisen
 */
plt_rtos_status_e app_component_post_from_isr(app_component_t *self,
                                              const app_event_t *event,
                                              plt_rtos_woken_t *woken)
{
	/* ISR 路径只投事件，不执行 handle_event/step，避免在中断里跑状态机。 */
	if ((self == 0) || (event == 0) || (woken == 0) || (self->inbox == 0)) {
		return PLT_RTOS_ERR_INVALID;
	}
	return plt_rtos_queue_send_from_isr(self->inbox, event, woken);
}

/**
 * @brief 执行一次组件轮询。
 *
 * @param self 组件对象。
 * @param wait_ms 取事件超时。
 * @return 组件处理结果。
 * @author Xue Weisen
 */
app_component_rc_e app_component_run_once(app_component_t *self, u32 wait_ms)
{
	app_event_t event;
	plt_rtos_status_e st;
	app_component_rc_e rc = APP_COMPONENT_OK;

	/* 校验阶段：行为表由 init 绑定；未初始化对象直接拒绝。 */
	if ((self == 0) || (self->ops == 0) || (self->ops->step == 0)) {
		return APP_COMPONENT_ERR_INVALID;
	}

	/* 事件阶段：有事件就处理；超时只表示本拍空闲，不是错误。 */
	st = plt_rtos_queue_receive(self->inbox, &event, wait_ms);
	if (st == PLT_RTOS_OK) {
		if (self->ops->handle_event != 0) {
			rc = self->ops->handle_event(self, &event);
			if (rc != APP_COMPONENT_OK) {
				return rc;
			}
		}
	} else if (st != PLT_RTOS_ERR_TIMEOUT) {
		return APP_COMPONENT_ERR_OS;
	}

	/* 推进阶段：每个周期都执行 step，处理定时器、状态机和周期任务。 */
	return self->ops->step(self, plt_rtos_now_ms());
}
