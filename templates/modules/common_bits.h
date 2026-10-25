//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: common_bits
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: 零依赖位域访问器
// Revision: 0.01
// Revision 0.01 - File Created
// Keywords: common-inline, bit-field-mask, get/set/clear, no-bsp
//////////////////////////////////////////////////////////////////////////////////
#ifndef COMMON_BITS_H
#define COMMON_BITS_H

#include "common_types.h"

/**
 * @brief 生成单 bit 掩码。
 *
 * @param bit bit 序号，0~31；调用方保证范围合法。
 * @return 1u << bit。
 * @warning 不做运行期范围检查；固定位定义优先。
 * @author Xue Weisen
 */
static inline u32 common_bit(u32 bit)
{
	return (1u << bit);
}

/**
 * @brief 读取位域。
 *
 * @param value 原始字。
 * @param mask 已左移的字段掩码。
 * @param shift 字段低位偏移。
 * @return 字段值。
 * @pre mask 必须与 shift 对应；调用方保证 mask 非 0。
 * @author Xue Weisen
 */
static inline u32 common_field_get(u32 value, u32 mask, u32 shift)
{
	return ((value & mask) >> shift);
}

/**
 * @brief 把字段值放到目标位置，不改其他位。
 *
 * @param old_value 原始字。
 * @param mask 已左移的字段掩码。
 * @param shift 字段低位偏移。
 * @param field 字段值，超出 mask 宽度的位会被截断。
 * @return 更新后的字。
 * @author Xue Weisen
 */
static inline u32 common_field_prep(u32 old_value, u32 mask, u32 shift, u32 field)
{
	return ((old_value & ~mask) | ((field << shift) & mask));
}

/**
 * @brief 把字段清 0。
 *
 * @param old_value 原始字。
 * @param mask 已左移的字段掩码。
 * @return 清位后的字。
 * @author Xue Weisen
 */
static inline u32 common_field_clear(u32 old_value, u32 mask)
{
	return (old_value & ~mask);
}

#endif /* COMMON_BITS_H */
