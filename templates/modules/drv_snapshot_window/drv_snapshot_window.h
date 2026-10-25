//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: drv_snapshot_window
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: PL/BRAM 双缓冲快照窗口：bank 选择、magic/schema 与 sequence 复核
// Revision: 0.01
// Revision: 0.01 - File Created
// Keywords: pl-bram-window, double-buffer, bank-sequence, magic-schema, stable-read,
//   payload-capacity, io-ops-ctx, mmio-adapter, no-product-semantics
//////////////////////////////////////////////////////////////////////////////////
#ifndef DRV_SNAPSHOT_WINDOW_H
#define DRV_SNAPSHOT_WINDOW_H

#include "common_types.h"

/** 快照窗口返回码。 */
typedef enum {
	DRV_SNAPSHOT_OK = 0,
	DRV_SNAPSHOT_ERR_INVALID = -1,
	DRV_SNAPSHOT_ERR_NO_VALID = -2,
	DRV_SNAPSHOT_ERR_IO = -3,
	DRV_SNAPSHOT_ERR_UNSTABLE = -4
} drv_snapshot_rc_e;

/** 32 bit 窗口读取回调；ctx 由绑定方提供。 */
typedef drv_snapshot_rc_e (*drv_snapshot_read32_fn)(void *ctx,
                                                    uintptr_t addr,
                                                    u32 *value);

/** 可替换的窗口读取方法；便于 MMIO、host fake 与测试传输共用。 */
typedef struct {
	drv_snapshot_read32_fn read32;
} drv_snapshot_io_ops_t;

/** 冻结的窗口布局；所有 word 偏移均为 bank 内 32 bit 字偏移。 */
typedef struct {
	uintptr_t base_addr;
	u32 bank_count;
	u32 bank_stride_words;
	u32 magic_word;
	u32 schema_word;
	u32 sequence_lo_word;
	u32 sequence_hi_word;
	u32 flags_word;
	u32 payload_word_offset;
	u32 payload_words;
	u32 magic;
	u32 schema;
	u32 max_attempts;
	u8 has_flags;
	u8 reserved[3];
} drv_snapshot_layout_t;

typedef struct drv_snapshot_window drv_snapshot_window_t;

/** 已稳定读回的 bank 元数据。 */
typedef struct {
	u32 bank;
	u64 sequence;
	u32 flags;
	u32 payload_words;
	u32 attempts;
} drv_snapshot_result_t;

/** 调用方拥有的窗口对象。 */
struct drv_snapshot_window {
	const drv_snapshot_io_ops_t *ops;
	void *io_ctx;
	drv_snapshot_layout_t layout;
	u8 initialized;
};

/** 基于 plt_mmio 的只读窗口方法表。 */
extern const drv_snapshot_io_ops_t drv_snapshot_mmio_ops;

/**
 * @brief 绑定双缓冲快照窗口并校验冻结布局。
 *
 * @param self 调用方提供的窗口对象；不可为 NULL。
 * @param ops 读取方法表；read32 必须非 NULL。
 * @param io_ctx 传给 read32 的上下文。
 * @param layout bank 数、stride、header 偏移与 payload 范围。
 * @return DRV_SNAPSHOT_OK / DRV_SNAPSHOT_ERR_INVALID。
 * @note 该函数不访问硬件；布局非法时不保留任何初始化状态。
 * @author Xue Weisen
 */
drv_snapshot_rc_e drv_snapshot_window_init(drv_snapshot_window_t *self,
                                           const drv_snapshot_io_ops_t *ops,
                                           void *io_ctx,
                                           const drv_snapshot_layout_t *layout);

/**
 * @brief 稳定读取指定 bank 的 payload。
 *
 * @param self 已初始化窗口。
 * @param bank bank 号；必须小于 bank_count。
 * @param payload 调用方 payload 缓冲；失败时内容可能已被部分覆盖。
 * @param capacity_words 缓冲容量；必须不小于 payload_words。
 * @param out 成功后的 bank/sequence/flags/重试次数；只在成功时写入。
 * @return SNAPSHOT 域返回码。
 * @note 读取前后复核 header；超过 max_attempts 返回 UNSTABLE。
 * @author Xue Weisen
 */
drv_snapshot_rc_e drv_snapshot_window_read_bank(drv_snapshot_window_t *self,
                                                u32 bank,
                                                u32 *payload,
                                                u32 capacity_words,
                                                drv_snapshot_result_t *out);

/**
 * @brief 扫描全部 bank 并稳定读取当前观察到的最新一帧。
 *
 * @param self 已初始化窗口。
 * @param payload 调用方 payload 缓冲；失败时内容可能已被部分覆盖。
 * @param capacity_words 缓冲容量；必须不小于 payload_words。
 * @param out 成功后的 bank/sequence/flags/重试次数；只在成功时写入。
 * @return SNAPSHOT 域返回码。
 * @note sequence 使用有符号差值比较，兼容自然回绕；不负责触发新快照。
 *       读取期间若有更新的帧到达，返回的是本轮通过复核的稳定帧，不保证绝对最新。
 * @author Xue Weisen
 */
drv_snapshot_rc_e drv_snapshot_window_read_newest(drv_snapshot_window_t *self,
                                                  u32 *payload,
                                                  u32 capacity_words,
                                                  drv_snapshot_result_t *out);

#endif /* DRV_SNAPSHOT_WINDOW_H */
