# 08 Error Handling and Boundary Conditions

## Learning Objectives

Map malformed inputs to the detecting layer, return value, state side effect, and actual test evidence. Do not equate the presence of an error branch with a test that passed.

## Core API Boundary Matrix

| Case | Detection and condition | Result/action | State impact | Test evidence |
|---|---|---|---|---|
| Required null pointer | Argument checks in Codec/Gateway/State APIs | Usually `INVALID_ARGUMENT`; some outputs are initialized first | Gateway does not process state before argument validation; State rejects missing input/output | Header, Message, Payload, Request/Response, Routing Activation, Gateway, and State tests include null/argument cases. |
| Header shorter than 8 bytes | Capacity check in `doip_header_decode` / `doip_message_decode` | `BUFFER_TOO_SMALL` | Gateway does not reach state processing | Covered by `test_doip_header` and `test_doip_message`. |
| Invalid Protocol Version | `doip_header_decode`: `buffer[0] != 0x02` | `INVALID_HEADER`; Server closes current client | This Request does not update Gateway state; client cleanup resets the context | Header unit tests; Server integration case `unsupported_protocol_version`; Tester fake-peer case `invalid-header`. |
| Invalid Inverse Protocol Version | `doip_header_decode`: `buffer[1] != 0xFD` | `INVALID_HEADER`; Server closes current client | Same as above | Header unit tests; Server integration case `mismatched_version_inverse`. |
| Unsupported Payload Type | Gateway requires `0x0005`; Tester requires `0x0006` for the Response | Gateway returns `UNSUPPORTED`, then Server closes the client; Tester returns `UNSUPPORTED` | Gateway rejects before state processing | `test_doip_routing_activation.c::test_invalid_payload_type`; Tester fake peer `wrong-payload-type`. Server socket tests do not separately send a valid-version frame with a wrong Type. |
| Declared Payload longer than complete input | Message Decode: available bytes `< payload_length` | `BUFFER_TOO_SMALL` | Gateway state unchanged | `test_doip_message.c::test_decode_truncated_payload_preserves_output`; malformed lengths in Routing Activation tests. |
| Trailing bytes after one message | Message Decode: available bytes `> payload_length` | `INVALID_LENGTH` | Gateway state unchanged | `test_doip_message.c::test_decode_trailing_data_preserves_output`. |
| Header Payload Length above Server limit | Server compares against `> 4096` before reading the Body | Logs and closes the client without reading the Body | State resets when client processing ends | `test_doip_tcp_server.c` case `oversized_payload_length` (4097). |
| Request Body too short/long | Request Codec: below 7 / not exactly 7 | Short: `BUFFER_TOO_SMALL`; long: `UNSUPPORTED` | Gateway fails before state processing | Boundary/OEM-length cases in `test_doip_routing_activation_request.c` and malformed Body cases in Routing Activation tests. |
| Invalid Response Body length | Response Codec: below 9, exactly 13, other non-9 | Respectively `BUFFER_TOO_SMALL`, `UNSUPPORTED`, `INVALID_LENGTH` | Body Codec has no Gateway state | `test_doip_routing_activation_response.c::test_decode_length_boundaries_preserves_output`; Tester checks an 8-byte Body length. |
| Nonzero Reserved bytes | Request/Response Body Decoder checks Reserved bytes | `INVALID_HEADER` | Gateway Body validation fails before state update | Request `test_decode_reserved_nonzero`; Response `test_decode_reserved_nonzero_preserves_output`. |
| Insufficient output capacity | Message Codec or Gateway’s fixed 17-byte check | `BUFFER_TOO_SMALL` | Gateway checks before State and does not activate | Capacity tests in Message/Routing Activation/Gateway, especially `test_capacity_check_precedes_state_change`. |
| Missing output-size pointer | Gateway/RA requires `output_size` | `INVALID_ARGUMENT`; if non-null it is cleared first | Gateway does not process state | Corresponding Gateway/RA argument tests. |
| Invalid state enum | State Process/Get permits only INACTIVE or ACTIVE | `INVALID_ARGUMENT` | Context unchanged; Response Code cleared | `test_invalid_context_state_is_rejected`. |
| Different logical address requests activation | State is ACTIVE and address differs | State API returns `OK` with configured conflict code; a Response can still be generated | Existing active address remains unchanged | State `test_different_client_rejected_without_overwrite`; Gateway repeat/conflict tests. |
| Unknown Response Code | Response Codec does not enumerate codes; Tester accepts only `0x10` | Body decode succeeds; Tester returns `UNSUPPORTED` for non-`0x10` | No Server state decision is involved | Response Codec `test_decode_unknown_response_code`; Tester tests a valid rejection. |

## TCP / Winsock Boundary Matrix

| Case | Detection | Current behavior | State and test evidence |
|---|---|---|---|
| TCP Partial Read | Server/Tester `receive_exact` loops over positive `recv()` results until the requested count is reached | Retry `WSAEWOULDBLOCK` / `WSAEINTR`; zero means disconnect; other errors fail | Server has a fragmented-Request path; Tester exact-read/fake-peer cases exercise staged data. |
| TCP Partial Write | Server/Tester `send_all` accumulates positive `send()` results | Retry transient errors; zero/other errors end the operation | The loop is implemented; there is no deterministic test that forces the OS to return a partial write on each call. Treat this as code coverage, not proof the case was triggered. |
| Peer disconnects early | `recv` returns zero | Server ends current-client processing; Tester returns `INVALID_LENGTH` for an incomplete Response | Server tests close after short Header/Body; Tester fake-peer cases `partial-header` / `partial-payload`. |
| Only part of a Header/Body arrives and connection stays open | Server `receive_exact` waits for remaining bytes and checks stop every 100 ms | No total frame/connection read deadline; it can wait until data, disconnect, error, or stop | Tests send a short fragment and close; they do not test timeout while the peer remains connected. This held-open case is **untested**. |
| Tester connection timeout | `connect_with_timeout` waits on a write set for up to 3000 ms | Timeout/failure maps to `UNSUPPORTED` | A no-listener connection failure is tested; there is no stable test that waits out exactly the full 3-second timeout. |
| Tester send timeout | One-stage timer in `send_all` | Deadline at 5 seconds returns `UNSUPPORTED` | No dedicated test was found that deterministically blocks the send until timeout. |
| Tester receive timeout | Each `receive_exact` call has a 5000 ms deadline | Timeout returns `UNSUPPORTED` | `test_slow_response_has_total_timeout` sends Header bytes slowly and checks a bounded elapsed time. |
| Server listener/Accept/Send/Recv error | Winsock return value and `last_socket_error` | Retry transient would-block/interrupted results; other errors end the client or Server | Some startup, stop, and disconnect cases are tested; every Winsock error code is not individually injected. |
| Invalid bind address | `InetPtonA` result in `doip_tcp_server_start` | `INVALID_ARGUMENT`, with acquired Winsock/socket resources released | Invalid bind address in the Server lifecycle test. |
| Invalid numeric CLI argument | `strtoul`, trailing-character, and range checks in `main` | Print usage/error and return `EXIT_FAILURE` | These executable-level bad-argument cases have no dedicated CTest. |

## State Side Effects and Output Guarantees

- Header/Body/Message decoders commonly decode into local variables before assigning outputs; tests verify that some failed decodes leave output unchanged.
- Gateway clears `*output_size` on entry. Message/Body failure, unsupported Type, and insufficient capacity occur before state processing.
- The Gateway checks response capacity before updating state; a dedicated test verifies that insufficient capacity does not activate the context.
- After successful State processing, the Gateway encodes a fixed Response using a decoded Request and prechecked capacity.
- When client processing ends—whether by disconnect or protocol error—the Server resets Routing Activation. Consequently, an erroneous client also clears the one context previously active in that Server instance.

## Limitations to Keep in Mind

1. The Server has no read timeout and may leave its only service loop waiting for a partial frame.
2. Multiple Winsock failures map to `DOIP_RESULT_UNSUPPORTED`; logs and `last_socket_error` are needed for more detail.
3. On a message error, the Server closes the connection instead of sending a specific protocol error Response for each case.
4. The Tester treats every non-`0x10` Response Code as failure and does not interpret the complete code set.
5. Existing tests cover only their explicitly constructed inputs and fake-peer modes. Untested error codes, races, stress, and fuzz cases must not be assumed safe or passing.

## Questions You Should Be Able to Answer

- How does the Message Decoder distinguish truncated data from trailing data?
- Why does the Server check the 4096-byte limit before reading a Body?
- How do Partial Read and peer close differ in the return values?
- Is the Server’s 100 ms polling interval a read timeout? What happens with a held-open partial frame?
- Can insufficient Gateway output capacity modify state?
- Which network error cases have integration tests, and which are only code branches?

## Key Takeaways

- `doip_result_t` is a coarse API error taxonomy; Winsock errors are recorded separately.
- Length checks apply at different layers: Header size, fixed Body size, exact message size, and the Server’s local limit.
- The Server loops for Partial Reads but has no application-level read deadline; the Tester uses per-operation-stage deadlines.
- Distinguish implemented error handling, a test case being present, and a test actually passing.
