//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: plt_rtos
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: 编译期 RTOS 抽象公共接口
// Revision: 0.02
// Revision: 0.01 - File Created
// Revision 0.02 - 栈改为字节、优先级改可移植枚举、补齐删除/延时/队列占用接口
// Keywords: compile-time-osal, opaque-handles, task/queue/mutex, stack-bytes, portable-priority, timeout-ms, lifecycle, from-isr-woken
//////////////////////////////////////////////////////////////////////////////////
#ifndef PLT_RTOS_H
#define PLT_RTOS_H

#include "common_types.h"

/** OSAL 返回码。 */
typedef enum {
	PLT_RTOS_OK = 0,              /**< success */
	PLT_RTOS_ERR_INVALID = -1,    /**< invalid argument */
	PLT_RTOS_ERR_NOMEM = -2,      /**< insufficient OS memory */
	PLT_RTOS_ERR_TIMEOUT = -3,    /**< bounded wait expired */
	PLT_RTOS_ERR_FULL = -4,       /**< queue full */
	PLT_RTOS_ERR_STATE = -5,      /**< wrong state */
	PLT_RTOS_ERR_UNSUPPORTED = -6 /**< backend does not implement the operation */
} plt_rtos_status_e;

/** Portable task priority levels; backend maps them to native priorities. */
typedef enum {
	PLT_RTOS_PRIO_IDLE = 0,    /**< idle/background */
	PLT_RTOS_PRIO_LOW = 1,     /**< low */
	PLT_RTOS_PRIO_NORMAL = 2,  /**< normal service */
	PLT_RTOS_PRIO_HIGH = 3,    /**< latency-sensitive service */
	PLT_RTOS_PRIO_REALTIME = 4 /**< highest portable level */
} plt_rtos_priority_e;

/* Opaque tokens: callers may store and pass them, never dereference them. */
typedef struct plt_rtos_task_token *plt_rtos_task_t;
typedef struct plt_rtos_queue_token *plt_rtos_queue_t;
typedef struct plt_rtos_mutex_token *plt_rtos_mutex_t;

/** ISR exit token; 0 means no context switch is required. */
typedef u32 plt_rtos_woken_t;

/** Task entry. */
typedef void (*plt_rtos_task_fn)(void *ctx);

/** ISR handler. */
typedef void (*plt_rtos_isr_fn)(void *ctx);

/**
 * @brief 初始化编译期 RTOS 后端。
 *
 * FreeRTOS 后端通常为空操作；fake 后端可在这里准备测试状态。
 *
 * @return PLT_RTOS_OK 或后端定义错误。
 * @author Xue Weisen
 */
plt_rtos_status_e plt_rtos_init(void);

/**
 * @brief 启动调度器。
 *
 * 成功启动后不返回；返回只表示启动失败。
 *
 * @return PLT_RTOS_ERR_STATE 或其他后端错误。
 * @warning 本函数成功时永不返回。
 * @author Xue Weisen
 */
plt_rtos_status_e plt_rtos_start(void);

/**
 * @brief 创建任务。
 *
 * @param fn 任务入口；不可为 NULL。
 * @param name 任务名；可为 NULL，由后端给默认名。
 * @param ctx 上下文指针；生命周期必须覆盖任务运行期。
 * @param stack_bytes 栈字节数；必须 > 0，后端按自身元素宽度向上取整。
 * @param priority 可移植优先级枚举；后端映射到原生优先级。
 * @param out_task 输出句柄；可为 NULL，但需要后续操作时应提供。
 * @return PLT_RTOS_OK / PLT_RTOS_ERR_INVALID / PLT_RTOS_ERR_NOMEM。
 * @author Xue Weisen
 */
plt_rtos_status_e plt_rtos_task_create(plt_rtos_task_fn fn,
                                       const char *name,
                                       void *ctx,
                                       u32 stack_bytes,
                                       plt_rtos_priority_e priority,
                                       plt_rtos_task_t *out_task);

/**
 * @brief 删除任务并释放后端资源。
 *
 * @param task 任务句柄；不可为 NULL。
 * @return PLT_RTOS_OK / PLT_RTOS_ERR_INVALID / PLT_RTOS_ERR_STATE。
 * @pre 不得删除正在运行的任务自身；组合根应在启动调度器前完成资源回收。
 * @note 可在任务上下文调用；是否允许删除任务按后端能力决定。
 * @author Xue Weisen
 */
plt_rtos_status_e plt_rtos_task_delete(plt_rtos_task_t task);

/**
 * @brief 让当前任务按毫秒延时。
 *
 * @param delay_ms 延时毫秒；0 = 不阻塞；正数至少延时 1 tick。
 * @return PLT_RTOS_OK / PLT_RTOS_ERR_STATE。
 * @pre 调度器已经运行；否则后端返回 STATE。
 * @note 只能在任务上下文调用，不可在 ISR 中调用。
 * @author Xue Weisen
 */
plt_rtos_status_e plt_rtos_task_delay_ms(u32 delay_ms);

/**
 * @brief 创建队列。
 *
 * @param depth 队列深度；必须 > 0。
 * @param item_size 单项字节数；必须 > 0。
 * @param out_queue 输出句柄；不可为 NULL。
 * @return PLT_RTOS_OK / PLT_RTOS_ERR_INVALID / PLT_RTOS_ERR_NOMEM。
 * @author Xue Weisen
 */
plt_rtos_status_e plt_rtos_queue_create(u32 depth,
                                        u32 item_size,
                                        plt_rtos_queue_t *out_queue);

/**
 * @brief 删除队列并释放后端资源。
 *
 * @param queue 队列句柄；不可为 NULL。
 * @return PLT_RTOS_OK / PLT_RTOS_ERR_INVALID。
 * @pre 队列中不再有正在等待的生产者或消费者。
 * @note 只能在任务上下文调用，不可在 ISR 中调用。
 * @author Xue Weisen
 */
plt_rtos_status_e plt_rtos_queue_delete(plt_rtos_queue_t queue);

/**
 * @brief 查询队列当前消息数。
 *
 * @param queue 队列句柄；不可为 NULL。
 * @param out_count 输出消息数；不可为 NULL，失败时不写。
 * @return PLT_RTOS_OK / PLT_RTOS_ERR_INVALID。
 * @note 快照值只用于观测；并发生产者/消费者下不能作为后续操作的保证。
 * @author Xue Weisen
 */
plt_rtos_status_e plt_rtos_queue_count(plt_rtos_queue_t queue,
                                       u32 *out_count);

/**
 * @brief 任务上下文发送一项。
 *
 * @param queue 队列句柄。
 * @param item 输入项；不可为 NULL。
 * @param timeout_ms 有限超时；0 = 不等待。
 * @return PLT_RTOS_OK；timeout_ms=0 且满时为 FULL；有等待且超时为 TIMEOUT；
 *         参数非法为 INVALID。
 * @note 禁止无限等待；调用方必须提供有限 timeout_ms。
 * @author Xue Weisen
 */
plt_rtos_status_e plt_rtos_queue_send(plt_rtos_queue_t queue,
                                      const void *item,
                                      u32 timeout_ms);

/**
 * @brief 任务上下文接收一项。
 *
 * @param queue 队列句柄。
 * @param item 输出存储；不可为 NULL。
 * @param timeout_ms 阻塞超时；0 = 不等待。
 * @return PLT_RTOS_OK / PLT_RTOS_ERR_TIMEOUT / PLT_RTOS_ERR_INVALID。
 * @note 禁止无限等待；调用方必须提供有限 timeout_ms。
 * @author Xue Weisen
 */
plt_rtos_status_e plt_rtos_queue_receive(plt_rtos_queue_t queue,
                                         void *item,
                                         u32 timeout_ms);

/**
 * @brief ISR 上下文发送一项。
 *
 * @param queue 队列句柄。
 * @param item 输入项；不可为 NULL。
 * @param woken 输出上下文切换请求；不可为 NULL。
 * @return PLT_RTOS_OK / PLT_RTOS_ERR_FULL / PLT_RTOS_ERR_INVALID。
 * @pre 只能在 ISR 上下文调用。
 * @author Xue Weisen
 */
plt_rtos_status_e plt_rtos_queue_send_from_isr(plt_rtos_queue_t queue,
                                               const void *item,
                                               plt_rtos_woken_t *woken);

/**
 * @brief 创建互斥量。
 *
 * @param out_mutex 输出句柄；不可为 NULL。
 * @return PLT_RTOS_OK / PLT_RTOS_ERR_INVALID / PLT_RTOS_ERR_NOMEM。
 * @author Xue Weisen
 */
plt_rtos_status_e plt_rtos_mutex_create(plt_rtos_mutex_t *out_mutex);

/**
 * @brief 删除互斥量并释放后端资源。
 *
 * @param mutex 互斥量句柄；不可为 NULL。
 * @return PLT_RTOS_OK / PLT_RTOS_ERR_INVALID / PLT_RTOS_ERR_STATE。
 * @pre 互斥量未处于锁定状态。
 * @note 只能在任务上下文调用，不可在 ISR 中调用。
 * @author Xue Weisen
 */
plt_rtos_status_e plt_rtos_mutex_delete(plt_rtos_mutex_t mutex);

/**
 * @brief 获取互斥量。
 *
 * @param mutex 互斥量句柄。
 * @param timeout_ms 有限超时；0 = 不等待。
 * @return PLT_RTOS_OK / PLT_RTOS_ERR_TIMEOUT / PLT_RTOS_ERR_INVALID。
 * @author Xue Weisen
 */
plt_rtos_status_e plt_rtos_mutex_lock(plt_rtos_mutex_t mutex, u32 timeout_ms);

/**
 * @brief 释放互斥量。
 *
 * @param mutex 互斥量句柄。
 * @return PLT_RTOS_OK / PLT_RTOS_ERR_INVALID / PLT_RTOS_ERR_STATE。
 * @author Xue Weisen
 */
plt_rtos_status_e plt_rtos_mutex_unlock(plt_rtos_mutex_t mutex);

/**
 * @brief 读取单调毫秒时间。
 *
 * @return 当前毫秒时间；32 bit 自然回绕，调用方用无符号差值比较。
 * @author Xue Weisen
 */
u32 plt_rtos_now_ms(void);

/**
 * @brief 安装 ISR。
 *
 * 公共 API 不暴露中断控制器类型；具体 Connect API 只出现在后端实现。
 *
 * @param irq 中断号。
 * @param handler ISR 函数；不可为 NULL。
 * @param ctx 上下文指针；生命周期覆盖注册期。
 * @return PLT_RTOS_OK / PLT_RTOS_ERR_INVALID / PLT_RTOS_ERR_STATE。
 * @author Xue Weisen
 */
plt_rtos_status_e plt_rtos_isr_connect(u32 irq,
                                       plt_rtos_isr_fn handler,
                                       void *ctx);

/**
 * @brief 使能中断。
 *
 * @param irq 中断号。
 * @return PLT_RTOS_OK / PLT_RTOS_ERR_INVALID。
 * @author Xue Weisen
 */
plt_rtos_status_e plt_rtos_isr_enable(u32 irq);

/**
 * @brief 禁用中断。
 *
 * @param irq 中断号。
 * @return PLT_RTOS_OK / PLT_RTOS_ERR_INVALID。
 * @author Xue Weisen
 */
plt_rtos_status_e plt_rtos_isr_disable(u32 irq);

/**
 * @brief ISR 退出时提交上下文切换请求。
 *
 * @param woken 由 queue_send_from_isr 产生的 token。
 * @pre 只能在 ISR 末尾调用一次。
 * @author Xue Weisen
 */
void plt_rtos_isr_exit(plt_rtos_woken_t woken);

#endif /* PLT_RTOS_H */
