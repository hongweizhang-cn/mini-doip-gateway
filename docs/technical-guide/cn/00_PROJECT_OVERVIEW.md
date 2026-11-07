# 00 项目总览

## 学习目标

读完本章后，应能在 5～10 分钟内介绍项目解决的问题、实际实现范围、主要组件与限制，并能说明它与真实 DoIP 诊断系统的差别。

## 一句话介绍

`mini-doip-gateway` 是一个 C99 教学项目：它在 Windows 上启动一个只处理单客户端的 TCP 服务端，用 DoIP Generic Header 对 TCP 字节流分帧，解析基本 Routing Activation Request，按简化状态规则生成 Routing Activation Response；同仓库的 Tester Simulator 可连接服务端并验证响应。项目没有实现 ECU 诊断转发，也不是完整 ISO 13400 协议栈或认证产品。

## 背景与解决的问题

Automotive Ethernet 提供车辆网络中的 Ethernet 通信基础。DoIP（Diagnostics over Internet Protocol）描述诊断通信如何使用 IP 网络承载诊断相关消息。一个实际诊断路径通常涉及：

- **Tester**：诊断测试设备或软件，发起发现、连接、路由激活及后续诊断请求。
- **DoIP Gateway / Entity**：提供 DoIP 网络入口，管理诊断访问，并在实际系统中可能将诊断请求转发到目标 ECU。
- **ECU**：被诊断的电子控制单元。

此仓库只演示 Tester 与一个虚拟 Gateway 之间的 Routing Activation 往返。Gateway 不连接真实 ECU，也不转发 Diagnostic Message；因此“Gateway”在这里是教学中的协议处理角色，不是可用于车辆诊断的完整网关。

## 当前实际实现

### 已实现

- C99 Generic Header 的 8 字节编码、解码和版本/反版本检查。
- 完整 DoIP Message 的编码与恰好一条消息的解码。
- 基本 Routing Activation Request（7 字节）和 Response（9 字节）的字段编解码。
- 单逻辑客户端的简化状态：空闲时接受第一个请求；活动时相同逻辑地址重复请求返回成功；不同逻辑地址收到配置的冲突码且不替换当前地址。
- Windows Winsock TCP Server：单客户端、非阻塞 socket、TCP 分段读取/写入循环、4096 字节本地载荷限制。
- Windows Tester Simulator：连接、发一个 Routing Activation Request、读取并校验响应；仅当响应码为 `0x10` 且两个逻辑地址匹配时报告成功。
- CTest 注册的协议核心单元测试，以及仅 Windows 构建的 Server / Tester 网络测试。

### 明确未实现

- UDP Vehicle Identification、Vehicle Announcement 的处理流程、Alive Check。
- Diagnostic Message 处理、目标 ECU 接口及诊断转发。
- 多客户端并发、完整 Routing Activation 状态机和完整标准响应码策略。
- OEM 扩展字段的完整支持。
- POSIX/Linux TCP Server / Tester 实现、官方一致性认证。
- 完整 ISO 13400 实现。`doip_payload_type.h` 中定义了部分 Vehicle Identification / Announcement 类型常量，不代表相应消息已可解析或处理。

## 技术栈与目录

| 路径 | 用途 |
|---|---|
| `CMakeLists.txt` | 根 CMake 项目，设置 C99，启用 CTest 并加入 `c-core`。 |
| `c-core/include/` | 公开类型、协议常量、结构体和模块 API。 |
| `c-core/src/` | Generic Header、Message、Payload、Routing Activation、Gateway 及 Windows 网络实现。 |
| `c-core/tests/` | 各协议模块单元测试；Windows 下增加 TCP Server 与 Tester 网络测试。 |
| `scripts/demo-doip.ps1` | Windows PowerShell 演示脚本，启动服务端、运行 Tester、清理自己启动的服务端进程。 |
| `docs/PROJECT_INTERVIEW_NOTES.md` | 现有面试提纲；本技术指南是新增的深入学习材料。 |
| `docs/test-results/` | 已存放的 loopback 与 LAN 抓包、截图。文件存在并不等于本次重新运行或验证了这些实验。 |
| `build/` | 当前目录中已有的 CMake/Ninja 构建产物；不属于源代码，也不能据此推断此次测试运行结果。 |

项目使用 CMake 3.16 以上的配置要求、C99；GNU/Clang 编译时启用 `-Wall -Wextra -Wpedantic`。Windows 网络组件链接 Winsock `ws2_32`。根配置以 `WIN32` 条件添加网络源文件、可执行程序及其测试。

## 运行方式

在仓库根目录的 PowerShell 中：

```powershell
cmake -S . -B build
cmake --build build
ctest --test-dir build -V
```

Windows 下分别启动：

```powershell
.\build\c-core\doip_tcp_server.exe 127.0.0.1 13400
.\build\c-core\doip_tester.exe 127.0.0.1 13400 0x0E00 0x1000
```

也可在 Windows PowerShell 中运行 `.\scripts\demo-doip.ps1`。服务端默认绑定 `127.0.0.1:13400`，Tester 默认连接此地址，逻辑地址默认 Tester `0x0E00`、Entity `0x1000`。服务端可通过命令行提供 IPv4 地址和端口；Tester 可提供目标 IPv4、端口、Tester 地址及 Entity 地址。服务端绑定 `0.0.0.0` 可监听所有本机 IPv4 接口，但跨电脑使用还需将 Tester 目标改为服务端 LAN IPv4，并确保网络路由及防火墙允许 TCP 13400（或实际配置端口）。`127.0.0.1` 是每台主机自己的回环地址，不是另一台电脑的地址。

## 建议学习顺序

1. 先读本章和 `01_SYSTEM_ARCHITECTURE.md`，建立组件边界。
2. 读 `02_DOIP_PROTOCOL_AND_MESSAGE_FORMAT.md`，把线上字节布局与 C 结构体区分开。
3. 读 `03_PROTOCOL_CORE_IMPLEMENTATION.md`，再读 `04_ROUTING_ACTIVATION_IMPLEMENTATION.md`。
4. 读 `05_TCP_SERVER_IMPLEMENTATION.md` 与 `06_TESTER_IMPLEMENTATION.md`，理解 TCP 分帧、超时和资源管理。
5. 用 `07_DATA_FLOW_AND_FUNCTION_CALLS.md` 从头跟踪一笔真实请求。
6. 最后读错误边界、构建测试、设计取舍、限制及问答章节，并对照源文件复述。

## 面试时需要能够讲清楚的问题

- 这个项目到底实现了哪一条 DoIP 交互路径？哪些名字存在但功能尚未实现？
- 为什么它不能称为完整 DoIP 协议栈或真实 ECU 诊断工具？
- 一条 Routing Activation Request 如何从 Tester 到 Gateway，再形成响应？
- 项目中哪些组件可跨平台复用，哪些明确依赖 Windows？
- 单客户端状态、本地 4096 字节上限和无诊断转发分别意味着什么？

## 本章需要掌握的知识点

- 项目是**简化的 Routing Activation 演示**，不是完整 DoIP 实现。
- TCP Server 与 Tester 是 Windows Winsock 实现；协议核心以 C99 组织。
- Gateway 不连接 ECU；Tester 只验证 Routing Activation Response。
- `127.0.0.1` 用于本机回环；LAN 测试需监听可达接口并连接服务器 LAN IPv4。
