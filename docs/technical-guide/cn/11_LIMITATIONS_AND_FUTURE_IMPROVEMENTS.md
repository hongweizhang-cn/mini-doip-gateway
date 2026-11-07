# 11 当前限制与未来改进方向

## 学习目标

能清晰区分当前功能、源码证据和可能的扩展工作；面试时不会把计划中的能力误报为已完成。

## 限制清单

| 主题 | 当前状态与代码证据 | 未来需要增加 | 对架构的影响 | 主要难点 |
|---|---|---|---|---|
| UDP Vehicle Identification | **未实现处理流程。** `doip_payload_type.h` 只有 `0x0001..0x0003` 等常量；CMake 没有 UDP 模块。 | UDP socket Transport、Vehicle Identification 请求解析、Entity/车辆信息响应、配置和测试。 | 新增 UDP Transport 与识别服务，不应塞进 TCP Server 的帧处理循环。 | UDP 广播/单播语义、超时/接口选择、不同请求类型和车辆配置。 |
| Vehicle Announcement | **未实现。** 有 `0x0004` 常量，无其 Body 结构体或 Codec。 | Announcement Body Codec、触发/发送策略及 UDP 集成。 | 与车辆识别状态/UDP 服务协作。 | 消息字段完整性、周期/触发行为及标准版本依据。 |
| Alive Check | **未实现。** 没有相应 Payload Type、状态或定时器处理。 | Alive Check Request/Response、连接状态及周期管理、测试。 | Server 连接生命周期和状态机需加入计时与检查。 | 并发连接下的时序、无响应动作和资源清理。 |
| Diagnostic Message | **未实现。** `doip_payload_type.h` 未定义 Diagnostic Message 类型；Gateway 只接受 `0x0005`。 | Diagnostic payload Codec、长度/地址检查、正/负确认或错误行为。 | Gateway 增加 payload dispatch 与诊断处理接口。 | 标准确认流程、诊断地址语义、分段/错误处理和测试面覆盖。 |
| ECU 转发 | **未实现。** 无 ECU/下游 socket、路由表或诊断转发源文件。 | ECU Transport Adapter、目标寻址/路由、请求/响应关联。 | Gateway 从本地响应生成扩展成上游/下游双向代理。 | ECU 网络、时序关联、失败传播、超时以及安全隔离。 |
| 多客户端 | **未实现。** `doip_tcp_server_run` 服务一个 client 到结束；State 仅一个地址。 | 多连接管理器、每连接或按标准定义的会话状态、并发安全和资源上限。 | Transport、Gateway 状态作用域及所有权均需重构。 | 同时请求竞争、状态隔离、慢客户端资源消耗和停止竞态。 |
| 完整 Routing Activation 策略 | **未实现。** `doip_routing_activation_state_process_request` 只区分 INACTIVE/ACTIVE 与地址相同/不同；Activation Type 未验证。 | 完整状态/响应码策略、认证或授权扩展点、配置/策略测试。 | State 变为更丰富的策略状态机，并影响 Gateway Response 映射。 | 必须基于采用的标准版本与车辆/OEM要求确认规则，避免仅凭示例码扩展。 |
| 安全机制 | **未实现/无法从源码确认安全承诺。** 连接为普通 Winsock TCP，无 TLS、客户端认证、加密、访问控制、审计或速率限制实现。 | 根据部署威胁模型增加传输保护、身份/访问策略、日志和资源限流。 | 可能增加安全层、证书/密钥配置、会话身份与错误处理。 | 车载网络信任边界、密钥管理、性能与兼容性；需安全评估而非只加一个开关。 |
| 超时与资源管理 | **部分实现。** Tester 有 connect/send/read deadline；Server `select` 100ms 轮询只检查 stop，无帧读取 deadline。Server 固定栈缓冲与单连接减少动态资源。 | 半帧 deadline、空闲连接超时、连接限额、可观测资源状态和更全面的错误回收测试。 | Transport 生命周期和配置增加；可能引入 per-connection timer。 | 超时定义、部分帧取消、线程/关闭竞态与错误后状态恢复。 |
| 跨平台 | **核心部分可在非 Windows 配置，网络模块不跨平台。** `WIN32` 才编译 Winsock Server/Tester。 | POSIX Transport、统一接口和平台 CI。 | 把 socket 生命周期/错误码封装在平台适配层。 | `select`、nonblocking、错误码、地址接口和关闭语义存在平台差异。 |
| ISO 13400 完整性/一致性 | **不是完整实现，未声称认证。** 仅当前代码列出的 Generic Header 与基本 Routing Activation 路径；README 明确列出大量未实现功能。 | 按明确标准版本逐项做规范追踪、完整消息集/状态行为、外部一致性测试。 | 范围将远超当前模块，需协议矩阵、测试框架和版本管理。 | 标准完整性、边界组合和一致性证据；当前仓库没有 conformance 报告。 |
| OEM 扩展 | **未支持。** Request Decoder 只收 7 字节；Response Decoder 对 13 字节形式返回 `UNSUPPORTED`；Codec 固定写 Reserved 为 0。 | 可明确版本化的扩展字段结构、长度规则和配置接口。 | Codec/API 需表达可选字段，缓冲区长度由固定常数转为可变。 | 不能把未知扩展误当 Reserved；互操作性和兼容策略。 |
| 可观测性/错误细分 | **有限。** `doip_result_t` 粗粒度；Server 有 printf 和 `last_socket_error`，Tester 有 stdout/stderr。 | 结构化日志、错误上下文、统计及测试报告。 | 需要统一日志/错误接口，在模块间传递上下文。 | 避免泄露敏感信息，保持跨平台一致并使错误可诊断。 |

## 建议的扩展次序

如果后续目标仍是学习项目，而不是宣称生产级支持，可先把行为范围定义清楚，再依次完善协议字段/负例测试、Server 半帧超时和错误分类、多连接状态隔离、UDP Discovery、Diagnostic Message 编解码、ECU Adapter 与跨平台 Transport。每一步都应增添针对新行为的测试和文档；不要仅凭新增 Payload Type 常量宣称功能完成。

## 当前能力的边界声明

- 代码中的 Header 版本常量、Payload Type 常量和 Response Code 常量不构成完整标准实现。
- 本次没有查验外部标准文本，也没有执行官方一致性验证；标准条款、版本差异与认证状态无法从仓库代码确认。
- 本指南中未来模块是改进方向，不表示仓库已有相关接口、行为或计划承诺。

## 面试时需要能够讲清楚的问题

- Diagnostic Message 转发缺少哪些关键模块？仅添加 Payload Type 是否足够？
- 多客户端支持会影响哪些现存结构体和调用边界？
- 当前服务器是否有读取超时？Tester 的 deadline 能否保护 Server 端资源？
- 为什么“有 Vehicle Identification 常量”不代表实现了 Discovery？
- 如何避免把一个教学状态策略包装为完整 Routing Activation 规范？
- 如果扩展到 ECU 转发，Gateway 和 Transport 应如何分工？

## 本章需要掌握的知识点

- 每项限制都能指出明确代码证据或明确说明无法确认的范围。
- 诊断转发、多客户端、安全机制、UDP 与完整一致性都不是现有功能。
- 扩展需要相应的架构、状态、错误策略和测试，而非只加常量。
- 不对当前项目宣称认证、生产可用或完整 ISO 13400 支持。
