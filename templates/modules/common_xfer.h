//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: common_xfer
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: 带长度、顺序与释放回调的数据传递描述符
// Revision: 0.02
// Revision 0.01 - File Created
// Revision 0.02 - 所有权进入类型契约，增加带上下文的释放回调与失败语义
// Keywords: data-transfer-descriptor, ownership-enum, borrowed/static/pooled/transferred, pointer-length, release-callback-ctx, sequence, flags
//////////////////////////////////////////////////////////////////////////////////
#ifndef COMMON_XFER_H
#define COMMON_XFER_H

#include "common_types.h"

/** 描述符边界处的 payload 所有权类型。 */
typedef enum {
	COMMON_XFER_BORROWED = 0,   /**< caller-owned, valid for the consumer window */
	COMMON_XFER_STATIC = 1,     /**< process-lifetime storage, never released */
	COMMON_XFER_POOLED = 2,     /**< fixed-pool storage, callback returns it */
	COMMON_XFER_TRANSFERRED = 3 /**< ownership moved, callback is mandatory */
} common_xfer_owner_e;

/** 描述符校验/释放返回码。 */
typedef enum {
	COMMON_XFER_OK = 0,           /**< valid or released */
	COMMON_XFER_ERR_INVALID = -1, /**< bad descriptor or ownership combination */
	COMMON_XFER_ERR_RELEASE = -2  /**< release callback rejected the payload */
} common_xfer_rc_e;

/**
 * @brief 释放一个已拥有的 payload。
 *
 * @param ctx 生产者提供的所有者上下文。
 * @param token 不透明所有者 token；过期 token 必须被所有者拒绝。
 * @param data 描述符记录的 payload 指针。
 * @param len 描述符记录的 payload 长度。
 * @return 成功返回 0；token/pointer 过期或释放失败返回非 0。
 * @note 回调在消费者上下文执行，必须有界且不得阻塞。
 */
typedef int (*common_xfer_release_fn)(void *ctx,
                                      u32 token,
                                      const u8 *data,
                                      u32 len);

/**
 * @brief 只承载元数据的传输描述符。
 *
 * 描述符不拥有 borrowed/static payload；pooled/transferred payload 必须带
 * 释放回调。释放一个描述符副本不会自动清除其他副本；生产者与消费者必须
 * 对一次所有权交接达成一致，且同一 lease 不得释放两次。
 */
typedef struct {
	const u8 *data;                 /**< borrowed payload view */
	u32 len;                        /**< valid payload bytes */
	u32 sequence;                   /**< protocol/frame sequence */
	u32 token;                      /**< owner-specific lease token */
	common_xfer_release_fn release; /**< mandatory for pooled/transferred */
	void *release_ctx;              /**< owner context for release */
	u16 flags;                      /**< protocol flags */
	u8 owner;                       /**< common_xfer_owner_e */
	u8 reserved;                    /**< reserved, keep zero */
} common_xfer_t;

/**
 * @brief 校验长度、所有权和释放回调的一致性。
 *
 * @param xfer 描述符；NULL 视为非法。
 * @return 1 表示有效；0 表示非法。
 * @note len=0 允许 data=NULL；borrowed/static 要求 release=NULL；
 *       pooled/transferred 要求 release 非 NULL。
 * @author Xue Weisen
 */
static inline int common_xfer_valid(const common_xfer_t *xfer)
{
	/* 基础校验：空描述符和“有长度但无数据”的组合无法安全释放，先拒绝。 */
	if (xfer == 0) {
		return 0;
	}
	if ((xfer->len > 0u) && (xfer->data == 0)) {
		return 0;
	}
	/* 所有权校验：借用/静态不得带释放回调，池化/转移必须恰好带一个释放回调。 */
	switch ((common_xfer_owner_e)xfer->owner) {
	case COMMON_XFER_BORROWED:
	case COMMON_XFER_STATIC:
		return (xfer->release == 0) ? 1 : 0;
	case COMMON_XFER_POOLED:
	case COMMON_XFER_TRANSFERRED:
		return (xfer->release != 0) ? 1 : 0;
	default:
		return 0;
	}
}

/**
 * @brief 释放已拥有的 payload，并在成功后使描述符失效。
 *
 * borrowed/static 描述符不调用回调，直接使其失效；pooled/transferred 描述符
 * 恰好调用一次释放回调。回调失败时保留描述符，供调用方重试或报告泄漏；
 * 成功时清空全部字段。
 *
 * @param xfer 待释放描述符；NULL 或非法返回 INVALID。
 * @return COMMON_XFER_OK / COMMON_XFER_ERR_INVALID / COMMON_XFER_ERR_RELEASE.
 * @note 同一描述符不可并发释放；消费者必须独占该描述符。
 * @author Xue Weisen
 */
static inline common_xfer_rc_e common_xfer_release(common_xfer_t *xfer)
{
	int rc;

	/* 校验阶段：非法所有权组合不允许触发未知回调。 */
	if (common_xfer_valid(xfer) == 0) {
		return COMMON_XFER_ERR_INVALID;
	}

	/* 释放阶段：只有真拥有 payload 的类型才调用 owner 回调。 */
	if ((xfer->owner == (u8)COMMON_XFER_POOLED) ||
	    (xfer->owner == (u8)COMMON_XFER_TRANSFERRED)) {
		rc = xfer->release(xfer->release_ctx, xfer->token, xfer->data, xfer->len);
		if (rc != 0) {
			return COMMON_XFER_ERR_RELEASE;
		}
	}

	/* 提交阶段：释放成功后才失活描述符，避免失败时丢失重试信息。 */
	xfer->data = 0;
	xfer->len = 0u;
	xfer->sequence = 0u;
	xfer->token = 0u;
	xfer->release = 0;
	xfer->release_ctx = 0;
	xfer->flags = 0u;
	xfer->owner = (u8)COMMON_XFER_BORROWED;
	xfer->reserved = 0u;
	return COMMON_XFER_OK;
}

#endif /* COMMON_XFER_H */
