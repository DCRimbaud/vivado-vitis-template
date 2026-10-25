//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: mw_command
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: 通用命令表查找与分发实现
// Revision: 0.01
// Revision: 0.01 - File Created
// Keywords: command-find, argc-contract, handler-ctx, no-print, result-passthrough
//////////////////////////////////////////////////////////////////////////////////
#include "mw_command.h"

#include <string.h>

/**
 * @brief 按名字查找命令表项。
 *
 * @param table 命令表。
 * @param count 表项数。
 * @param name 命令名。
 * @return 命中项或 NULL。
 * @author Xue Weisen
 */
const mw_command_t *mw_command_find(const mw_command_t *table,
                                    u32 count,
                                    const char *name)
{
	u32 i;

	/* 查找阶段跳过 help-only 行；name 大小写严格匹配。 */
	if ((table == 0) || (name == 0)) {
		return 0;
	}
	for (i = 0u; i < count; i++) {
		if ((table[i].name != 0) && (strcmp(table[i].name, name) == 0)) {
			return &table[i];
		}
	}
	return 0;
}

/**
 * @brief 执行一条已分词命令。
 *
 * @param table 命令表。
 * @param count 表项数。
 * @param ctx handler 上下文。
 * @param argc token 数。
 * @param argv 只读 token 数组。
 * @return handler 返回码或分发错误码。
 * @author Xue Weisen
 */
int mw_command_dispatch(const mw_command_t *table,
                        u32 count,
                        void *ctx,
                        u32 argc,
                        const char *const argv[])
{
	const mw_command_t *cmd;

	/* 校验阶段：空行、空 argv 和小于表项下界的输入都不进入 handler。 */
	if ((argc == 0u) || (argv == 0) || (argv[0] == 0)) {
		return MW_COMMAND_ARGS;
	}
	cmd = mw_command_find(table, count, argv[0]);
	if ((cmd == 0) || (cmd->fn == 0)) {
		return MW_COMMAND_UNKNOWN;
	}
	if ((cmd->argc_min != 0u) && (argc < (u32)cmd->argc_min)) {
		return MW_COMMAND_ARGS;
	}

	/* 分派阶段：handler 返回码原样透传，便于上层决定 ACK。 */
	return cmd->fn(ctx, argc, argv);
}
