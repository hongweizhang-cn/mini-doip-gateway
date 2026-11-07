# 06 Tester Implementation

## Learning Objectives

Explain the Tester’s scope, command-line arguments, nonblocking connection and timed I/O, and the conditions it checks before reporting a successful Routing Activation test.

## What the Tester Is—and Is Not

`doip_tester.c` is a Windows Winsock test client. It builds one basic Routing Activation Request, connects to an IPv4 TCP server, receives one Response, and validates it. It is not a full automotive Diagnostic Tester: it does not implement UDP Vehicle Discovery, Alive Check, Diagnostic Messages, DTC read/clear operations, ECU addressing configuration, or conformance validation.

## Command-Line Entry Point and Defaults

`doip_tester_main.c` uses these arguments:

| Position | Default | Validation |
|---|---|---|
| `server-ipv4` | `127.0.0.1` | A nonempty argument is passed to the Tester; the connection function requires numeric IPv4. |
| `port` | `13400` | Decimal 1–65535; `main` rejects zero. |
| `tester-logical-address` | `0x0E00` | `strtoul` base 0; accepts decimal or `0x` hexadecimal, range 0–65535. |
| `entity-logical-address` | `0x1000` | Same parsing and range. |

`-h` / `--help` displays usage only when it is the sole argument. `doip_tester_run` manages its own Winsock reference with `WSAStartup` and `WSACleanup`.

## Connection, Request Transmission, and Response Reception

### Connecting

`connect_with_timeout` creates an `AF_INET/SOCK_STREAM/IPPROTO_TCP` socket, validates the numeric IPv4 address, and makes the socket nonblocking before calling `connect`. If the connection is in progress, it waits up to 3 seconds on a write set with `select`, then uses `getsockopt(SO_ERROR)` to determine the connection result. The socket remains nonblocking after success.

### Building the Request

The Tester creates:

- `doip_routing_activation_request_t`: the command-line Tester address and fixed `activation_type = 0x01`.
- A 7-byte Request Body using `doip_routing_activation_request_encode`.
- A `doip_header_t` with version `02 FD`, Request type `0x0005`, and Payload Length 7.
- A 15-byte complete message using `doip_message_encode`.

`send_all` uses one 5-second deadline for the entire send operation. It waits for writability with `select` and loops over partial `send()` results. `WSAEWOULDBLOCK` and `WSAEINTR` are retried. Other errors, zero progress, or deadline expiration fail the operation.

### Receiving and Validating the Response

1. `receive_exact` reads all 8 Response Header bytes within its own 5-second deadline.
2. `doip_header_decode` validates the version, and the Tester requires Payload Type `0x0006`.
3. A Payload Length above the local 4096-byte limit returns `BUFFER_TOO_SMALL`; a length other than the basic 9-byte Response Body returns `INVALID_LENGTH`.
4. A second `receive_exact` call uses a new 5-second deadline to read the Body.
5. `doip_routing_activation_response_decode` decodes addresses and Response Code.
6. The Tester requires the response client address to match the requested Tester address and the Entity address to match the expected value. A mismatch returns `INVALID_HEADER`.
7. The call returns `OK` only for `DOIP_ROUTING_ACTIVATION_RESPONSE_CODE_SUCCESS` (`0x10`); any other code returns `UNSUPPORTED`.

Immediately after decoding the Body, the Tester copies it into `*response`. Thus the caller can inspect a decoded Body even if address or Response Code validation later fails; the header documents that a rejection code is retained. The output’s contents on earlier failures depend on how far processing progressed.

## Exact Timeout Scope

- **Connect:** waits for at most 3 seconds.
- **Complete Request send:** one 5-second deadline, not 5 seconds per `send()`.
- **Response Header read:** up to 5 seconds.
- **Response Body read:** another independent 5-second deadline.

Therefore, a 5-second limit per read stage does not mean the whole transaction has a fixed 5-second deadline from connection start. `wait_for_socket` uses `GetTickCount()` to calculate the remaining time for each `select`; each `receive_exact` call records its own start time.

## Cleanup and Error Classification

After connecting, all paths reach the `cleanup` label, which shuts down/closes the socket and calls `WSACleanup`. Connection, I/O, and Winsock-startup failures commonly map to `DOIP_RESULT_UNSUPPORTED`; a peer closing before an exact read completes maps to `INVALID_LENGTH`. Invalid version, bad length, insufficient capacity, and invalid arguments retain their respective results. The function also prints stage-specific messages to stderr.

`main` maps anything other than `DOIP_RESULT_OK` to `EXIT_FAILURE`. A successful command therefore means this demonstration exchange passed its connection, basic message, address, and success-code checks; it does not establish broader protocol conformance.

## Evidence from the Tests

`test_doip_tester.c` uses a real local Gateway TCP Server for an end-to-end case and fake TCP peers for failure responses. The cases cover:

- A successful exchange and a non-success Response Code.
- Invalid Header, Payload above 4096, and an incorrect fixed Body length.
- Premature close during Header/Body, wrong Payload Type, and mismatched addresses.
- A slow Header timeout with an elapsed-time bound, and connection failure when no server is listening.

These are test cases present in the source; CTest was not run while preparing this documentation.

## Questions You Should Be Able to Answer

- What independent conditions determine Tester success?
- Why does a successful TCP connection not imply successful Routing Activation?
- What do the 3-second and 5-second timeouts cover? Why can total time exceed 5 seconds?
- Does the Tester validate the standard semantics of every possible Response Code?
- If the Body is valid but has an unknown Response Code, what do the Body Codec and Tester do?
- What capabilities would a real diagnostic Tester need that this project lacks?

## Key Takeaways

- The Tester supports one basic Routing Activation round trip; it is not a general diagnostic client.
- The nonblocking connection has a 3-second wait; send and each exact-read stage have separate 5-second limits.
- The Response Codec preserves the Response Code; the Tester accepts only `0x10` and checks both addresses.
- Complete-message length, Payload Type, Body format, and business success code are separate validation layers.
