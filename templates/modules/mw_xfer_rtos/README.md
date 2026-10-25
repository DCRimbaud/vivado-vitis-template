# mw_xfer_rtos

用途：在任务/ISR 之间传递 `common_xfer_t` 描述符，不复制 payload。

## 所有权

1. 生产者先从固定池取得 lease，或准备静态/借用 payload；
2. `mw_xfer_rtos_send()` 成功后，队列取得描述符的消费权；
3. 发送失败时，生产者仍拥有 payload，必须回滚或释放 lease；
4. 消费者收到后处理 payload，再调用 `common_xfer_release()`；
5. `common_xfer_release()` 失败时保留描述符信息，记录泄漏并进入故障路径。

## ISR

`mw_xfer_rtos_send_from_isr()` 只接受 ISR 前已准备好的描述符。
ISR 不 acquire pool、不释放 payload、不打印、不解析协议。
ISR 侧 drop 由设备/端口自己的单写者计数器记录，避免在公共层做非原子 `u32++`。

## 不要使用

- 把 payload 字节直接塞进 OS 队列（会产生额外复制和不确定内存）；
- 用 `mw_xfer_queue` 替代跨任务队列；
- 在 ISR 内调用 `common_xfer_release()`；
- 成功后生产者继续写 payload。
