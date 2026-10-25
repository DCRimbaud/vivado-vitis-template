//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: mw_xfer_rtos
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: 跨任务/ISR 的定长描述符队列，不复制 payload
// Revision: 0.01
// Revision 0.01 - File Created
// Keywords: rtos-descriptor-queue, metadata-only, ownership-handoff,
//   timeout-ms, isr-send, common-xfer, output-preserved-on-failure
//////////////////////////////////////////////////////////////////////////////////
#ifndef MW_XFER_RTOS_H
#define MW_XFER_RTOS_H

#include "common_types.h"
#include "common_xfer.h"
#include "plt_rtos.h"

/** 描述符队列返回码。 */
typedef enum {
	MW_XFER_RTOS_OK = 0,            /**< operation completed */
	MW_XFER_RTOS_ERR_INVALID = -1,  /**< invalid argument or unbound queue */
	MW_XFER_RTOS_ERR_NOMEM = -2,    /**< OS queue allocation failed */
	MW_XFER_RTOS_ERR_FULL = -3,     /**< non-blocking send found no space */
	MW_XFER_RTOS_ERR_TIMEOUT = -4,  /**< finite wait expired */
	MW_XFER_RTOS_ERR_STATE = -5     /**< wrong lifecycle state */
} mw_xfer_rtos_rc_e;

/** 调用方持有的描述符队列控制块。 */
typedef struct {
	plt_rtos_queue_t queue; /**< OS-owned queue handle */
	u32 depth;              /**< maximum descriptor count */
	u32 drop_count;         /**< task-context send failures */
	u8 initialized;         /**< 1 after successful init */
} mw_xfer_rtos_t;

/**
 * @brief 创建承载 common_xfer_t 元数据的 OS 队列。
 *
 * @param q 控制块；必须非 NULL，首次使用前必须清零。
 * @param depth 描述符容量；必须大于 0。
 * @return MW_XFER_RTOS_OK / MW_XFER_RTOS_ERR_INVALID /
 *         MW_XFER_RTOS_ERR_NOMEM / MW_XFER_RTOS_ERR_STATE。
 * @pre OSAL 后端已初始化，且 q 处于未初始化状态。
 * @note 队列只复制 common_xfer_t，payload 存储位于队列之外。
 * @author Xue Weisen
 */
mw_xfer_rtos_rc_e mw_xfer_rtos_init(mw_xfer_rtos_t *q, u32 depth);

/**
 * @brief 删除 OS 队列并清空控制块。
 *
 * @param q 已初始化的控制块。
 * @return MW_XFER_RTOS_OK / MW_XFER_RTOS_ERR_INVALID /
 *         MW_XFER_RTOS_ERR_STATE。
 * @pre 所有生产者和消费者都已停止使用该队列。
 * @note 队列中已拥有的 payload 不会自动释放，调用方必须先排空或接管。
 * @author Xue Weisen
 */
mw_xfer_rtos_rc_e mw_xfer_rtos_deinit(mw_xfer_rtos_t *q);

/**
 * @brief 从任务上下文发送一个描述符。
 *
 * 成功后 payload 的消费/释放权转移给队列，生产者不得再修改或释放该 payload；
 * FULL/TIMEOUT 时生产者仍拥有 payload，必须回滚 lease 并记录丢弃。
 *
 * @param q 已初始化的队列。
 * @param xfer 有效描述符；调用方保留本地元数据副本。
 * @param timeout_ms 有限毫秒等待；0 表示非阻塞。
 * @return MW_XFER_RTOS_OK / MW_XFER_RTOS_ERR_FULL /
 *         MW_XFER_RTOS_ERR_TIMEOUT / MW_XFER_RTOS_ERR_INVALID /
 *         MW_XFER_RTOS_ERR_STATE。
 * @note 队列不复制 payload 字节；drop_count 只允许一个指定任务写者维护，
 *       多任务并发生产者应使用外部原子统计或外部串行化。
 * @author Xue Weisen
 */
mw_xfer_rtos_rc_e mw_xfer_rtos_send(mw_xfer_rtos_t *q,
                                    const common_xfer_t *xfer,
                                    u32 timeout_ms);

/**
 * @brief 从 ISR 上下文发送一个描述符。
 *
 * @param q 已初始化的队列。
 * @param xfer 进入 ISR 前已准备完成的描述符。
 * @param woken 输出上下文切换 token；必须非 NULL。
 * @return MW_XFER_RTOS_OK / MW_XFER_RTOS_ERR_FULL /
 *         MW_XFER_RTOS_ERR_INVALID / MW_XFER_RTOS_ERR_STATE。
 * @pre 调用方已在进入 ISR 前取得 payload lease；ISR 内不得分配、释放、
 *      打印或阻塞。
 * @note ISR 侧丢弃计数由调用方维护，因为跨 CPU 的非原子计数器更新不可移植。
 * @author Xue Weisen
 */
mw_xfer_rtos_rc_e mw_xfer_rtos_send_from_isr(mw_xfer_rtos_t *q,
                                             const common_xfer_t *xfer,
                                             plt_rtos_woken_t *woken);

/**
 * @brief 从任务上下文接收一个描述符。
 *
 * @param q 已初始化的队列。
 * @param out 输出描述符；只在成功时写入。
 * @param timeout_ms 有限毫秒等待；0 表示非阻塞。
 * @return MW_XFER_RTOS_OK / MW_XFER_RTOS_ERR_TIMEOUT /
 *         MW_XFER_RTOS_ERR_INVALID / MW_XFER_RTOS_ERR_STATE。
 * @note 成功后由消费者负责释放 pooled/transferred payload。
 * @author Xue Weisen
 */
mw_xfer_rtos_rc_e mw_xfer_rtos_receive(mw_xfer_rtos_t *q,
                                       common_xfer_t *out,
                                       u32 timeout_ms);

/**
 * @brief 查询当前描述符数量。
 *
 * @param q 已初始化的队列。
 * @param out_count 输出数量；只在成功时写入。
 * @return MW_XFER_RTOS_OK / MW_XFER_RTOS_ERR_INVALID /
 *         MW_XFER_RTOS_ERR_STATE。
 * @note 返回值是并发快照，不是预留或容量保证。
 * @author Xue Weisen
 */
mw_xfer_rtos_rc_e mw_xfer_rtos_count(const mw_xfer_rtos_t *q,
                                     u32 *out_count);

/**
 * @brief 返回任务上下文发送失败/丢弃计数。
 *
 * @param q 队列控制块；NULL 返回 0。
 * @return 任务侧累计失败计数。
 * @author Xue Weisen
 */
u32 mw_xfer_rtos_drop_count(const mw_xfer_rtos_t *q);

#endif /* MW_XFER_RTOS_H */
