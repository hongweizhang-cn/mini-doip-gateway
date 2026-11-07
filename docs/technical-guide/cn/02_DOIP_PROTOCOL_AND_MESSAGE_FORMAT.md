# 02 DoIP 协议与消息格式

## 学习目标

能够解释项目实际处理的 Header、Routing Activation Request/Response 字段及其字节偏移；能将结构体、Payload Body 与 TCP 字节流区分开。

## 先区分三层概念

- **协议背景**：DoIP 消息在 Generic Header 后带 Payload；Header 中的 Payload Type 决定载荷类型，Payload Length 表示后续载荷字节数。Routing Activation 是诊断路由可用前的一种交互。
- **项目实现**：只接受版本 `0x02`/反版本 `0xFD`，实现 Routing Activation Request/Response 的基本固定格式，并通过 TCP 演示一问一答。
- **简化范围**：不实现完整 ISO 13400 规定的所有消息、状态和策略。下面的解释只把代码支持的行为视作已实现；标准背景不等同于本项目的功能声明。

## Generic Header：8 字节

`doip_header_t` 是内存对象；线上头部由 `doip_header_encode/decode` 在 `doip_header.c` 中逐字段写读。偏移从完整 DoIP 消息开头算起。

| 偏移 | 宽度 | 字段 | 协议作用 | 当前代码 |
|---:|---:|---|---|---|
| 0 | 1 | Protocol Version | 标识协议版本。 | `DOIP_PROTOCOL_VERSION` 固定为 `0x02`；Header Encoder/Decoder 均仅接受该值。 |
| 1 | 1 | Inverse Protocol Version | 与版本字节互补，用于检测 Header 一致性。 | 固定为 `0xFD`，代码检查其与 `0x02` 配对；不是任意版本通用实现。 |
| 2–3 | 2 | Payload Type | 标识后续载荷类别。 | 按大端读写；Gateway 只接受 `0x0005` Request。 |
| 4–7 | 4 | Payload Length | 后续 Payload Body 的字节数，不含 8 字节 Header。 | 按大端读写为 `uint32_t`；Message Codec 校验长度，Server 另限制为 4096。 |

`doip_payload_type.h` 定义的常量包括 `0x0001` Vehicle Identification Request、`0x0002` 带 EID 的请求、`0x0003` 带 VIN 的请求、`0x0004` Vehicle Announcement、`0x0005` Routing Activation Request、`0x0006` Routing Activation Response。定义常量本身不实现解析或处理。Gateway 的请求分发只支持 `0x0005`；`doip_tester.c` 期望响应类型 `0x0006`。

## Routing Activation Request：项目支持的 7 字节基本载荷

偏移相对 Payload Body 起点；完整消息中的绝对偏移为此表偏移加 8。

| Body 偏移 | 宽度 | 字段 / 当前表示 | 处理 |
|---:|---:|---|---|
| 0–1 | 2 | Source / Tester Logical Address，`source_address` | Request Codec 以大端读写，Gateway 用它作为单状态上下文的客户端标识。 |
| 2 | 1 | Activation Type，`activation_type` | Codec 保留该值；当前状态逻辑不校验或依据它决策。 |
| 3–6 | 4 | Reserved | Encoder 写零；Decoder 要求 4 字节均为零。 |

常见 Routing Activation 概念中，Tester 请求 Gateway 为逻辑地址建立诊断路由上下文；但本项目的接受策略仅基于是否空闲及逻辑地址是否相同，不等同于完整授权。请求长度必须恰为 7 字节；带额外数据的更长请求会以 `DOIP_RESULT_UNSUPPORTED` 退出，而不是解析扩展。

## Routing Activation Response：项目支持的 9 字节基本载荷

| Body 偏移 | 宽度 | 字段 / 当前表示 | 处理 |
|---:|---:|---|---|
| 0–1 | 2 | Client Logical Address，`client_logical_address` | 从已解码 Request 的 `source_address` 回填。 |
| 2–3 | 2 | DoIP Entity Logical Address，`doip_entity_logical_address` | 来自 Gateway 初始化配置；Tester 会与命令行期望值比较。 |
| 4 | 1 | Response Code，`response_code` | Codec 原样保存未知值；状态模块对成功使用 `0x10`，冲突值由 Server 配置为 `0x03`。Tester 只将 `0x10` 作为成功。 |
| 5–8 | 4 | Reserved | Encoder 写零；Decoder 要求为零。 |

Response Decoder 对长度为 13 的 Body 返回 `DOIP_RESULT_UNSUPPORTED`（与其注释所述 4 字节 OEM 扩展相对应）；长度小于 9 返回 `BUFFER_TOO_SMALL`，其他非 9 长度返回 `INVALID_LENGTH`。Response Code 的合法性并不由 Codec 判断。

## Logical Address、Activation Type、Response Code

- **Logical Address**：DoIP 消息中的 16 位标识。本项目用 Request `source_address` 区分状态中的逻辑客户端，并用配置的 Entity 地址生成 Response。代码不验证地址是否落在标准/车辆具体配置允许的范围内。
- **Activation Type**：Request 的一个字节字段。当前代码仅保存、回填于解码请求结构中；状态函数完全不读该成员。因此不能称为已实现 activation-type 策略。
- **Response Code**：结果字节。状态模块把空闲时的首请求和活动时同地址重试映射为 `0x10`；不同地址返回配置的 `conflict_response_code`；Response Codec 不解释它；Tester 把非 `0x10` 一律作为失败。
- 代码没有完整 response-code 集合或统一语义映射。`0x03` 只是 `doip_tcp_server_main.c` 的默认冲突配置，不应被概括为所有部署中的标准策略。

## 网络字节序（大端序）

多字节整数最高有效字节先出现。例如地址 `0x0E00` 编为 `0E 00`，载荷长度 7 编为 `00 00 00 07`。代码通过移位、掩码和按字节组合实现，而不是复制 C 结构体内存。这样不依赖 CPU 字节序、对齐或结构体 padding。

## 完整示例

默认 Tester 向默认 Entity 发送基本 Routing Activation Request（Tester `0x0E00`、Activation Type `0x01`）：

```text
02 FD 00 05 00 00 00 07  0E 00 01 00 00 00 00
| Header (8 bytes)       | Request Body (7 bytes)        |
```

逐字段为：

```text
02       Protocol Version
FD       Inverse Protocol Version
00 05    Routing Activation Request Payload Type
00 00 00 07 Payload Length = 7
0E 00    Source / Tester Logical Address
01       Activation Type
00 00 00 00 Reserved
```

成功响应（Entity `0x1000`，Response Code `0x10`）：

```text
02 FD 00 06 00 00 00 09  0E 00 10 00 10 00 00 00 00
| Header (8 bytes)       | Response Body (9 bytes)         |
```

不同地址冲突时，若 Server 默认冲突码 `0x03`，只将 Body 中 Response Code 字节（完整报文偏移 12）改为 `03`；其它字段仍由当前编码路径生成。

## TCP 字节流与消息边界

TCP 提供有序字节流，不提供 `send()` 对应的接收消息边界。一次发送可能被多次接收，多次发送也可能合并。因此 Server 使用 `receive_exact`：先累积 8 字节 Header，调用 Header Decoder，再按 `payload_length` 累积 Body。只有拿到完整报文，才交给 `doip_message_decode` 和 Gateway。

Message Codec 的 `doip_message_decode` 处理的是内存里**恰好一条**完整消息：Header 后可用字节少于声明长度时返回 `BUFFER_TOO_SMALL`，多于声明长度时返回 `INVALID_LENGTH`。TCP Server 逐帧读取后传入准确的消息长度，因此不把同一缓冲区中的尾随帧当作第一帧的一部分。

## 面试时需要能够讲清楚的问题

- Payload Length 是否包含 Generic Header？TCP Server 如何用它确定一帧大小？
- `doip_header_t` 为什么不能直接写入网络？
- 如何从上面的十六进制示例中读出 Payload Type、地址和长度？
- Activation Type 和 Response Code 分别由哪些函数解释、哪些只保存？
- 为什么类型常量存在不代表 Vehicle Identification 已实现？
- `doip_message_decode` 与 TCP 分帧的职责有何不同？

## 本章需要掌握的知识点

- Header 固定 8 字节；长度字段只统计 Payload Body。
- Request / Response 的项目基本格式分别为 7 / 9 字节。
- 多字节字段显式大端编码；`0x02` 和 `0xFD` 是项目固定接受值。
- 逻辑地址策略和状态码处理是项目简化，不应扩大解释成标准完整行为。
- TCP 收发必须自行累积/发送完整字节数。
