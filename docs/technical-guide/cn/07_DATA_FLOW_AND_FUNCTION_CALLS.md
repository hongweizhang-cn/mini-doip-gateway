# 07 数据流与函数调用关系

## 学习目标

以真实函数、文件、变量和结构体为线索，独立复述一次 Request/Response 的端到端路径，并能够指出每一层的输入输出和副作用。

## 1. Tester 启动到 TCP 连接建立

```mermaid
flowchart TD
    A["doip_tester_main.c: main(argc, argv)"] --> B["parse_unsigned(): 校验端口/逻辑地址"]
    B --> C["doip_tester_run(server_ipv4, port, tester_addr, entity_addr, &response)"]
    C --> D["WSAStartup(MAKEWORD(2,2))"]
    D --> E["connect_with_timeout()"]
    E --> F["socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)"]
    F --> G["InetPtonA(): numeric IPv4"]
    G --> H["ioctlsocket(FIONBIO): nonblocking"]
    H --> I["connect()"]
    I --> J["select(write set, 3 s)"]
    J --> K["getsockopt(SO_ERROR)"]
    K --> L["返回有效 SOCKET；进入消息收发"]
```

依赖文件：`doip_tester_main.c` 的 `main` / `parse_unsigned`，`doip_tester.c` 的 `doip_tester_run` / `connect_with_timeout`。socket 成功仅建立 TCP 连接，不改变 Gateway Routing Activation 状态。

## 2. Tester 发送 Routing Activation Request

```mermaid
flowchart TD
    A["doip_tester_run()"] --> B["request.source_address = tester_logical_address"]
    B --> C["request.activation_type = 0x01"]
    C --> D["doip_routing_activation_request_encode()"]
    D --> E["request_body[7]"]
    E --> F["request_header: 02 / FD / type 0005 / length 7"]
    F --> G["doip_message_encode()"]
    G --> H["doip_header_encode()"]
    G --> I["doip_payload_copy()"]
    H --> J["request_message[15]"]
    I --> J
    J --> K["send_all(): deadline 5 s"]
    K --> L["wait_for_socket(select writable)"]
    L --> M["send() 循环，累计 sent_total"]
```

关键对象为 `doip_routing_activation_request_t request`、`request_body[7]`、`doip_header_t request_header`、`request_message[15]`。`send_all` 可能多次发送；它只保证字节全部交给 socket 或报告失败。

## 3. Server 收字节并解析 DoIP 消息

```mermaid
flowchart TD
    A["doip_tcp_server_run()"] --> B["wait_for_socket(listen, readable)"]
    B --> C["accept() 一个 client"]
    C --> D["ioctlsocket(client, FIONBIO)"]
    D --> E["process_client_messages()"]
    E --> F["receive_exact(input_message, 8)"]
    F --> G["wait_for_socket()"]
    G --> H["recv() 累计 received_total"]
    H --> I["doip_header_decode()"]
    I --> J{"payload_length <= 4096?"}
    J -- 否 --> K["拒绝该 client 消息并关闭连接"]
    J -- 是 --> L["receive_exact(input_message + 8, payload_length)"]
    L --> M["计算 complete_message_size = 8 + payload_length"]
    M --> N["doip_gateway_process_message()"]
```

这里的第一层 Header 解析是为了确定 TCP 帧的剩余字节数；随后 Gateway 内部 `doip_message_decode` 再对完整消息进行一致性验证并构造 `doip_message_view_t`。这不是重复搬运 Payload：Header 字段确实再次经通用消息 API 解释，但 Payload 视图借用原数组，不复制。

## 4. Gateway Core 处理 Routing Activation

```mermaid
flowchart TD
    A["doip_gateway_process_message(gateway, input, size, output, capacity, output_size)"] --> B["先将 *output_size 置 0"]
    B --> C["检查指针"]
    C --> D["doip_message_decode()"]
    D --> E["doip_header_decode() + 精确长度比较"]
    E --> F{"Payload Type == 0005?"}
    F -- 否 --> X["UNSUPPORTED"]
    F -- 是 --> G["doip_routing_activation_request_decode()"]
    G --> H{"output capacity >= 17?"}
    H -- 否 --> Y["BUFFER_TOO_SMALL；状态不变"]
    H -- 是 --> I["doip_routing_activation_state_process_request()"]
    I --> J["response_code"]
    J --> K["doip_routing_activation_encode_response_message()"]
    K --> L["response body + response header"]
    L --> M["doip_message_encode()"]
    M --> N["输出 17-byte 完整 Response / *output_size = 17"]
```

请求字段在 `doip_routing_activation_request_t activation_request`；状态在 `gateway->routing_activation`；返回码先放局部 `response_code`。Gateway 容量校验在状态调用之前，是避免响应空间不足却已提交状态副作用的关键顺序。

## 5. Server 生成并发送 Response

```mermaid
flowchart TD
    A["Gateway 返回 DOIP_RESULT_OK"] --> B["output_message[4104] 与 output_message_size"]
    B --> C["process_client_messages(): send_all(client_socket, output_message, output_message_size)"]
    C --> D["wait_for_socket(select writable, 100 ms polling)"]
    D --> E["send()"]
    E --> F{"发送完成?"}
    F -- 未完成 --> G["累加 sent_total；处理 WSAEWOULDBLOCK / WSAEINTR"]
    G --> D
    F -- 完成 --> H["继续读取该客户端下一条消息"]
    F -- 失败/停止 --> I["结束当前客户端处理"]
```

注意 `output_message` 的实际数组是 Server 侧固定缓冲区，`output_message_size` 表示有效的 17 字节，不是数组容量。Response 的字节序列由 Gateway/Codec 已经生成，Server 不再改写。

## 6. Tester 接收并验证 Response

```mermaid
flowchart TD
    A["send_all(Request) 成功"] --> B["receive_exact(response_header_bytes, 8): 5 s"]
    B --> C["doip_header_decode()"]
    C --> D{"payload_type == 0006?"}
    D -- 否 --> X["UNSUPPORTED"]
    D -- 是 --> E{"length <= 4096 且 length == 9?"}
    E -- 超限 --> Y["BUFFER_TOO_SMALL"]
    E -- 不等于 9 --> Z["INVALID_LENGTH"]
    E -- 是 --> F["receive_exact(response_payload, 9): 独立 5 s"]
    F --> G["doip_routing_activation_response_decode()"]
    G --> H["复制 decoded_response 到 *response"]
    H --> I{"client / entity address 均匹配?"}
    I -- 否 --> U["INVALID_HEADER"]
    I -- 是 --> J{"response_code == 0x10?"}
    J -- 否 --> V["UNSUPPORTED"]
    J -- 是 --> W["DOIP_RESULT_OK；main 返回 EXIT_SUCCESS"]
```

`doip_tester_main.c` 将 `DOIP_RESULT_OK` 映射为 `EXIT_SUCCESS`，其它结果映射为 `EXIT_FAILURE`。错误日志来自 Tester 的 stderr，部分请求/结果信息输出到 stdout。

## 完整实例串联

使用默认配置时：

1. `doip_tester_main.c::main` 取目标 `127.0.0.1:13400`、Tester 地址 `0x0E00`、Entity 地址 `0x1000`。
2. `doip_tester.c::doip_tester_run` 初始化 Winsock 并由 `connect_with_timeout` 建立 TCP 连接。
3. Request Codec 将 `doip_routing_activation_request_t{0x0E00, 0x01}` 转为 `0E 00 01 00 00 00 00`。
4. Message Codec 在前面加 8 字节 `02 FD 00 05 00 00 00 07`，总计 15 字节。
5. Tester `send_all` 完成 15 字节发送；TCP 可能分段传输，协议不依赖分段。
6. `doip_tcp_server.c::doip_tcp_server_run` 接受连接，`process_client_messages` 先 `receive_exact` 收 8 字节，再以长度 7 收 Body。
7. `doip_header_decode` 提取版本、类型和长度；`process_client_messages` 先确保 `7 <= 4096`。
8. `doip_gateway.c::doip_gateway_process_message` 解完整消息，再由 Request Codec 提取 `source_address = 0x0E00`。
9. `doip_routing_activation_state_process_request` 若上下文 INACTIVE，则记录地址、设 ACTIVE 并输出 `0x10`。
10. `doip_routing_activation_encode_response_message` 构造 9 字节 Body `0E 00 10 00 10 00 00 00 00`，再与 Response Header 合成为 17 字节。
11. Server `send_all` 把这 17 字节写回 TCP。
12. Tester 分阶段精确读取 Header/Body，校验 `0x0006`、长度 9、两个地址及成功码 `0x10`，然后返回成功。

若活动状态收到另一个逻辑地址，步骤 9 改为保留原状态并输出配置冲突码；Gateway 仍可编码 Response，但 Tester 将其判为非成功。

## 面试时需要能够讲清楚的问题

- 为什么 TCP Server 和 Gateway 都出现 Header Decoder？二者分别解决什么问题？
- 从 `request_body` 到 wire bytes，按调用顺序经过哪些函数？
- 哪个对象保存激活地址？该地址何时更新、何时清除？
- Tester 读取 Response 时何时知道 Body 长度？为什么要先校验上限？
- 哪些变量是借用指针，哪些是拥有实际字节的缓冲区？
- 若输出容量不足，调用链在哪一步停止，状态是否变化？

## 本章需要掌握的知识点

- 能从命令行入口追到 socket 建立、请求编码、分帧、Gateway、状态、响应编码和验证。
- TCP Server 的 framing 解码与 Gateway 完整消息校验各有边界职责。
- `input_message`/`output_message` 是固定缓冲区，Message View 的 Payload 指针借用输入。
- 成功判定最终发生在 Tester：地址匹配且 Response Code 等于 `0x10`。
