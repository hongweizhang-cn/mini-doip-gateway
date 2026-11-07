# 10 Design Characteristics and Trade-Offs

## Learning Objectives

Explain implementation choices and their costs using source evidence. Distinguish an observable code characteristic from the developer’s original intent.

> Unless a source comment explicitly states the motivation, the items below are described as **observable characteristics of the current implementation**, not as claims about the original design intent.

| Choice / observed characteristic | Problem addressed | Current implementation | Benefits | Costs | Alternatives | Reasonable explanation for this project |
|---|---|---|---|---|---|---|
| C99 | Keep byte and memory handling explicit at a low level. | Root CMake requires C99; fixed-width integers come from `<stdint.h>`. | Demonstrates protocol byte handling common in embedded work. | Manual memory/bounds management; fewer language-level safety abstractions. | C++, Rust, or generated protocol codecs. | It is an explicit project constraint suitable for learning C codecs, not a requirement for all production automotive software. |
| Protocol Core separated from Winsock Transport | Avoid coupling protocol parsing to socket APIs. | Gateway accepts complete messages; TCP Server handles I/O/framing; CMake gates network code on Windows. | Core can be unit-tested without a network; transport responsibilities are localized. | Core has no incremental stream API; Transport remains platform-specific. | Transport interface, cross-platform socket adapter, event loop. | The separation keeps the demo testable; portability still needs additional abstraction. |
| Field-by-field serialization | Avoid structure padding, alignment, and host-byte-order differences. | `doip_header.c` and Body codecs use shifts, masks, and explicit byte access. | Wire format is visible and reviewable. | More boilerplate as field count grows. | Generic serializer or code generation. | Formats are small and fixed, making explicit code easy to inspect. |
| No C bit-fields for wire representation | Avoid compiler/ABI-dependent bit-field layout. | `doip_header_t` stores semantic values; Codec maps them to 8 bytes. | Separates memory layout from protocol layout. | Manual shifts; more verbose for complex bit fields. | Manual masks or generated codecs. | The current Header does not need bit-fields; ordinary integer members are straightforward. |
| Explicit Buffer + Capacity arguments | Let callers state writable bounds. | Encoders accept `buffer_size`/`output_capacity` and check before writing. | Works with static arrays and boundary tests. | More API parameters; cannot verify the actual allocated size or pointer validity. | `span`-like abstractions, owned buffers, dynamic containers. | C99 has no built-in slice type, but callers must still pass the real capacity. |
| Separate Header, Payload Body, and Message layers | Give each layer a focused validation responsibility. | Separate Header and Request/Response codecs plus complete-message API. | Reuse, clear error boundaries, framing validation before business decoding. | Some Header parsing occurs at multiple layers. | One end-to-end parser or a stream-parser object. | The layers support focused tests; the Server must read Header first for TCP framing, and the Gateway validates the complete input through the Message API. |
| Borrowed Payload in `doip_message_view_t` | Avoid an extra Payload copy during decode. | `payload = buffer + 8`; decoded Header values are stored in the view. | No allocation or redundant Body copy. | Pointer lifetime depends on the input buffer. | Owned message object or copy-based decode. | The current fixed-buffer synchronous path keeps the input alive until Request decoding; asynchronous retention would need ownership or a copy. |
| TCP exact-read/write loops | `send`/`recv` may not complete a requested byte count. | Server/Tester accumulate positive return values and retry would-block/interrupted errors. | Handles short I/O and nonblocking sockets. | More state/error branches; Server has no frame deadline. | Blocking exact I/O, IOCP/async framework, incremental parser. | The loops make TCP stream semantics explicit; production code would need fuller timeout/cancellation policies. |
| Synchronous single-client service | Simplify connection and state ownership. | `doip_tcp_server_run` serves one client to completion before accepting another. | Easy to follow; one Gateway state object. | A slow client blocks others; no concurrent throughput. | Thread pool, multi-client select/poll/IOCP model. | Appropriate for a minimal demo, not a scalable server design. |
| Fixed local 4096-byte Payload limit | Avoid dynamic allocation and bound stack buffers. | Server and Tester use a local limit; Server arrays hold 8+4096 bytes. | Simple and auditable; rejects oversized declarations. | Fixed resource/feature ceiling; not the protocol’s general maximum. | Configurable cap, streaming/dynamic buffers, negotiated policy. | The code and README explicitly describe it as a local safety policy. |
| Error enum returns | Avoid exceptions or a global error variable in C APIs. | Shared `doip_result_t`; socket details are recorded separately. | Explicit C-style branching and testable results. | `UNSUPPORTED` is broad; diagnostics need logs/context. | Rich error structure, module-specific codes, HRESULT-style result. | Simple for a small C API; a larger system would need more precise diagnostics. |
| Check Gateway output capacity before state update | Avoid committing activation when no Response can be produced. | Decode/validate, check 17 bytes, then call State Process. | Capacity failure has no state side effect; explicitly tested. | Couples the check to a fixed Response size. | Build a temporary Response, then commit state transactionally. | The Response is fixed at 17 bytes, so the precheck is simple and effective. |
| Associate state with Logical Address rather than socket | Keep the current policy based on Tester Logical Address. | State stores one `client_logical_address`; Server resets it when a client ends. | Demonstrates a minimal logical-address policy. | Cannot represent multiple connections, routes, or address-binding policies. | Per-connection contexts or address-to-ECU routing table. | Suitable for a demo, not a substitute for real Gateway session management. |

## Overall Engineering Priorities

The current implementation favors readability, fixed bounds, and module-level tests. It does not attempt to solve high concurrency, complete standards coverage, ECU integration, or production observability. In an interview, state the code fact first, explain when the approach is useful, then discuss its cost; do not claim performance or security validation that has not been performed.

## Questions You Should Be Able to Answer

- What source evidence shows that a C structure should not be copied as a DoIP Header?
- What does explicit capacity checking solve, and what can it not solve?
- How does `doip_message_view_t` reduce copying, and what lifetime risk does that introduce?
- Why is it not enough to add threads to turn the single-client server into a multi-client server?
- What does the 4096-byte limit mean, and what does it not mean?
- Which motivations are explicitly stated in comments, and which are reasonable interpretations of the implementation?

## Key Takeaways

- Start with source evidence, then discuss benefits and costs; do not present guesses as original design intent.
- Fixed buffers, explicit lengths, and field-wise serialization aid review but do not constitute a complete memory-safety proof.
- Single-client behavior and the 4096-byte limit are implementation boundaries, not DoIP protocol properties.
- Streaming or concurrency changes affect state ownership, Transport, error handling, and tests together.
