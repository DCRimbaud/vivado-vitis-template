/*
 * SPDX-License-Identifier: MIT
 * Author: Xue Weisen <xueweisen96@gmail.com>
 * GitHub: https://github.com/DCRimbaud
 */
#ifndef AXI_LITE_REGS_MAP_H
#define AXI_LITE_REGS_MAP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AXI_LITE_REG_CONTROL     0x00u
#define AXI_LITE_REG_STATUS      0x04u
#define AXI_LITE_REG_SCRATCH0    0x08u
#define AXI_LITE_REG_SCRATCH1    0x0Cu
#define AXI_LITE_REG_SCRATCH2    0x10u
#define AXI_LITE_REG_SCRATCH3    0x14u
#define AXI_LITE_REG_IRQ_ENABLE  0x18u
#define AXI_LITE_REG_IRQ_STATUS  0x1Cu

#define AXI_LITE_REG_BYTES       0x20u
#define AXI_LITE_REG_ALIGN_MASK  0x03u

#ifdef __cplusplus
}
#endif

#endif
