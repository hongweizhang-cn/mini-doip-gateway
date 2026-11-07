# 05 Windows TCP Server 实现

## 学习目标

能从 `main` 跟踪到 Winsock socket 的创建、监听、接受客户端、TCP 分帧、调用 Gateway、发送响应及清理；理解 Server 的超时与资源生命周期边界。

## 平台与入口

Server 仅在 Windows 构建。`doip_tcp_server_main.c` 默认值：

| 配置 | 默认值 | 参数 |
|---|---|---|
| Bind Address | `127.0.0.1` | 第 1 个位置参数，可传 numeric IPv4。 |
| TCP Port | `13400` | 第 2 个位置参数，十进制 `0..65535`；0 表示由 OS 选临时端口。 |
| Entity Logical Address | `0x1000` | 当前在 main 中常量配置，不是命令行参数。 |
| Conflict Response Code | `0x03` | 当前在 main 中常量配置，不是命令行参数。 |

`doip_tcp_server_init` 检查地址字符串非空且小于 16 字节，并复制到 `doip_tcp_server_t.bind_address`；它不解析 IP。真正的 IPv4 解析由 `doip_tcp_server_start` 用 `InetPtonA(AF_INET, ...)` 执行。仅数字 IPv4 字符串可用，不支持主机名或 IPv6。

### 本机与 LAN

- `127.0.0.1` 只指向本机回环接口，另一台主机上的 Tester 使用自己的 loopback，因此无法据此连接服务端。
- 两机 LAN 演示时，服务端可绑定明确的 LAN IPv4，或绑定 `0.0.0.0` 监听所有本机 IPv4 接口；Tester 的目标参数必须是服务端可达的 LAN IPv4，而不是 `0.0.0.0`。
- 需确认路由/网段可达、防火墙准入 TCP 端口（默认 13400）、端口未被占用，并确保 Tester 也是 Windows 版本。绑定 `0.0.0.0` 会扩大监听范围，应只在可信环境按需使用。

## 启动流程：`doip_tcp_server_start`

1. 检查 `server != NULL` 及 `lifecycle_state == STOPPED`。
2. `WSAStartup(MAKEWORD(2,2), ...)`，记录 `winsock_started`。
3. 把配置的 numeric IPv4 转为 `sockaddr_in`，port 用 `htons`。
4. `socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)` 创建 IPv4 TCP socket。
5. 设置 `SO_EXCLUSIVEADDRUSE`，再依次调用 `bind` 和 `listen(SOMAXCONN)`。
6. 将 listen socket 设为 nonblocking（`ioctlsocket` + `FIONBIO`）。
7. `getsockname` 读取实际绑定端口，支持配置端口 0 时查询 OS 分配结果。
8. 清除停止标志、设置 LISTENING 状态并打印 endpoint。

任一 Winsock 初始化、socket、option、bind、listen 或查询失败时记录部分 Winsock 错误、释放已创建资源并返回失败结果。`doip_result_t` 在此类失败常映射为 `UNSUPPORTED`；非法 numeric IPv4 返回 `INVALID_ARGUMENT`。

## TCP 字节流分帧

TCP 保证有序字节流，但不保留 `send()` 调用边界。一次 `recv()` 可能只拿到一个字节、多个部分甚至多条应用数据；因此不能把一次 `recv()` 等同于一条 DoIP 消息。

`process_client_messages` 每次循环的处理顺序：

1. `receive_exact(..., DOIP_GENERIC_HEADER_SIZE)` 累积 8 字节 Header。
2. `doip_header_decode` 检查版本 `0x02` 和反版本 `0xFD`，并解析 Type/Length。
3. 比较 `decoded_header.payload_length` 与 `DOIP_TCP_SERVER_MAX_PAYLOAD_SIZE`（4096）；超限先拒绝，不读 Body。
4. 计算 `8 + payload_length`，在 `input_message + 8` 处再次调用 `receive_exact` 累积 Body。
5. 将准确的完整消息长度交给 `doip_gateway_process_message`。
6. Gateway 成功后调用 `send_all` 发送 `output_message` 的 `output_message_size` 字节。

两个栈数组大小均为 `8 + 4096`。此处上限是本地资源策略，不是 DoIP 通用最大载荷的声明。

## `wait_for_socket`、`receive_exact`、`send_all`

- 所有 Server socket 都设为 nonblocking。`wait_for_socket` 使用 `select` 等待读/写准备情况，每次等待最多 100 ms；若超时则再次循环，借此定期检查 `stop_requested`。遇到 `WSAEINTR` 重试，其他 `select` 错误记录到 `last_socket_error` 并失败。
- `receive_exact` 持续累计 `recv()` 字节，直到目标长度达成。正数表示本次读到的字节数；0 表示对端有序关闭/断开；负值时 `WSAEWOULDBLOCK` / `WSAEINTR` 重试，其它错误失败。
- `send_all` 持续累计 `send()` 的正数返回值直到全部写完；0 表示无进展，负值下 `WSAEWOULDBLOCK` / `WSAEINTR` 重试，其它错误失败。

Server 没有针对一帧 Header 或 Body 的总读取 deadline；100 ms 是停止响应轮询间隔，不是网络超时。一个连接若只发半个 Header/Body 后保持打开，Server 可持续等数据，直到对端断开、发生 socket 错误或请求停止。停止标志可使上述等待循环退出。

## 客户端管理与状态生命周期

`doip_tcp_server_run` 通过原子生命周期状态从 LISTENING 进入 RUNNING，然后循环等待 listen socket 可读、调用 `accept`。每次只接受并服务一个 client；处理当前连接期间不并发 accept 其他连接。

新 client 也被设为 nonblocking。连接结束、消息非法或 socket 错误后，Server 关闭 `client_socket` 并执行 `doip_gateway_reset_routing_activation`，再返回接受连接循环。监听器错误则清理 Server 资源、置 STOPPED 并返回错误。

## 停止流程与资源所有权

- main 注册 `console_control_handler`，处理 Ctrl+C、Ctrl+Break、窗口关闭时调用 `doip_tcp_server_request_stop`。此 API 只设置原子停止标志，不直接清理 socket，适用于活动服务线程。
- `doip_tcp_server_run` 退出循环后调用 `release_server_resources`，关闭 client/listen socket 并在需要时 `WSACleanup`。
- `doip_tcp_server_stop` 请求停止；若仍处于 LISTENING、`run` 尚未开始，则直接关闭资源；若 RUNNING，则不等待，清理由 `run` 执行。
- main 注销控制处理器并根据结果返回进程退出码。

`doip_tcp_server_t` 记录 bind address、configured/bound port、socket 句柄、停止与生命周期状态、Winsock 引用状态、最后错误及 `doip_gateway_t`。头文件说明不要与 init/start 并发调用。

## Server 与 Gateway 的关系

Transport 不解释 Routing Activation 字段的业务含义。它仅确保一条完整的、长度在界内的 DoIP 消息，然后交给 Gateway。Gateway 输出的仍是完整报文字节，Transport 不重新编码 Header，而是照长度发送。Gateway 错误会导致当前客户端连接被关闭；本 Server 不生成通用 DoIP 错误消息。

## 错误与测试覆盖

Windows `test_doip_tcp_server.c` 覆盖 fragmented Request 能产生完整响应、短 Header/Body 后断开、非法协议版本/反版本、超过 4096 的长度、非法客户端后能重新连接、Server stop/lifecycle 等。它没有模拟所有 Winsock 错误，也没有验证一个保持半帧不关闭的客户端最终会因超时退出——因为 Server 侧没有该 deadline。

## 面试时需要能够讲清楚的问题

- 为什么 Server 要先收 8 字节，再按 Payload Length 读 Body？
- `recv()` 的 0、正数、`SOCKET_ERROR` 分别如何解释？
- 100 ms `select` 轮询是否等于协议读取超时？
- 超大 Payload Length 为什么必须在读取 Body 前拒绝？
- 客户端断开后为什么重置 Gateway 状态？
- LAN 场景为什么服务端绑定 `0.0.0.0`，客户端却连接具体 LAN IP？

## 本章需要掌握的知识点

- TCP 收发必须累积实际字节数，不能假设一次 `recv` / `send` 完成一帧。
- Server 的 4096 是本地固定缓冲区上限；它没有应用层读取 deadline。
- Server 单客户端、同步处理；完成客户端后清状态并回到 accept。
- socket、Winsock 生命周期由 Transport 持有，Gateway 只处理完整消息。
