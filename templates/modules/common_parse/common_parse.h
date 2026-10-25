//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: common_parse
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: 零依赖无符号整数解析
// Revision: 0.01
// Revision 0.01 - File Created
// Keywords: common-parse, decimal, hexadecimal, out-param, overflow-check
//////////////////////////////////////////////////////////////////////////////////
#ifndef COMMON_PARSE_H
#define COMMON_PARSE_H

#include "common_types.h"

/** 解析结果。 */
typedef enum {
	COMMON_PARSE_OK = 0,          /**< 成功 */
	COMMON_PARSE_ERR_INVALID = -1,/**< 空串 / 字符非法 */
	COMMON_PARSE_ERR_OVERFLOW = -2/**< u32 溢出 */
} common_parse_rc_e;

/**
 * @brief 解析十进制 u32。
 *
 * 输入必须非 NULL、非空，且整串只能包含十进制数字；成功才写 *out。
 *
 * @param text 输入字符串；不可为 NULL。
 * @param out 输出值；不可为 NULL，失败时保持原值。
 * @return COMMON_PARSE_OK / COMMON_PARSE_ERR_INVALID / COMMON_PARSE_ERR_OVERFLOW。
 * @author Xue Weisen
 */
int common_parse_u32(const char *text, u32 *out);

/**
 * @brief 解析十六进制 u32。
 *
 * 输入可带 0x/0X 前缀；只接受完整十六进制数字串；成功才写 *out。
 *
 * @param text 输入字符串；不可为 NULL。
 * @param out 输出值；不可为 NULL，失败时保持原值。
 * @return COMMON_PARSE_OK / COMMON_PARSE_ERR_INVALID / COMMON_PARSE_ERR_OVERFLOW。
 * @author Xue Weisen
 */
int common_parse_hex(const char *text, u32 *out);

#endif /* COMMON_PARSE_H */
