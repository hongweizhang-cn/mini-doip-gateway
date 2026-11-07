# 03 Protocol Core 实现

## 学习目标

能逐步追踪 Header、Payload Body、完整 Message 的编解码，说明每个 API 的边界检查、返回值、指针所有权及相关测试。

## 统一返回值

`doip_types.h` 定义 `doip_result_t`：

| 返回值 | 含义（结合当前实现） |
|---|---|
| `DOIP_RESULT_OK` | 操作成功。 |
| `DOIP_RESULT_INVALID_ARGUMENT` | 必要指针为空等参数错误。 |
| `DOIP_RESULT_BUFFER_TOO_SMALL` | 输入不足或输出容量不足；具体函数决定语境。 |
| `DOIP_RESULT_INVALID_HEADER` | Header 版本不匹配，或 Request/Response Reserved 字节不符合要求。 |
| `DOIP_RESULT_INVALID_LENGTH` | 长度不符合固定/精确消息长度要求。 |
| `DOIP_RESULT_UNSUPPORTED` | 项目不处理的消息类型、版本扩展形式等；个别 API 具体返回值需查函数。 |

错误枚举不携带 socket 错误详情；网络模块另外在 `doip_tcp_server_t.last_socket_error` 留存部分 Winsock 错误并输出日志。

## Generic Header

### `doip_header_encode`

输入是 `const doip_header_t *header`、目标 `buffer` 和 `buffer_size`；成功写 8 个字节，返回 `DOIP_RESULT_OK`。执行顺序：

1. Header 或 Buffer 为空时返回 `INVALID_ARGUMENT`。
2. 容量小于 8 时返回 `BUFFER_TOO_SMALL`，不写字段。
3. Protocol Version 不是 `DOIP_PROTOCOL_VERSION` 或 inverse 不是 `DOIP_PROTOCOL_VERSION_INVERSE` 时返回 `INVALID_HEADER`。
4. 逐字节写入版本、反版本、16 位 Payload Type、32 位 Payload Length。

Payload Type 与 Length 使用移位、掩码按大端编码；缓冲区若大于 8 字节，只写前 8 字节。没有用 `sizeof(doip_header_t)` 写入，因 C struct 布局不是协议格式。

### `doip_header_decode`

需要至少 8 字节。先校验输入指针与容量，再验证首两字节恰为 `02 FD`。之后将 Header 各字节组装成局部 `decoded_header`，最后一次性赋给输出对象。因此校验失败时不部分覆盖输出。该函数不判断 Payload Type 是否受支持，也不检查后续 Body 是否完整；这由上层完成。

测试：`test_doip_header.c` 覆盖成功编码/解码、往返、NULL、容量不足、版本与反版本不匹配、大端布局及大于 Header 的缓冲区保留尾部。

## Payload Helpers

### `doip_payload_check_length`

比较 `(size_t)payload_length <= available_size`，满足返回 `OK`，否则 `BUFFER_TOO_SMALL`。它不访问数据，也不分配内存。当前核心流程没有调用它；它有 `test_doip_payload.c` 的直接单元覆盖。

### `doip_payload_copy`

1. `payload_length == 0` 时立即返回 `OK`，即使源/目的指针为 NULL。
2. 非零长度时检查源与目的非 NULL。
3. 检查目标容量至少为 `payload_length`。
4. 用 `for` 循环逐字节复制。

这是带显式容量的拷贝 helper。当前代码没有 `memmove` 语义保证；源、目的重叠时不能依赖结果。`doip_message_encode` 用它复制 Payload Body。测试覆盖零长度、NULL、精确/较大容量和不足容量时不写目标等。

## Routing Activation Body Codec

### Request Codec

`doip_routing_activation_request_encode` 检查指针与 7 字节容量后，将 `source_address` 以大端写入 `[0..1]`，`activation_type` 写 `[2]`，将 `[3..6]` Reserved 写零。

`doip_routing_activation_request_decode` 要求输入恰为 7 字节：短输入返回 `BUFFER_TOO_SMALL`，长输入返回 `UNSUPPORTED`；检查 Reserved 四字节为零，否则返回 `INVALID_HEADER`。成功后先填局部 `decoded`，再写入输出结构。它没有验证 Activation Type 值或 Logical Address 范围。

相关测试 `test_doip_routing_activation_request.c` 覆盖编解码、NULL、容量、过长载荷和 Reserved 非零。

### Response Codec

`doip_routing_activation_response_encode` 写入 Client 地址、Entity 地址、Response Code，并将 4 个 Reserved 字节写零。

`doip_routing_activation_response_decode` 的返回顺序为：NULL 检查；不足 9 字节返回 `BUFFER_TOO_SMALL`；长度 13 返回 `UNSUPPORTED`；其他非 9 字节返回 `INVALID_LENGTH`；Reserved 非零返回 `INVALID_HEADER`；最后局部解码并提交到输出。Response Code 原样保留，包括未知值。

相关测试 `test_doip_routing_activation_response.c` 覆盖成功/拒绝/未知码、NULL、长度、Reserved 和输出对象失败时保留。

## 完整 Message Codec

### `doip_message_encode`

输入 Header、Payload 指针及 `payload_size`、目标和目标容量。执行顺序：

1. Header/目标 Buffer 非空。
2. `payload_size` 不超过 `UINT32_MAX`，且必须与 `header->payload_length` 完全相同。
3. 非零 Body 要求 Payload 指针非空。
4. 先检查总缓冲区至少 8 字节，再用 `payload_size <= buffer_size - 8` 检查 Body 容量。先减后比，避免直接无检查地把两项长度相加。
5. 调用 `doip_header_encode` 写 Header。
6. 调用 `doip_payload_copy` 把 Body 写到 `buffer + 8`。

对合法且满足容量的输入，Body 源/目标不得重叠；该函数没有临时复制整个输入 Payload。长度不匹配、容量不足等前置错误时不会开始写 Header。Header 已写后若后续 Payload Copy 失败，通常是同一容量条件已预检，故正常调用中不会出现新的长度失败；调用者仍须遵守内存有效性约定。

### `doip_message_decode`

输入必须至少有 8 字节。函数：

1. 检查指针、Header 最小长度。
2. 调用 `doip_header_decode`；其版本校验失败直接返回。
3. 计算 `available_payload_size = buffer_size - 8`。
4. 小于 Header 声明长度返回 `BUFFER_TOO_SMALL`；大于声明长度返回 `INVALID_LENGTH`。
5. 成功时写 `message->header`，并令 `message->payload = buffer + 8`。

它不会复制 Body、不验证 Payload Type，也不调用具体 Payload Codec。返回的 Payload 指针借用输入缓冲区，不能超过其生命周期；零长度 Body 的指针指向 Header 末尾，不应解引用。

测试 `test_doip_message.c` 覆盖非空/空消息往返、Header 与 body 长度不一致、空间不足、NULL、超 `UINT32_MAX` 的输入构造（在可表达的平台条件下）、短 Header、截断、尾随数据、未知类型和大长度。

## 为什么拆成 Header / Body / Message

- Header Codec 只负责 8 字节 framing metadata；可被不同 Payload 类型复用。
- Request/Response Codec 只理解自身 Body 的固定字段与 Reserved 字节。
- Message Codec 保证 Header 声明长度与实际单条消息一致。
- Gateway 才负责按 Payload Type 路由到业务逻辑。

这种分层避免每个 Payload Codec 重复解析 8 字节 Header，也避免把 C 结构体内存布局误当作线上格式。`doip_message_view_t` 返回借用指针，避免为了分层解析再拷贝一份 Body；Gateway 再将需要长期/结构化使用的 Request 字段解码进小结构体。

## Buffer Overflow 防护与边界

核心代码没有动态分配报文内存。写操作前检查固定容量，Message Codec 用减法比较剩余容量，Server 使用固定数组并先限制 Payload Length 至 4096，之后才读取 Body。Body 编码只写预定长度，Header 编码只写前 8 字节。

这些检查不能替代调用者提供真实且有效的指针/对象大小，也不提供任意缓冲区重叠语义。Transport 有自己更高一层的长度上限；纯 Message Codec 允许 `uint32_t` 范围长度（受调用者实际缓冲区约束），并不硬编码 4096。

## 重要调用链

```text
doip_message_encode
  ├─ doip_header_encode
  └─ doip_payload_copy

doip_message_decode
  └─ doip_header_decode

doip_gateway_process_message
  ├─ doip_message_decode
  │   └─ doip_header_decode
  ├─ doip_routing_activation_request_decode
  ├─ doip_routing_activation_state_process_request
  └─ doip_routing_activation_encode_response_message
      ├─ doip_routing_activation_response_encode
      └─ doip_message_encode
          ├─ doip_header_encode
          └─ doip_payload_copy
```

## 面试时需要能够讲清楚的问题

- 为什么 Header 结构体不能直接 `memcpy` 到网络？
- `doip_message_decode` 是否分配或复制 Payload？为什么其输出结构体不会在失败时被部分写入？
- `payload_length` 与 `payload_size` 为何要同时校验？
- 4096 限制在哪一层？纯 Message Codec 是否也限制为 4096？
- Request Codec 对更长载荷和 Reserved 非零分别返回什么？
- 哪些 API 允许输入输出重叠，哪些需要调用者避免？

## 本章需要掌握的知识点

- 每层各自负责字段、载荷格式、完整消息长度或业务分发，不相互替代。
- Header 与 Body 的多字节字段均显式大端序处理。
- Message View 借用输入内存，不复制；固定缓冲区和显式容量构成主要边界保护。
- Generic Codec 不判断 Payload Type 的业务语义，也不自动应用 Server 的 4096 字节限制。
