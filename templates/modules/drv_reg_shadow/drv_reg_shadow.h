//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: drv_reg_shadow
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: 驱动层：PS-PL 控制字影子 + 回读校验 + 握手与 toggle ACK
// Revision: 0.01
// Revision 0.01 - File Created
// Keywords: pl-control-plane-driver, cpu-shadow-register, write-readback-verify,
//   busy-clear-handshake(poll-budget), toggle-ack-change-detect, split-half-stable-read,
//   no-silent-success
//////////////////////////////////////////////////////////////////////////////////

#ifndef DRV_REG_SHADOW_H
#define DRV_REG_SHADOW_H

#include "common_types.h"

/* 统一返回码：0 = 成功；负值 = 失败，调用方按失败语义决定重试或降级。 */
typedef enum {
	DRV_REG_OK             = 0,
	DRV_REG_ERR_INVALID    = -1,   /* 指针为空 / 未 init / 参数非法 */
	DRV_REG_ERR_READBACK   = -2,   /* 写后回读与期望不一致，影子已对齐实际值 */
	DRV_REG_ERR_TIMEOUT    = -3,   /* busy 未落下或 ACK 未变化 */
} drv_reg_status_e;

/* 一个控制字的影子上下文：控制字地址、状态地址、busy 位与轮询预算。
 * 只描述“怎么写 + 怎么确认生效”，不携带任何业务语义。 */
typedef struct {
	uintptr_t ctrl_addr;   /* 控制字寄存器地址 */
	uintptr_t status_addr; /* busy/状态寄存器地址；为 0 时回退到 ctrl_addr */
	u32 busy_mask;      /* 状态寄存器中表示“PL 仍在应用”的位；为 0 表示不等握手 */
	u32 max_poll;       /* busy 轮询与 ACK 等待的最大次数（按总线访问次数计） */
	u32 shadow;         /* CPU 侧期望值（影子） */
	u8  initialized;    /* 0 = 未 init，所有写操作拒绝 */
} drv_reg_shadow_t;

/**
 * @brief  初始化一路控制字影子。
 *
 * 记录地址与握手参数，并把 shadow 对齐到当前回读值；init 之后才能调用写接口。
 * 参数非法（地址为 0、max_poll 为 0）返回失败，不改变结构体内容。
 *
 * @param reg 调用方提供的影子上下文；调用方保证非 NULL 且生命周期覆盖使用期。
 * @param ctrl_addr 控制字地址；不得为 0。
 * @param status_addr 状态/busy 地址；为 0 时使用 ctrl_addr。
 * @param busy_mask busy 位掩码；为 0 表示写完即认为生效（不推荐，需在注释中说明理由）。
 * @param max_poll 最大轮询次数；不得为 0。
 * @retval DRV_REG_OK 初始化成功。
 * @retval DRV_REG_ERR_INVALID 参数非法。
 * @author Xue Weisen
 */
int drv_reg_shadow_init(drv_reg_shadow_t *reg,
						uintptr_t ctrl_addr,
						uintptr_t status_addr,
						u32 busy_mask,
						u32 max_poll);

/**
 * @brief  写控制字：更新影子、回读校验、等待 busy 落下。
 *
 * 先用影子保存期望值，写寄存器后立即回读并要求与期望一致；不一致时把影子对齐实际值
 * 并返回 DRV_REG_ERR_READBACK。回读一致且 busy_mask 非 0 时继续等 busy 位清零；
 * 等待超过 max_poll 次返回 DRV_REG_ERR_TIMEOUT。整个过程不静默成功。
 *
 * @param reg 已初始化的影子上下文；NULL 或未 init 返回 DRV_REG_ERR_INVALID。
 * @param value 要写入的控制字完整 32bit 值。
 * @retval DRV_REG_OK 写入且确认生效。
 * @retval DRV_REG_ERR_INVALID 参数非法或未初始化。
 * @retval DRV_REG_ERR_READBACK 回读不一致（影子已对齐实际值）。
 * @retval DRV_REG_ERR_TIMEOUT busy 未在预算内落下。
 * @pre 控制字地址映射已由 PL 配置完成，总线访问不会挂死。
 * @note 轮询预算按总线周期估算；上板后按实测最坏响应时间标定。
 * @author Xue Weisen
 */
int drv_reg_shadow_write(drv_reg_shadow_t *reg, u32 value);

/**
 * @brief  toggle 类操作：翻转一次触发，等待 ACK 位发生变化。
 *
 * 先记录 ACK 当前值，再翻转控制字中的 toggle 位；随后轮询 ACK，判断“是否变化”
 * 而不是判断“是否为 1”。在 max_poll 次内发生变化返回成功，否则超时。
 * 适用于 snapshot / clear 一类只翻转一次生效的握手。
 *
 * @param ctrl_addr 控制字地址；不得为 0。
 * @param toggle_mask 控制字中的 toggle 位掩码；不得为 0。
 * @param ack_addr ACK 寄存器地址；不得为 0。
 * @param ack_mask ACK 位掩码；不得为 0。
 * @param max_poll 最大轮询次数；不得为 0。
 * @retval DRV_REG_OK ACK 已变化。
 * @retval DRV_REG_ERR_INVALID 参数非法。
 * @retval DRV_REG_ERR_TIMEOUT ACK 未在预算内变化。
 * @note 只翻转 toggle 位，不改动控制字其他位；共享控制字需调用方保证互斥。
 * @author Xue Weisen
 */
int drv_reg_toggle_wait(uintptr_t ctrl_addr,
						u32 toggle_mask,
						uintptr_t ack_addr,
						u32 ack_mask,
						u32 max_poll);

/**
 * @brief  防 32bit 撕裂的稳定读：ACK 未变化才认可读数。
 *
 * 先记 ACK，再读目标寄存器，最后复读 ACK；ACK 未变返回读数，变化则重试，
 * 超过 max_poll 次返回超时。用于统计半字（total / errors）一类跨状态读。
 *
 * @param data_addr 目标数据寄存器地址；不得为 0。
 * @param ack_addr ACK 寄存器地址；不得为 0。
 * @param ack_mask ACK 位掩码；不得为 0。
 * @param out 输出读数；调用方保证非 NULL，失败时不写入。
 * @param max_poll 最大重试次数；不得为 0。
 * @retval DRV_REG_OK 读到 ACK 未变化期间的稳定值。
 * @retval DRV_REG_ERR_INVALID 参数非法。
 * @retval DRV_REG_ERR_TIMEOUT 重试次数内未读到稳定值。
 * @author Xue Weisen
 */
int drv_reg_read_stable_u32(uintptr_t data_addr,
							uintptr_t ack_addr,
							u32 ack_mask,
							u32 *out,
							u32 max_poll);

#endif /* DRV_REG_SHADOW_H */
