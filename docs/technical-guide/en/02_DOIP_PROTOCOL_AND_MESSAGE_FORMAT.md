# 02 DoIP Protocol and Message Format

## Learning Objectives

Explain the Header and Routing Activation fields handled by this project, identify their byte offsets, and distinguish C structures, Payload Bodies, and the TCP byte stream.

## Three Things to Keep Separate

- **Protocol background:** a DoIP message has a Generic Header followed by a Payload. The Payload Type identifies the payload kind, and Payload Length counts the following payload bytes. Routing Activation is an exchange used before diagnostic routing is available.
- **Project implementation:** accepts only version `0x02` / inverse `0xFD`, implements basic fixed-format Routing Activation Request/Response bodies, and demonstrates a TCP request/response exchange.
- **Simplifications:** this project does not implement all ISO 13400 messages, states, or policies. Only behavior present in the source should be described as implemented; protocol background is not a claim about project coverage.

## Generic Header: 8 Bytes

`doip_header_t` is an in-memory representation. The wire Header is written/read field by field by `doip_header_encode/decode` in `doip_header.c`. Offsets below are relative to the beginning of a complete DoIP message.

| Offset | Width | Field | Protocol role | Current implementation |
|---:|---:|---|---|---|
| 0 | 1 | Protocol Version | Identifies the protocol version. | `DOIP_PROTOCOL_VERSION` is fixed at `0x02`; the encoder and decoder accept only this value. |
| 1 | 1 | Inverse Protocol Version | Complements the version byte as a consistency check. | Fixed at `0xFD`; the code checks the `0x02`/`0xFD` pair, not arbitrary versions. |
| 2–3 | 2 | Payload Type | Identifies the following payload category. | Encoded/decoded big-endian; the Gateway accepts only Request type `0x0005`. |
| 4–7 | 4 | Payload Length | Number of bytes in the following Payload Body; excludes the 8-byte Header. | Big-endian `uint32_t`; the Message Codec checks exact length, and the Server additionally enforces 4096 bytes. |

`doip_payload_type.h` defines constants including `0x0001` Vehicle Identification Request, `0x0002` request with EID, `0x0003` request with VIN, `0x0004` Vehicle Announcement, `0x0005` Routing Activation Request, and `0x0006` Routing Activation Response. Defining a constant does not implement a parser or handler. The Gateway dispatches only `0x0005`; `doip_tester.c` expects `0x0006`.

## Routing Activation Request: Basic 7-Byte Body

Offsets are relative to the Payload Body. Add 8 to obtain absolute offsets in the complete message.

| Body offset | Width | Field / project representation | Handling |
|---:|---:|---|---|
| 0–1 | 2 | Source / Tester Logical Address, `source_address` | Request Codec reads/writes big-endian; the Gateway uses it as the client identifier in its single state context. |
| 2 | 1 | Activation Type, `activation_type` | Preserved by the Codec; the current state logic does not validate it or make decisions based on it. |
| 3–6 | 4 | Reserved | Encoder writes zeros; decoder requires all four bytes to be zero. |

In the general Routing Activation concept, a Tester requests a logical routing context from a Gateway. This project’s acceptance policy only checks whether the context is inactive and whether the logical address matches; it is not a complete authorization policy. The Request Body must be exactly 7 bytes. A longer body, including extension data, returns `DOIP_RESULT_UNSUPPORTED` rather than being parsed.

## Routing Activation Response: Basic 9-Byte Body

| Body offset | Width | Field / project representation | Handling |
|---:|---:|---|---|
| 0–1 | 2 | Client Logical Address, `client_logical_address` | Copied from the decoded Request’s `source_address`. |
| 2–3 | 2 | DoIP Entity Logical Address, `doip_entity_logical_address` | Supplied by Gateway configuration; the Tester compares it with its expected value. |
| 4 | 1 | Response Code, `response_code` | Codec preserves the value; the state module uses `0x10` for success, while the Server configures `0x03` as its default conflict code. The Tester accepts only `0x10`. |
| 5–8 | 4 | Reserved | Encoder writes zeros; decoder requires zeros. |

For a 13-byte Body, the Response Decoder returns `DOIP_RESULT_UNSUPPORTED` (the header comment associates this with a 4-byte OEM-specific extension). A body shorter than 9 bytes returns `BUFFER_TOO_SMALL`; other non-9-byte lengths return `INVALID_LENGTH`. The Codec does not decide whether a Response Code is valid.

## Logical Address, Activation Type, and Response Code

- **Logical Address:** a 16-bit identifier in DoIP messages. This project uses the Request’s `source_address` as the one logical client identity and uses a configured Entity address in the Response. The code does not validate address ranges against a vehicle-specific configuration.
- **Activation Type:** one byte in the Request. The code stores it in the decoded structure, but the state function never reads it. Therefore, the project does not implement an activation-type policy.
- **Response Code:** one result byte. The state module maps the first request while inactive and a retry from the active address to `0x10`; a different address receives the configured `conflict_response_code`. The Response Codec does not interpret it, while the Tester treats any value other than `0x10` as failure.
- The source does not define a complete response-code set or common semantic mapping. `0x03` is the Server main function’s default conflict configuration, not a universally correct policy for every deployment.

## Network Byte Order (Big-Endian)

For a multi-byte integer, the most significant byte appears first. For example, address `0x0E00` is encoded as `0E 00`, and a payload length of 7 as `00 00 00 07`. The code uses shifts, masks, and byte-wise assembly rather than copying C structure memory. This avoids dependence on CPU byte order, alignment, or structure padding.

## Complete Examples

With the defaults, the Tester sends a basic Routing Activation Request using Tester address `0x0E00` and Activation Type `0x01`:

```text
02 FD 00 05 00 00 00 07  0E 00 01 00 00 00 00
| Header (8 bytes)       | Request Body (7 bytes)        |
```

Field by field:

```text
02       Protocol Version
FD       Inverse Protocol Version
00 05    Routing Activation Request Payload Type
00 00 00 07 Payload Length = 7
0E 00    Source / Tester Logical Address
01       Activation Type
00 00 00 00 Reserved
```

A successful Response for Entity `0x1000` and Response Code `0x10` is:

```text
02 FD 00 06 00 00 00 09  0E 00 10 00 10 00 00 00 00
| Header (8 bytes)       | Response Body (9 bytes)         |
```

For a different logical address, if the Server uses its default conflict code `0x03`, only the Response Code byte in the Body (absolute message offset 12) changes to `03`; the current encoder generates the other fields as usual.

## TCP Byte Stream and Message Boundaries

TCP provides an ordered byte stream, not boundaries corresponding to `send()` calls. One send can arrive through multiple receives, and multiple sends can be coalesced. The Server therefore uses `receive_exact`: accumulate 8 Header bytes, decode the Header, then accumulate the Body according to `payload_length`. Only a complete message is passed to `doip_message_decode` and the Gateway.

The Message Codec’s `doip_message_decode` handles exactly one complete in-memory message. If fewer bytes follow the Header than declared, it returns `BUFFER_TOO_SMALL`; if there are extra trailing bytes, it returns `INVALID_LENGTH`. The TCP Server reads one frame at a time and passes the exact size, so trailing frames are not treated as part of the current message.

## Questions You Should Be Able to Answer

- Does Payload Length include the Generic Header? How does the Server use it to determine the frame size?
- Why can’t `doip_header_t` be written directly to the network?
- How do you read the Payload Type, address, and length from the hexadecimal example?
- Which functions interpret Activation Type and Response Code, and which merely preserve them?
- Why do Payload Type constants not prove Vehicle Identification is implemented?
- How does `doip_message_decode` differ from TCP framing?

## Key Takeaways

- The Header is 8 bytes; Payload Length counts only the Payload Body.
- The project’s basic Request and Response bodies are 7 and 9 bytes.
- Multi-byte fields use explicit big-endian encoding; `0x02`/`0xFD` are the project’s fixed accepted version.
- Address policy and Response Code handling are simplified and must not be presented as complete standard behavior.
- TCP send/receive code must accumulate the required number of bytes itself.
