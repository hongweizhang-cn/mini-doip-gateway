#ifndef DOIP_MESSAGE_H
#define DOIP_MESSAGE_H

#include <stddef.h>
#include <stdint.h>

#include "doip_header.h"

/*
 * A decoded message view borrows its Payload Body from the input buffer.
 * The payload pointer remains valid only while that input buffer remains valid.
 * For an empty Payload Body, payload points to the end of the Generic Header
 * and must not be dereferenced.
 */
typedef struct {
    doip_header_t header;
    const uint8_t *payload;
} doip_message_view_t;

/*
 * Encode one complete DoIP message. header->payload_length must equal
 * payload_size; payload_size is the Payload Body length and excludes the
 * Generic Header. A NULL payload is allowed only when payload_size is zero.
 * The caller must ensure that the source Payload Body and destination message
 * regions do not overlap.
 */
doip_result_t doip_message_encode(
    const doip_header_t *header,
    const uint8_t *payload,
    size_t payload_size,
    uint8_t *buffer,
    size_t buffer_size);

/*
 * Decode exactly one complete DoIP message. The input is not copied, and the
 * returned payload pointer borrows the input buffer and must not outlive it.
 * Trailing bytes beyond the single message are rejected.
 */
doip_result_t doip_message_decode(
    const uint8_t *buffer,
    size_t buffer_size,
    doip_message_view_t *message);

#endif /* DOIP_MESSAGE_H */
