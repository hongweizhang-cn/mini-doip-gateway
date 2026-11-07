# 11 Limitations and Future Improvements

## Learning Objectives

Separate current functionality from source evidence and possible future work. Avoid describing planned extensions as delivered capabilities in an interview.

## Limitations

| Topic | Current status and source evidence | What future work would add | Architectural impact | Main challenges |
|---|---|---|---|---|
| UDP Vehicle Identification | **No processing flow implemented.** `doip_payload_type.h` defines constants such as `0x0001..0x0003`; CMake has no UDP module. | UDP socket Transport, request parsing, Entity/vehicle information responses, configuration, and tests. | Add a UDP Transport and discovery service; do not put it into the TCP Server framing loop. | UDP broadcast/unicast semantics, timeouts/interface selection, request variants, vehicle configuration. |
| Vehicle Announcement | **Not implemented.** Constant `0x0004` exists, but there is no Body structure or Codec. | Announcement Body Codec, trigger/transmission policy, and UDP integration. | Coordinate with vehicle-identification state and UDP service. | Correct fields, triggering/periodic behavior, and the applicable standard version. |
| Alive Check | **Not implemented.** No corresponding Payload Type, state, or timer handling exists. | Alive Check Request/Response, connection state and timer management, and tests. | Add timing and check behavior to Server lifecycle/state management. | Timing with concurrent connections, no-response behavior, cleanup. |
| Diagnostic Message | **Not implemented.** `doip_payload_type.h` has no Diagnostic Message type; Gateway accepts only `0x0005`. | Diagnostic payload Codec, length/address validation, acknowledgement/error behavior. | Add payload dispatch and diagnostic processing interfaces to the Gateway. | Standard acknowledgement flow, diagnostic address semantics, segmentation/error handling, test coverage. |
| ECU forwarding | **Not implemented.** No ECU/downstream socket, route table, or forwarding source file exists. | ECU Transport Adapter, destination routing, and request/response correlation. | Extend the Gateway from local Response generation to an upstream/downstream proxy. | ECU network behavior, timing correlation, failure propagation, timeouts, security boundaries. |
| Multiple clients | **Not implemented.** `doip_tcp_server_run` serves one client to completion; State stores one address. | Multi-connection manager, per-connection or standard-defined session state, concurrency safety, resource limits. | Refactor Transport, Gateway state scope, and ownership. | Concurrent requests, state isolation, slow-client resource use, shutdown races. |
| Complete Routing Activation policy | **Not implemented.** `doip_routing_activation_state_process_request` distinguishes only INACTIVE/ACTIVE and same/different address; Activation Type is not validated. | Full state/Response Code policy, authorization extension points, configuration, policy tests. | Replace the minimal State logic with a richer state machine and Response mapping. | Confirm rules against the selected standard version and vehicle/OEM requirements; do not extend policy from example codes alone. |
| Security mechanisms | **Not implemented; no security assurance can be inferred from the source.** Plain Winsock TCP is used; there is no TLS, client authentication, encryption, access control, audit, or rate limiting. | Add transport protection, identity/access policy, logging, and resource controls based on a deployment threat model. | May add security layers, certificate/key configuration, session identity, and error handling. | Vehicle-network trust boundaries, key management, performance, compatibility; requires security assessment, not just a switch. |
| Timeouts and resource management | **Partially implemented.** Tester has connect/send/read deadlines; Server’s 100 ms `select` polling checks stop but has no frame read deadline. Server uses fixed stack buffers and one client. | Partial-frame and idle-connection deadlines, connection limits, resource observability, and cleanup tests. | Extend Transport lifecycle/configuration, possibly with per-connection timers. | Timeout semantics, partial-frame cancellation, thread/close races, recovery after errors. |
| Cross-platform support | **Protocol core can be configured outside Windows; network modules are not portable.** CMake builds Winsock Server/Tester only under `WIN32`. | POSIX Transport, common interfaces, and platform CI. | Encapsulate socket lifecycle and error codes in platform adapters. | Differences in `select`, nonblocking behavior, error codes, address APIs, and close semantics. |
| ISO 13400 completeness/conformance | **Not complete and not claimed as certified.** The source covers only its listed Generic Header and basic Routing Activation path; README lists major omissions. | Trace behavior against a specified standard version, implement the full message/state scope, and use external conformance tests. | Much larger scope, requiring a protocol matrix, test framework, and version management. | Completeness, boundary combinations, and conformance evidence; this repository has no conformance report. |
| OEM extensions | **Unsupported.** Request Decoder accepts only 7 bytes; Response Decoder returns `UNSUPPORTED` for 13 bytes; codecs write Reserved bytes as zero. | Versioned extension structures, length rules, and configuration. | Make Codec/API represent optional fields and variable lengths. | Do not mistake unknown extensions for Reserved data; manage interoperability and compatibility. |
| Observability/error detail | **Limited.** `doip_result_t` is coarse; Server has `printf` and `last_socket_error`, Tester has stdout/stderr. | Structured logs, error context, metrics, and test reports. | Add common logging/error interfaces and pass context across modules. | Avoid sensitive-data leakage while keeping diagnostics consistent across platforms. |

## Suggested Extension Order

For a learning project—not a production-readiness claim—first define the intended behavior. Then consider improving protocol boundary/negative tests, adding a Server partial-frame timeout and clearer errors, isolating multi-client state, implementing UDP Discovery, adding Diagnostic Message codecs, introducing an ECU Adapter, and adding cross-platform Transport. Each step should bring tests and documentation. Adding a Payload Type constant alone does not complete a feature.

## Scope Caveats

- Header-version, Payload-Type, and Response-Code constants do not constitute a complete standards implementation.
- This guide does not consult the external standard text or perform official conformance validation. The exact standard clauses, version differences, and certification status cannot be confirmed from this repository.
- Future modules described here are possible improvement directions, not existing interfaces, behaviors, or commitments.

## Questions You Should Be Able to Answer

- What key modules are missing for Diagnostic Message forwarding? Is adding a Payload Type enough?
- Which current structures and call boundaries would multi-client support affect?
- Does the Server have a read timeout? Can the Tester’s deadlines protect Server-side resources?
- Why does a Vehicle Identification constant not mean discovery is implemented?
- How would you avoid presenting the teaching state policy as a complete Routing Activation specification?
- How should Gateway and Transport responsibilities be divided when adding ECU forwarding?

## Key Takeaways

- Each limitation has source evidence or is explicitly identified as unconfirmed.
- Diagnostic forwarding, multiple clients, security mechanisms, UDP handling, and full conformance are not current features.
- Extensions require architecture, state, error-policy, and test work—not just constants.
- Do not claim certification, production readiness, or complete ISO 13400 support.
