# 09 构建、测试与调试

## 学习目标

能解释 CMake 如何按平台配置目标，能从仓库根目录构建与运行测试，并能按 TCP 流和 DoIP 字段定位演示中的常见问题。

## CMake 项目结构

- 根 `CMakeLists.txt` 要求 CMake 3.16，项目语言只有 C，设置 `CMAKE_C_STANDARD 99`、`CMAKE_C_STANDARD_REQUIRED ON`、`CMAKE_C_EXTENSIONS OFF`，启用 CTest 并加入 `c-core`。
- `c-core/CMakeLists.txt` 创建静态库 `doip_core`，加入 Header、Gateway、Message、Payload、Routing Activation 等跨平台协议核心文件。
- 仅 `WIN32` 时把 `doip_tcp_server.c` / `doip_tester.c` 加入库，链接 `ws2_32`，并构建 `doip_tcp_server` / `doip_tester` 两个程序。
- `BUILD_TESTING` 打开时添加 `c-core/tests`。协议核心测试按测试文件创建独立 executable；`WIN32` 下另创建 TCP Server 与 Tester 网络测试，并配置 CTest 超时。
- GNU/Clang 编译目标添加 `-Wall -Wextra -Wpedantic`。CMake 文件没有设置 sanitizer、覆盖率或严格告警即错误模式。

当前仓库已有的 `build/CMakeCache.txt` 显示此构建目录配置为 Ninja，C 编译器为 MSYS2 UCRT64 `gcc.exe`。这只描述现存 cache，不代表所有用户必须使用同一 Generator 或编译器，也不表示此处重新执行过构建。

## Windows 构建和测试

在仓库根目录 PowerShell：

```powershell
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

列出当前配置注册的测试：

```powershell
ctest --test-dir build -N
```

只运行一个测试（例如 Message Codec）：

```powershell
ctest --test-dir build -R test_doip_message --output-on-failure
```

详细显示测试输出：

```powershell
ctest --test-dir build -V
```

可在首次配置时显式选 Ninja 和 GCC（仅当机器安装并可找到该工具时）：

```powershell
cmake -S . -B build-ninja -G Ninja -DCMAKE_C_COMPILER=gcc
cmake --build build-ninja
ctest --test-dir build-ninja --output-on-failure
```

`build-ninja` 是示例新构建目录；CMake Generator 一旦写入某个 build directory 后不能通过简单重复 configure 改成另一种 Generator，应使用另一个新目录。上述命令是使用说明，不是本次执行记录。

### 平台差异

- Windows：构建 Protocol Core、两个 Winsock 程序以及全部可用单元/网络测试；`ws2_32` 为系统库。
- 非 Windows：CMake 不加入 Server/Tester 的 Winsock 实现、可执行程序和网络测试；仅配置 Protocol Core 与跨平台测试。`scripts/demo-doip.ps1` 也是 Windows PowerShell 流程。
- 项目没有 POSIX socket 适配；不能把跨平台核心库的构建能力推断成跨平台 TCP 应用能力。

## 测试分层

| 层次 | 测试文件 | 主要对象 |
|---|---|---|
| 单元 | `test_doip_header.c` | Header 编解码、版本、容量、大端序。 |
| 单元 | `test_doip_payload.c` | 长度检查、字节拷贝、NULL、容量。 |
| 单元 | `test_doip_message.c` | 完整消息长度、Header/Body、截断和尾随数据。 |
| 单元 | Request / Response Codec tests | Body 字段、Reserved、长度边界。 |
| 单元 | `test_doip_routing_activation_state.c` | 单上下文状态转移和冲突行为。 |
| 单元/集成核心 | `test_doip_routing_activation.c`, `test_doip_gateway.c` | 完整报文处理、类型分发、状态副作用、Response。 |
| Windows 网络集成 | `test_doip_tcp_server.c` | 动态端口、分片请求、连接断开、非法 Header/长度及停止。 |
| Windows 网络集成 | `test_doip_tester.c` | Tester 对真实本地 Gateway Server 与多类 fake peer 响应的行为。 |

CTest 结果只反映当前配置下实际注册并运行的测试。文档中列出的测试用例是从源码读取的测试设计；未运行测试时不要称其“已通过”。构建目录内已有历史 `Testing` 结果也不能证明当前源代码与历史二进制完全对应。

## PowerShell Demo

先构建，再从仓库根目录运行：

```powershell
.\scripts\demo-doip.ps1
```

可指定服务端地址和端口：

```powershell
.\scripts\demo-doip.ps1 -ServerAddress 127.0.0.1 -Port 13400
```

脚本检查 Windows IPv4 numeric address 和两个可执行文件是否存在，先探测 endpoint 是否已接受连接，再启动 Server，轮询最多 10 秒等待可用，启动 Tester 并等待最多 20 秒。最后它只清理由脚本启动的 Server 进程。运行脚本的进程终止方式与 Server 自身 Ctrl+C 的优雅 stop 路径不同：脚本 `finally` 对子进程调用 `Kill()`。

## 调试建议

1. 单独运行 `doip_tcp_server.exe`，观察启动输出中的实际 bind endpoint。
2. 另一个终端启动 Tester；对照 Tester 打印的目标、请求大小、响应字段和退出码。
3. 若失败，先看参数/IP/端口，其次看 Server 的 Winsock error 与 Tester 的错误阶段，再区分 Header、Payload Length、Body Reserved、地址或 Response Code。
4. 使用 Visual Studio / VS Code C 调试器时，在 `doip_tester_run`、`connect_with_timeout`、`send_all`、`receive_exact`、`process_client_messages`、`doip_gateway_process_message`、`doip_routing_activation_state_process_request` 设置断点。查看 `received_total`、`sent_total`、`decoded_header.payload_length`、`activation_request`、`gateway.routing_activation` 和 `output_message_size`。
5. 当前源代码没有配置结构化日志、统一日志级别或协议报文 dump；调试时可用现有 `printf` / `fprintf` 和调试器观察内存。

## Wireshark 分析方法

仓库 `docs/test-results/` 存在 loopback/LAN 抓包和截图。要自行验证：

1. 在 Tester 与 Server 运行的对应接口开始捕获；loopback 场景需使用支持本机 loopback 捕获的接口。
2. 先用显示过滤器 `tcp.port == 13400`，若端口不同则替换端口。
3. 选中连接后可用 `tcp.stream eq N` 聚焦该 TCP stream，并用 Follow TCP Stream 查看重组后的字节流。
4. 在 TCP 载荷中按 8 字节 Generic Header 手动核对：例如 `02 FD 00 05 00 00 00 07` 后应是 7 字节 Request Body；Response 是 Payload Type `00 06`、长度 9。
5. Wireshark 的协议自动解码效果取决于版本、端口映射和 dissector 设置；应以原始 Bytes、TCP stream 重组和本项目的实际字段布局交叉验证，不能仅凭界面标签下结论。

捕获能证明线上观察到的字节和时序，不能单独证明所有测试通过或项目符合全部 ISO 13400 要求。

## 常见问题定位

| 现象 | 优先检查 |
|---|---|
| CMake 找不到编译器 / 生成器不兼容 | 检查安装的 C 编译器、CMake Generator；不要在现有 build cache 上混换 Generator。 |
| `doip_tcp_server.exe` 或 Tester 不存在 | 确认 Windows 配置、构建成功及输出路径 `build\c-core`。 |
| Bind/Connect 失败 | 检查地址是 numeric IPv4、端口、端口占用、进程监听地址、防火墙及 LAN 路由。 |
| Tester 连接成功但激活失败 | 检查 Response Payload Type/Length、逻辑地址、Response Code；非 `0x10` 会被 Tester 报失败。 |
| TCP 连接等待不结束 | Tester 有自身 deadline；Server 没有半帧读取 deadline，连接只发部分数据后保持打开会占住单客户端循环。 |
| CTest 没看到网络测试 | 确认当前配置平台为 Windows，查看 `ctest -N` 实际注册列表。 |

## 面试时需要能够讲清楚的问题

- 为什么网络测试只在 Windows 条件下注册？
- 如何区分 Protocol Core 单测和 TCP 集成测试？
- `ctest -R` 与 `ctest -V` 分别解决什么问题？
- 如何用 Wireshark 从 TCP stream 中找出 DoIP Header 和 Body？
- 历史构建目录中的测试日志能否当作当前源码验证结果？

## 本章需要掌握的知识点

- CMake 用 `WIN32` 明确隔离 Winsock 源码、目标和网络测试。
- 测试源码存在、CTest 注册、测试执行成功是三个不同事实。
- PowerShell Demo 是进程级演示辅助，不是另一种协议实现。
- Wireshark 应关注 TCP 重组后的原始字节，并按本项目固定布局核对。
