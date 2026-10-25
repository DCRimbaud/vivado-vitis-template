//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: plt_time
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: 单调时间源的绑定与毫秒投影
// Revision: 0.01
// Revision: 0.01 - File Created
// Keywords: time-source-bind, us-to-ms-projection, output-preserved-on-failure
//////////////////////////////////////////////////////////////////////////////////
#include "plt_time.h"

/**
 * @brief 判断时间源是否已完成绑定。
 *
 * @param self 时间对象；允许为 NULL。
 * @return 1 = 可读取；0 = 未初始化。
 * @author Xue Weisen
 */
static int plt_time_ready(const plt_time_t *self)
{
	return ((self != 0) && (self->initialized != 0u) && (self->read_us != 0)) ? 1 : 0;
}

/**
 * @brief 绑定单调微秒时间源。
 *
 * @param self 调用方拥有的时间对象。
 * @param read_us 读取回调。
 * @param ctx 回调上下文。
 * @return PLT_TIME 域返回码。
 * @author Xue Weisen
 */
plt_time_rc_e plt_time_init(plt_time_t *self,
                            plt_time_read_us_fn read_us,
                            void *ctx)
{
	if ((self == 0) || (read_us == 0)) {
		return PLT_TIME_ERR_INVALID;
	}
	self->read_us = read_us;
	self->ctx = ctx;
	self->initialized = 1u;
	return PLT_TIME_OK;
}

/**
 * @brief 读取当前单调微秒时间。
 *
 * 回调先写本地变量，成功后才提交给调用方，避免失败时覆盖旧时间戳。
 *
 * @param self 已绑定对象。
 * @param now_us 输出时间。
 * @return PLT_TIME 域返回码。
 * @author Xue Weisen
 */
plt_time_rc_e plt_time_now_us(plt_time_t *self, u64 *now_us)
{
	u64 local_us = 0u;
	plt_time_rc_e rc;

	if ((plt_time_ready(self) == 0) || (now_us == 0)) {
		return PLT_TIME_ERR_INVALID;
	}
	rc = self->read_us(self->ctx, &local_us);
	if (rc == PLT_TIME_OK) {
		*now_us = local_us;
	}
	return rc;
}

/**
 * @brief 读取当前单调毫秒时间（64 bit）。
 *
 * @param self 已绑定对象。
 * @param now_ms 输出时间。
 * @return PLT_TIME 域返回码。
 * @author Xue Weisen
 */
plt_time_rc_e plt_time_now_ms64(plt_time_t *self, u64 *now_ms)
{
	u64 local_us = 0u;
	plt_time_rc_e rc;

	if (now_ms == 0) {
		return PLT_TIME_ERR_INVALID;
	}
	rc = plt_time_now_us(self, &local_us);
	if (rc == PLT_TIME_OK) {
		*now_ms = local_us / 1000u;
	}
	return rc;
}

/**
 * @brief 读取当前单调毫秒时间（32 bit 投影）。
 *
 * @param self 已绑定对象。
 * @param now_ms 输出时间。
 * @return PLT_TIME 域返回码。
 * @note 与 plt_poll 配合时按无符号差值处理自然回绕。
 * @author Xue Weisen
 */
plt_time_rc_e plt_time_now_ms(plt_time_t *self, u32 *now_ms)
{
	u64 local_ms = 0u;
	plt_time_rc_e rc;

	if (now_ms == 0) {
		return PLT_TIME_ERR_INVALID;
	}
	rc = plt_time_now_ms64(self, &local_ms);
	if (rc == PLT_TIME_OK) {
		*now_ms = (u32)local_ms;
	}
	return rc;
}
