# 03 Protocol Core Implementation

## Learning Objectives

Trace Header, Payload Body, and complete-message codecs step by step. Explain each API’s boundary checks, return values, pointer ownership, and related tests.

## Common Result Type

`doip_types.h` defines the `doip_result_t` enum:

| Result | Meaning in this implementation |
|---|---|
| `DOIP_RESULT_OK` | Operation succeeded. |
| `DOIP_RESULT_INVALID_ARGUMENT` | A required pointer is null or another argument is invalid. |
| `DOIP_RESULT_BUFFER_TOO_SMALL` | Input is shorter than required or output capacity is insufficient; the exact meaning depends on the function. |
| `DOIP_RESULT_INVALID_HEADER` | Header version mismatch, or nonzero Reserved bytes in a Request/Response Body. |
| `DOIP_RESULT_INVALID_LENGTH` | Length does not satisfy a fixed-body or exact-message-length requirement. |
| `DOIP_RESULT_UNSUPPORTED` | The project does not support the message/extension form; inspect the specific function for its exact behavior. |

This enum does not carry socket error details. The network layer separately records some Winsock errors in `doip_tcp_server_t.last_socket_error` and prints logs.

## Generic Header

### `doip_header_encode`

Inputs are `const doip_header_t *header`, a destination `buffer`, and `buffer_size`. On success, the function writes 8 bytes and returns `DOIP_RESULT_OK`:

1. A null Header or Buffer returns `INVALID_ARGUMENT`.
2. Capacity below 8 returns `BUFFER_TOO_SMALL`, without writing fields.
3. A Protocol Version other than `DOIP_PROTOCOL_VERSION` or inverse other than `DOIP_PROTOCOL_VERSION_INVERSE` returns `INVALID_HEADER`.
4. The function writes the version, inverse version, 16-bit Payload Type, and 32-bit Payload Length byte by byte.

Payload Type and Length use shifts and masks to produce big-endian bytes. If the destination is larger than 8 bytes, only its first 8 bytes are written. The code does not serialize `sizeof(doip_header_t)`, because C structure layout is not the wire format.

### `doip_header_decode`

The input must contain at least 8 bytes. The function validates pointers and capacity, then requires the first two bytes to be exactly `02 FD`. It assembles the fields into a local `decoded_header` and assigns the output structure only after all checks pass. Therefore a validation failure does not partially overwrite the output.

The decoder does not decide whether a Payload Type is supported and does not check whether the following Body is complete. Higher layers do that.

Tests in `test_doip_header.c` cover successful encoding/decoding, round trips, null arguments, insufficient capacity, bad version/inverse, big-endian layout, and preservation of bytes beyond the 8-byte Header.

## Payload Helpers

### `doip_payload_check_length`

Compares `(size_t)payload_length <= available_size`; returns `OK` if true and `BUFFER_TOO_SMALL` otherwise. It does not access payload memory or allocate storage. The current core flow does not call it, but `test_doip_payload.c` tests it directly.

### `doip_payload_copy`

1. If `payload_length == 0`, returns `OK` immediately, even if source and destination pointers are null.
2. For nonzero length, checks that both pointers are non-null.
3. Checks that destination capacity is at least `payload_length`.
4. Copies one byte at a time in a `for` loop.

This helper copies with an explicit capacity. It does not provide `memmove` semantics, so callers must not rely on correct behavior for overlapping regions. `doip_message_encode` uses it to copy the Payload Body. Tests cover zero length, null pointers, exact/larger capacity, and insufficient capacity without writing the destination.

## Routing Activation Body Codecs

### Request Codec

`doip_routing_activation_request_encode` checks pointers and 7-byte capacity, then writes `source_address` big-endian to `[0..1]`, `activation_type` to `[2]`, and zeros to Reserved bytes `[3..6]`.

`doip_routing_activation_request_decode` requires exactly 7 bytes. A shorter input returns `BUFFER_TOO_SMALL`; a longer input returns `UNSUPPORTED`. It requires all four Reserved bytes to be zero, otherwise returning `INVALID_HEADER`. On success, it decodes into a local structure and then assigns the output. It does not validate the Activation Type value or the Logical Address range.

`test_doip_routing_activation_request.c` covers encoding/decoding, null pointers, capacity, an overlength body, and nonzero Reserved bytes.

### Response Codec

`doip_routing_activation_response_encode` writes the Client address, Entity address, and Response Code, then writes zero to the four Reserved bytes.

`doip_routing_activation_response_decode` checks, in order: null pointers; minimum 9-byte length (`BUFFER_TOO_SMALL` if shorter); a special 13-byte case (`UNSUPPORTED`); other non-9-byte lengths (`INVALID_LENGTH`); and zero Reserved bytes (`INVALID_HEADER`). It decodes to a local structure and assigns the output only on success. The Response Code is preserved as-is, including unknown values.

`test_doip_routing_activation_response.c` covers successful/rejection/unknown codes, null arguments, lengths, Reserved bytes, and output preservation on failure.

## Complete Message Codec

### `doip_message_encode`

Inputs are a Header, Payload pointer and `payload_size`, destination buffer, and destination capacity. The steps are:

1. Require non-null Header and destination Buffer.
2. Require `payload_size <= UINT32_MAX` and exact equality with `header->payload_length`.
3. Require a non-null Payload pointer for a nonzero body.
4. Check for at least 8 bytes of Header capacity, then check `payload_size <= buffer_size - 8`. Subtracting before comparison avoids blindly adding the lengths.
5. Call `doip_header_encode` to write the Header.
6. Call `doip_payload_copy` to write the Body at `buffer + 8`.

For valid input, the source Body and destination message region must not overlap. The function does not make a temporary copy of the complete source Payload. Length mismatch and insufficient capacity are rejected before the Header is written. After writing the Header, a later Payload-copy failure should not occur for the prevalidated lengths and capacity; callers must still provide valid memory and follow the API’s overlap requirements.

### `doip_message_decode`

The input must contain at least the 8-byte Header:

1. Validate pointers and minimum Header length.
2. Call `doip_header_decode`; propagate a version-validation failure.
3. Compute `available_payload_size = buffer_size - 8`.
4. Return `BUFFER_TOO_SMALL` if this is less than the declared length, or `INVALID_LENGTH` if it is greater.
5. On success, set `message->header` and `message->payload = buffer + 8`.

The function does not copy the Body, validate Payload Type, or call a type-specific Body Codec. The returned payload pointer borrows the input buffer and must not outlive it. For an empty Body, it points just after the Header and must not be dereferenced.

`test_doip_message.c` covers nonempty/empty round trips, inconsistent Header/body lengths, insufficient capacity, null arguments, a `payload_size` above `UINT32_MAX` where representable, short Header, truncated Body, trailing bytes, unknown type, and large declared length.

## Why Header, Payload, and Message Are Separate

- The Header Codec handles only the 8-byte framing metadata and can be reused with different payloads.
- Request/Response codecs understand only their own fixed Body fields and Reserved bytes.
- The Message Codec enforces consistency between the declared Header length and one complete message.
- The Gateway dispatches by Payload Type and applies business logic.

This separation prevents every Body Codec from re-parsing the Header and prevents C structure layout from being mistaken for wire layout. `doip_message_view_t` returns a borrowed pointer to avoid another Body copy; the Gateway then decodes fields it needs into a small Request structure.

## Buffer-Overflow Boundaries

The core does not dynamically allocate message buffers. Write operations check fixed capacities first. The Message Codec compares against the remaining capacity using subtraction; the Server uses fixed arrays and rejects a Payload Length above 4096 before reading the Body. Body codecs write only their defined lengths, and the Header Codec writes only 8 bytes.

These checks do not prove that a pointer is valid or that the caller supplied the true object size, and they do not establish arbitrary overlap semantics. The Transport has a higher-level local limit; the generic Message Codec is constrained by `uint32_t` and the caller’s actual buffer capacity, not by a hard-coded 4096.

## Important Call Chains

```text
doip_message_encode
  ├─ doip_header_encode
  └─ doip_payload_copy

doip_message_decode
  └─ doip_header_decode

doip_gateway_process_message
  ├─ doip_message_decode
  │   └─ doip_header_decode
  ├─ doip_routing_activation_request_decode
  ├─ doip_routing_activation_state_process_request
  └─ doip_routing_activation_encode_response_message
      ├─ doip_routing_activation_response_encode
      └─ doip_message_encode
          ├─ doip_header_encode
          └─ doip_payload_copy
```

## Questions You Should Be Able to Answer

- Why should `doip_header_t` not be copied directly to the network?
- Does `doip_message_decode` allocate or copy the Payload? Why is the output structure not partially updated on failure?
- Why must `payload_length` equal `payload_size`?
- Which layer enforces 4096 bytes? Does the generic Message Codec impose that limit?
- What does the Request Codec return for an overlength body versus nonzero Reserved bytes?
- Which APIs permit input/output message overlap, and which require the caller to avoid it?

## Key Takeaways

- Each layer handles its own fields, Body format, complete-message length, or business dispatch.
- Header and Body multi-byte fields are encoded explicitly in big-endian order.
- A Message View borrows input memory; fixed buffers and explicit capacities provide the main bounds checks.
- The generic Message Codec neither interprets Payload Type semantics nor automatically applies the Server’s 4096-byte limit.
