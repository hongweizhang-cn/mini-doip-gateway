# 12 技术面试问答

## 学习目标

用源码而不是泛化教材回答项目问题；每题先给核心答案，再准备可验证的代码位置和追问思路。回答中的限制是项目事实，不应为了面试效果夸大。

## 1. 项目整体架构

### Q1（基础）你用几句话介绍这个项目？

- **建议回答：** 这是一个 C99 DoIP Routing Activation 教学项目。Windows Tester 通过 TCP 发基本 Request；单客户端 Winsock Server 负责 TCP 分帧并把完整消息交给 Gateway；Gateway 按一份简化状态生成 Response。它不连接 ECU、不转发诊断消息，也不是完整 DoIP 协议栈。
- **源码：** `c-core/src/doip_tester.c::doip_tester_run`、`doip_tcp_server.c::process_client_messages`、`doip_gateway.c::doip_gateway_process_message`。
- **追问：** “这项目实际解决什么问题？”
- **回答思路：** 展示 Header/Body 序列化、TCP framing 和简单状态交互；不声称解决真实车辆诊断路由。

### Q2（实现细节）Protocol Core 和 Transport 如何分工？

- **建议回答：** Server 将完整 DoIP 消息传给 Gateway；Gateway 不知道 socket。Transport 管 socket、连接和字节流，Core 管 Header/Body/状态及 Response 字节。
- **源码：** `doip_tcp_server.c::process_client_messages` 调用 `doip_gateway_process_message`；`doip_message.c` 与 `doip_gateway.c` 不依赖 Winsock。
- **追问：** Gateway 为什么不接收 `SOCKET`？
- **回答思路：** 避免协议业务绑定平台 I/O，便于直接对完整消息做单测；代价是 Transport 必须先 framing。

### Q3（设计）这个 Gateway 是真实 ECU 诊断 Gateway 吗？

- **建议回答：** 不是。`doip_gateway_t` 保存一个实体逻辑地址、冲突码和单一 Routing Activation 上下文；仓库没有 ECU adapter、诊断消息处理或转发模块。
- **源码：** `doip_gateway.h::doip_gateway_t`、`doip_gateway.c`。
- **追问：** 真实转发要加什么？
- **回答思路：** Diagnostic Message Codec/dispatch、目标路由与 ECU Transport Adapter、请求响应关联、超时/错误传播及相应测试。

### Q4（扩展）支持并发客户端会首先遇到哪些结构问题？

- **建议回答：** 当前 Server 同步处理一个客户端；Gateway 只有一个地址状态。需明确状态按连接还是按逻辑路由关联，增加连接对象、生命周期隔离、并发控制及资源上限。
- **源码：** `doip_tcp_server.c::doip_tcp_server_run`、`doip_tcp_server.h::doip_tcp_server_t`、`doip_routing_activation_state.h::doip_routing_activation_context_t`。
- **追问：** 只把 accept 放到多个线程是否够？
- **回答思路：** 不够；共享的单一 Gateway state 会竞争/串扰，连接断开时 reset 也可能清错状态，需重构所有权。

## 2. DoIP 协议

### Q5（基础）Generic Header 的长度和字段是什么？

- **建议回答：** 8 字节：版本 1、反版本 1、Payload Type 2、Payload Length 4。后续长度不包含 Header。
- **源码：** `doip_header.h::DOIP_GENERIC_HEADER_SIZE`、`doip_header.c::doip_header_encode/decode`。
- **追问：** `payload_length = 7` 的完整 Request 一共多少字节？
- **回答思路：** 8+7=15 字节。

### Q6（实现细节）为什么当前代码只接受 `02 FD`？

- **建议回答：** `DOIP_PROTOCOL_VERSION` 和 `DOIP_PROTOCOL_VERSION_INVERSE` 常量固定为 `0x02`、`0xFD`，Header Encode/Decode 都做精确比较。它是当前支持范围，不是版本协商器。
- **源码：** `doip_header.h`、`doip_header.c::doip_header_decode`。
- **追问：** 反版本错会返回什么？
- **回答思路：** `DOIP_RESULT_INVALID_HEADER`；Header 单测和 Server 集成测试都覆盖了错误组合。

### Q7（实现细节）Request Body 的 7 字节是什么？

- **建议回答：** Source Address（Tester Logical Address）2 字节大端、Activation Type 1 字节、Reserved 4 字节。Request Codec 写 Reserved 为 0，解码要求全 0。
- **源码：** `doip_routing_activation_request.c::doip_routing_activation_request_encode/decode`。
- **追问：** Activation Type 目前如何影响状态？
- **回答思路：** 它被解码并保存在 Request 结构中，但 `doip_routing_activation_state_process_request` 未读取它。

### Q8（设计/边界）定义了 Vehicle Identification 常量，为什么仍说未实现？

- **建议回答：** `doip_payload_type.h` 只定义了数值，Gateway 只接受 Routing Activation Request，且没有 UDP socket、识别请求 Body Codec 或 Announcement 处理。
- **源码：** `doip_payload_type.h`、`doip_gateway.c::doip_gateway_process_message`、`c-core/CMakeLists.txt`。
- **追问：** 新增常量后最少还需要什么？
- **回答思路：** 定义并验证 Body 格式、加入分发路径、Transport 行为、响应/错误处理及测试。

## 3. C99 编码与内存管理

### Q9（基础）为什么不把 `doip_header_t` 直接发到 TCP？

- **建议回答：** C struct 可能有 padding、对齐和主机端序差异；线上格式固定为 8 字节大端，所以 Codec 逐字段写读。
- **源码：** `doip_header.h` 注释、`doip_header.c`。
- **追问：** 即使某台机器测试 `sizeof(struct)==8` 能否直接复制？
- **回答思路：** 不能据此保证 ABI、编译器及端序稳定；协议布局必须独立表达。

### Q10（实现细节）显式 Buffer Capacity 有什么好处和限制？

- **建议回答：** Encoder 可在写入前比较实际声明容量，返回 `BUFFER_TOO_SMALL`；但它无法验证调用者传的容量与真实对象大小一致，也无法验证指针有效性。
- **源码：** Header/Message/Body Encode API。
- **追问：** 4096 限制是否由所有 Codec 强制？
- **回答思路：** 不是；Server 与 Tester 有本地限制，Message Codec 按调用者容量和 `uint32_t` 长度检查。

### Q11（实现细节）`doip_message_view_t` 是拥有型还是借用型？

- **建议回答：** 借用型。`payload` 指向原输入 Buffer 的 Header 后方，不分配也不复制；输入 Buffer 释放/重用后 view 失效。
- **源码：** `doip_message.h::doip_message_view_t`、`doip_message.c::doip_message_decode`。
- **追问：** 为什么当前 Gateway 这样用安全？
- **回答思路：** 同步调用中输入数组仍在栈上，Request Body 解码成独立结构后才输出；若异步排队则需复制或转移所有权。

### Q12（异常）容量失败时是否会部分写输出？

- **建议回答：** Message Encode 在调用 Header Encode 前完成基础长度与总容量检查；Body Codec 也在写之前检查容量。Gateway 对 17 字节响应容量在状态更新前检查。各测试有 buffer 保持原样/状态未变的场景。
- **源码：** `doip_message.c::doip_message_encode`、`doip_gateway.c::doip_gateway_process_message`、`test_doip_gateway.c::test_capacity_check_precedes_state_change`。
- **追问：** 所有 API 都有相同的强保证吗？
- **回答思路：** 不能泛化；具体函数检查顺序不同，且所有权/缓冲区别名需遵循头文件约定。

## 4. Header / Payload 编解码

### Q13（实现细节）Header 如何把 32 位长度编码成 4 字节？

- **建议回答：** 依次右移 24、16、8、0 位并掩码 `0xFF`，写入索引 4 到 7；Decode 反向移位后 OR 组合。
- **源码：** `doip_header.c::doip_header_encode/decode`。
- **追问：** `0x00000007` 在线上是什么？
- **回答思路：** `00 00 00 07`。

### Q14（实现细节）Header Decoder 会拒绝未知 Payload Type 吗？

- **建议回答：** 不会。Header Decoder 仅检查固定版本并解析 Type/Length；Message Decoder 也不判 Type。Gateway 或 Tester 的上层按期待类型分发。
- **源码：** `doip_header.c::doip_header_decode`、`doip_message.c::doip_message_decode`、`doip_gateway.c`。
- **追问：** 这种分工有什么意义？
- **回答思路：** Header 语法解码与 Payload 语义处理分层；Message Codec 可保留任意 16 位 Type。

### Q15（边界）`doip_message_decode` 如何区分截断与尾随字节？

- **建议回答：** Header 后可用长度小于声明 Payload Length 返回 `BUFFER_TOO_SMALL`；大于则返回 `INVALID_LENGTH`；相等才成功。
- **源码：** `doip_message.c::doip_message_decode`。
- **追问：** 为什么它不直接接受多帧拼接的 TCP Buffer？
- **回答思路：** API 契约是解析恰好一条消息；TCP 层先按 Header Length 切分帧。

### Q16（边界）Response Decoder 如何处理 13 字节？

- **建议回答：** 先要求至少 9 字节，然后长度为 13 特判 `UNSUPPORTED`，其他不等于 9 的长度返回 `INVALID_LENGTH`。它不解析额外 4 字节。
- **源码：** `doip_routing_activation_response.c::doip_routing_activation_response_decode`。
- **追问：** Response Code 未知会被 Codec 拒绝吗？
- **回答思路：** 不会；Response Code 原样传回，Tester 之后只把 `0x10` 视为成功。

## 5. Routing Activation

### Q17（基础）项目中第一次请求如何激活？

- **建议回答：** State 为 INACTIVE 时保存 Request 的 `source_address`，设为 ACTIVE，输出成功码 `0x10`。
- **源码：** `doip_routing_activation_state.c::doip_routing_activation_state_process_request`。
- **追问：** 激活前做了哪些 Request 检查？
- **回答思路：** Gateway 已完成 Message/Type/7-byte Body/Reserved 检查；但 State 不验证 Activation Type 或地址策略。

### Q18（实现细节）同地址和不同地址重试有何差异？

- **建议回答：** 活动时相同地址幂等返回 `0x10`；不同地址返回配置冲突码，保留原活动地址。
- **源码：** `doip_routing_activation_state.c`，测试 `test_same_client_reactivation_is_idempotent`、`test_different_client_rejected_without_overwrite`。
- **追问：** `0x03` 从哪里来？
- **回答思路：** Server main 的默认配置传入 Gateway；State API 接收参数，不自行认定该值为普适标准策略。

### Q19（异常）输出 Buffer 不足会不会先激活再失败？

- **建议回答：** 不会。Gateway 在 State Process 前检查 `output_message_capacity >= 17`，失败直接返回 `BUFFER_TOO_SMALL`。
- **源码：** `doip_gateway.c::doip_gateway_process_message`。
- **追问：** 为什么容量门槛固定 17？
- **回答思路：** 当前 Response 是 8 字节 Header + 9 字节基本 Body，Gateway 用固定宏计算。

### Q20（概念）TCP 连接和 Routing Activation 是什么关系？

- **建议回答：** TCP Connect 仅建立传输连接；只有完整 Request 到达并由 Gateway 状态逻辑处理后才得到 Routing Activation Response。TCP 成功不代表 DoIP 激活成功。
- **源码：** `doip_tcp_server.c::doip_tcp_server_run`、`doip_gateway.c`、`doip_tester.c::doip_tester_run`。
- **追问：** Server 断开时如何处理状态？
- **回答思路：** 每个客户端处理结束后调用 `doip_gateway_reset_routing_activation`，清空单一逻辑上下文。

## 6. TCP / Winsock

### Q21（实现细节）Server 启动的系统调用顺序是什么？

- **建议回答：** `WSAStartup`、numeric IPv4 解析、`socket`、`SO_EXCLUSIVEADDRUSE`、`bind`、`listen`、nonblocking `ioctlsocket`、`getsockname`。
- **源码：** `doip_tcp_server.c::doip_tcp_server_start`。
- **追问：** `port = 0` 有何意义？
- **回答思路：** OS 分配端口，`getsockname` 把结果写入 `bound_port`，测试常用动态端口减少冲突。

### Q22（边界）Server 为什么限制 4096？

- **建议回答：** 输入与输出使用静态数组 `8 + 4096`，所以先拒绝声明长度超过 4096，避免读写越界；这是项目本地资源上限，不是通用标准最大载荷声明。
- **源码：** `doip_tcp_server.h::DOIP_TCP_SERVER_MAX_PAYLOAD_SIZE`、`doip_tcp_server.c::process_client_messages`。
- **追问：** 若用户把宏调大就足够支持任意消息了吗？
- **回答思路：** 还需考察栈空间、Tester/Codec 边界、资源策略、标准格式及相关测试。

### Q23（设计）为什么服务端默认绑定 `127.0.0.1`？

- **建议回答：** main 中的明确默认值让演示只接收本机回环连接。LAN 测试需传服务端 LAN IP 或 `0.0.0.0`，Tester 则连接可达的具体服务器 IP。
- **源码：** `doip_tcp_server_main.c::DEFAULT_BIND_ADDRESS`、`doip_tester_main.c` 默认目标。
- **追问：** Tester 应连接 `0.0.0.0` 吗？
- **回答思路：** 不应；它是服务器绑定通配地址，客户端连接服务器真实可达的接口地址。

### Q24（资源管理）谁负责 Winsock 与 socket 清理？

- **建议回答：** Server `release_server_resources` 关闭 client/listen socket 并匹配 `WSACleanup`；Tester 的 cleanup label 关闭其 socket 并 `WSACleanup`。调用者不应重复释放 Server 所有的句柄。
- **源码：** `doip_tcp_server.c::release_server_resources`、`doip_tester.c::doip_tester_run`。
- **追问：** Ctrl+C 如何通知正在阻塞的 Server？
- **回答思路：** Console Handler 调 `doip_tcp_server_request_stop` 设置标志；100ms `select` 轮询使运行循环观察到它，Run 负责清理。

## 7. Partial Read / Partial Write

### Q25（基础）为什么一次 `recv()` 不一定拿到完整 Header？

- **建议回答：** TCP 是字节流，分段和应用层调用边界无对应关系；即使发送 8 字节 Header，也可能多次到达。
- **源码：** `doip_tcp_server.c::receive_exact`、`doip_tester.c::receive_exact`。
- **追问：** 什么时候 Server 能开始解析 Payload Length？
- **回答思路：** 8 字节 Header 全部累积完成后。

### Q26（实现细节）`receive_exact` 怎么处理部分读取？

- **建议回答：** `received_total` 初值 0，每次把剩余区间传给 `recv`，正数累加，直到达到 `byte_count`；非阻塞暂不可读或中断则重试。
- **源码：** 两处 `receive_exact`，Server 另用 `wait_for_socket`。
- **追问：** `recv` 返回 0 怎么办？
- **回答思路：** 对端关闭，无法完成消息；Server 退出当前连接，Tester 报响应长度不完整。

### Q27（实现细节）`send_all` 解决什么问题？

- **建议回答：** `send` 返回的正数可能小于待发送长度；循环按已发送偏移更新指针，只到总长度完成后才成功。
- **源码：** `doip_tcp_server.c::send_all`、`doip_tester.c::send_all`。
- **追问：** 测试是否强制每次 `send` 都只写一部分？
- **回答思路：** 可见网络测试强调分片请求/响应和完整读取，但不能据此说 OS 一定触发过 partial write；部分写分支由代码循环处理。

### Q28（异常）Server 的 100ms wait 是不是数据超时？

- **建议回答：** 不是。它是 `select` 的轮询间隔，用于检查 stop 标志；没有累计帧读取 deadline。半帧连接保持打开可能长期占住唯一服务循环。
- **源码：** `DOIP_TCP_SERVER_POLL_INTERVAL_MS`、`wait_for_socket`、`receive_exact`。
- **追问：** 如何改进？
- **回答思路：** 增加明确的 per-frame/per-connection deadline，并确保停止/断开/部分数据状态可测试。

## 8. 异常处理

### Q29（实现细节）Header 版本失败如何向上冒泡？

- **建议回答：** Server 收满 8 字节 Generic Header 后，先直接调用 `doip_header_decode`，此时尚未读取 Body，也尚未调用 Gateway。版本或反版本不匹配时返回 `INVALID_HEADER`；Server 记录拒绝并结束当前 client 处理，随后关闭 client socket 并 reset Routing Activation state。
- **源码：** `doip_tcp_server.c::process_client_messages`、`doip_header.c::doip_header_decode`、`doip_tcp_server.c::doip_tcp_server_run`。
- **追问：** Server 是否返回一个协议错误 Response？
- **回答思路：** 不会。无效 Header 在 Server 层被拒绝，当前实现关闭连接，不生成协议错误 Response。

### Q30（边界）Payload Length 截断与超限如何区分？

- **建议回答：** Server 读 Header 后先以 4096 本地上限检查；未超限再 exact-read Body。若对端关闭导致读不足，是连接中断；纯 Message Decoder 收到长度不够的完整输入时返回 `BUFFER_TOO_SMALL`。
- **源码：** `doip_tcp_server.c::process_client_messages`、`doip_message.c::doip_message_decode`。
- **追问：** 若输入包含多余尾随数据？
- **回答思路：** Message Decoder 返回 `INVALID_LENGTH`，它只接受恰好一条完整消息。

### Q31（状态副作用）非法消息能否修改 Gateway 状态？

- **建议回答：** Gateway 先解码 Header/Message、检查 Type/Body、检查输出容量，之后才调用 State Process；这些前置失败不改变状态。客户端处理结束时 Server 会 reset。
- **源码：** `doip_gateway.c::doip_gateway_process_message`。
- **追问：** 哪个测试明确验证容量失败行为？
- **回答思路：** `test_doip_gateway.c::test_capacity_check_precedes_state_change`。

### Q32（错误模型）为什么 Winsock 错误常映射为 `UNSUPPORTED`？

- **建议回答：** 当前 `doip_result_t` 是粗粒度跨模块枚举；网络模块另通过日志和 `last_socket_error` 保留部分具体 Winsock 信息。这种映射诊断粒度有限。
- **源码：** `doip_types.h`、`doip_tcp_server.c`、`doip_tester.c`。
- **追问：** 如何改进错误可观测性？
- **回答思路：** 为错误类型和阶段提供结构化上下文/平台码，同时保持 API 可测试，避免把所有 I/O 错误压成同一值。

## 9. 单元测试与集成测试

### Q33（基础）项目测试分几类？

- **建议回答：** 核心 Codec/状态/Gateway 单元测试；Windows 下额外的 TCP Server 与 Tester 网络测试。CMake 用 `WIN32` 条件注册网络测试。
- **源码：** `c-core/tests/CMakeLists.txt`。
- **追问：** 非 Windows 能否运行 Server 集成测试？
- **回答思路：** 当前构建配置不创建 Winsock Server/Tester target 和测试。

### Q34（实现细节）Tester 集成测试如何构造异常响应？

- **建议回答：** `test_doip_tester.c` 创建 fake TCP peer，按模式返回无效 Header、超限/错误长度、分片或慢 Header、错误类型/地址/Response Code。
- **源码：** `fake_peer_mode_t`、`run_fake_peer`、`test_fake_response`。
- **追问：** 超时测试证明哪件事？
- **回答思路：** 慢响应场景在有界时间内失败；它不证明所有网络延迟/错误都已覆盖。

### Q35（测试质量）测试源文件存在能否说测试通过？

- **建议回答：** 不能。源码只能证明用例被编写，CMake 配置证明注册规则；通过需要在目标构建下运行 CTest 并看退出结果。本次文档任务没有运行测试。
- **源码：** `c-core/tests/CMakeLists.txt` 与各测试 `main`。
- **追问：** 如何记录平台差异？
- **回答思路：** 报告 Generator、编译器、平台、注册测试清单和实际命令/结果。

### Q36（异常场景）Server 网络测试实际覆盖哪些 framing 问题？

- **建议回答：** 有分段 Request、短 Header/Body 后断开、非法版本/反版本、超 4096 长度及恢复接受后续客户端；未覆盖保持半帧连接的 deadline，因为 Server 没有该超时。
- **源码：** `test_doip_tcp_server.c::test_server_lifecycle_and_stream_handling`。
- **追问：** 如何加一个连接保持的测试？
- **回答思路：** fake client 只发部分 Header/Body 后保持 socket，测预期 deadline 与释放/重新 accept 行为；当前实现需先加入读取超时才可验证其到期路径。

## 10. 设计取舍与未来扩展

### Q37（设计）当前固定缓冲区有什么收益和代价？

- **建议回答：** 限制分配、提供可见边界，便于容量检查；但固定 4096 可能拒绝更长数据，数组占栈空间，也不是标准最大值。
- **源码：** `doip_tcp_server.h` 与 Server/Tester 缓冲区定义。
- **追问：** 如果改动态缓冲区需要增加哪些检查？
- **回答思路：** 整数溢出、分配失败、最大资源策略、释放路径和异常测试。

### Q38（扩展）Diagnostic Message 转发如何放进架构？

- **建议回答：** Gateway 增加 Payload Type dispatch 与诊断处理；新增诊断 Payload Codec 和 ECU Transport Adapter；Server 仍负责 framing，新增端到端关联和错误行为测试。
- **源码：** 当前 `doip_gateway.c` 只分发 Request；`doip_payload_type.h` 未定义 Diagnostic Type；无 ECU 模块。
- **追问：** 为什么不能在 TCP Server 直接解析诊断内容？
- **回答思路：** 会把 transport framing 与 payload 业务耦合；应继续保持完整消息交给 Protocol Core。

### Q39（扩展）要支持 Linux，是否只把 Winsock 函数名换成 POSIX？

- **建议回答：** 不够。构建条件、错误码、nonblocking connect/select、socket 关闭、控制台停止以及测试都依赖 Windows API；需要平台适配和跨平台测试。
- **源码：** `c-core/CMakeLists.txt` 的 `WIN32` 条件、Server/Tester 对 Winsock/Windows API 的使用。
- **追问：** 哪些代码可直接保留？
- **回答思路：** Header/Message/Body Codec、Gateway/State 等不依赖网络的核心可继续复用。

### Q40（设计）如何说明项目“不完整”而不贬低项目价值？

- **建议回答：** 把目标限定为教育性 Routing Activation 路径，展示字节序、TCP framing、C 缓冲区边界、状态逻辑和测试；明确尚未支持 UDP Discovery、诊断转发、多客户端和完整状态机。
- **源码：** README 的 Scope and Limitations 与当前模块清单。
- **追问：** 面试官问“是否符合 ISO 13400”怎么答？
- **回答思路：** 不能声称完整符合或认证；只描述实现并经过哪些具体测试，标准一致性需独立规范追踪和验证。

## 面试时需要能够讲清楚的问题

- 至少选一个问题，现场沿真实函数调用链解释，而不是背模块名称。
- 能否用十六进制展示请求和响应并指出长度不含 Header？
- 能否区分 API 处理分支与已有测试证据？
- 被问到未实现的标准功能时，是否能清楚解释扩展模块和当前缺口？

## 本章需要掌握的知识点

- 以上 40 题覆盖架构、协议、C99、Codec、状态、Winsock、TCP 部分收发、异常、测试及扩展。
- 回答采用“代码事实 → 边界/原因 → 代价/追问”的结构。
- 不把预期、历史构建物或未来改进描述成当前测试结果与已交付功能。
