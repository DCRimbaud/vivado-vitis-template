//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: cfg_memory
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: 固件静态/堆/队列/池/DMA 的内存预算入口
// Revision: 0.01
// Revision 0.01 - File Created
// Keywords: memory-budget, freertos-heap, task-stack, descriptor-pool,
//   queue-depth, dma-alignment, compile-time-budget-check
//////////////////////////////////////////////////////////////////////////////////
#ifndef CFG_MEMORY_H
#define CFG_MEMORY_H

/*
 * 以下数值只是起点，不是通用常量。首次目标构建前应在工程配置中覆盖，
 * 并用 map 文件核对实际占用。
 */
#ifndef CFG_MEM_FREERTOS_HEAP_BYTES
#define CFG_MEM_FREERTOS_HEAP_BYTES 65536u
#endif

#ifndef CFG_MEM_TASK_STACKS_BYTES
#define CFG_MEM_TASK_STACKS_BYTES 12288u
#endif

#ifndef CFG_MEM_RTOS_OBJECTS_BYTES
#define CFG_MEM_RTOS_OBJECTS_BYTES 8192u
#endif

#ifndef CFG_MEM_XFER_SLOT_COUNT
#define CFG_MEM_XFER_SLOT_COUNT 8u
#endif

#ifndef CFG_MEM_XFER_SLOT_BYTES
#define CFG_MEM_XFER_SLOT_BYTES 2048u
#endif

#ifndef CFG_MEM_XFER_QUEUE_DEPTH
#define CFG_MEM_XFER_QUEUE_DEPTH 8u
#endif

#ifndef CFG_MEM_CONSOLE_RING_BYTES
#define CFG_MEM_CONSOLE_RING_BYTES 256u
#endif

#ifndef CFG_MEM_LOG_RING_BYTES
#define CFG_MEM_LOG_RING_BYTES 1024u
#endif

#ifndef CFG_MEM_DMA_ALIGN_BYTES
#define CFG_MEM_DMA_ALIGN_BYTES 32u
#endif

/*
 * 本模板的 pool arena 位于 FreeRTOS heap 之外。断言只保护 OS heap 必须覆盖的
 * 部分：任务栈 + RTOS 对象。DMA/pool arena 应放在 BSS 或专用链接段，并在
 * map 预算中单独核算。
 */
_Static_assert(CFG_MEM_FREERTOS_HEAP_BYTES >=
               (CFG_MEM_TASK_STACKS_BYTES + CFG_MEM_RTOS_OBJECTS_BYTES),
               "FreeRTOS heap must cover task stacks and RTOS objects");

_Static_assert(CFG_MEM_XFER_SLOT_COUNT > 0u,
               "buffer pool needs at least one slot");
_Static_assert(CFG_MEM_XFER_SLOT_BYTES > 0u,
               "buffer slot must have storage");
_Static_assert(CFG_MEM_XFER_QUEUE_DEPTH > 0u,
               "descriptor queue needs positive depth");
_Static_assert(CFG_MEM_DMA_ALIGN_BYTES > 0u,
               "DMA alignment must be positive");

#endif /* CFG_MEMORY_H */
