/*
 * SPDX-License-Identifier: MIT
 * Author: Xue Weisen <xueweisen96@gmail.com>
 * GitHub: https://github.com/DCRimbaud
 */
#include "axi_lite_client.h"

#include <stddef.h>

int axi_lite_client_init(axi_lite_client_t *client, uint32_t base_address)
{
    if ((client == NULL) || (base_address == 0u) ||
        ((base_address & AXI_LITE_REG_ALIGN_MASK) != 0u)) {
        return 0;
    }

    client->base_address = base_address;
    return 1;
}

uint32_t axi_lite_client_read(const axi_lite_client_t *client, uint32_t offset)
{
    if ((client == NULL) || (client->base_address == 0u) ||
        (offset >= AXI_LITE_REG_BYTES) ||
        ((offset & AXI_LITE_REG_ALIGN_MASK) != 0u)) {
        return 0u;
    }

    return ec_mmio_read32((uintptr_t)(client->base_address + offset));
}

int axi_lite_client_write(const axi_lite_client_t *client,
                          uint32_t offset,
                          uint32_t value)
{
    if ((client == NULL) || (client->base_address == 0u) ||
        (offset >= AXI_LITE_REG_BYTES) ||
        ((offset & AXI_LITE_REG_ALIGN_MASK) != 0u)) {
        return 0;
    }

    ec_mmio_write32((uintptr_t)(client->base_address + offset), value);
    return 1;
}

uint32_t axi_lite_client_status(const axi_lite_client_t *client)
{
    return axi_lite_client_read(client, AXI_LITE_REG_STATUS);
}

uint32_t axi_lite_client_irq_status(const axi_lite_client_t *client)
{
    return axi_lite_client_read(client, AXI_LITE_REG_IRQ_STATUS);
}
