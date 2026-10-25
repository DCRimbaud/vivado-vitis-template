# drv_snapshot_window

## 职责

本模块只解决 PL 双缓冲内存窗口的通用一致性问题：

1. 扫描各 bank 的 magic/schema；
2. 按 64 bit sequence 选最新 bank；
3. 读 payload 前后复核 header；
4. 在有限重试内拒绝撕裂帧。

它不定义 lane、ADC、PRBS 等产品业务字段。产品语义应放在项目专用 ABI 头和上层解析模块。

## 产品 ABI 注入

项目应提供自己的 ABI 头和参数表，包括：

- `base_addr`、bank 数、bank stride；
- magic/schema/sequence/flags 的 word 偏移；
- payload 起点、产品字段 stride 和容量；
- 产品专用字段解析。

模板不携带具体项目地址、lane 数、magic 或 schema。触发快照、等待 busy 清零和产品字段解析
属于领域层；通用模块只负责 bank 选择、sequence 复核和稳定读取。