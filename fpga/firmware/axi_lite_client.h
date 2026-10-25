/*
 * SPDX-License-Identifier: MIT
 * Author: Xue Weisen <xueweisen96@gmail.com>
 * GitHub: https://github.com/DCRimbaud
 */
#ifndef AXI_LITE_CLIENT_H
#define AXI_LITE_CLIENT_H

#include "embedded_c/mmio.h"
#include "axi_lite_regs_map.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Borrowed AXI-Lite register client bound to one BD address. */
typedef struct {
    uint32_t base_address; /**< MMIO base address from the generated BSP. */
} axi_lite_client_t;

/**
 * @brief Bind a client to a generated AXI-Lite base address.
 *
 * The client stores only the address; it does not map memory, allocate a
 * resource, or retain a platform object. The address must already be valid for
 * the current CPU and must refer to the matching IP version.
 *
 * @param client Caller-owned client object.
 * @param base_address AXI-Lite base address from the current XSA/BSP.
 * @return One when the base address is non-zero and aligned, otherwise zero.
 * @pre The platform MMIO region and device power/clock are initialized.
 * @note The client remains valid until the caller changes the address or
 *       regenerates the hardware platform.
 * @author Xue Weisen
 */
int axi_lite_client_init(axi_lite_client_t *client, uint32_t base_address);

/**
 * @brief Read one 32-bit register from the bound AXI-Lite client.
 *
 * @param client Initialized client.
 * @param offset Register offset from axi_lite_regs_map.h.
 * @return Register value, or zero for invalid or unaligned offsets.
 * @pre The client was successfully initialized and the IP clock is active.
 * @note This is a volatile MMIO read and must not be optimized away.
 * @author Xue Weisen
 */
uint32_t axi_lite_client_read(const axi_lite_client_t *client, uint32_t offset);

/**
 * @brief Write one 32-bit register through the bound AXI-Lite client.
 *
 * @param client Initialized client.
 * @param offset Register offset from axi_lite_regs_map.h.
 * @param value Value written after the offset is validated.
 * @return One on a valid write, or zero for invalid or unaligned offsets.
 * @pre The client was successfully initialized and the IP clock is active.
 * @note This is a volatile MMIO write; the caller owns ordering and cache policy.
 * @author Xue Weisen
 */
int axi_lite_client_write(const axi_lite_client_t *client,
                          uint32_t offset,
                          uint32_t value);

/**
 * @brief Read the generic status register.
 *
 * @param client Initialized client.
 * @return STATUS register value, or zero for an invalid client.
 * @note STATUS is read-only in the reference RTL.
 * @author Xue Weisen
 */
uint32_t axi_lite_client_status(const axi_lite_client_t *client);

/**
 * @brief Read the generic interrupt-status register.
 *
 * @param client Initialized client.
 * @return IRQ_STATUS register value, or zero for an invalid client.
 * @note IRQ_STATUS is read-only in the reference RTL.
 * @author Xue Weisen
 */
uint32_t axi_lite_client_irq_status(const axi_lite_client_t *client);

#ifdef __cplusplus
}
#endif

#endif
