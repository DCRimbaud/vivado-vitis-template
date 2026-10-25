//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: mw_xfer_rtos
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: 跨上下文描述符队列实现
// Revision: 0.01
// Revision 0.01 - File Created
// Keywords: rtos-descriptor-queue-impl, osal-only-dependency, metadata-copy,
//   finite-timeout, isr-no-allocation, explicit-ownership
//////////////////////////////////////////////////////////////////////////////////
#include "mw_xfer_rtos.h"

/**
 * @brief 将 OSAL 队列结果映射为本模块的域返回码。
 *
 * @param st OSAL 返回码。
 * @param timeout_ms 原始有限超时。
 * @return 映射后的描述符队列返回码。
 * @author Xue Weisen
 */
static mw_xfer_rtos_rc_e mw_xfer_rtos_map_send(plt_rtos_status_e st,
                                               u32 timeout_ms)
{
	/* Preserve the distinction between immediate-full and waited-timeout. */
	if (st == PLT_RTOS_OK) {
		return MW_XFER_RTOS_OK;
	}
	if (st == PLT_RTOS_ERR_FULL) {
		return MW_XFER_RTOS_ERR_FULL;
	}
	if (st == PLT_RTOS_ERR_TIMEOUT) {
		return (timeout_ms == 0u) ? MW_XFER_RTOS_ERR_FULL
		                          : MW_XFER_RTOS_ERR_TIMEOUT;
	}
	if (st == PLT_RTOS_ERR_INVALID) {
		return MW_XFER_RTOS_ERR_INVALID;
	}
	return MW_XFER_RTOS_ERR_STATE;
}

/**
 * @brief 创建承载 common_xfer_t 元数据的 OS 队列。
 *
 * @param q 控制块。
 * @param depth 描述符容量。
 * @return 队列返回码。
 * @author Xue Weisen
 */
mw_xfer_rtos_rc_e mw_xfer_rtos_init(mw_xfer_rtos_t *q, u32 depth)
{
	plt_rtos_queue_t queue = 0;
	plt_rtos_status_e st;

	/* 入参校验：容量非法或重复初始化时不得创建新的 OS 资源。 */
	if ((q == 0) || (depth == 0u)) {
		return MW_XFER_RTOS_ERR_INVALID;
	}
	if ((q->initialized != 0u) || (q->queue != 0)) {
		return MW_XFER_RTOS_ERR_STATE;
	}

	/* 创建阶段：队列项宽由公共描述符契约固定，失败按无内存或参数错误上抛。 */
	st = plt_rtos_queue_create(depth, (u32)sizeof(common_xfer_t), &queue);
	if (st != PLT_RTOS_OK) {
		return (st == PLT_RTOS_ERR_INVALID) ? MW_XFER_RTOS_ERR_INVALID
		                                    : MW_XFER_RTOS_ERR_NOMEM;
	}

	/* 提交阶段：仅在 OS 队列创建成功后发布句柄、容量和统计，避免半初始化对象。 */
	q->queue = queue;
	q->depth = depth;
	q->drop_count = 0u;
	q->initialized = 1u;
	return MW_XFER_RTOS_OK;
}

/**
 * @brief 删除 OS 队列并清空控制块。
 *
 * @param q 控制块。
 * @return 队列返回码。
 * @author Xue Weisen
 */
mw_xfer_rtos_rc_e mw_xfer_rtos_deinit(mw_xfer_rtos_t *q)
{
	plt_rtos_status_e st;

	/* Unbound state is a programming error, not a successful cleanup. */
	if ((q == 0) || (q->initialized == 0u) || (q->queue == 0)) {
		return MW_XFER_RTOS_ERR_INVALID;
	}
	st = plt_rtos_queue_delete(q->queue);
	if (st != PLT_RTOS_OK) {
		return MW_XFER_RTOS_ERR_STATE;
	}
	q->queue = 0;
	q->depth = 0u;
	q->drop_count = 0u;
	q->initialized = 0u;
	return MW_XFER_RTOS_OK;
}

/**
 * @brief 从任务上下文发送一个描述符。
 *
 * @param q 队列。
 * @param xfer 描述符。
 * @param timeout_ms 有限超时。
 * @return 队列返回码。
 * @author Xue Weisen
 */
mw_xfer_rtos_rc_e mw_xfer_rtos_send(mw_xfer_rtos_t *q,
                                    const common_xfer_t *xfer,
                                    u32 timeout_ms)
{
	plt_rtos_status_e st;
	mw_xfer_rtos_rc_e rc;

	/* Only a live queue and a self-consistent descriptor may cross the boundary. */
	if ((q == 0) || (q->initialized == 0u) || (q->queue == 0) ||
	    (xfer == 0) || (common_xfer_valid(xfer) == 0)) {
		return MW_XFER_RTOS_ERR_INVALID;
	}

	st = plt_rtos_queue_send(q->queue, xfer, timeout_ms);
	rc = mw_xfer_rtos_map_send(st, timeout_ms);
	if (rc != MW_XFER_RTOS_OK) {
		q->drop_count++;
	}
	return rc;
}

/**
 * @brief 从 ISR 上下文发送一个描述符。
 *
 * @param q 队列。
 * @param xfer 预分配描述符。
 * @param woken 上下文切换 token。
 * @return 队列返回码。
 * @author Xue Weisen
 */
mw_xfer_rtos_rc_e mw_xfer_rtos_send_from_isr(mw_xfer_rtos_t *q,
                                             const common_xfer_t *xfer,
                                             plt_rtos_woken_t *woken)
{
	plt_rtos_status_e st;

	/* ISR 校验：中断路径只允许有限操作，不分配内存、不修改共享统计。 */
	if ((q == 0) || (q->initialized == 0u) || (q->queue == 0) ||
	    (xfer == 0) || (woken == 0) || (common_xfer_valid(xfer) == 0)) {
		return MW_XFER_RTOS_ERR_INVALID;
	}

	st = plt_rtos_queue_send_from_isr(q->queue, xfer, woken);
	/* 映射阶段：保留满、非法和状态错误，禁止把 ISR 投递失败伪装成成功。 */
	if (st == PLT_RTOS_OK) {
		return MW_XFER_RTOS_OK;
	}
	if (st == PLT_RTOS_ERR_FULL) {
		return MW_XFER_RTOS_ERR_FULL;
	}
	if (st == PLT_RTOS_ERR_INVALID) {
		return MW_XFER_RTOS_ERR_INVALID;
	}
	return MW_XFER_RTOS_ERR_STATE;
}

/**
 * @brief 从任务上下文接收一个描述符。
 *
 * @param q 队列。
 * @param out 输出描述符。
 * @param timeout_ms 有限超时。
 * @return 队列返回码。
 * @author Xue Weisen
 */
mw_xfer_rtos_rc_e mw_xfer_rtos_receive(mw_xfer_rtos_t *q,
                                       common_xfer_t *out,
                                       u32 timeout_ms)
{
	plt_rtos_status_e st;

	/* 入参校验：队列或输出无效时不得触碰输出描述符。 */
	if ((q == 0) || (q->initialized == 0u) || (q->queue == 0) || (out == 0)) {
		return MW_XFER_RTOS_ERR_INVALID;
	}

	st = plt_rtos_queue_receive(q->queue, out, timeout_ms);
	/* 接收阶段：只有 OS 队列确认取到项后才认为输出有效，超时和空队列分开映射。 */
	if (st == PLT_RTOS_OK) {
		return MW_XFER_RTOS_OK;
	}
	if (st == PLT_RTOS_ERR_TIMEOUT) {
		return MW_XFER_RTOS_ERR_TIMEOUT;
	}
	if (st == PLT_RTOS_ERR_INVALID) {
		return MW_XFER_RTOS_ERR_INVALID;
	}
	return MW_XFER_RTOS_ERR_STATE;
}

/**
 * @brief 查询当前描述符数量。
 *
 * @param q 队列。
 * @param out_count 输出数量。
 * @return 队列返回码。
 * @author Xue Weisen
 */
mw_xfer_rtos_rc_e mw_xfer_rtos_count(const mw_xfer_rtos_t *q,
                                     u32 *out_count)
{
	plt_rtos_status_e st;

	/* The OS query is a snapshot; callers cannot reserve based on this value. */
	if ((q == 0) || (q->initialized == 0u) || (q->queue == 0) ||
	    (out_count == 0)) {
		return MW_XFER_RTOS_ERR_INVALID;
	}
	st = plt_rtos_queue_count(q->queue, out_count);
	if (st == PLT_RTOS_OK) {
		return MW_XFER_RTOS_OK;
	}
	return (st == PLT_RTOS_ERR_INVALID) ? MW_XFER_RTOS_ERR_INVALID
	                                   : MW_XFER_RTOS_ERR_STATE;
}

/**
 * @brief 返回任务上下文发送失败/丢弃计数。
 *
 * @param q 队列；NULL 返回 0。
 * @return 任务侧累计失败计数。
 * @author Xue Weisen
 */
u32 mw_xfer_rtos_drop_count(const mw_xfer_rtos_t *q)
{
	return ((q != 0) ? q->drop_count : 0u);
}
