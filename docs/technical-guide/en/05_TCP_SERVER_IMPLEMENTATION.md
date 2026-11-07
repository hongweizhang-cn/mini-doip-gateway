# 05 Windows TCP Server Implementation

## Learning Objectives

Trace the Server from `main` through Winsock socket creation, listening, accepting a client, TCP framing, Gateway processing, response transmission, and cleanup. Understand its timeout and resource-lifecycle boundaries.

## Platform and Entry Point

The Server is built only on Windows. Defaults in `doip_tcp_server_main.c` are:

| Setting | Default | Argument |
|---|---|---|
| Bind Address | `127.0.0.1` | First positional argument; numeric IPv4. |
| TCP Port | `13400` | Second positional argument, decimal `0..65535`; zero requests an OS-assigned port. |
| Entity Logical Address | `0x1000` | Constant in `main`; not a command-line argument. |
| Conflict Response Code | `0x03` | Constant in `main`; not a command-line argument. |

`doip_tcp_server_init` requires a nonempty address string shorter than 16 bytes and copies it into `doip_tcp_server_t.bind_address`; it does not parse the IP. `doip_tcp_server_start` later uses `InetPtonA(AF_INET, ...)` to parse a numeric IPv4 address. Hostnames and IPv6 are not supported.

### Localhost and LAN Use

- `127.0.0.1` refers to the local loopback interface. A Tester on another computer uses its own loopback address and cannot reach this Server through it.
- For a two-computer LAN demo, bind to the Server’s LAN IPv4 or to `0.0.0.0` to listen on all local IPv4 interfaces. The Tester must connect to the Server’s reachable LAN IPv4, not to `0.0.0.0`.
- Ensure routing is available, the firewall permits TCP on the selected port (13400 by default), the port is not already in use, and the Tester is also the Windows build. Binding to `0.0.0.0` exposes the listener on all local IPv4 interfaces, so use it only when appropriate for the network.

## Startup: `doip_tcp_server_start`

1. Validate `server != NULL` and that `lifecycle_state == STOPPED`.
2. Call `WSAStartup(MAKEWORD(2,2), ...)` and record `winsock_started`.
3. Convert the configured numeric IPv4 address to `sockaddr_in`; convert the port with `htons`.
4. Create an IPv4 TCP socket with `socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)`.
5. Set `SO_EXCLUSIVEADDRUSE`, then call `bind` and `listen(SOMAXCONN)`.
6. Make the listening socket nonblocking using `ioctlsocket` and `FIONBIO`.
7. Call `getsockname` to obtain the actual bound port; this supports querying an OS-assigned port when configuration used zero.
8. Clear the stop flag, set LISTENING state, and print the endpoint.

On Winsock startup, socket, option, bind, listen, or query failure, the code records available error information, releases resources already acquired, and returns an error. Many such failures map to `UNSUPPORTED`; an invalid numeric IPv4 address maps to `INVALID_ARGUMENT`.

## TCP Stream Framing

TCP is an ordered byte stream and does not preserve `send()` call boundaries. One `recv()` may return only part of a Header, several pieces, or data spanning application writes. A single `recv()` must not be treated as one complete DoIP message.

On each iteration, `process_client_messages`:

1. Calls `receive_exact(..., DOIP_GENERIC_HEADER_SIZE)` to accumulate 8 Header bytes.
2. Calls `doip_header_decode` to validate version `0x02` and inverse `0xFD`, and to parse Type and Length.
3. Compares `decoded_header.payload_length` with `DOIP_TCP_SERVER_MAX_PAYLOAD_SIZE` (4096). An oversized value is rejected before reading the Body.
4. Computes `8 + payload_length` and calls `receive_exact` again to accumulate the Body at `input_message + 8`.
5. Passes the exact complete-message length to `doip_gateway_process_message`.
6. If Gateway processing succeeds, calls `send_all` for `output_message_size` bytes.

Both stack buffers are sized to `8 + 4096` bytes. This is a local resource limit, not a claim about a general DoIP maximum Payload Length.

## `wait_for_socket`, `receive_exact`, and `send_all`

- All Server sockets are nonblocking. `wait_for_socket` uses `select` to wait for readability or writability, with waits of up to 100 ms. On a timeout it loops and checks `stop_requested`. It retries `WSAEINTR`; other `select` errors are saved in `last_socket_error` and reported as failures.
- `receive_exact` accumulates positive `recv()` results until the requested byte count is reached. Zero means the peer disconnected. On a negative result, `WSAEWOULDBLOCK` and `WSAEINTR` are retried; other errors fail.
- `send_all` accumulates positive `send()` results until all bytes are sent. Zero means no progress. On a negative result, `WSAEWOULDBLOCK` and `WSAEINTR` are retried; other errors fail.

The Server has no total read deadline for a Header or Body. The 100 ms interval is a stop-check polling interval, not a network timeout. If a client sends only part of a Header or Body and keeps the connection open, the Server may wait indefinitely until more data arrives, the peer disconnects, a socket error occurs, or a stop is requested.

## Client Handling and State Lifetime

`doip_tcp_server_run` atomically transitions the lifecycle from LISTENING to RUNNING, waits for the listening socket to become readable, and calls `accept`. It handles one client at a time; it does not accept/process other clients concurrently while the current client is being served.

The accepted client is also made nonblocking. After the connection ends, a message is rejected, or a socket error occurs, the Server closes `client_socket`, calls `doip_gateway_reset_routing_activation`, and returns to the accept loop. A fatal listening-socket error causes resource cleanup, transitions to STOPPED, and returns an error.

## Shutdown and Resource Ownership

- `doip_tcp_server_main.c` registers `console_control_handler`. For Ctrl+C, Ctrl+Break, or console close, it calls `doip_tcp_server_request_stop`. This API only sets the atomic stop flag; it does not clean up sockets, making it suitable for the active service loop.
- When `doip_tcp_server_run` exits, it calls `release_server_resources`, which closes client/listening sockets and calls `WSACleanup` when needed.
- `doip_tcp_server_stop` requests a stop. If the state is still LISTENING and `run` has not started, it releases resources immediately. If RUNNING, it does not wait; `run` performs cleanup.
- `main` unregisters the handler and maps the result to the process exit code.

`doip_tcp_server_t` stores the bind address, configured/bound ports, socket handles, stop/lifecycle state, Winsock status, last error, and `doip_gateway_t`. The header warns against calling init/start concurrently.

## Relationship Between the Server and Gateway

The Transport does not interpret Routing Activation business fields. It ensures that one complete DoIP message is available within the local size limit, then passes it to the Gateway. The Gateway returns a complete encoded message; the Transport sends those bytes without rebuilding the Header. A Gateway error ends the current client connection. The Server does not generate a generic DoIP error Response.

## Error Handling and Test Coverage

Windows `test_doip_tcp_server.c` covers a fragmented Request producing a complete Response, short Header/Body followed by disconnect, invalid protocol version/inverse, a Payload Length above 4096, accepting a new client after invalid input, and Server stop/lifecycle behavior. It does not inject every Winsock error, nor does it test a client that holds a partial frame open until a timeout—there is no such Server deadline.

## Questions You Should Be Able to Answer

- Why does the Server read 8 bytes first and then read the Body according to Payload Length?
- How should `recv()` results of zero, a positive number, and `SOCKET_ERROR` be interpreted?
- Is the 100 ms `select` interval a protocol read timeout?
- Why must an oversized Payload Length be rejected before reading the Body?
- Why does the Server reset Gateway state after a client disconnects?
- On a LAN, why might the Server bind `0.0.0.0` while the client connects to a specific IPv4 address?

## Key Takeaways

- TCP send/receive operations must accumulate actual byte counts; one `recv` or `send` is not guaranteed to complete a frame.
- The Server’s 4096-byte limit is local to its fixed buffers; it has no application-level frame read deadline.
- The Server is synchronous and single-client; it resets state after client processing and returns to `accept`.
- The Transport owns socket/Winsock lifetime; the Gateway handles complete messages.
