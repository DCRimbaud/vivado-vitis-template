//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: app_main
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: 应用入口实现：分阶段启动、失败观测与主轮询
// Revision: 0.02
// Revision 0.01 - File Created
// Revision 0.02 - 中断/控制台返回码逐级上抛，删除 stat_init_done 和公共可变全局
// Keywords: boot-stages, explicit-rc, fail-step-observer, poll-loop, scutimer-flag, no-mutable-global
//////////////////////////////////////////////////////////////////////////////////
#include "app_main.h"
#include "mw_console.h"
#include "plt_int.h"

/* 启动失败阶段名只供只读查询，不让业务层写共享全局。 */
static const char *s_boot_fail_step;

/**
 * @brief 按依赖顺序初始化 ScuGic/ScuTimer、控制台与业务服务。
 *
 * @return APP_BOOT_OK 或阶段专属失败码。
 * @pre 平台和 cache 初始化已完成。
 * @note 任一步失败立即停止后续初始化，调用方可读取失败阶段。
 * @author Xue Weisen
 */
app_boot_rc_e app_start(void)
{
	plt_int_rc_e int_rc;
	mw_console_rc_e console_rc;

	/* 清空观测状态：本次启动失败不得沿用上一次阶段名。 */
	s_boot_fail_step = NULL;

	/* 中断阶段：GIC 与 1ms ScuTimer 必须先就绪。 */
	int_rc = plt_int_init();
	if (int_rc != PLT_INT_OK) {
		s_boot_fail_step = "plt_int";
		return APP_BOOT_ERR_INT;
	}

	/* 控制台阶段：UART、ISR 连接和使能任一失败都停止启动。 */
	console_rc = mw_console_init();
	if (console_rc != MW_CONSOLE_OK) {
		s_boot_fail_step = "mw_console";
		return APP_BOOT_ERR_CONSOLE;
	}

	/* 业务阶段：按需插入可失败初始化；示例当前无业务模块。 */

	/* 放行阶段：命令回调由应用在所有依赖就绪后通过注册显式放行。 */
	uart_print("\r\napplication started.\r\n");
	return APP_BOOT_OK;
}

/**
 * @brief 返回最后一次启动失败阶段名。
 *
 * @return 静态只读字符串；NULL 表示没有失败。
 * @author Xue Weisen
 */
const char *app_boot_fail_step(void)
{
	return s_boot_fail_step;
}

/**
 * @brief 应用主轮询体：按 1s 节拍输出存活信息。
 *
 * @note 每拍读取并清除 1s 标志，禁止阻塞超过一个节拍。
 * @author Xue Weisen
 */
void app_poll(void)
{
	/* 节拍阶段：is_timer_1s 读取即清零，本函数是唯一消费者。 */
	if (is_timer_1s() != 0u) {
		uart_print("alive %u s\r\n", (unsigned)plt_int_now_s());
	}
}
