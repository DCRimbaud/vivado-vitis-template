//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: app_cmd
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: 应用层命令系统实现：行编辑、分词、表驱动分发（含 argc_min 兜底）与 help
// Revision: 0.04
// Revision 0.02 - 分发改返回结果码、表项加 argc_min、新增 app_cmd_exec_line（独立缓冲）
// Revision 0.01 - File Created
// Revision 0.03 - handler argv 收紧为只读视图
// Revision 0.04 - 查找与分发收敛到 mw_command，删除 app_cmd 平行实现
// Keywords: cli-impl, line-editor(backspace/echo/overflow-drop), whitespace-tokenizer,
//   mw-command-single-dispatch, unknown/args-error, help-from-same-table,
//   exec-line-reuses-same-dispatch(separate-static-buffer)
//////////////////////////////////////////////////////////////////////////////////

#include "app_cmd.h"

#include <string.h>

#include "mw_console.h"

/* 行缓冲长度：含结尾 '\0'。按最长命令行的 1.5 倍留余量，过长即丢弃并提示。 */
#define APP_CMD_LINE_MAX   64u

/* 一次最多分出的参数个数（含命令名）；超出部分按“参数过多”拒绝执行。 */
#define APP_CMD_ARGS_MAX   8u

/** Console command context passed through the generic dispatcher. */
typedef struct {
	char *line_buf; /**< active, mutable line buffer */
	u32 line_cap;   /**< buffer capacity including terminator */
} app_cmd_ctx_t;

static char s_line[APP_CMD_LINE_MAX];       /* 行编辑缓冲（feed 使用） */
static u32  s_line_len;
static char s_exec[APP_CMD_LINE_MAX];       /* 整行执行专用，不打断 s_line */
static const char *s_argv[APP_CMD_ARGS_MAX];
static u32  s_argc;
static app_cmd_ctx_t s_cmd_ctx = { s_line, APP_CMD_LINE_MAX };

static int cmd_help(void *ctx, u32 argc, const char *const argv[]);
static int cmd_echo(void *ctx, u32 argc, const char *const argv[]);

/* 命令表：顺序即 help 顺序。新增命令只改这一张表 + 对应 handler。
 * designated initializer：加字段/调顺序不会让初始化静默错位。 */
static const app_cmd_t s_cmd_table[] = {
	{ .name = "help", .usage = "help",        .desc = "列出全部命令",       .argc_min = 1, .fn = cmd_help },
	{ .name = "echo", .usage = "echo <text>", .desc = "回显参数，用于联调", .argc_min = 2, .fn = cmd_echo },
};

/**
 * @brief  把一行命令按空白字符切分成 argv / argc（原地写 '\0'，不分配内存）。
 *
 * 参数超过 APP_CMD_ARGS_MAX 时停止切分并返回 -1，调用方按“参数过多”拒绝执行，
 * 避免半条命令被执行。
 *
 * @param line 待切分的一行（会被原地修改）；调用方保证非 NULL。
 * @return 0 = 切分完成（可能 argc=0）；-1 = 参数过多。
 * @author Xue Weisen
 */
static int split_line(char *line)
{
	char *p = line;

	/* 初始化阶段：每次切分都从空 argv 开始，避免上一行残留参数影响本次分发。 */
	s_argc = 0u;
	while (*p != '\0') {
		/* 跳过分隔空白；若已到结尾则保留空参数结果交调用方按空行处理。 */
		while ((*p == ' ') || (*p == '\t')) {
			p++;
		}
		if (*p == '\0') {
			break;
		}
		/* 容量阶段：先确认还能写入一个参数，超限时清空 argc 并整体拒绝本行。 */
		if (s_argc >= APP_CMD_ARGS_MAX) {
			s_argc = 0u;
			return -1;
		}
		/* 记录当前参数起点，再扫描到下一个空白或字符串结尾。 */
		s_argv[s_argc] = p;
		s_argc++;

		while ((*p != '\0') && (*p != ' ') && (*p != '\t')) {
			p++;
		}
		if (*p != '\0') {
			/* 原地切断当前参数并前移读到下一参数；命令行缓冲由调用方独占。 */
			*p = '\0';
			p++;
		}
	}
	return 0;
}

/**
 * @brief  执行缓冲区里的命令（feed 与 exec_line 的唯一分发内核）。
 *
 * 空行返回 ARGS；参数过多、命令未命中、参数下界不足都只打印提示并返回结果码。
 * 命令回调允许阻塞输出，因此本函数不可在 ISR 上下文调用。
 *
 * @param line 可变命令行缓冲；调用方保证非 NULL。
 * @return APP_CMD_RC_OK / APP_CMD_RC_FAIL / APP_CMD_RC_ARGS。
 * @author Xue Weisen
 */
static int dispatch_line(char *line)
{
	const app_cmd_t *cmd;
	u32 table_count;

	/* 分词阶段：空行和超参数都不进入命令表或 handler。 */
	if (split_line(line) != 0) {
		uart_print("[ERR] too many arguments (max %u)\r\n", (unsigned)APP_CMD_ARGS_MAX);
		return APP_CMD_RC_ARGS;
	}
	if (s_argc == 0u) {
		return APP_CMD_RC_ARGS;
	}

	/* 查找阶段：错误提示需要 usage，因此先用统一 mw_command 查找。 */
	table_count = (u32)ARRAY_SIZE(s_cmd_table);
	cmd = mw_command_find(s_cmd_table, table_count, s_argv[0]);
	if ((cmd == NULL) || (cmd->fn == NULL)) {
		uart_print("[ERR] unknown command: %s (try help)\r\n", s_argv[0]);
		return APP_CMD_RC_FAIL;
	}

	/* 参数阶段：表声明是唯一事实源，handler 内不再重复检查下界。 */
	if ((cmd->argc_min != 0u) && (s_argc < (u32)cmd->argc_min)) {
		uart_print("usage: %s\r\n", (cmd->usage != NULL) ? cmd->usage : cmd->name);
		return APP_CMD_RC_ARGS;
	}

	/* 分派阶段：真正调用统一由 mw_command 完成，handler 返回码原样上抛。 */
	return mw_command_dispatch(s_cmd_table, table_count, &s_cmd_ctx, s_argc,
	                           (const char *const *)s_argv);
}

/**
 * @brief  help 命令：打印命令表全部条目。
 *
 * @param ctx 命令上下文；本示例未使用。
 * @param argc 参数个数（未使用）。
 * @param argv 参数数组（未使用）。
 * @return APP_CMD_RC_OK。
 * @author Xue Weisen
 */
static int cmd_help(void *ctx, u32 argc, const char *const argv[])
{
	(void)ctx;
	(void)argc;
	(void)argv;
	app_cmd_help();
	return APP_CMD_RC_OK;
}

/**
 * @brief  echo 命令：原样回显命令名之后的参数。
 *
 * 用于确认行编辑、分词与串口双向链路均正常。
 *
 * @param ctx 命令上下文；本示例未使用。
 * @param argc 参数个数；分发层已保证 >= 2。
 * @param argv 参数数组；argv[1..argc-1] 为回显内容。
 * @return APP_CMD_RC_OK。
 * @author Xue Weisen
 */
static int cmd_echo(void *ctx, u32 argc, const char *const argv[])
{
	u32 i;

	(void)ctx;
	for (i = 1u; i < argc; i++) {
		uart_print("%s%s", (i == 1u) ? "" : " ", argv[i]);
	}
	uart_print("\r\n");
	return APP_CMD_RC_OK;
}

/**
 * @brief  返回静态命令表首地址与表项数。
 *
 * @param count 输出表项数；允许为 NULL。
 * @return 静态命令表首地址。
 * @author Xue Weisen
 */
const app_cmd_t *app_cmd_table(u32 *count)
{
	if (count != NULL) {
		*count = (u32)ARRAY_SIZE(s_cmd_table);
	}
	return s_cmd_table;
}

/**
 * @brief  遍历命令表打印帮助。
 *
 * 输出为阻塞式串口发送；usage / desc 为 NULL 时对应行跳过。
 *
 * @author Xue Weisen
 */
void app_cmd_help(void)
{
	const app_cmd_t *table;
	u32 count;
	u32 i;

	table = app_cmd_table(&count);
	uart_print("\r\n-- commands --\r\n");
	for (i = 0u; i < count; i++) {
		if (table[i].usage != NULL) {
			uart_print("  %s\r\n", table[i].usage);
		}
		if (table[i].desc != NULL) {
			uart_print("      %s\r\n", table[i].desc);
		}
	}
	uart_print("\r\n");
}

/**
 * @brief  命令行字符状态机：回显、退格、组行，回车时执行命令。
 *
 * 行超过 APP_CMD_LINE_MAX-1 时丢弃该行并提示过长；空行回车不执行任何命令。
 *
 * @param c 控制台收到的 1 个字节；'\r'/'\n' 触发执行，0x08/0x7F 视为退格。
 * @author Xue Weisen
 */
void app_cmd_feed(u8 c)
{
	/* 回车/换行：结束本行并执行；CRLF 的第二个字节执行空行，dispatch_line 直接返回。 */
	if ((c == (u8)'\r') || (c == (u8)'\n')) {
		uart_print("\r\n");
		s_line[s_line_len] = '\0';
		(void)dispatch_line(s_line);
		s_line_len = 0u;
		s_line[0] = '\0';
		return;
	}

	/* 退格：删掉最后一个字符并回显“退格 + 空格 + 退格”。 */
	if ((c == (u8)0x08u) || (c == (u8)0x7Fu)) {
		if (s_line_len > 0u) {
			s_line_len--;
			s_line[s_line_len] = '\0';
			uart_print("\b \b");
		}
		return;
	}

	/* 可打印字符：空间足够则追加并回显，否则丢弃本行剩余输入。 */
	if ((c >= (u8)0x20u) && (c <= (u8)0x7Eu)) {
		if (s_line_len >= (APP_CMD_LINE_MAX - 1u)) {
			uart_print("\r\n[ERR] line too long, dropped\r\n");
			s_line_len = 0u;
			s_line[0] = '\0';
			return;
		}
		s_line[s_line_len] = (char)c;
		s_line_len++;
		uart_print("%c", (char)c);
	}
}

/**
 * @brief  整行执行一条命令（二进制通道 / 测试入口）。
 *
 * 拷进独立静态缓冲后走与 feed 相同的分发内核；不改动 s_line 的行编辑状态。
 *
 * @param line 一行命令（不含回车）；NULL 返回 ARGS。
 * @return APP_CMD_RC_OK / APP_CMD_RC_FAIL / APP_CMD_RC_ARGS。
 * @author Xue Weisen
 */
int app_cmd_exec_line(const char *line)
{
	u32 i = 0u;

	if (line == NULL) {
		return APP_CMD_RC_ARGS;
	}
	while ((line[i] != '\0') && (i < (APP_CMD_LINE_MAX - 1u))) {
		s_exec[i] = line[i];
		i++;
	}
	if (line[i] != '\0') {
		uart_print("[ERR] line too long, dropped\r\n");
		return APP_CMD_RC_ARGS;
	}
	s_exec[i] = '\0';
	return dispatch_line(s_exec);
}
