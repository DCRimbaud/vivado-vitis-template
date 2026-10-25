//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: common_parse
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: 零依赖无符号整数解析实现
// Revision: 0.01
// Revision 0.01 - File Created
// Keywords: common-parse-impl, digit-validation, overflow-detect, output-preserved-on-failure
//////////////////////////////////////////////////////////////////////////////////
#include "common_parse.h"

/**
 * @brief 解析任意进制的 u32 字符串。
 *
 * 逐字符累加，先判断乘法/加法溢出，再写输出；失败不修改 *out。
 *
 * @param text 输入字符串。
 * @param base 2~16 的进制。
 * @param allow_prefix 是否允许 0x/0X 前缀。
 * @param out 输出值。
 * @return COMMON_PARSE_OK / COMMON_PARSE_ERR_INVALID / COMMON_PARSE_ERR_OVERFLOW。
 * @author Xue Weisen
 */
static int common_parse_core(const char *text, u32 base, int allow_prefix, u32 *out)
{
	const char *p;
	u32 value = 0u;
	u32 digit;
	u32 limit;
	u32 next;

	/* 校验阶段：空指针、空串和非法进制直接拒绝，避免后续解引用无效地址。 */
	if ((text == 0) || (out == 0) || (base < 2u) || (base > 16u)) {
		return COMMON_PARSE_ERR_INVALID;
	}
	p = text;

	/* 前缀阶段：只允许十六进制输入跳过 0x/0X，前缀后必须仍有数字。 */
	if ((allow_prefix != 0) && (p[0] == '0') && ((p[1] == 'x') || (p[1] == 'X'))) {
		p += 2;
		if (*p == '\0') {
			return COMMON_PARSE_ERR_INVALID;
		}
	}

	/* 累加阶段：每一位先做 digit 范围检查，再做 u32 溢出检查。 */
	while (*p != '\0') {
		if ((*p >= '0') && (*p <= '9')) {
			digit = (u32)(*p - '0');
		} else if ((*p >= 'a') && (*p <= 'f')) {
			digit = (u32)(*p - 'a') + 10u;
		} else if ((*p >= 'A') && (*p <= 'F')) {
			digit = (u32)(*p - 'A') + 10u;
		} else {
			return COMMON_PARSE_ERR_INVALID;
		}
		if (digit >= base) {
			return COMMON_PARSE_ERR_INVALID;
		}
		limit = (0xFFFFFFFFu - digit) / base;
		if (value > limit) {
			return COMMON_PARSE_ERR_OVERFLOW;
		}
		next = (value * base) + digit;
		value = next;
		p++;
	}

	/* 提交阶段：整串验证通过后才写输出，失败路径保持调用方原值。 */
	*out = value;
	return COMMON_PARSE_OK;
}

/**
 * @brief 解析十进制 u32。
 *
 * 不跳前缀、不跳过空白；输入必须是完整数字串。
 *
 * @param text 输入字符串。
 * @param out 输出值；失败时保持原值。
 * @return COMMON_PARSE_OK / COMMON_PARSE_ERR_INVALID / COMMON_PARSE_ERR_OVERFLOW。
 * @author Xue Weisen
 */
int common_parse_u32(const char *text, u32 *out)
{
	/* 十进制入口统一走 core，保持溢出和输出语义只有一处实现。 */
	return common_parse_core(text, 10u, 0, out);
}

/**
 * @brief 解析十六进制 u32。
 *
 * 允许 0x/0X 前缀；输入必须是完整十六进制数字串。
 *
 * @param text 输入字符串。
 * @param out 输出值；失败时保持原值。
 * @return COMMON_PARSE_OK / COMMON_PARSE_ERR_INVALID / COMMON_PARSE_ERR_OVERFLOW。
 * @author Xue Weisen
 */
int common_parse_hex(const char *text, u32 *out)
{
	/* 十六进制入口与十进制共用 core，避免两套溢出/边界逻辑漂移。 */
	return common_parse_core(text, 16u, 1, out);
}
