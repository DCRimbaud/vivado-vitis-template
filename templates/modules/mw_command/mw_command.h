//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: mw_command
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: 通用命令表查找与分发
// Revision: 0.01
// Revision: 0.01 - File Created
// Keywords: command-table, handler-ctx, read-only-argv, argc-min, unknown/args-result
//////////////////////////////////////////////////////////////////////////////////
#ifndef MW_COMMAND_H
#define MW_COMMAND_H

#include "common_types.h"

/** 命令处理函数：ctx 由调用方持有，argv 是只读视图。 */
typedef int (*mw_command_fn)(void *ctx, u32 argc, const char *const argv[]);

/** 命令表项。 */
typedef struct {
	const char *name;  /**< dispatch name */
	const char *usage; /**< one-line usage */
	const char *desc;  /**< help description */
	u8 argc_min;       /**< minimum argc including command name */
	mw_command_fn fn;  /**< handler, NULL = help-only row */
} mw_command_t;

/** 分发结果；handler 的返回码原样返回。 */
typedef enum {
	MW_COMMAND_UNKNOWN = 1, /**< command not found */
	MW_COMMAND_ARGS = 2      /**< argc below table minimum */
} mw_command_rc_e;

/**
 * @brief 按名字查找命令表项。
 *
 * @param table 命令表；可为 NULL。
 * @param count 表项数。
 * @param name 命令名；不可为 NULL。
 * @return 命中项；未命中或参数非法返回 NULL。
 * @author Xue Weisen
 */
const mw_command_t *mw_command_find(const mw_command_t *table,
                                    u32 count,
                                    const char *name);

/**
 * @brief 执行一条已分词命令。
 *
 * 查找命令、检查 argc 下界、调用 handler；不打印、不分配内存。
 *
 * @param table 命令表。
 * @param count 表项数。
 * @param ctx 传给 handler 的上下文；允许为 NULL，由 handler 解释。
 * @param argc token 数。
 * @param argv 只读 token 数组。
 * @return handler 返回码；未命中返回 MW_COMMAND_UNKNOWN；参数不足返回 MW_COMMAND_ARGS。
 * @author Xue Weisen
 */
int mw_command_dispatch(const mw_command_t *table,
                        u32 count,
                        void *ctx,
                        u32 argc,
                        const char *const argv[]);

#endif /* MW_COMMAND_H */
