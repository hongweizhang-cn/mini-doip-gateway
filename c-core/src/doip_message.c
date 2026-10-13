#include "doip_message.h"

#include "doip_payload.h"

doip_result_t doip_message_encode(
    const doip_header_t *header,
    const uint8_t *payload,
    size_t payload_size,
    uint8_t *buffer,
    size_t buffer_size)
{
    doip_result_t result;

    if ((header == NULL) || (buffer == NULL)) {
        return DOIP_RESULT_INVALID_ARGUMENT;
    }

    if (payload_size > UINT32_MAX) {
        return DOIP_RESULT_INVALID_LENGTH;
    }

    if (header->payload_length != payload_size) {
        return DOIP_RESULT_INVALID_LENGTH;
    }

    if ((payload_size > 0u) && (payload == NULL)) {
        return DOIP_RESULT_INVALID_ARGUMENT;
    }

    if (buffer_size < DOIP_GENERIC_HEADER_SIZE) {
        return DOIP_RESULT_BUFFER_TOO_SMALL;
    }

    if (payload_size > (buffer_size - DOIP_GENERIC_HEADER_SIZE)) {
        return DOIP_RESULT_BUFFER_TOO_SMALL;
    }

    result = doip_header_encode(header, buffer, buffer_size);
    if (result != DOIP_RESULT_OK) {
        return result;
    }

    return doip_payload_copy(
        payload,
        header->payload_length,
        buffer + DOIP_GENERIC_HEADER_SIZE,
        buffer_size - DOIP_GENERIC_HEADER_SIZE);
}

doip_result_t doip_message_decode(
    const uint8_t *buffer,
    size_t buffer_size,
    doip_message_view_t *message)
{
    doip_header_t decoded_header;
    doip_message_view_t decoded_message;
    size_t available_payload_size;
    doip_result_t result;

    if ((buffer == NULL) || (message == NULL)) {
        return DOIP_RESULT_INVALID_ARGUMENT;
    }

    if (buffer_size < DOIP_GENERIC_HEADER_SIZE) {
        return DOIP_RESULT_BUFFER_TOO_SMALL;
    }

    result = doip_header_decode(buffer, buffer_size, &decoded_header);
    if (result != DOIP_RESULT_OK) {
        return result;
    }

    available_payload_size = buffer_size - DOIP_GENERIC_HEADER_SIZE;
    if (available_payload_size < decoded_header.payload_length) {
        return DOIP_RESULT_BUFFER_TOO_SMALL;
    }

    if (available_payload_size > decoded_header.payload_length) {
        return DOIP_RESULT_INVALID_LENGTH;
    }

    decoded_message.header = decoded_header;
    decoded_message.payload = buffer + DOIP_GENERIC_HEADER_SIZE;
    *message = decoded_message;

    return DOIP_RESULT_OK;
}
