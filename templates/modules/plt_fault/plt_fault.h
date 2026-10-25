//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: plt_fault
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: 无动态分配的故障记录环
// Revision: 0.01
// Revision: 0.01 - File Created
// Keywords: fault-ring, caller-owned-storage, last-event, optional-hook-ctx, observability
//////////////////////////////////////////////////////////////////////////////////
#ifndef PLT_FAULT_H
#define PLT_FAULT_H

#include "common_types.h"

/** 一条故障事件。 */
typedef struct {
	u32 timestamp_ms; /**< monotonic timestamp */
	u16 code;         /**< fault code */
	u16 flags;        /**< caller-defined flags */
	u32 detail;       /**< code-specific detail */
} plt_fault_record_t;

/** Optional fault observer; called in the raising context. */
typedef void (*plt_fault_hook_fn)(const plt_fault_record_t *record, void *ctx);

/**
 * @brief 绑定故障环和可选观察者。
 *
 * @param storage 调用方存储；容量必须 > 0。
 * @param capacity 条目数。
 * @param hook 可选观察者；可为 NULL。
 * @param ctx 观察者上下文；生命周期覆盖注册期。
 * @return 0 = 成功；-1 = 参数非法。
 * @note 故障记录不动态分配；共享访问由调用方加锁或规定单一上下文。
 * @author Xue Weisen
 */
int plt_fault_init(plt_fault_record_t *storage,
                   u32 capacity,
                   plt_fault_hook_fn hook,
                   void *ctx);

/**
 * @brief 记录一条故障。
 *
 * @param code 故障码。
 * @param flags 调用方标志。
 * @param detail 细节。
 * @param now_ms 当前单调毫秒。
 * @return 0 = 已记录；-1 = 未初始化或参数非法。
 * @note 不打印、不阻塞；可在 ISR 中调用，但调用方必须保证记录环互斥。
 * @author Xue Weisen
 */
int plt_fault_raise(u16 code, u16 flags, u32 detail, u32 now_ms);

/**
 * @brief 读取最近一条故障。
 *
 * @param out 输出记录；不可为 NULL。
 * @return 0 = 有记录；-1 = 无记录或参数非法。
 * @author Xue Weisen
 */
int plt_fault_last(plt_fault_record_t *out);

/**
 * @brief 取累计故障数。
 *
 * @return 累计记录数；未初始化返回 0。
 * @author Xue Weisen
 */
u32 plt_fault_count(void);

/**
 * @brief 取因环满被覆盖的旧记录数。
 *
 * @return 覆盖计数；未初始化返回 0。
 * @author Xue Weisen
 */
u32 plt_fault_dropped(void);

#endif /* PLT_FAULT_H */
