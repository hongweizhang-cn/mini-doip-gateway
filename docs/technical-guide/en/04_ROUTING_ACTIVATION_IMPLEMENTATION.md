# 04 Routing Activation Implementation

## Learning Objectives

Trace a Request from complete-message decoding through state update and Response encoding. Explain why a TCP connection and logical Routing Activation state are different things.

## Purpose and Project Scope

In DoIP diagnostics, Routing Activation is used to request a logical routing context from a Gateway before subsequent diagnostic communication. A real system includes more state, authorization/policy, and failure-response semantics. This project simulates only a basic request/response exchange and one client context; it does not forward data to an ECU.

## Data Structures and Actual Policy

- `doip_routing_activation_request_t`: `source_address` (16 bits) and `activation_type` (8 bits).
- `doip_routing_activation_response_t`: client address, Entity address, and Response Code.
- `doip_routing_activation_context_t`: `INACTIVE` / `ACTIVE` and one `client_logical_address`.
- `doip_gateway_t`: the context, Entity address, and explicit conflict-code configuration.

The actual decisions in `doip_routing_activation_state_process_request` are:

| Current state | Request address | Result | State side effect |
|---|---|---|---|
| INACTIVE | Any 16-bit address | `DOIP_RESULT_OK`, Response Code `0x10` | Save address and transition to ACTIVE. |
| ACTIVE | Same as saved address | `DOIP_RESULT_OK`, `0x10` | Keep current state. |
| ACTIVE | Different address | `DOIP_RESULT_OK`, caller-supplied conflict code (Server default `0x03`) | Do not replace the saved address. |
| Invalid enum value | Any | `DOIP_RESULT_INVALID_ARGUMENT` | Context unchanged; Response Code output is initialized to zero. |

The state module does not validate `activation_type` or Logical Address ranges. The Request Body Codec checks only fixed length and Reserved bytes.

## Gateway Request-Processing Steps

`doip_gateway_process_message` takes one complete input message and its exact length, an output message buffer/capacity, and `output_size`:

1. If `output_size` is non-null, set it to zero; then validate required pointers.
2. `doip_message_decode` validates the Generic Header and exact complete-message length.
3. If Payload Type is not `DOIP_PAYLOAD_TYPE_ROUTING_ACTIVATION_REQUEST`, return `UNSUPPORTED`.
4. `doip_routing_activation_request_decode` parses the 7-byte Request Body.
5. Check for at least `8 + 9 = 17` bytes of output capacity **before state processing**. Insufficient capacity returns `BUFFER_TOO_SMALL` without activating the context.
6. Call `doip_routing_activation_state_process_request` to get a Response Code and possibly update the context.
7. Set the Entity address and Response Code, then call `doip_routing_activation_encode_response_message` to write the complete Response.

Failures in earlier validation, unsupported Payload Type, or insufficient output capacity do not change Routing Activation state. Input and output message regions may overlap because the complete Request is decoded into a view/local structure before output is written; neither region may overlap the Gateway object. After state processing succeeds, the code encodes a fixed, valid Response using prechecked capacity.

## Response Construction Chain

`doip_routing_activation_encode_response_message`:

1. Validates arguments and clears `*output_size`.
2. Requires at least 17 bytes of output capacity.
3. Uses Request `source_address` as the response client address and takes the Entity address and Response Code from the parameters.
4. Calls the Response Body Codec to produce a 9-byte Body with four zero Reserved bytes.
5. Fills a `doip_header_t` with version `02 FD`, type `0x0006`, and length 9.
6. Calls `doip_message_encode` to write 8+9 bytes and sets `*output_size = 17`.

`doip_routing_activation_process_request_message` is a convenience entry point without the state machine: it decodes one complete Request, checks its type and Body, then delegates to the Response builder. The Server’s main Gateway path uses `doip_gateway_process_message`, which includes state processing.

## Sequence Diagram

```mermaid
sequenceDiagram
    participant T as Tester
    participant TCP as Windows TCP Server
    participant G as Gateway Core
    participant S as Routing Activation State
    participant C as Request/Response and Message codecs

    T->>TCP: TCP Connect
    T->>C: Encode 7-byte Request Body + 8-byte Header
    C-->>T: 15-byte complete Request
    T->>TCP: send_all(Request bytes)
    TCP->>TCP: receive_exact(8-byte Header)
    TCP->>TCP: Validate version and payload_length <= 4096
    TCP->>TCP: receive_exact(payload_length)
    TCP->>G: doip_gateway_process_message(complete message)
    G->>C: doip_message_decode
    C-->>G: doip_message_view_t (borrows input Body)
    G->>C: doip_routing_activation_request_decode
    C-->>G: source_address / activation_type
    G->>S: doip_routing_activation_state_process_request
    S-->>G: Response Code; may update ACTIVE and client address
    G->>C: doip_routing_activation_encode_response_message
    C->>C: Encode 9-byte Response Body
    C->>C: doip_message_encode(Header + Body)
    C-->>G: 17-byte complete Response
    G-->>TCP: output_message / output_size
    TCP->>T: send_all(Response bytes)
    T->>T: Exact-read Header / Payload
    T->>C: Decode Response Body
    C-->>T: Addresses / Response Code
    T->>T: Compare addresses and require Response Code == 0x10
```

## Routing Activation Is Not a TCP Connection

A TCP Connect establishes a transport connection. It does not call the state-processing function and does not mean DoIP routing activation succeeded. A complete, decodable Request must reach the Gateway and pass its state logic before a Response is generated. The Tester considers the exchange successful only after validating Response Code `0x10` and matching addresses.

The Server processes one client synchronously. When the client disconnects or its message is rejected, the Server closes the client socket and calls `doip_gateway_reset_routing_activation`, then returns to the accept loop. This is the project’s connection-lifecycle policy, not a claim about all DoIP implementations.

## Success, Rejection, and Failure Paths

- **State success:** first request while inactive or retry from the same address; Response Code `0x10`.
- **Business conflict response:** a different address while active receives the configured code. `state_process_request` itself returns `OK`, so the Gateway still encodes a Response. The Tester treats any non-`0x10` value as failure.
- **Message-layer failure:** invalid Header, length error, unsupported Request Payload Type, malformed Body, or invalid arguments returns the corresponding `doip_result_t`; no normal Response is generated.
- **Insufficient output capacity:** the Gateway fails before the state update.
- **Network disconnect/protocol rejection:** the Server closes the client connection and resets the single Routing Activation context when that client’s processing ends.

## Related Tests

- `test_doip_routing_activation_state.c`: first activation, same-client retry, conflict without overwrite, argument errors, reset/reactivation, invalid enum state.
- `test_doip_routing_activation.c`: success/rejection Responses, message type/length/body errors, input/output overlap, and capacity checks.
- `test_doip_gateway.c`: first activation, repeat/conflict, invalid messages preserving state/output, capacity checked before state change, and reactivation after reset.
- Windows `test_doip_tcp_server.c` and `test_doip_tester.c`: end-to-end exchange and connection-lifecycle paths.

These files show that test cases are written and registered in CMake. Tests were not run while preparing this documentation, so no claim is made that they passed in the current environment.

## Questions You Should Be Able to Answer

- Does the state context store a TCP socket identity or a Logical Address?
- How does the Tester handle a successful TCP connection followed by Response Code `0x03`?
- Why does the Gateway check Response capacity before updating state?
- Why is a retry from the same address idempotently successful, while a different address does not replace it?
- Where is Activation Type validated or ignored?
- What happens to state after a client disconnects, and what does that mean for the next client?

## Key Takeaways

- TCP Connection and Routing Activation are different events at different layers.
- The Gateway validates a complete message and delegates a deliberately minimal address-based policy to the State module.
- The Request Codec stores Activation Type, but the state logic does not use it.
- Output capacity is checked before state side effects; the Server resets its single context when client processing ends.
