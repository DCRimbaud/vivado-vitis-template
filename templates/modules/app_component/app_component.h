//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: app_component
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: 主动组件抽象
// Revision: 0.02
// Revision 0.01 - File Created
// Revision 0.02 - 初始化改为事务式提交并补充去初始化接口
// Keywords: active-component, event-inbox, component-ops, transactional-init, deinit, step-handle-reset, osal-only
//////////////////////////////////////////////////////////////////////////////////
#ifndef APP_COMPONENT_H
#define APP_COMPONENT_H

#include "common_types.h"
#include "plt_rtos.h"

/** 组件结果码。 */
typedef enum {
	APP_COMPONENT_OK = 0,          /**< event handled / step completed */
	APP_COMPONENT_IDLE = 1,        /**< no event this cycle */
	APP_COMPONENT_ERR_INVALID = -1,/**< invalid argument */
	APP_COMPONENT_ERR_OS = -2,     /**< queue/OS failure */
	APP_COMPONENT_ERR_HANDLER = -3 /**< handler rejected event */
} app_component_rc_e;

/** 通用事件载荷。 */
typedef struct {
	u16 type;   /**< event type */
	u16 flags;  /**< event flags */
	u32 arg0;   /**< first argument */
	u32 arg1;   /**< second argument */
} app_event_t;

typedef struct app_component app_component_t;

/** 主动组件可替换行为。 */
typedef struct {
	app_component_rc_e (*handle_event)(app_component_t *self,
	                                   const app_event_t *event);
	app_component_rc_e (*step)(app_component_t *self, u32 now_ms);
	void (*reset)(app_component_t *self);
} app_component_ops_t;

/** 组件实例；由组合根创建并绑定。 */
struct app_component {
	const app_component_ops_t *ops; /**< shared const behavior table */
	plt_rtos_queue_t inbox;        /**< event inbox */
	const char *name;              /**< debug name */
	void *impl;                    /**< private implementation state */
};

/**
 * @brief 初始化主动组件并创建事件队列。
 *
 * @param self 组件对象；由调用方提供并保持生命周期。
 * @param name 调试名；可为 NULL。
 * @param ops 行为表；不可为 NULL，至少提供 step。
 * @param impl 私有实现指针；可为 NULL。
 * @param inbox_depth 事件队列深度；必须 > 0。
 * @return PLT_RTOS_OK / PLT_RTOS_ERR_INVALID / PLT_RTOS_ERR_NOMEM。
 * @author Xue Weisen
 */
plt_rtos_status_e app_component_init(app_component_t *self,
                                     const char *name,
                                     const app_component_ops_t *ops,
                                     void *impl,
                                     u32 inbox_depth);

/**
 * @brief 停止主动组件并释放事件队列。
 *
 * @param self 组件对象；可为 NULL（此时为空操作）。
 * @return PLT_RTOS_OK / PLT_RTOS_ERR_INVALID。
 * @pre 组件任务已停止，且没有生产者继续调用 post。
 * @note 只释放本模块创建的资源，不释放 impl 所指的调用方对象。
 * @author Xue Weisen
 */
plt_rtos_status_e app_component_deinit(app_component_t *self);

/**
 * @brief 从任务上下文投递事件。
 *
 * @param self 组件对象。
 * @param event 事件；不可为 NULL，函数只读。
 * @param timeout_ms 有限超时。
 * @return PLT_RTOS_OK / PLT_RTOS_ERR_TIMEOUT / PLT_RTOS_ERR_INVALID。
 * @author Xue Weisen
 */
plt_rtos_status_e app_component_post(app_component_t *self,
                                     const app_event_t *event,
                                     u32 timeout_ms);

/**
 * @brief 从 ISR 上下文投递事件。
 *
 * @param self 组件对象。
 * @param event 事件；不可为 NULL，函数只读。
 * @param woken 上下文切换 token；由调用方初始化。
 * @return PLT_RTOS_OK / PLT_RTOS_ERR_FULL / PLT_RTOS_ERR_INVALID。
 * @author Xue Weisen
 */
plt_rtos_status_e app_component_post_from_isr(app_component_t *self,
                                              const app_event_t *event,
                                              plt_rtos_woken_t *woken);

/**
 * @brief 执行一次组件轮询。
 *
 * 先按 wait_ms 取一个事件，再调用 handle_event 和 step；无事件时仍调用 step。
 *
 * @param self 组件对象。
 * @param wait_ms 取事件超时；0 = 不等待。
 * @return 组件处理结果；参数非法返回 APP_COMPONENT_ERR_INVALID。
 * @author Xue Weisen
 */
app_component_rc_e app_component_run_once(app_component_t *self, u32 wait_ms);

#endif /* APP_COMPONENT_H */
