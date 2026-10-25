//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: plt_time
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: 单调时间源契约：微秒读取、毫秒投影与失败可见
// Revision: 0.01
// Revision: 0.01 - File Created
// Keywords: monotonic-time-source, read-us-callback-ctx, now-us/ms, caller-owned-object,
//   os-tick-independent, no-global-clock
//////////////////////////////////////////////////////////////////////////////////
#ifndef PLT_TIME_H
#define PLT_TIME_H

#include "common_types.h"

/** 时间源返回码。 */
typedef enum {
	PLT_TIME_OK = 0,
	PLT_TIME_ERR_INVALID = -1,
	PLT_TIME_ERR_IO = -2
} plt_time_rc_e;

/** 微秒时间读取回调；ctx 由绑定方提供。 */
typedef plt_time_rc_e (*plt_time_read_us_fn)(void *ctx, u64 *now_us);

/** 调用方拥有的单调时间源。 */
typedef struct {
	plt_time_read_us_fn read_us;
	void *ctx;
	u8 initialized;
} plt_time_t;

/**
 * @brief 绑定单调微秒时间源。
 *
 * @param self 调用方拥有的时间对象；不可为 NULL。
 * @param read_us 读取回调；必须非 NULL 且返回单调时间。
 * @param ctx 传给回调的上下文。
 * @return PLT_TIME_OK / PLT_TIME_ERR_INVALID。
 * @note 本函数不启动定时器，只保存绑定关系。
 * @author Xue Weisen
 */
plt_time_rc_e plt_time_init(plt_time_t *self,
                            plt_time_read_us_fn read_us,
                            void *ctx);

/**
 * @brief 读取当前单调微秒时间。
 *
 * @param self 已绑定对象。
 * @param now_us 输出时间；只在成功时写入。
 * @return PLT_TIME_OK / PLT_TIME_ERR_INVALID / PLT_TIME_ERR_IO。
 * @author Xue Weisen
 */
plt_time_rc_e plt_time_now_us(plt_time_t *self, u64 *now_us);

/**
 * @brief 读取当前单调毫秒时间（64 bit）。
 *
 * @param self 已绑定对象。
 * @param now_ms 输出时间；只在成功时写入。
 * @return PLT_TIME_OK / PLT_TIME_ERR_INVALID / PLT_TIME_ERR_IO。
 * @author Xue Weisen
 */
plt_time_rc_e plt_time_now_ms64(plt_time_t *self, u64 *now_ms);

/**
 * @brief 读取当前单调毫秒时间（32 bit 投影）。
 *
 * @param self 已绑定对象。
 * @param now_ms 输出时间；只在成功时写入。
 * @return PLT_TIME_OK / PLT_TIME_ERR_INVALID / PLT_TIME_ERR_IO。
 * @note 超过 2^32 ms 后自然截断，配合 plt_poll 的无符号差值使用。
 * @author Xue Weisen
 */
plt_time_rc_e plt_time_now_ms(plt_time_t *self, u32 *now_ms);

#endif /* PLT_TIME_H */
