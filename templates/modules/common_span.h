//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: common_span
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: 带长度的只读/可写字节视图
// Revision: 0.01
// Revision 0.01 - File Created
// Keywords: common-inline, const-buffer, writable-buffer, pointer-length-pair
//////////////////////////////////////////////////////////////////////////////////
#ifndef COMMON_SPAN_H
#define COMMON_SPAN_H

#include "common_types.h"

/** 只读借用视图；data 由调用方拥有，调用期间不得释放。 */
typedef struct {
	const u8 *data; /**< read-only bytes */
	u32 len;        /**< valid byte count */
} common_const_buf_t;

/** 可写输出视图；len 表示已写入字节数。 */
typedef struct {
	u8 *data;  /**< writable storage */
	u32 cap;   /**< storage capacity */
	u32 len;   /**< bytes written */
} common_buf_t;

/**
 * @brief 判断只读视图是否满足 data/len 组合。
 *
 * @param buf 视图；NULL 视为无效。
 * @return 1 = 有效；0 = 无效。
 * @author Xue Weisen
 */
static inline int common_const_buf_valid(const common_const_buf_t *buf)
{
	return ((buf != 0) && ((buf->len == 0u) || (buf->data != 0))) ? 1 : 0;
}

/**
 * @brief 判断可写视图是否满足 data/cap 组合。
 *
 * @param buf 视图；NULL 视为无效。
 * @return 1 = 有效；0 = 无效。
 * @author Xue Weisen
 */
static inline int common_buf_valid(const common_buf_t *buf)
{
	return ((buf != 0) && (buf->data != 0) && (buf->cap > 0u)) ? 1 : 0;
}

#endif /* COMMON_SPAN_H */
