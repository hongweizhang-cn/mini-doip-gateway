# 08 错误处理与边界条件

## 学习目标

能把输入异常映射到检测层、返回值、状态副作用及实际测试证据；不把“有错误分支”误认为“已测试通过”。

## 核心 API 边界矩阵

| 情况 | 检测位置与条件 | 返回/动作 | 状态影响 | 测试证据 |
|---|---|---|---|---|
| NULL 必需指针 | 各 Codec/Gateway/State API 参数检查 | 通常 `INVALID_ARGUMENT`；部分输出值先初始化 | Gateway 在参数验证前不改状态；State 对空输出/输入拒绝 | Header、Message、Payload、Request/Response、Routing Activation、Gateway、State 测试均有 NULL/参数用例。 |
| Header 短于 8 字节 | `doip_header_decode` / `doip_message_decode` 容量比较 | `BUFFER_TOO_SMALL` | Gateway 不进入状态处理 | `test_doip_header`、`test_doip_message` 有覆盖。 |
| Protocol Version 错 | `doip_header_decode`: `buffer[0] != 0x02` | `INVALID_HEADER`；Server 关闭当前 client | Gateway 状态未由此请求更新；Server 客户端结束时重置上下文 | Header 单测；Server 集成用例 `unsupported_protocol_version`；Tester fake peer `invalid-header`。 |
| Inverse Version 错 | `doip_header_decode`: `buffer[1] != 0xFD` | `INVALID_HEADER`；Server 关闭当前 client | 同上 | Header 单测；Server 集成 `mismatched_version_inverse`。 |
| 不支持 Payload Type | Gateway 比较是否 `0x0005`；Tester 比较响应是否 `0x0006` | Gateway 返回 `UNSUPPORTED` 并由 Server 关闭连接；Tester 返回 `UNSUPPORTED` | Gateway 在状态处理前拒绝 | `test_doip_routing_activation.c::test_invalid_payload_type`；Tester fake peer `wrong-payload-type`。Server socket 测试未单独发送有效版本但错误 Type 的帧。 |
| Header 声明长度大于实际完整输入 | Message Decode: available `< payload_length` | `BUFFER_TOO_SMALL` | Gateway 不改状态 | `test_doip_message.c::test_decode_truncated_payload_preserves_output`；Routing Activation malformed lengths。 |
| 完整输入有尾随字节 | Message Decode: available `> payload_length` | `INVALID_LENGTH` | Gateway 不改状态 | `test_doip_message.c::test_decode_trailing_data_preserves_output`。 |
| Header Payload Length 大于 Server 本地上限 | Server 先比较 `> 4096` | 记录日志并关闭该 client，不读 Body | 当前连接结束后状态重置 | `test_doip_tcp_server.c` 的 `oversized_payload_length`（4097）。 |
| Request Body 短/长 | Request Codec：小于 7 / 不等于 7 | 短为 `BUFFER_TOO_SMALL`，长为 `UNSUPPORTED` | Gateway 状态调用前失败 | `test_doip_routing_activation_request.c` 边界/OEM 长度用例及 Routing Activation malformed Body。 |
| Response Body 长度错误 | Response Codec：小于 9、恰为 13、其他非 9 | 依次 `BUFFER_TOO_SMALL`、`UNSUPPORTED`、`INVALID_LENGTH` | Codec 不含 Gateway 状态 | `test_doip_routing_activation_response.c::test_decode_length_boundaries_preserves_output`；Tester 测 8 字节错误长度。 |
| Reserved 非零 | Request/Response Body Decoder 检查 Reserved 字节 | `INVALID_HEADER` | Gateway Body 校验失败，状态不更新 | Request `test_decode_reserved_nonzero`；Response `test_decode_reserved_nonzero_preserves_output`。 |
| 输出容量不足 | Message Codec 或 Gateway 固定 17 字节门槛 | `BUFFER_TOO_SMALL` | Gateway 特意先于 State 检查，不激活 | Message/RA/Gateway 中的 capacity tests，特别是 `test_capacity_check_precedes_state_change`。 |
| 输出长度指针 | Gateway/RA: `output_size` 必须非空 | `INVALID_ARGUMENT`；若非空先清零 | Gateway 不进入状态处理 | 对应 Gateway/RA 参数测试。 |
| 逻辑状态枚举非法 | State Process/Get 校验只允许 INACTIVE/ACTIVE | `INVALID_ARGUMENT` | 不改上下文；Response Code 清零 | `test_invalid_context_state_is_rejected`。 |
| 不同逻辑地址请求激活 | State 已 ACTIVE 且地址不同 | State API 返回 `OK`，写配置的冲突码；仍可生成 Response | 当前地址/ACTIVE 状态保持 | State 的 `test_different_client_rejected_without_overwrite`；Gateway 的 repeat/conflict 测试。 |
| 未知 Response Code | Response Codec 不做枚举检查；Tester 只接受 `0x10` | Body 解码成功；Tester 对非 `0x10` 返回 `UNSUPPORTED` | 不涉及 Server 状态判定 | Response Codec `test_decode_unknown_response_code`；Tester 测有效拒绝码。 |

## TCP / Winsock 边界矩阵

| 情况 | 检测位置与条件 | 当前处理 | 状态与测试 |
|---|---|---|---|
| TCP Partial Read | Server/Tester `receive_exact` 在循环中累计正数 `recv` 返回值，直至目标字节数完成 | `WSAEWOULDBLOCK`、`WSAEINTR` 重试；0 表示断开；其他错误失败 | Server `send_fragmented_request` 路径测试分段到达；Tester 通过 exact-read/fake peer 覆盖分阶段数据。 |
| TCP Partial Write | Server/Tester `send_all` 累计 `send` 返回的正数字节数 | 可重试错误重试；0/其他错误结束当前操作 | `send_all` 的循环存在；没有独立强制操作系统每次只写部分字节的确定性测试。标记为部分覆盖，不推断 partial-write 场景必然触发。 |
| 对端提前断开 | `recv` 返回 0 | Server 结束当前客户端处理；Tester 返回 `INVALID_LENGTH` 并说明响应不完整 | Server 测试发送短 Header/Body 后关闭；Tester fake peer `partial-header` / `partial-payload`。 |
| Header/Body 只发一部分且连接保持 | Server `receive_exact` 等待剩余字节；每 100ms 检查停止标志 | Server 没有帧级/连接级总读取 deadline；可能持续等待，直到数据、断开、错误或 stop | 有短片段后关闭测试，不等价于“连接保持超时”；该保持型超时场景**未测试**。 |
| Tester 连接超时 | `connect_with_timeout` 对 write set `select` 最多 3000ms | 连接失败/超时映射 `UNSUPPORTED` | 有无监听端口连接失败测试；没有稳定测试严格耗尽完整 3 秒超时。 |
| Tester 发送超时 | `send_all` 的单个总阶段计时器 | 5 秒 deadline 后返回 `UNSUPPORTED` | 未发现单独确定触发发送阻塞超时的测试。 |
| Tester 接收超时 | 每次 `receive_exact` 使用 5000ms deadline | 超时返回 `UNSUPPORTED` | `test_slow_response_has_total_timeout` 通过逐字节慢发 Header 检查 bounded duration。 |
| Server 监听器/Accept/Send/Recv 错误 | Winsock 返回值和 `last_socket_error` | transient would-block/interrupted retry；其他错误结束当前 client 或 Server | 测试涵盖部分初始化、stop 与断开场景；没有为所有错误码逐一注入测试。 |
| 服务端启动地址错误 | `InetPtonA` 返回值 | `INVALID_ARGUMENT`，释放 Winsock/socket 资源 | Server lifecycle test 中 invalid bind address。 |
| 无效命令行数字 | main 中 `strtoul` 后检查尾字符和范围 | 打印用法/错误，返回 `EXIT_FAILURE` | 这些 main 级错误参数没有单独 CTest 用例。 |

## 状态副作用和输出保证

- Header/Body/Message Codec 在检测失败时先用局部变量完成解析，再写输出结构体；测试中有多处“失败后保持输出不变”验证。
- Gateway 将 `*output_size` 置零；Message/Body 失败、类型不支持及容量不足均在状态处理前发生。
- Gateway 的输出容量先检查，测试明确验证容量不足不激活状态。
- 成功执行 State Process 后，Gateway 使用已解码 Request、固定响应长度和预留容量编码响应。API 文档描述此固定路径使编码所需前置条件成立。
- Server 一旦当前 client 处理结束（含协议错误/断开）会重置 Routing Activation；这是连接收尾策略，因此错误连接也会清除同一 Server 实例中此前活动的上下文。

## 还需谨慎理解的不足

1. Server 没有读取超时，可能长期占用唯一服务流程等待半帧。
2. 多种 Winsock 错误统一映射 `DOIP_RESULT_UNSUPPORTED`，调用者需结合日志和 `last_socket_error` 区分。
3. Server 错误时关闭连接，而不是按协议针对每种无效请求生成具体错误 Response。
4. Tester 将非 `0x10` 都当失败，不解释完整响应码语义。
5. 现有测试覆盖的是明确列出的输入和 fake-peer 场景；未覆盖的错误码、竞态、压力或 fuzz 行为不能推断安全或通过。

## 面试时需要能够讲清楚的问题

- Length 错误如何区分“不足”和“尾随数据”？
- Server 检查 4096 的顺序为何重要？
- Partial Read 和连接关闭分别由哪些返回值识别？
- 当前 Server 的 100ms 轮询是不是读取超时？半帧客户端会怎样？
- Gateway 输出容量不足时能否污染状态？
- 哪些网络异常确实有集成测试，哪些只是代码分支而未测试？

## 本章需要掌握的知识点

- `doip_result_t` 是粗粒度 API 错误分类；Winsock 错误另有记录。
- 各层长度检查含义不同：8 字节 Header、Body 固定长度、完整消息精确长度、Server 本地上限。
- Server Partial Read 可循环但无应用层 deadline；Tester 有分阶段 timeout。
- 需明确区分代码处理能力、测试用例存在与测试已通过。
