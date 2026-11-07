# 09 Build, Test, and Debugging

## Learning Objectives

Explain how CMake configures targets by platform, build and run the project from the repository root, and use TCP/DoIP data to diagnose common demo failures.

## CMake Project Structure

- The root `CMakeLists.txt` requires CMake 3.16, enables only the C language, sets `CMAKE_C_STANDARD 99`, `CMAKE_C_STANDARD_REQUIRED ON`, and `CMAKE_C_EXTENSIONS OFF`, enables CTest, and adds `c-core`.
- `c-core/CMakeLists.txt` creates the static library `doip_core` from the Header, Gateway, Message, Payload, and Routing Activation protocol-core sources.
- Under `WIN32` only, it adds `doip_tcp_server.c` and `doip_tester.c` to the library, links `ws2_32`, and builds the `doip_tcp_server` and `doip_tester` executables.
- When `BUILD_TESTING` is enabled, CMake adds `c-core/tests`. Each protocol test has its own executable; Windows additionally registers the TCP Server and Tester network tests with CTest timeouts.
- GNU/Clang targets enable `-Wall -Wextra -Wpedantic`. The CMake files do not configure sanitizers, coverage, or warnings-as-errors.

The existing `build/CMakeCache.txt` in this repository records Ninja and an MSYS2 UCRT64 `gcc.exe` compiler. This describes the current cache only; it is not a requirement for every user and does not mean a build was run while preparing these documents.

## Build and Test on Windows

From PowerShell at the repository root:

```powershell
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

List tests registered by the current configuration:

```powershell
ctest --test-dir build -N
```

Run one test, such as the Message Codec test:

```powershell
ctest --test-dir build -R test_doip_message --output-on-failure
```

Show detailed test output:

```powershell
ctest --test-dir build -V
```

If installed and available on `PATH`, Ninja and GCC can be selected explicitly in a new build directory:

```powershell
cmake -S . -B build-ninja -G Ninja -DCMAKE_C_COMPILER=gcc
cmake --build build-ninja
ctest --test-dir build-ninja --output-on-failure
```

`build-ninja` is an example of a separate build directory. Once a CMake Generator is recorded in a build directory, use a different directory to select another Generator rather than trying to switch the existing cache. The commands above are instructions, not commands executed for this documentation task.

### Platform Differences

- **Windows:** builds the Protocol Core, the two Winsock executables, and available unit/network tests; `ws2_32` is a system library.
- **Non-Windows:** CMake omits the Winsock Server/Tester sources, executables, and network tests; it configures the Protocol Core and portable tests only. `scripts/demo-doip.ps1` is also a Windows PowerShell workflow.
- There is no POSIX socket adapter. A portable core library does not imply a portable TCP application.

## Test Layers

| Layer | Test file(s) | Main focus |
|---|---|---|
| Unit | `test_doip_header.c` | Header codec, version, capacity, and big-endian layout. |
| Unit | `test_doip_payload.c` | Length checks, byte copying, null pointers, and capacity. |
| Unit | `test_doip_message.c` | Complete-message lengths, Header/Body, truncation, and trailing data. |
| Unit | Request/Response Codec tests | Body fields, Reserved bytes, and length boundaries. |
| Unit | `test_doip_routing_activation_state.c` | State transitions and conflict policy for one context. |
| Unit/core integration | `test_doip_routing_activation.c`, `test_doip_gateway.c` | Complete-message processing, dispatch, state side effects, and Responses. |
| Windows network integration | `test_doip_tcp_server.c` | Dynamic port, fragmented Request, disconnects, invalid Header/length, and stop. |
| Windows network integration | `test_doip_tester.c` | Tester against a local Gateway Server and multiple fake-peer responses. |

CTest results describe only tests registered and executed by the configured build. A test case listed in these documents means it was found in the source; unless CTest was actually run, do not claim that it passed. Historical `Testing` files in a build directory do not prove that the current sources match the binaries that produced those results.

## PowerShell Demo

Build first, then run from the repository root:

```powershell
.\scripts\demo-doip.ps1
```

Choose a Server address and port:

```powershell
.\scripts\demo-doip.ps1 -ServerAddress 127.0.0.1 -Port 13400
```

The script validates that the address is numeric IPv4 and checks for both executables. It first probes whether the endpoint is already accepting connections, starts the Server, polls for readiness for up to 10 seconds, and runs the Tester with a 20-second wait. During cleanup it stops only the Server process started by this script. The script’s `finally` block uses `Kill()` on that child process; this differs from the Server’s own graceful Ctrl+C stop path.

## Debugging Workflow

1. Start `doip_tcp_server.exe` by itself and inspect the printed bind endpoint.
2. Run the Tester in another terminal. Compare its target, request-size, response-field, and exit-code output.
3. If it fails, check arguments/IP/port first, then Server Winsock errors and the Tester failure stage, then Header, Payload Length, Body Reserved bytes, addresses, or Response Code.
4. With Visual Studio or the VS Code C debugger, set breakpoints in `doip_tester_run`, `connect_with_timeout`, `send_all`, `receive_exact`, `process_client_messages`, `doip_gateway_process_message`, and `doip_routing_activation_state_process_request`. Inspect `received_total`, `sent_total`, `decoded_header.payload_length`, `activation_request`, `gateway.routing_activation`, and `output_message_size`.
5. The source does not configure structured logging, log levels, or a protocol packet dump. Use the existing `printf`/`fprintf` output and debugger memory views.

## Inspecting Traffic with Wireshark

The repository contains loopback/LAN captures and screenshots under `docs/test-results/`. To capture a new exchange:

1. Start capture on the interface used by the Tester and Server. A loopback scenario requires an interface capable of capturing local loopback traffic.
2. Start with display filter `tcp.port == 13400`, replacing the port if needed.
3. Select the connection, use `tcp.stream eq N` to focus on that stream, and use Follow TCP Stream to inspect the reassembled byte stream.
4. Verify the raw TCP payload against the 8-byte Generic Header. For example, `02 FD 00 05 00 00 00 07` should be followed by a 7-byte Request Body; the Response uses Payload Type `00 06` and length 9.
5. Automatic protocol dissection depends on Wireshark version, port mapping, and dissector settings. Cross-check raw bytes and TCP reassembly against the project’s actual layout rather than relying only on a GUI label.

A capture can show bytes and timing observed on the network; it cannot by itself prove every test passed or that the project meets all ISO 13400 requirements.

## Common Troubleshooting

| Symptom | First checks |
|---|---|
| CMake cannot find a compiler / Generator mismatch | Check installed C compiler and CMake Generator; do not mix Generators in an existing build cache. |
| `doip_tcp_server.exe` or Tester is missing | Confirm a Windows configuration, successful build, and output path `build\c-core`. |
| Bind/Connect fails | Check numeric IPv4, port, port ownership, bind interface, firewall, and LAN routing. |
| Tester connects but activation fails | Check Response Payload Type/Length, logical addresses, and Response Code; a code other than `0x10` is a Tester failure. |
| TCP wait does not end | The Tester has deadlines; the Server has no partial-frame read deadline. A client that sends a partial frame and keeps the connection open can occupy the single-client loop. |
| Network tests are absent from CTest | Check that the build was configured on Windows and inspect the registered list with `ctest -N`. |

## Questions You Should Be Able to Answer

- Why are the network tests registered only on Windows?
- How do protocol unit tests differ from TCP integration tests?
- When would you use `ctest -R` versus `ctest -V`?
- How can Wireshark reveal the Header and Body in a reassembled TCP stream?
- Can historical test logs in a build directory be reported as validation of the current source?

## Key Takeaways

- `WIN32` gates the Winsock sources, executables, and network tests.
- A test source file, CTest registration, and a passing test result are three different facts.
- The PowerShell Demo orchestrates processes; it is not another protocol implementation.
- Use TCP reassembly and raw bytes in Wireshark to verify the project’s message layout.
