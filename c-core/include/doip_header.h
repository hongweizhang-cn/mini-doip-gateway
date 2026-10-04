#ifndef DOIP_HEADER_H
#define DOIP_HEADER_H

#include <stddef.h>
#include <stdint.h>

#include "doip_types.h"

/*
 * ISO 13400-2 Generic Header is 8 bytes on the wire.
 * doip_header_t is an in-memory representation only. Do not memcpy it as a
 * network message, and do not rely on its struct layout or padding.
 */
#define DOIP_GENERIC_HEADER_SIZE ((size_t)8)

typedef struct {
    uint8_t protocol_version;
    uint8_t inverse_protocol_version;
    uint16_t payload_type;
    uint32_t payload_length;
} doip_header_t;

doip_result_t doip_header_encode(
    const doip_header_t *header,
    uint8_t *buffer,
    size_t buffer_size);

doip_result_t doip_header_decode(
    const uint8_t *buffer,
    size_t buffer_size,
    doip_header_t *header);

#endif /* DOIP_HEADER_H */
