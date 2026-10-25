//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: plt_time_xtime
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: Xilinx XTime 单调时间源绑定
// Revision: 0.01
// Revision: 0.01 - File Created
// Keywords: xtime-adapter, counts-per-second, us-projection, no-vendor-type-in-api
//////////////////////////////////////////////////////////////////////////////////
#ifndef PLT_TIME_XTIME_H
#define PLT_TIME_XTIME_H

#include "plt_time.h"

/**
 * @brief 把 XTime 全局计数器绑定为单调微秒源。
 *
 * @param self 调用方拥有的时间对象；不可为 NULL。
 * @return PLT_TIME_OK / PLT_TIME_ERR_INVALID。
 * @note 仅适用于 BSP 提供 xtime_l.h 与 COUNTS_PER_SECOND 的处理器。
 * @author Xue Weisen
 */
int plt_time_xtime_bind(plt_time_t *self);

#endif /* PLT_TIME_XTIME_H */
