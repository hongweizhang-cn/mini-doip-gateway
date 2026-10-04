#include "doip_header.h"

/*
 * Wire format (ISO 13400-2 Generic Header, 8 bytes, big-endian):
 *
 *   Byte 0     Protocol Version
 *   Byte 1     Inverse Protocol Version
 *   Bytes 2-3  Payload Type
 *   Bytes 4-7  Payload Length
 */

doip_result_t doip_header_encode(
    const doip_header_t *header,
    uint8_t *buffer,
    size_t buffer_size)
{
    if ((header == NULL) || (buffer == NULL)) {
        return DOIP_RESULT_INVALID_ARGUMENT;
    }

    if (buffer_size < DOIP_GENERIC_HEADER_SIZE) {
        return DOIP_RESULT_BUFFER_TOO_SMALL;
    }

    buffer[0] = header->protocol_version;
    buffer[1] = header->inverse_protocol_version;
    buffer[2] = (uint8_t)((header->payload_type >> 8) & 0xFFu);
    buffer[3] = (uint8_t)(header->payload_type & 0xFFu);
    buffer[4] = (uint8_t)((header->payload_length >> 24) & 0xFFu);
    buffer[5] = (uint8_t)((header->payload_length >> 16) & 0xFFu);
    buffer[6] = (uint8_t)((header->payload_length >> 8) & 0xFFu);
    buffer[7] = (uint8_t)(header->payload_length & 0xFFu);

    return DOIP_RESULT_OK;
}

doip_result_t doip_header_decode(
    const uint8_t *buffer,
    size_t buffer_size,
    doip_header_t *header)
{
    if ((buffer == NULL) || (header == NULL)) {
        return DOIP_RESULT_INVALID_ARGUMENT;
    }

    if (buffer_size < DOIP_GENERIC_HEADER_SIZE) {
        return DOIP_RESULT_BUFFER_TOO_SMALL;
    }

    header->protocol_version = buffer[0];
    header->inverse_protocol_version = buffer[1];
    header->payload_type = (uint16_t)(
        ((uint16_t)buffer[2] << 8) | (uint16_t)buffer[3]);
    header->payload_length =
        ((uint32_t)buffer[4] << 24) |
        ((uint32_t)buffer[5] << 16) |
        ((uint32_t)buffer[6] << 8) |
        (uint32_t)buffer[7];

    return DOIP_RESULT_OK;
}
