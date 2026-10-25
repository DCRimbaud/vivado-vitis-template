//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: app_cmd
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: 应用层命令系统接口：命令表（argc_min）、行编辑、整行执行与 help
// Revision: 0.04
// Revision 0.02 - 表项加 argc_min、handler 返回结果码、新增 app_cmd_exec_line 整行入口
// Revision 0.01 - File Created
// Revision 0.03 - handler argv 收紧为只读视图
// Revision 0.04 - 复用 mw_command 表与分发机制，删除 app_cmd 平行实现
// Keywords: cli-api-header, mw-command-table, handler-ctx, read-only-argv,
//   app_cmd_feed/app_cmd_help/app_cmd_exec_line-decls, single-dispatch-mechanism
//////////////////////////////////////////////////////////////////////////////////

#ifndef APP_CMD_H
#define APP_CMD_H

#include "common_types.h"
#include "mw_command.h"

/* 命令结果码：工程有统一状态码时改用统一枚举；二进制通道按它回 ACK。 */
#define APP_CMD_RC_OK   0
#define APP_CMD_RC_FAIL (-1)
#define APP_CMD_RC_ARGS (-2)

/* 命令处理与表项由 mw_command 统一提供；ctx 在轮询上下文传递，argv 只读。 */
typedef mw_command_fn app_cmd_fn;
typedef mw_command_t app_cmd_t;

/**
 * @brief  返回静态命令表首地址与表项数。
 *
 * 命令表是分发与 help 的唯一事实源；调用方只读，不得释放或改写。
 * count 为 NULL 时只返回表首地址，不写表项数。
 *
 * @param count 输出表项数；允许为 NULL。
 * @return 静态命令表首地址；表为空时返回 NULL。
 * @author Xue Weisen
 */
const app_cmd_t *app_cmd_table(u32 *count);

/**
 * @brief  遍历命令表打印帮助。
 *
 * 帮助文本逐字取自已注册命令表项，与分发同源；输出为阻塞式串口发送。
 *
 * @author Xue Weisen
 */
void app_cmd_help(void);

/**
 * @brief  命令行字符状态机：回显、退格、组行，回车时执行命令。
 *
 * 由接收回调在轮询上下文逐字节喂入。行超过 APP_CMD_LINE_MAX-1 时丢弃该行并提示过长；
 * 空行回车不执行任何命令；未命中命令打印错误并保持其他命令可继续使用。
 *
 * @param c 控制台收到的 1 个字节；'\r'/'\n' 触发执行，0x08/0x7F 视为退格。
 * @author Xue Weisen
 */
void app_cmd_feed(u8 c);

/**
 * @brief  整行执行一条命令（二进制通道 / 测试用入口）。
 *
 * 与 app_cmd_feed 共用同一张命令表、同一套分词与分发：参数下界、用法提示、各命令实现
 * 全部同源。本函数用独立的静态行缓冲，不打断操作者正在输入的那半行。
 *
 * @param line 一行命令（不含回车）；为 NULL 或空行返回 APP_CMD_RC_ARGS。
 * @return APP_CMD_RC_OK / APP_CMD_RC_FAIL / APP_CMD_RC_ARGS。
 * @author Xue Weisen
 */
int app_cmd_exec_line(const char *line);

#endif /* APP_CMD_H */
