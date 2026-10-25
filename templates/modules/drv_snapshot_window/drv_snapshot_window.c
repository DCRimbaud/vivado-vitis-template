//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: drv_snapshot_window
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: 双缓冲窗口布局校验、最新 bank 选择与读前读后复核
// Revision: 0.01
// Revision: 0.01 - File Created
// Keywords: snapshot-layout-validation, sequence-wrap-compare, bank-scan,
//   read-before-after-header, caller-payload-scratch, bounded-retry
//////////////////////////////////////////////////////////////////////////////////
#include "drv_snapshot_window.h"

typedef struct {
	u32 bank;
	u64 sequence;
	u32 flags;
} drv_snapshot_header_t;

/**
 * @brief 校验冻结窗口布局是否自洽。
 *
 * 检查 bank 数、stride、header 偏移和 payload 范围，避免运行时出现越界读。
 *
 * @param layout 待检查布局；允许为 NULL。
 * @return 1 = 合法；0 = 参数或范围非法。
 * @author Xue Weisen
 */
static int drv_snapshot_layout_valid(const drv_snapshot_layout_t *layout)
{
	u64 payload_end;

	/* 基本范围：bank/stride/attempt 上限必须在可计算的窄范围内。 */
	if ((layout == 0) || (layout->base_addr == 0u) || (layout->bank_count == 0u) ||
	    (layout->bank_count > 8u) || (layout->bank_stride_words == 0u) ||
	    (layout->payload_words == 0u) || (layout->max_attempts == 0u) ||
	    (layout->max_attempts > 16u) || (layout->has_flags > 1u)) {
		return 0;
	}
	/* header 阶段：所有 header word 必须位于 payload 之前。 */
	if ((layout->magic_word >= layout->payload_word_offset) ||
	    (layout->schema_word >= layout->payload_word_offset) ||
	    (layout->sequence_lo_word >= layout->payload_word_offset) ||
	    (layout->sequence_hi_word >= layout->payload_word_offset)) {
		return 0;
	}
	if ((layout->has_flags != 0u) && (layout->flags_word >= layout->payload_word_offset)) {
		return 0;
	}
	/* payload 阶段：payload 必须完整落在一个 bank stride 内。 */
	payload_end = (u64)layout->payload_word_offset + (u64)layout->payload_words;
	return (payload_end <= layout->bank_stride_words) ? 1 : 0;
}

/**
 * @brief 判断窗口对象是否已经完成绑定。
 *
 * @param self 窗口对象；允许为 NULL。
 * @return 1 = 可访问；0 = 未初始化。
 * @note 只检查本地字段，不访问目标内存。
 * @author Xue Weisen
 */
static int drv_snapshot_window_ready(const drv_snapshot_window_t *self)
{
	return ((self != 0) && (self->initialized != 0u) && (self->ops != 0) &&
	        (self->ops->read32 != 0)) ? 1 : 0;
}

/**
 * @brief 计算 bank 内指定 word 的绝对地址。
 *
 * @param self 已初始化窗口。
 * @param bank bank 号。
 * @param word bank 内字偏移。
 * @param addr 输出地址；只在返回 1 时有效。
 * @return 1 = 地址有效；0 = 越界或 uintptr_t 溢出。
 * @author Xue Weisen
 */
static int drv_snapshot_word_addr(const drv_snapshot_window_t *self,
                                  u32 bank,
                                  u32 word,
                                  uintptr_t *addr)
{
	u64 byte_offset;

	/* 边界阶段：bank/word 必须位于冻结布局内。 */
	if ((bank >= self->layout.bank_count) || (word >= self->layout.bank_stride_words)) {
		return 0;
	}
	/* 地址阶段：用 64 bit 中间量计算，再检查 uintptr_t 是否溢出。 */
	byte_offset = ((u64)bank * self->layout.bank_stride_words + word) * 4u;
	if (byte_offset > (u64)UINTPTR_MAX) {
		return 0;
	}
	if ((u64)self->layout.base_addr > ((u64)UINTPTR_MAX - byte_offset)) {
		return 0;
	}
	*addr = (uintptr_t)((u64)self->layout.base_addr + byte_offset);
	return 1;
}

/**
 * @brief 读取窗口中的一个 32 bit 字。
 *
 * @param self 已初始化窗口。
 * @param bank bank 号。
 * @param word bank 内字偏移。
 * @param value 输出值；只在成功时写入。
 * @return SNAPSHOT 域返回码。
 * @author Xue Weisen
 */
static drv_snapshot_rc_e drv_snapshot_read_word(const drv_snapshot_window_t *self,
                                                u32 bank,
                                                u32 word,
                                                u32 *value)
{
	uintptr_t addr;

	if ((self == 0) || (value == 0) ||
	    (drv_snapshot_word_addr(self, bank, word, &addr) == 0)) {
		return DRV_SNAPSHOT_ERR_INVALID;
	}
	return self->ops->read32(self->io_ctx, addr, value);
}

/**
 * @brief 读取并验证一个 bank 的 magic/schema/sequence/flags。
 *
 * @param self 已初始化窗口。
 * @param bank bank 号。
 * @param out header 输出；只在成功时写入。
 * @return SNAPSHOT 域返回码；magic/schema 不匹配返回 NO_VALID。
 * @author Xue Weisen
 */
static drv_snapshot_rc_e drv_snapshot_read_header(const drv_snapshot_window_t *self,
                                                  u32 bank,
                                                  drv_snapshot_header_t *out)
{
	drv_snapshot_rc_e rc;
	u32 magic;
	u32 schema;
	u32 seq_lo;
	u32 seq_hi;
	u32 flags = 0u;

	/* header 前两字先确定这是本 schema 的帧。 */
	rc = drv_snapshot_read_word(self, bank, self->layout.magic_word, &magic);
	if (rc != DRV_SNAPSHOT_OK) {
		return rc;
	}
	rc = drv_snapshot_read_word(self, bank, self->layout.schema_word, &schema);
	if (rc != DRV_SNAPSHOT_OK) {
		return rc;
	}
	if ((magic != self->layout.magic) || (schema != self->layout.schema)) {
		return DRV_SNAPSHOT_ERR_NO_VALID;
	}
	/* sequence 阶段：lo/hi 同帧读取，拼成 64 bit 序号。 */
	rc = drv_snapshot_read_word(self, bank, self->layout.sequence_lo_word, &seq_lo);
	if (rc != DRV_SNAPSHOT_OK) {
		return rc;
	}
	rc = drv_snapshot_read_word(self, bank, self->layout.sequence_hi_word, &seq_hi);
	if (rc != DRV_SNAPSHOT_OK) {
		return rc;
	}
	if (self->layout.has_flags != 0u) {
		/* flags 可选；缺失时保持 0，不把未定义行为带进复核。 */
		rc = drv_snapshot_read_word(self, bank, self->layout.flags_word, &flags);
		if (rc != DRV_SNAPSHOT_OK) {
			return rc;
		}
	}
	out->bank = bank;
	out->sequence = ((u64)seq_hi << 32) | (u64)seq_lo;
	out->flags = flags;
	return DRV_SNAPSHOT_OK;
}

/**
 * @brief 比较读前与读后的 header 指纹。
 *
 * @param a 读前 header。
 * @param b 读后 header。
 * @return 1 = bank/sequence/flags 完全一致；0 = 可能发生覆盖。
 * @author Xue Weisen
 */
static int drv_snapshot_header_equal(const drv_snapshot_header_t *a,
                                     const drv_snapshot_header_t *b)
{
	return ((a->bank == b->bank) && (a->sequence == b->sequence) &&
	        (a->flags == b->flags)) ? 1 : 0;
}

/**
 * @brief 从指定 bank 复制 payload 到调用方缓冲。
 *
 * @param self 已初始化窗口。
 * @param bank bank 号。
 * @param payload 调用方缓冲；容量已由公共入口检查。
 * @return SNAPSHOT 域返回码。
 * @note 失败时 payload 可能已被部分覆盖，调用方不得使用。
 * @author Xue Weisen
 */
static drv_snapshot_rc_e drv_snapshot_read_payload(drv_snapshot_window_t *self,
                                                   u32 bank,
                                                   u32 *payload)
{
	u32 i;
	drv_snapshot_rc_e rc;

	/* copy 阶段：逐字读取，失败时立即停止；payload 被视为 scratch。 */
	for (i = 0u; i < self->layout.payload_words; i++) {
		rc = drv_snapshot_read_word(self, bank,
		                            self->layout.payload_word_offset + i,
		                            &payload[i]);
		if (rc != DRV_SNAPSHOT_OK) {
			return rc;
		}
	}
	return DRV_SNAPSHOT_OK;
}

/**
 * @brief 执行一次“读 header、读 payload、复核 header”的稳定读。
 *
 * @param self 已初始化窗口。
 * @param bank 目标 bank。
 * @param payload payload 缓冲。
 * @param out 成功结果。
 * @return SNAPSHOT 域返回码；header 变化返回 UNSTABLE。
 * @author Xue Weisen
 */
static drv_snapshot_rc_e drv_snapshot_read_bank_once(drv_snapshot_window_t *self,
                                                     u32 bank,
                                                     u32 *payload,
                                                     drv_snapshot_result_t *out)
{
	drv_snapshot_header_t before;
	drv_snapshot_header_t after;
	drv_snapshot_rc_e rc;

	/* 指纹阶段：先记录读前 bank/sequence/flags。 */
	rc = drv_snapshot_read_header(self, bank, &before);
	if (rc != DRV_SNAPSHOT_OK) {
		return rc;
	}
	rc = drv_snapshot_read_payload(self, bank, payload);
	if (rc != DRV_SNAPSHOT_OK) {
		return rc;
	}
	rc = drv_snapshot_read_header(self, bank, &after);
	if (rc != DRV_SNAPSHOT_OK) {
		return rc;
	}
	if (drv_snapshot_header_equal(&before, &after) == 0) {
		/* 复核阶段：header 变化说明读期间被覆盖，本帧作废。 */
		return DRV_SNAPSHOT_ERR_UNSTABLE;
	}
	/* 提交阶段：只有读前读后一致时才构造调用方结果。 */
	out->bank = bank;
	out->sequence = before.sequence;
	out->flags = before.flags;
	out->payload_words = self->layout.payload_words;
	out->attempts = 1u;
	return DRV_SNAPSHOT_OK;
}

/**
 * @brief 绑定双缓冲快照窗口并校验冻结布局。
 *
 * @param self 窗口对象。
 * @param ops 读取方法表。
 * @param io_ctx 读取上下文。
 * @param layout 冻结布局。
 * @return SNAPSHOT 域返回码。
 * @author Xue Weisen
 */
drv_snapshot_rc_e drv_snapshot_window_init(drv_snapshot_window_t *self,
                                           const drv_snapshot_io_ops_t *ops,
                                           void *io_ctx,
                                           const drv_snapshot_layout_t *layout)
{
	if (self == 0) {
		return DRV_SNAPSHOT_ERR_INVALID;
	}
	if ((ops == 0) || (ops->read32 == 0) || (drv_snapshot_layout_valid(layout) == 0)) {
		/* 非法重绑定必须关闭旧对象，避免调用方继续使用过期布局。 */
		self->ops = 0;
		self->io_ctx = 0;
		self->initialized = 0u;
		return DRV_SNAPSHOT_ERR_INVALID;
	}
	self->ops = ops;
	self->io_ctx = io_ctx;
	self->layout = *layout;
	self->initialized = 1u;
	return DRV_SNAPSHOT_OK;
}

/**
 * @brief 在有限重试内稳定读取指定 bank。
 *
 * @param self 已初始化窗口。
 * @param bank bank 号。
 * @param payload payload 缓冲。
 * @param capacity_words 缓冲容量。
 * @param out 成功结果。
 * @return SNAPSHOT 域返回码。
 * @note 只有 UNSTABLE 会重试；参数和 IO 错误立即返回。
 * @author Xue Weisen
 */
drv_snapshot_rc_e drv_snapshot_window_read_bank(drv_snapshot_window_t *self,
                                                u32 bank,
                                                u32 *payload,
                                                u32 capacity_words,
                                                drv_snapshot_result_t *out)
{
	drv_snapshot_result_t local;
	drv_snapshot_rc_e rc = DRV_SNAPSHOT_ERR_UNSTABLE;
	u32 attempt;

	/* 公共入口：容量和 bank 越界在触碰硬件前拒绝。 */
	if ((drv_snapshot_window_ready(self) == 0) || (bank >= self->layout.bank_count) ||
	    (payload == 0) || (out == 0) || (capacity_words < self->layout.payload_words)) {
		return DRV_SNAPSHOT_ERR_INVALID;
	}
	for (attempt = 0u; attempt < self->layout.max_attempts; attempt++) {
		rc = drv_snapshot_read_bank_once(self, bank, payload, &local);
		if (rc == DRV_SNAPSHOT_OK) {
			local.attempts = attempt + 1u;
			*out = local;
			return DRV_SNAPSHOT_OK;
		}
		if (rc != DRV_SNAPSHOT_ERR_UNSTABLE) {
			return rc;
		}
		/* 只有可恢复的覆盖才重试；IO/参数错误立即返回。 */
	}
	return rc;
}

/**
 * @brief 用有符号差值判断 sequence 是否更新。
 *
 * @param candidate 待比较 sequence。
 * @param reference 参考 sequence。
 * @return 1 = candidate 更新；0 = 相同或更旧。
 * @note 有符号差值比较兼容 64 bit 自然回绕。
 * @author Xue Weisen
 */
static int drv_snapshot_sequence_newer(u64 candidate, u64 reference)
{
	return ((s64)(candidate - reference) > 0) ? 1 : 0;
}

/**
 * @brief 扫描全部 bank 并稳定读取当前观察到的最新 payload。
 *
 * 每次尝试都重新扫描 bank；选中最新 header 后执行一次稳定读。若读取期间
 * bank 被覆盖，则继续下一次尝试；IO 错误会保留并在最终无有效帧时返回。
 *
 * @param self 已初始化窗口。
 * @param payload payload 缓冲。
 * @param capacity_words 缓冲容量。
 * @param out 成功结果。
 * @return SNAPSHOT 域返回码。
 * @author Xue Weisen
 */
drv_snapshot_rc_e drv_snapshot_window_read_newest(drv_snapshot_window_t *self,
                                                  u32 *payload,
                                                  u32 capacity_words,
                                                  drv_snapshot_result_t *out)
{
	drv_snapshot_result_t local;
	drv_snapshot_header_t best;
	drv_snapshot_header_t header;
	drv_snapshot_rc_e rc;
	drv_snapshot_rc_e last = DRV_SNAPSHOT_ERR_NO_VALID;
	u32 attempt;
	u32 bank;
	int saw_valid;
	int saw_io = 0;

	/* 公共入口：容量不足时不启动扫描。 */
	if ((drv_snapshot_window_ready(self) == 0) || (payload == 0) || (out == 0) ||
	    (capacity_words < self->layout.payload_words)) {
		return DRV_SNAPSHOT_ERR_INVALID;
	}
	for (attempt = 0u; attempt < self->layout.max_attempts; attempt++) {
		saw_valid = 0;
		/* 扫描阶段：每轮都重读所有 bank，bank 可能刚被新帧覆盖。 */
		for (bank = 0u; bank < self->layout.bank_count; bank++) {
			rc = drv_snapshot_read_header(self, bank, &header);
			if (rc == DRV_SNAPSHOT_ERR_IO) {
				saw_io = 1;
				continue;
			}
			if (rc != DRV_SNAPSHOT_OK) {
				continue;
			}
			if ((saw_valid == 0) ||
			    (drv_snapshot_sequence_newer(header.sequence, best.sequence) != 0)) {
				/* 选择阶段：sequence 用有符号差值比较，兼容自然回绕。 */
				best = header;
				saw_valid = 1;
			}
		}
		if (saw_valid == 0) {
			/* 无有效帧与 IO 错误分开报告，便于上层决定复位或等待。 */
			last = (saw_io != 0) ? DRV_SNAPSHOT_ERR_IO : DRV_SNAPSHOT_ERR_NO_VALID;
			continue;
		}
		rc = drv_snapshot_read_bank_once(self, best.bank, payload, &local);
		if (rc == DRV_SNAPSHOT_OK) {
			local.attempts = attempt + 1u;
			*out = local;
			return DRV_SNAPSHOT_OK;
		}
		last = rc;
		/* 读后复核失败时进入下一轮重新选 bank。 */
		if (rc == DRV_SNAPSHOT_ERR_IO) {
			saw_io = 1;
		}
	}
	if ((last == DRV_SNAPSHOT_ERR_NO_VALID) && (saw_io != 0)) {
		return DRV_SNAPSHOT_ERR_IO;
	}
	return last;
}
