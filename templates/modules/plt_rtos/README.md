# plt_rtos

## 放置

把本目录复制为工程的：

```text
src/plt/rtos/plt_rtos.h
src/plt/rtos/freertos/plt_rtos_freertos.c
```

host 测试另外使用：

```text
src/plt/rtos/fake/plt_rtos_fake.c
```

## FreeRTOS 后端

- 只把 `src/plt/rtos/freertos` 加入 FreeRTOS include 路径；
- `app/mw/drv` 只 include `plt_rtos.h`；
- `plt_rtos_queue_*` 的 timeout 使用毫秒；
- `plt_rtos_isr_exit()` 由 ISR 末尾调用。

## fake 后端

host 测试显式定义：

```text
-DPLT_RTOS_FAKE_HOST=1
```

fake 不启动真实调度器，测试直接调用组件的 `handle_event()` / `step()`。它使用堆，仅用于 host，不得编入目标固件。

## 约束

- 不在 OSAL 之外使用 `TickType_t`、`QueueHandle_t`、`TaskHandle_t`；
- 不把 `void *` 句柄解引用；
- 不静默使用空实现；未支持的后端返回 `PLT_RTOS_ERR_STATE`。
