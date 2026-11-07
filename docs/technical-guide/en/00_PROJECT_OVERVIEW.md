# 00 Project Overview

## Learning Objectives

After this chapter, you should be able to introduce the project in five to ten minutes, explain what it actually implements, and distinguish it from a production DoIP diagnostic system.

## One-Sentence Summary

`mini-doip-gateway` is a C99 learning project that demonstrates a basic DoIP Routing Activation exchange. On Windows, a single-client Winsock TCP server frames the byte stream using the DoIP Generic Header, passes complete messages to a simplified Gateway Core, and returns a Routing Activation Response. A companion Tester Simulator sends a request and validates the response. The project does not forward diagnostics to an ECU and is neither a complete ISO 13400 stack nor a certified product.

## Background and Problem Scope

Automotive Ethernet provides Ethernet-based communication in a vehicle. DoIP (Diagnostics over Internet Protocol) describes how diagnostic communication can be carried over IP networks. A typical diagnostic path may involve:

- **Diagnostic Tester**: a device or application that discovers an entity, connects, requests Routing Activation, and may then send diagnostic requests.
- **DoIP Gateway / Entity**: the network-facing DoIP endpoint. In a real system it may manage diagnostic access and route requests to an ECU.
- **ECU**: the electronic control unit being diagnosed.

This repository demonstrates only a Routing Activation request/response exchange between a Tester and a simulated Gateway. The Gateway does not connect to a real ECU or forward Diagnostic Messages. Here, “Gateway” describes the protocol-processing role in a learning example, not a complete vehicle gateway.

## What Is Implemented

### Implemented

- C99 encoding and decoding of the 8-byte Generic Header, including the project’s version/inverse-version check.
- Encoding and decoding of complete DoIP messages; the decoder accepts exactly one message.
- Basic Routing Activation Request and Response body codecs (7 and 9 bytes, respectively).
- A simplified single-logical-client state policy: accept the first request while inactive; treat a repeat from the same logical address as successful; return a configured conflict code for a different address without replacing the active address.
- A Windows Winsock TCP server with one client at a time, nonblocking sockets, partial-I/O loops, and a local 4096-byte payload limit.
- A Windows Tester Simulator that connects, sends one Routing Activation Request, and validates the response. It reports success only for response code `0x10` and matching logical addresses.
- CTest-registered protocol-core unit tests and Windows-only Server/Tester network tests.

### Explicitly Not Implemented

- UDP Vehicle Identification and Vehicle Announcement processing, or Alive Check.
- Diagnostic Message handling, a target-ECU interface, or diagnostic forwarding.
- Concurrent clients, the complete Routing Activation state machine, or the full standardized response-code policy.
- Full support for OEM extension data.
- A POSIX/Linux TCP Server or Tester, or official conformance certification.
- A complete ISO 13400 implementation. `doip_payload_type.h` defines some Vehicle Identification/Announcement type constants, but those constants do not mean the corresponding messages can be parsed or handled.

## Technology Stack and Repository Layout

| Path | Purpose |
|---|---|
| `CMakeLists.txt` | Root CMake project; selects C99, enables CTest, and adds `c-core`. |
| `c-core/include/` | Public types, protocol constants, structures, and module APIs. |
| `c-core/src/` | Header, Message, Payload, Routing Activation, Gateway, and Windows networking implementations. |
| `c-core/tests/` | Protocol-module unit tests; Windows builds also add TCP Server and Tester network tests. |
| `scripts/demo-doip.ps1` | Windows PowerShell demo that starts the Server, runs the Tester, and cleans up the Server process it started. |
| `docs/PROJECT_INTERVIEW_NOTES.md` | Existing interview notes; this technical guide is a deeper learning resource. |
| `docs/test-results/` | Existing loopback/LAN captures and screenshots. Their presence does not mean they were re-run or verified during this documentation task. |
| `build/` | Existing CMake/Ninja build artifacts. They are not source code and do not establish that the current sources passed tests. |

The project requires CMake 3.16 or later and C99. GNU/Clang targets enable `-Wall -Wextra -Wpedantic`. Windows networking targets link `ws2_32`. The root configuration adds the networking sources, executables, and tests under a `WIN32` condition.

## Running the Project

From PowerShell at the repository root:

```powershell
cmake -S . -B build
cmake --build build
ctest --test-dir build -V
```

On Windows, run the two executables in separate terminals:

```powershell
.\build\c-core\doip_tcp_server.exe 127.0.0.1 13400
.\build\c-core\doip_tester.exe 127.0.0.1 13400 0x0E00 0x1000
```

You can also run `.\scripts\demo-doip.ps1` in Windows PowerShell. The Server defaults to `127.0.0.1:13400`; the Tester uses the same destination and defaults to Tester address `0x0E00` and Entity address `0x1000`. The Server accepts an IPv4 address and port as command-line arguments; the Tester accepts a destination IPv4 address, port, Tester address, and expected Entity address.

Binding the Server to `0.0.0.0` listens on all local IPv4 interfaces. For communication between two computers, point the Tester at the Server’s reachable LAN IPv4 address and ensure routing and firewall rules allow the selected TCP port (13400 by default). `127.0.0.1` is each host’s own loopback address, not the address of another computer.

## Recommended Learning Order

1. Read this overview and `01_SYSTEM_ARCHITECTURE.md` to understand component boundaries.
2. Read `02_DOIP_PROTOCOL_AND_MESSAGE_FORMAT.md` and separate wire bytes from C structures.
3. Study `03_PROTOCOL_CORE_IMPLEMENTATION.md`, then `04_ROUTING_ACTIVATION_IMPLEMENTATION.md`.
4. Read `05_TCP_SERVER_IMPLEMENTATION.md` and `06_TESTER_IMPLEMENTATION.md` for TCP framing, timeouts, and resource ownership.
5. Trace a request end to end using `07_DATA_FLOW_AND_FUNCTION_CALLS.md`.
6. Finish with error boundaries, build/test guidance, trade-offs, limitations, and interview questions; then explain the flow while looking at the source.

## Questions You Should Be Able to Answer

- Which DoIP exchange does this project actually implement, and which named features are not implemented?
- Why is this not a complete DoIP stack or a real ECU diagnostic tool?
- How does a Routing Activation Request travel from the Tester through the Gateway and back as a Response?
- Which components are platform-independent and which explicitly depend on Windows?
- What do the single-client state, local 4096-byte limit, and lack of diagnostic forwarding mean?

## Key Takeaways

- This is a **simplified Routing Activation demonstration**, not a complete DoIP implementation.
- The TCP Server and Tester use Windows Winsock; the protocol core is organized as C99 modules.
- The Gateway has no ECU connection; the Tester validates only a Routing Activation Response.
- `127.0.0.1` is for local loopback. A LAN test needs a reachable server interface address.
