# Project Interview Notes

## Project Summary

`mini-doip-gateway` is an educational C99 implementation of a small DoIP Routing Activation path. It combines explicit Generic Header and payload codecs, a simplified single-client Routing Activation state model, a Windows Winsock TCP server that frames messages from a byte stream, and a Tester Simulator that validates the server response. The project is intentionally limited in scope and is not a complete ISO 13400 implementation or a certified product.

## Resume Bullet Drafts

- Implemented C99 DoIP Generic Header and Routing Activation Request/Response codecs with explicit big-endian field handling, bounded buffers, and input-length validation.
- Built a simplified Routing Activation Gateway Core and a Windows Winsock TCP server that reads complete messages from a TCP byte stream before passing them to the protocol core.
- Added a Windows Tester Simulator and automated unit/integration tests covering response validation, partial TCP reads, malformed inputs, timeout behavior, and resource cleanup.

## Technical Interview Topics

1. **Why is TCP stream framing necessary?**  
   TCP provides an ordered byte stream, not message boundaries. The server reads the fixed 8-byte Generic Header first, validates its declared payload size against a local limit, then loops until the complete payload is received.

2. **Why not serialize a C structure by copying its memory?**  
   Structure padding, alignment, and host byte order are implementation-dependent. The codecs write and read each protocol field explicitly, including big-endian multi-byte values.

3. **How is the Generic Header validated?**  
   The decoder requires an 8-byte header and accepts the project's configured protocol version `0x02` with inverse `0xFD`; payload type and 32-bit payload length are decoded explicitly.

4. **How does the Gateway handle Routing Activation state?**  
   It tracks one simplified logical-client context. The first request activates it, a repeat from the same address succeeds, and a different address receives the configured conflict code without replacing the stored address.

5. **How are buffer boundaries protected?**  
   Message and payload APIs check declared lengths and destination capacities before copying. The TCP server enforces its fixed 4096-byte local payload bound before reading the body.

6. **How are partial sends, partial receives, and timeouts handled by the Tester?**  
   Nonblocking Winsock operations are retried in loops. Connection establishment is bounded to 3 seconds; the complete send operation and each exact-read stage use a 5-second deadline, recalculated as remaining time for `select()`.

7. **Why are Gateway Core and TCP Server separate?**  
   Gateway Core consumes a complete encoded DoIP message and applies protocol/state logic; the Windows transport layer owns sockets, stream framing, connection lifecycle, and delivery of complete messages.

8. **What is the trade-off of the single-client server?**  
   It keeps connection ownership and Gateway state simple for a learning/demo project, but it does not support concurrent or multiple active clients.

9. **What would be needed to add Diagnostic Message forwarding?**  
   It would require defining and validating the relevant payload representation, adding a Gateway processing path and response/error behavior, and integrating a real downstream transport or ECU interface. Those behaviors are not implemented now.

10. **Which protocol behaviors are deliberately out of scope?**  
    The project does not implement the complete ISO 13400 state machine and response-code set, UDP Vehicle Identification, Alive Check, diagnostic forwarding, OEM-specific extensions, or official conformance.

11. **What is the scope of the 4096-byte limit?**  
    It is a local fixed-buffer resource policy used by the server and Tester, not a claim about the general maximum payload size defined by the DoIP standard.

12. **Which platform boundary matters?**  
    The protocol codecs, Gateway, and state model are C99 modules. The current TCP Server, Tester, and their socket integration tests are Windows-specific and use Winsock; a POSIX server port is future work.

## Scope and Limitations

- Only the basic Routing Activation Request and Response payload forms are handled end to end.
- The Gateway state model represents one logical client and applies a simplified activation policy; activation type and full authorization/business policy are not evaluated.
- The TCP Server handles one client at a time, uses a fixed local payload limit, and resets Routing Activation state when a client connection ends.
- The Tester validates the Generic Header, expected Response Payload Type and length, response body, logical addresses, and success Response Code. A non-success Response Code is reported as failure.
- The transport implementation and network integration tests are Windows Winsock-specific. Other platforms have not been established as supported for the TCP Server or Tester.
- The project is an educational implementation; it does not claim complete ISO 13400 behavior, production deployment, performance measurements, or official certification.
