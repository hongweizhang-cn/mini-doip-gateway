# 06 Tester 实现

## 学习目标

能说明 Tester 的职责边界、命令行参数、非阻塞连接与定时收发流程，以及它如何判断一次 Routing Activation 测试成功。

## Tester 是什么、又不是什么

`doip_tester.c` 是一个 Windows Winsock 测试客户端：生成一条基本 Routing Activation Request，连接一个 IPv4 TCP 服务端，接收一条 Response 并检查响应。它不是完整汽车诊断 Tester，不做 UDP Vehicle Discovery、Alive Check、Diagnostic Message、DTC 读取/清除、ECU 寻址配置或标准一致性验证。

## 命令行入口和默认值

`doip_tester_main.c`：

| 参数位置 | 默认值 | 校验 |
|---|---|---|
| `server-ipv4` | `127.0.0.1` | 非空参数传递给 Tester；连接函数要求 numeric IPv4。 |
| `port` | `13400` | 十进制 1–65535，0 被 main 拒绝。 |
| `tester-logical-address` | `0x0E00` | `strtoul` base 0，故支持十进制或 `0x` 十六进制，范围 0–65535。 |
| `entity-logical-address` | `0x1000` | 同上。 |

`-h` / `--help` 只在单独作为唯一参数时显示用法。`doip_tester_run` 通过自己的 `WSAStartup` / `WSACleanup` 管理 Winsock 引用。

## 连接、发送和接收流程

### 连接

`connect_with_timeout` 创建 `AF_INET/SOCK_STREAM/IPPROTO_TCP` socket，检查 numeric IPv4，再以 nonblocking 模式发起 `connect`。若连接正在进行，则对写集合 `select` 最多等待 3 秒，并用 `getsockopt(SO_ERROR)` 判断最终连接结果。成功后 socket 保持 nonblocking。

### Request 构造

Tester 创建：

- `doip_routing_activation_request_t`：命令行地址 + 固定 `activation_type = 0x01`。
- 7 字节 Request Body：调用 `doip_routing_activation_request_encode`。
- `doip_header_t`：版本 `02 FD`、类型 Request `0x0005`、Payload Length 7。
- 15 字节完整消息：通过 `doip_message_encode` 组合。

之后 `send_all` 对整次消息发送使用单个 5 秒 deadline。每轮 `select` 等待可写，循环处理部分 `send()`；可重试的 `WSAEWOULDBLOCK` / `WSAEINTR` 不立即失败。其它错误、0 进展或 deadline 到期返回失败。

### Response 接收与校验

1. `receive_exact` 在一个独立的 5 秒 deadline 内读满 8 字节 Response Header。
2. `doip_header_decode` 校验版本，Tester 确认 Payload Type 等于 `0x0006`。
3. Payload Length 大于本地 4096 限制返回 `BUFFER_TOO_SMALL`；长度不等于基本 Response 的 9 字节返回 `INVALID_LENGTH`。
4. 第二次 `receive_exact` 用新的 5 秒 deadline 读满 Body。
5. `doip_routing_activation_response_decode` 解码地址与 Response Code。
6. 校验 Response client 地址等于本次 Tester 地址、Entity 地址等于期望值；任一不匹配返回 `INVALID_HEADER`。
7. 仅当 Response Code 是 `DOIP_ROUTING_ACTIVATION_RESPONSE_CODE_SUCCESS` (`0x10`) 才返回 `OK`；否则返回 `UNSUPPORTED`。

Tester 在 Body 解码成功后立即把结果写入 `*response`，因此即使地址/Response Code 校验失败，调用者仍可检查已解码的 Body（文档头文件也说明拒绝码仍会保留）。出错时输出含义取决于失败发生阶段。

## 超时精确范围

- **连接**：最多等待 3 秒。
- **整次 Request send**：最多 5 秒，而不是每个 `send()` 5 秒。
- **读 Header**：最多 5 秒。
- **读 Body**：另有最多 5 秒。

因此“每次读阶段 5 秒”不等于从连接开始到完整 Response 的总事务超时固定为 5 秒。`wait_for_socket` 每轮根据 `GetTickCount()` 计算该阶段剩余时间，传给 `select`；`receive_exact` 每个调用各自记录起始时刻。

## 清理与错误分类

所有连接后流程通过 `cleanup` 关闭 socket（`shutdown` 后 `closesocket`），并调用 `WSACleanup`。连接、I/O、Winsock 启动等多种失败映射为 `DOIP_RESULT_UNSUPPORTED`；对端提前关闭完整读取返回 `INVALID_LENGTH`；无效版本、错误长度、容量不足和参数错误则分别保留对应结果。函数同时向 stderr 输出阶段日志。

`main` 将非 `DOIP_RESULT_OK` 映射为 `EXIT_FAILURE`。因此命令行成功只说明此演示路径的连接、基本报文结构、地址及成功码校验均通过，不代表更广泛的协议符合性。

## 测试代码提供的证据

`test_doip_tester.c` 使用实际 Gateway TCP Server 做端到端场景，并使用 fake peer 构造失败响应，覆盖：

- 成功完整交互、非成功 Response Code。
- 无效 Header、Payload 超过 4096、错误固定 Payload Length。
- Header / Payload 提前关闭、错误 Payload Type、逻辑地址不匹配。
- 慢速 Header 超时（并检查总体耗时上界）、无监听端口的连接失败。

这些是源码中的测试设计；文档编写期间没有执行 CTest。

## 面试时需要能够讲清楚的问题

- Tester 的成功判定有哪些独立条件？
- 为什么连接成功不代表 Routing Activation 成功？
- Tester 的 3 秒和 5 秒分别覆盖什么操作？为什么总时间可大于 5 秒？
- Tester 是否校验所有可能 Response Code 的标准语义？
- 若 Response Code 是未知值但 Body 格式正确，解码和 Tester 主流程分别如何处理？
- 一个真实诊断 Tester 还需要哪些本项目没有的功能？

## 本章需要掌握的知识点

- Tester 只支持单次基本 Routing Activation 往返，不是通用诊断客户端。
- 连接是 nonblocking + 3 秒等待；send 和每个 exact-read 阶段分别有 5 秒上限。
- Response Codec 保留响应码；Tester 只接受 `0x10`，并校验两个地址。
- 完整报文长度、类型、Body 格式和业务成功码是不同层次的校验。
