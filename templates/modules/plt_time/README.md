# plt_time

## 职责

提供与 OS tick 解耦的单调时间源。业务和故障记录只依赖 `plt_time.h`，
不直接包含 `xtime_l.h`、`FreeRTOS.h` 或 BSP timer 头。

## 后端选择

| 场景 | 后端 |
|---|---|
| Zynq-7000 / Cortex-A9 | `plt_time_xtime_bind()`，读取 Global Timer |
| MicroBlaze | 新增 AXI Timer/`XTime` 等价适配器 |
| host fake | 提供可手工推进的微秒回调 |
| FreeRTOS | 优先专用硬件时间源；tick 只用于调度，不承担高精度时间戳 |

## 失败语义

读取失败不返回“0 时间”。`plt_time_now_*()` 返回错误码，调用方必须决定降级或保持旧时间。
