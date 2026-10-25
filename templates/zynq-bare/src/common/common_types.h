//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: common_types
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: 基础类型入口：统一各层共用的 u8/u16/s32 等类型定义
// Revision: 0.01
// Revision 0.01 - File Created
// Keywords: zero-dependency-typedefs(u8/u16/u32/u64/s8/s16/s32/s64), array_size-macro,
//   stdint-stddef-backed, no-bsp-headers
//////////////////////////////////////////////////////////////////////////////////
#ifndef COMMON_TYPES_H
#define COMMON_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>   /* NULL / size_t */

/* 零依赖类型别名：范围与位宽以 stdint 为准，不引 BSP 头。
 * s8/s16 预留给需要有符号字节/半字的驱动或协议字段；未使用时可自行裁剪。 */
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;

/* 目标平台假设：寄存器访问与协议字段按 u32/u64 定宽读写，宽度不符即编译失败。 */
_Static_assert(sizeof(u32) == 4u, "u32 must be 4 bytes");
_Static_assert(sizeof(u64) == 8u, "u64 must be 8 bytes");

/* 数组元素个数（编译期常量） */
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

#endif /* COMMON_TYPES_H */
