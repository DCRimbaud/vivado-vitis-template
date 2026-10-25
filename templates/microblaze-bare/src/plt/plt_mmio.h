//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: plt_mmio
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: 平台层：32 bit MMIO 读写原语
// Revision: 0.01
// Revision 0.01 - File Created
// Keywords: plt-inline, mmio, volatile, uintptr_t, 32bit-register, no-cache-maintenance
//////////////////////////////////////////////////////////////////////////////////
#ifndef PLT_MMIO_H
#define PLT_MMIO_H

#include "common_types.h"

/**
 * @brief 向 32 bit 内存映射寄存器写入一个值。
 *
 * 该原语只提供 volatile 设备访问语义，不负责地址映射、对齐检查、cache 维护或
 * CPU 级内存屏障。地址必须由板级映射和寄存器表保证有效；调用上下文必须已串行化
 * 对同一寄存器的读改写。
 *
 * @param addr 目标 MMIO 地址；必须有效、4 字节对齐。
 * @param value 写入值。
 * @pre PL/PS 地址映射已完成，访问不会落到未译码总线。
 * @note 可在 ISR 或任务上下文调用；本函数本身不阻塞、不可重入地保护复合序列。
 * @author Xue Weisen
 */
static inline void plt_mmio_write32(uintptr_t addr, u32 value)
{
	_Static_assert(sizeof(u32) == 4u, "MMIO write requires 32-bit u32");
	/* 设备寄存器必须逐次真实访问，禁止编译器合并或缓存 volatile 写。 */
	*(volatile u32 *)addr = value;
}

/**
 * @brief 从 32 bit 内存映射寄存器读取一个值。
 *
 * 该原语只提供 volatile 设备访问语义，不负责 cache 一致性、总线错误恢复或
 * 业务层重试。轮询等待的预算和超时必须由驱动层明确实现。
 *
 * @param addr 源 MMIO 地址；必须有效、4 字节对齐。
 * @return 该地址的 32 bit 位模式。
 * @pre PL/PS 地址映射已完成，访问不会落到未译码总线。
 * @note 可在 ISR 或任务上下文调用；本函数不阻塞。
 * @author Xue Weisen
 */
static inline u32 plt_mmio_read32(uintptr_t addr)
{
	_Static_assert(sizeof(u32) == 4u, "MMIO read requires 32-bit u32");
	/* volatile 保证每次轮询都产生一次真实读取，不会被优化成单次缓存值。 */
	return *(volatile const u32 *)addr;
}

#endif /* PLT_MMIO_H */
