//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: drv_reg_shadow
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: 驱动层实现：控制字影子、回读校验、busy 握手与 toggle ACK
// Revision: 0.01
// Revision 0.01 - File Created
// Keywords: shadow-impl, write-readback(busy-clear), toggle-ack-change-detect,
//   stable-read(ack-guard), volatile-mmio-via-plt_mmio, poll-budget-no-wall-clock
//////////////////////////////////////////////////////////////////////////////////

#include "drv_reg_shadow.h"

#include "plt_mmio.h"

/**
 * @brief  初始化一路控制字影子。
 *
 * 参数非法直接拒绝；成功时把 shadow 对齐当前回读值，保证第一次写之前
 * CPU 侧记录与实际寄存器一致。
 *
 * @param reg 影子上下文；非 NULL。
 * @param ctrl_addr 控制字地址；不得为 0。
 * @param status_addr 状态地址；为 0 时使用 ctrl_addr。
 * @param busy_mask busy 位；0 表示不等握手。
 * @param max_poll 轮询预算；不得为 0。
 * @retval DRV_REG_OK 初始化成功。
 * @retval DRV_REG_ERR_INVALID 参数非法。
 * @author Xue Weisen
 */
int drv_reg_shadow_init(drv_reg_shadow_t *reg,
						uintptr_t ctrl_addr,
						uintptr_t status_addr,
						u32 busy_mask,
						u32 max_poll)
{
	/* 校验阶段：地址与预算缺一不可，避免“看起来成功、实际无法确认生效”。 */
	if ((reg == NULL) || (ctrl_addr == 0u) || (max_poll == 0u)) {
		return DRV_REG_ERR_INVALID;
	}

	reg->ctrl_addr   = ctrl_addr;
	reg->status_addr = (status_addr == 0u) ? ctrl_addr : status_addr;
	reg->busy_mask   = busy_mask;
	reg->max_poll    = max_poll;
	reg->shadow      = plt_mmio_read32(ctrl_addr);
	reg->initialized = 1u;
	return DRV_REG_OK;
}

/**
 * @brief  写控制字：更新影子、回读校验、等待 busy 落下。
 *
 * 回读不一致时按“实际值优先”把影子对齐寄存器，并返回失败；
 * 只有回读一致且 busy 落下才返回成功。
 *
 * @param reg 已初始化影子；NULL/未 init 返回 DRV_REG_ERR_INVALID。
 * @param value 目标控制字。
 * @retval DRV_REG_OK 写入且确认生效。
 * @retval DRV_REG_ERR_INVALID 参数非法。
 * @retval DRV_REG_ERR_READBACK 回读不一致。
 * @retval DRV_REG_ERR_TIMEOUT busy 未落下。
 * @author Xue Weisen
 */
int drv_reg_shadow_write(drv_reg_shadow_t *reg, u32 value)
{
	u32 i;
	u32 actual;

	/* 校验阶段：未 init 的上下文不能猜地址，直接拒绝。 */
	if ((reg == NULL) || (reg->initialized == 0u)) {
		return DRV_REG_ERR_INVALID;
	}

	/* 写与回读阶段：影子先记期望值；回读不一致说明总线或 PL 语义有偏差。 */
	reg->shadow = value;
	plt_mmio_write32(reg->ctrl_addr, value);
	actual = plt_mmio_read32(reg->ctrl_addr);
	if (actual != value) {
		reg->shadow = actual;   /* 实际值优先：下次写基于真实寄存器，不带病重试。 */
		return DRV_REG_ERR_READBACK;
	}

	/* 握手阶段：busy_mask=0 表示设计上无需等待，由调用方在注释里说明理由。 */
	if (reg->busy_mask == 0u) {
		return DRV_REG_OK;
	}
	for (i = 0u; i < reg->max_poll; i++) {
		if ((plt_mmio_read32(reg->status_addr) & reg->busy_mask) == 0u) {
			return DRV_REG_OK;
		}
	}
	return DRV_REG_ERR_TIMEOUT;
}

/**
 * @brief  toggle 类操作：翻转一次触发，等 ACK 变化。
 *
 * 只翻转 toggle 位；判断 ACK“是否变化”而不是“是否为 1”，
 * 避免上一次操作的 ACK 残留导致假成功。
 *
 * @param ctrl_addr 控制字地址；不得为 0。
 * @param toggle_mask toggle 位；不得为 0。
 * @param ack_addr ACK 地址；不得为 0。
 * @param ack_mask ACK 位；不得为 0。
 * @param max_poll 轮询预算；不得为 0。
 * @retval DRV_REG_OK ACK 已变化。
 * @retval DRV_REG_ERR_INVALID 参数非法。
 * @retval DRV_REG_ERR_TIMEOUT ACK 未变化。
 * @author Xue Weisen
 */
int drv_reg_toggle_wait(uintptr_t ctrl_addr,
						u32 toggle_mask,
						uintptr_t ack_addr,
						u32 ack_mask,
						u32 max_poll)
{
	u32 ack_before;
	u32 ctrl;
	u32 i;

	/* 校验阶段：四个地址/掩码缺一不可，避免误翻转或误判。 */
	if ((ctrl_addr == 0u) || (toggle_mask == 0u) ||
	    (ack_addr == 0u) || (ack_mask == 0u) || (max_poll == 0u)) {
		return DRV_REG_ERR_INVALID;
	}

	/* 触发阶段：先记 ACK 基线，再翻转 toggle 位，保持控制字其他位不变。 */
	ack_before = plt_mmio_read32(ack_addr) & ack_mask;
	ctrl = plt_mmio_read32(ctrl_addr);
	plt_mmio_write32(ctrl_addr, ctrl ^ toggle_mask);

	/* 确认阶段：ACK 变化即生效；超时返回失败，由调用方决定重试或上报。 */
	for (i = 0u; i < max_poll; i++) {
		if ((plt_mmio_read32(ack_addr) & ack_mask) != ack_before) {
			return DRV_REG_OK;
		}
	}
	return DRV_REG_ERR_TIMEOUT;
}

/**
 * @brief  防 32bit 撕裂的稳定读：ACK 未变化才认可读数。
 *
 * 先记 ACK，读数据，再复读 ACK；两段 ACK 一致说明读数没有跨状态变化，
 * 不一致则重试。超过预算返回超时，绝不把撕裂值当结果。
 *
 * @param data_addr 数据地址；不得为 0。
 * @param ack_addr ACK 地址；不得为 0。
 * @param ack_mask ACK 位；不得为 0。
 * @param out 输出读数；非 NULL，失败时不写入。
 * @param max_poll 最大重试次数；不得为 0。
 * @retval DRV_REG_OK 读到稳定值。
 * @retval DRV_REG_ERR_INVALID 参数非法。
 * @retval DRV_REG_ERR_TIMEOUT 未读到稳定值。
 * @author Xue Weisen
 */
int drv_reg_read_stable_u32(uintptr_t data_addr,
							uintptr_t ack_addr,
							u32 ack_mask,
							u32 *out,
							u32 max_poll)
{
	u32 i;
	u32 ack_a;
	u32 ack_b;
	u32 data;

	/* 校验阶段：输出指针必须有效，失败时不写 *out，保持调用方原值。 */
	if ((data_addr == 0u) || (ack_addr == 0u) || (ack_mask == 0u) ||
	    (out == NULL) || (max_poll == 0u)) {
		return DRV_REG_ERR_INVALID;
	}

	/* 读取阶段：ACK 前后夹住数据读，任何一侧变化都视为读数可能撕裂。 */
	for (i = 0u; i < max_poll; i++) {
		ack_a = plt_mmio_read32(ack_addr) & ack_mask;
		data  = plt_mmio_read32(data_addr);
		ack_b = plt_mmio_read32(ack_addr) & ack_mask;
		if (ack_a == ack_b) {
			*out = data;
			return DRV_REG_OK;
		}
	}
	return DRV_REG_ERR_TIMEOUT;
}
