//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: plt_fault
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: 故障记录环实现
// Revision: 0.01
// Revision: 0.01 - File Created
// Keywords: fault-ring-impl, caller-owned, overwrite-oldest, hook-after-store, no-print
//////////////////////////////////////////////////////////////////////////////////
#include "plt_fault.h"

static plt_fault_record_t *s_storage;
static u32 s_capacity;
static u32 s_head;
static u32 s_count;
static u32 s_total;
static u32 s_dropped;
static plt_fault_hook_fn s_hook;
static void *s_hook_ctx;

/**
 * @brief 绑定故障环。
 *
 * @param storage 调用方存储。
 * @param capacity 容量。
 * @param hook 可选观察者。
 * @param ctx 观察者上下文。
 * @return 0 = 成功；-1 = 参数非法。
 * @author Xue Weisen
 */
int plt_fault_init(plt_fault_record_t *storage,
                   u32 capacity,
                   plt_fault_hook_fn hook,
                   void *ctx)
{
	/* 绑定阶段只借存储，不复制、不分配；空存储或零容量直接拒绝。 */
	if ((storage == 0) || (capacity == 0u)) {
		return -1;
	}
	s_storage = storage;
	s_capacity = capacity;
	s_head = 0u;
	s_count = 0u;
	s_total = 0u;
	s_dropped = 0u;
	s_hook = hook;
	s_hook_ctx = ctx;
	return 0;
}

/**
 * @brief 记录一条故障。
 *
 * @param code 故障码。
 * @param flags 标志。
 * @param detail 细节。
 * @param now_ms 当前毫秒。
 * @return 0 = 已记录；-1 = 未初始化/参数非法。
 * @author Xue Weisen
 */
int plt_fault_raise(u16 code, u16 flags, u32 detail, u32 now_ms)
{
	plt_fault_record_t record;
	u32 index;

	/* 记录阶段：保留最后 capacity 条，旧记录被覆盖时单独计数。 */
	if ((s_storage == 0) || (s_capacity == 0u)) {
		return -1;
	}
	record.timestamp_ms = now_ms;
	record.code = code;
	record.flags = flags;
	record.detail = detail;
	index = s_head;
	s_storage[index] = record;
	s_head = (s_head + 1u) % s_capacity;
	if (s_count < s_capacity) {
		s_count++;
	} else {
		s_dropped++;
	}
	s_total++;

	/* 观察阶段：先落环再通知，hook 不应改变记录事实；hook 内禁止长阻塞。 */
	if (s_hook != 0) {
		s_hook(&record, s_hook_ctx);
	}
	return 0;
}

/**
 * @brief 读取最近一条故障。
 *
 * @param out 输出记录。
 * @return 0 = 有记录；-1 = 无记录/参数非法。
 * @author Xue Weisen
 */
int plt_fault_last(plt_fault_record_t *out)
{
	u32 index;

	/* 空环不返回伪记录；读取方必须按返回码决定是否展示。 */
	if ((out == 0) || (s_count == 0u) || (s_storage == 0)) {
		return -1;
	}
	index = (s_head + s_capacity - 1u) % s_capacity;
	*out = s_storage[index];
	return 0;
}

/**
 * @brief 取累计故障数。
 *
 * @return 累计故障数。
 * @author Xue Weisen
 */
u32 plt_fault_count(void)
{
	return s_total;
}

/**
 * @brief 取被覆盖的旧记录数。
 *
 * @return 覆盖计数。
 * @author Xue Weisen
 */
u32 plt_fault_dropped(void)
{
	return s_dropped;
}
