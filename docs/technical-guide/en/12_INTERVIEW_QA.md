# 12 Technical Interview Q&A

## Learning Objectives

Answer project questions from source evidence rather than generic textbook definitions. Start with the implementation, then explain its boundary and likely follow-up. Do not exaggerate features that are not present.

## 1. Overall Project Architecture

### Q1. How would you describe this project in a few sentences?

**Answer:** This is a C99 learning project for a basic DoIP Routing Activation exchange. A Windows Tester sends a Request over TCP. A single-client Winsock Server frames the TCP stream and passes a complete message to the Gateway. The Gateway applies a simplified state policy and creates a Response. It does not connect to an ECU or forward diagnostic messages, and it is not a complete DoIP stack.

**Code References:** `c-core/src/doip_tester.c::doip_tester_run`; `c-core/src/doip_tcp_server.c::process_client_messages`; `c-core/src/doip_gateway.c::doip_gateway_process_message`.

**Possible Follow-up:** What problem does the project demonstrate?

**Answering Approach:** Discuss Header/Body serialization, TCP framing, and a small state transition. Do not claim that it provides real vehicle diagnostic routing.

### Q2. How are the Protocol Core and Transport Layer separated?

**Answer:** The Server passes a complete DoIP message to the Gateway, which does not know about sockets. The Transport owns socket operations, connection handling, and byte-stream framing. The Core handles Header/Body encoding, validation, state, and Response bytes.

**Code References:** `doip_tcp_server.c::process_client_messages` calls `doip_gateway_process_message`; `doip_message.c` and `doip_gateway.c` do not depend on Winsock.

**Possible Follow-up:** Why not pass a `SOCKET` to the Gateway?

**Answering Approach:** That would couple protocol processing to platform I/O and make direct unit testing harder. The trade-off is that the Transport must frame the stream first.

### Q3. Is this a real ECU diagnostic Gateway?

**Answer:** No. `doip_gateway_t` holds one Entity logical address, a conflict code, and one simplified Routing Activation context. There is no ECU adapter, Diagnostic Message processing, or forwarding module in the repository.

**Code References:** `doip_gateway.h::doip_gateway_t`; `doip_gateway.c`.

**Possible Follow-up:** What would real diagnostic forwarding require?

**Answering Approach:** A Diagnostic Message Codec and dispatch path, destination routing and an ECU Transport Adapter, request/response correlation, timeout/error propagation, and corresponding tests.

### Q4. What structural problems would arise when adding concurrent clients?

**Answer:** The current Server processes one client synchronously, and the Gateway has one logical-address state. We would need to define whether state belongs to a connection or a logical route, add connection objects and isolated lifetimes, and address concurrency and resource limits.

**Code References:** `doip_tcp_server.c::doip_tcp_server_run`; `doip_tcp_server.h::doip_tcp_server_t`; `doip_routing_activation_state.h::doip_routing_activation_context_t`.

**Possible Follow-up:** Would putting `accept` in multiple threads be enough?

**Answering Approach:** No. A shared single Gateway context could race or mix clients, and one connection’s reset could clear another connection’s state. Ownership must be redesigned.

## 2. DoIP Protocol

### Q5. What fields are in the Generic Header, and how long is it?

**Answer:** It is 8 bytes: 1 byte Protocol Version, 1 byte Inverse Protocol Version, 2 bytes Payload Type, and 4 bytes Payload Length. The length counts bytes after the Header.

**Code References:** `doip_header.h::DOIP_GENERIC_HEADER_SIZE`; `doip_header.c::doip_header_encode/decode`.

**Possible Follow-up:** How many bytes are in a complete Request with `payload_length = 7`?

**Answering Approach:** 8 + 7 = 15 bytes.

### Q6. Why does the current code accept only `02 FD`?

**Answer:** `DOIP_PROTOCOL_VERSION` and `DOIP_PROTOCOL_VERSION_INVERSE` are fixed at `0x02` and `0xFD`. Both Header encoding and decoding enforce this pair. This defines the project’s supported range; it is not a version-negotiation mechanism.

**Code References:** `doip_header.h`; `doip_header.c::doip_header_decode`.

**Possible Follow-up:** What does a bad inverse return?

**Answering Approach:** `DOIP_RESULT_INVALID_HEADER`. Header unit tests and the Server integration test cover invalid combinations.

### Q7. What are the seven bytes of the Request Body?

**Answer:** A 2-byte big-endian Source Address (the Tester Logical Address), a 1-byte Activation Type, and 4 Reserved bytes. The Request Codec writes the Reserved bytes as zero and requires them to be zero when decoding.

**Code References:** `doip_routing_activation_request.c::doip_routing_activation_request_encode/decode`.

**Possible Follow-up:** How does Activation Type affect the current state policy?

**Answering Approach:** It is decoded and stored in the Request structure, but `doip_routing_activation_state_process_request` never reads it.

### Q8. Why are Vehicle Identification constants not evidence that discovery is implemented?

**Answer:** `doip_payload_type.h` defines numeric values only. The Gateway accepts only Routing Activation Request, and the repository has no UDP socket, Vehicle Identification Body Codec, or Announcement handler.

**Code References:** `doip_payload_type.h`; `doip_gateway.c::doip_gateway_process_message`; `c-core/CMakeLists.txt`.

**Possible Follow-up:** What else is needed after adding a constant?

**Answering Approach:** Define and validate the Body format, add dispatch and Transport behavior, specify responses/errors, and add tests.

## 3. C99 Coding and Memory Management

### Q9. Why not send `doip_header_t` directly over TCP?

**Answer:** A C structure may contain padding, have alignment constraints, and use host byte order. The wire Header has a fixed 8-byte big-endian layout, so the Codec serializes fields explicitly.

**Code References:** Comment in `doip_header.h`; `doip_header.c`.

**Possible Follow-up:** If `sizeof(struct)` happens to be 8 on one machine, is direct copying safe?

**Answering Approach:** No. That does not guarantee ABI, compiler, or endianness compatibility. The wire format must be independent of structure layout.

### Q10. What are the benefits and limitations of explicit Buffer Capacity parameters?

**Answer:** An Encoder can compare the declared capacity before writing and return `BUFFER_TOO_SMALL`. It cannot verify that the caller’s capacity matches the actual object size, nor can it prove that a pointer is valid.

**Code References:** Header, Message, and Body Encoder APIs.

**Possible Follow-up:** Does every Codec enforce the 4096-byte limit?

**Answering Approach:** No. The Server and Tester have local limits; the generic Message Codec checks the caller’s capacity and the `uint32_t` length.

### Q11. Does `doip_message_view_t` own or borrow its payload?

**Answer:** It borrows it. `payload` points just after the Header in the original input buffer; no allocation or copy occurs. The view becomes invalid when the input buffer is released or reused.

**Code References:** `doip_message.h::doip_message_view_t`; `doip_message.c::doip_message_decode`.

**Possible Follow-up:** Why is this safe in the current Gateway path?

**Answering Approach:** The call is synchronous and the input array remains alive while the Request Body is decoded into a separate structure. An asynchronous queue would need a copy or transferred ownership.

### Q12. Can a capacity failure partially write the output?

**Answer:** `doip_message_encode` performs basic length and total-capacity checks before calling the Header Encoder. Body Codecs also check capacity before writing. The Gateway checks for 17 Response bytes before changing state. Tests cover buffer preservation and unchanged state in specific failure paths.

**Code References:** `doip_message.c::doip_message_encode`; `doip_gateway.c::doip_gateway_process_message`; `test_doip_gateway.c::test_capacity_check_precedes_state_change`.

**Possible Follow-up:** Do all APIs provide the same strong guarantee?

**Answering Approach:** Do not generalize. Check each function’s validation order and follow its ownership and non-overlap requirements.

## 4. Header and Payload Codecs

### Q13. How does the Header Codec encode a 32-bit length?

**Answer:** It shifts right by 24, 16, 8, and 0 bits, masks with `0xFF`, and writes bytes at indexes 4 through 7. Decoding shifts the bytes back and combines them with bitwise OR.

**Code References:** `doip_header.c::doip_header_encode/decode`.

**Possible Follow-up:** How is `0x00000007` represented on the wire?

**Answering Approach:** `00 00 00 07`.

### Q14. Does the Header Decoder reject an unknown Payload Type?

**Answer:** No. The Header Decoder validates the fixed version and parses Type/Length. The Message Decoder also does not interpret Type. The Gateway or Tester applies the expected Type at a higher layer.

**Code References:** `doip_header.c::doip_header_decode`; `doip_message.c::doip_message_decode`; `doip_gateway.c`.

**Possible Follow-up:** What is the benefit of that separation?

**Answering Approach:** Header syntax parsing stays separate from payload semantics, and the generic Message Codec can represent any 16-bit Type.

### Q15. How does `doip_message_decode` distinguish truncation from trailing bytes?

**Answer:** If the available bytes after the Header are fewer than the declared Payload Length, it returns `BUFFER_TOO_SMALL`. If there are more, it returns `INVALID_LENGTH`. It succeeds only when the lengths match.

**Code References:** `doip_message.c::doip_message_decode`.

**Possible Follow-up:** Why does it not accept several concatenated TCP messages?

**Answering Approach:** Its contract is to decode exactly one message. The TCP layer uses the Header Length to extract one frame at a time.

### Q16. How does the Response Decoder handle a 13-byte Body?

**Answer:** After requiring at least 9 bytes, it treats exactly 13 bytes as `UNSUPPORTED`; other lengths that are not 9 return `INVALID_LENGTH`. It does not parse the additional four bytes.

**Code References:** `doip_routing_activation_response.c::doip_routing_activation_response_decode`.

**Possible Follow-up:** Does the Codec reject an unknown Response Code?

**Answering Approach:** No. It preserves the code; the Tester later accepts only `0x10`.

## 5. Routing Activation

### Q17. How does the first request activate the project’s state?

**Answer:** If the state is INACTIVE, the function saves the Request’s `source_address`, changes the state to ACTIVE, and returns success code `0x10`.

**Code References:** `doip_routing_activation_state.c::doip_routing_activation_state_process_request`.

**Possible Follow-up:** What Request checks happen before the state update?

**Answering Approach:** The Gateway has already validated the Message, Payload Type, 7-byte Body, and Reserved bytes. The state module itself does not validate Activation Type or address policy.

### Q18. What is the difference between a same-address and different-address retry?

**Answer:** While active, a request from the same address is idempotently successful with `0x10`. A different address receives the configured conflict code, and the current active address is preserved.

**Code References:** `doip_routing_activation_state.c`; tests `test_same_client_reactivation_is_idempotent` and `test_different_client_rejected_without_overwrite`.

**Possible Follow-up:** Where does `0x03` come from?

**Answering Approach:** It is the Server main function’s default configuration passed into the Gateway. The State API receives the code as a parameter and does not claim it is universal standard policy.

### Q19. Can insufficient output capacity activate the state and then fail?

**Answer:** No. The Gateway checks `output_message_capacity >= 17` before calling State Process. Otherwise it returns `BUFFER_TOO_SMALL` without activating the context.

**Code References:** `doip_gateway.c::doip_gateway_process_message`.

**Possible Follow-up:** Why is the threshold 17?

**Answering Approach:** The current basic Response is an 8-byte Header plus a 9-byte Body; the Gateway calculates this with a fixed-size macro.

### Q20. What is the relationship between a TCP connection and Routing Activation?

**Answer:** TCP Connect establishes transport only. A complete Request must reach the Gateway and be processed before a Routing Activation Response is produced. The Tester validates the Response to decide whether activation succeeded.

**Code References:** `doip_tcp_server.c::doip_tcp_server_run`; `doip_gateway.c`; `doip_tester.c::doip_tester_run`.

**Possible Follow-up:** What happens to state when the Server closes a client?

**Answering Approach:** After each client’s processing ends, the Server calls `doip_gateway_reset_routing_activation`, clearing the single context.

## 6. TCP and Winsock

### Q21. What is the Server startup call sequence?

**Answer:** `WSAStartup`, numeric IPv4 parsing, `socket`, `SO_EXCLUSIVEADDRUSE`, `bind`, `listen`, nonblocking `ioctlsocket`, and `getsockname`.

**Code References:** `doip_tcp_server.c::doip_tcp_server_start`.

**Possible Follow-up:** Why use port zero?

**Answering Approach:** The OS assigns an available port, and `getsockname` stores it in `bound_port`. Tests use dynamic ports to avoid collisions.

### Q22. Why does the Server limit Payload Length to 4096?

**Answer:** Its input and output arrays are statically sized to `8 + 4096` bytes, so it rejects a larger declaration before reading to avoid overrunning those buffers. This is a local resource limit, not a general DoIP maximum.

**Code References:** `doip_tcp_server.h::DOIP_TCP_SERVER_MAX_PAYLOAD_SIZE`; `doip_tcp_server.c::process_client_messages`.

**Possible Follow-up:** Would increasing the macro alone support arbitrary messages?

**Answering Approach:** No. Consider stack usage, Tester/Codec limits, resource policy, message formats, and tests.

### Q23. Why does the Server bind to `127.0.0.1` by default?

**Answer:** The explicit default in `main` limits the demo to a local loopback connection. For LAN testing, bind to the Server’s LAN IP or `0.0.0.0`; the Tester should connect to a reachable, specific Server address.

**Code References:** `doip_tcp_server_main.c::DEFAULT_BIND_ADDRESS`; default in `doip_tester_main.c`.

**Possible Follow-up:** Should the Tester connect to `0.0.0.0`?

**Answering Approach:** No. That is a wildcard bind address for the Server; a client should connect to the Server’s actual reachable interface address.

### Q24. Who owns Winsock and socket cleanup?

**Answer:** Server `release_server_resources` closes the client/listening sockets and balances its Winsock startup with `WSACleanup`. The Tester’s cleanup label closes its socket and calls `WSACleanup`. Callers should not release Server-owned handles a second time.

**Code References:** `doip_tcp_server.c::release_server_resources`; `doip_tester.c::doip_tester_run`.

**Possible Follow-up:** How does Ctrl+C notify an active Server?

**Answering Approach:** The console handler calls `doip_tcp_server_request_stop` to set the stop flag. The 100 ms `select` polling lets the run loop observe it; `run` performs cleanup.

## 7. Partial Read and Partial Write

### Q25. Why might one `recv()` not return a complete Header?

**Answer:** TCP is a byte stream, and its segment boundaries do not match application I/O calls. Even an 8-byte Header sent in one call may arrive through multiple receives.

**Code References:** `doip_tcp_server.c::receive_exact`; `doip_tester.c::receive_exact`.

**Possible Follow-up:** When can the Server parse Payload Length?

**Answering Approach:** After all 8 Header bytes have been accumulated.

### Q26. How does `receive_exact` handle a partial read?

**Answer:** It initializes `received_total` to zero, asks `recv` for the remaining range, and adds each positive result until `byte_count` is reached. On nonblocking “would block” or interruption it retries.

**Code References:** Both `receive_exact` implementations; the Server also uses `wait_for_socket`.

**Possible Follow-up:** What if `recv` returns zero?

**Answering Approach:** The peer closed the connection before the requested bytes arrived. The Server ends that client; the Tester reports an incomplete response length.

### Q27. What problem does `send_all` solve?

**Answer:** `send` may return a positive count smaller than the requested length. The loop advances by the number already sent and succeeds only when every byte has been written.

**Code References:** `doip_tcp_server.c::send_all`; `doip_tester.c::send_all`.

**Possible Follow-up:** Do tests force every `send` to be partial?

**Answering Approach:** Network tests exercise fragmented requests/responses and complete reads, but that does not prove the OS returned a partial write in every run. The code handles that branch with its loop.

### Q28. Is the Server’s 100 ms wait a data timeout?

**Answer:** No. It is the `select` polling interval used to check the stop flag. There is no total frame-read deadline, so a client that holds a partial frame open can occupy the single-client loop indefinitely.

**Code References:** `DOIP_TCP_SERVER_POLL_INTERVAL_MS`; `wait_for_socket`; `receive_exact`.

**Possible Follow-up:** How would you improve this?

**Answering Approach:** Add an explicit per-frame or per-connection deadline and test timeout, stop, disconnect, and partial-data behavior.

## 8. Error Handling

### Q29. How does an invalid Header version propagate through the Server?

**Answer:** The Server reads the 8-byte Header and calls `doip_header_decode` directly before reading the Body or calling the Gateway. If the version pair is invalid, the decoder returns `INVALID_HEADER`; the Server logs the rejection and ends the current client. It then closes that client socket and resets the Routing Activation state.

**Code References:** `doip_tcp_server.c::process_client_messages`; `doip_header.c::doip_header_decode`; `doip_tcp_server.c::doip_tcp_server_run`.

**Possible Follow-up:** Does the Server return a protocol-error Response?

**Answering Approach:** No. The invalid Header is rejected at the Transport/Server boundary, and the current implementation closes the connection without generating a protocol-error Response.

### Q30. How are a truncated Payload and an oversized Payload distinguished?

**Answer:** After reading the Header, the Server first checks its local 4096-byte limit. If within the limit, it exact-reads the Body. A peer close before completion ends the connection. In the standalone Message Decoder, an input buffer shorter than the declared message returns `BUFFER_TOO_SMALL`.

**Code References:** `doip_tcp_server.c::process_client_messages`; `doip_message.c::doip_message_decode`.

**Possible Follow-up:** What if the input contains trailing bytes?

**Answering Approach:** Message Decode returns `INVALID_LENGTH`; its contract is exactly one complete message.

### Q31. Can an invalid message modify Gateway state?

**Answer:** The Gateway decodes and validates the message, checks Payload Type and Body, and checks output capacity before calling State Process. Those earlier failures do not change state. The Server resets state when client processing ends.

**Code References:** `doip_gateway.c::doip_gateway_process_message`.

**Possible Follow-up:** Which test explicitly checks the capacity case?

**Answering Approach:** `test_doip_gateway.c::test_capacity_check_precedes_state_change`.

### Q32. Why do Winsock failures often map to `UNSUPPORTED`?

**Answer:** `doip_result_t` is a coarse shared enum. The network modules separately log or record some specific Winsock errors in `last_socket_error`. This mapping limits diagnostic precision.

**Code References:** `doip_types.h`; `doip_tcp_server.c`; `doip_tester.c`.

**Possible Follow-up:** How could error observability be improved?

**Answering Approach:** Return structured error context or platform codes while preserving testable APIs, rather than collapsing every I/O failure into one result.

## 9. Unit and Integration Testing

### Q33. What kinds of tests does the project have?

**Answer:** It has unit tests for the core Codecs, State, and Gateway. Windows builds additionally register TCP Server and Tester network tests. CMake gates those network targets on `WIN32`.

**Code References:** `c-core/tests/CMakeLists.txt`.

**Possible Follow-up:** Can the Server integration test run on non-Windows?

**Answering Approach:** Not with the current configuration; the Winsock targets and tests are not created there.

### Q34. How does the Tester integration test create malformed responses?

**Answer:** `test_doip_tester.c` starts a fake TCP peer and selects modes for invalid Header, oversized or wrong length, partial/slow Header, wrong Type, mismatched address, and non-success Response Code.

**Code References:** `fake_peer_mode_t`; `run_fake_peer`; `test_fake_response`.

**Possible Follow-up:** What does the timeout test prove?

**Answering Approach:** It checks that a deliberately slow response fails within a bounded time. It does not prove every network delay/error case is covered.

### Q35. Can I say the tests passed just because test source files exist?

**Answer:** No. Source files show that cases were written, and CMake shows registration rules. A passing result requires running CTest in the intended build and checking its exit status. CTest was not run for this documentation task.

**Code References:** `c-core/tests/CMakeLists.txt` and each test’s `main`.

**Possible Follow-up:** What should a test report include?

**Answering Approach:** Platform, Generator, compiler, registered tests, actual command, and observed result.

### Q36. Which framing failures are actually covered by the Server network test?

**Answer:** It covers a fragmented Request, short Header/Body followed by close, invalid version/inverse, Payload Length above 4096, and accepting a later client after invalid input. It does not test a held-open partial frame reaching a deadline, because the Server has no such deadline.

**Code References:** `test_doip_tcp_server.c::test_server_lifecycle_and_stream_handling`.

**Possible Follow-up:** How would you add a held-open test?

**Answering Approach:** Have a fake client send a partial Header/Body and keep the socket open; assert the intended deadline and cleanup behavior. The Server would first need a read timeout for such a test to succeed.

## 10. Design Trade-Offs and Future Extensions

### Q37. What are the benefits and costs of fixed buffers?

**Answer:** They avoid dynamic allocation and make bounds explicit, which helps review. The fixed 4096-byte cap can reject longer data and consumes stack space; it is not the standard’s general maximum.

**Code References:** `doip_tcp_server.h`; buffer definitions in Server/Tester sources.

**Possible Follow-up:** What checks would dynamic buffers require?

**Answering Approach:** Integer overflow, allocation failure, resource limits, cleanup on every path, and negative tests.

### Q38. How would you add Diagnostic Message forwarding?

**Answer:** Add Payload Type dispatch and a diagnostic processing path in the Gateway, a Diagnostic Payload Codec, and an ECU Transport Adapter. The Server should continue handling framing, while tests cover request/response correlation and failures end to end.

**Code References:** `doip_gateway.c` currently dispatches only Request; `doip_payload_type.h` has no Diagnostic Message type; no ECU module exists.

**Possible Follow-up:** Why not parse Diagnostic payloads directly in the TCP Server?

**Answering Approach:** That would couple transport framing with payload business logic. Keep the Server responsible for complete messages and the Protocol Core responsible for payload interpretation.

### Q39. Would Linux support require only replacing Winsock function names with POSIX calls?

**Answer:** No. Build conditions, error codes, nonblocking connect/select behavior, socket closure, console stop handling, and tests all depend on Windows APIs. A platform adapter and cross-platform tests are needed.

**Code References:** `WIN32` conditions in `c-core/CMakeLists.txt`; Windows API use in Server/Tester.

**Possible Follow-up:** Which parts could be reused unchanged?

**Answering Approach:** Header/Message/Body Codecs, Gateway, and State do not depend on the network APIs and can remain portable core modules.

### Q40. How can you describe the project’s incompleteness without undervaluing it?

**Answer:** Frame it as an educational Routing Activation path that demonstrates byte order, TCP framing, C buffer boundaries, state logic, and tests. Be explicit that UDP Discovery, diagnostic forwarding, multiple clients, and the full state machine are out of scope.

**Code References:** README Scope and Limitations and the current module list.

**Possible Follow-up:** What if asked whether it conforms to ISO 13400?

**Answering Approach:** Do not claim complete conformance or certification. State exactly which behavior is implemented and tested; full conformance would require specification traceability and independent validation.

## Questions You Should Be Able to Answer

- Can you pick one question and trace the real call chain at the source level?
- Can you show the Request/Response bytes and explain that Payload Length excludes the Header?
- Can you distinguish code behavior from test evidence?
- Can you explain missing standard features and the modules an extension would require?

## Key Takeaways

- These 40 questions cover architecture, protocol, C99, Codecs, state, Winsock, partial I/O, errors, tests, and extensions.
- Structure answers as “source fact → boundary/reason → trade-off/follow-up.”
- Do not describe expectations, historical build artifacts, or future work as current features or passing test results.
