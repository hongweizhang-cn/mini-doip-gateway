#include "doip_routing_activation_request.h"

doip_result_t doip_routing_activation_request_encode(
    const doip_routing_activation_request_t *request,
    uint8_t *buffer,
    size_t buffer_size)
{
    if ((request == NULL) || (buffer == NULL)) {
        return DOIP_RESULT_INVALID_ARGUMENT;
    }

    if (buffer_size < DOIP_ROUTING_ACTIVATION_REQUEST_PAYLOAD_SIZE) {
        return DOIP_RESULT_BUFFER_TOO_SMALL;
    }

    buffer[0] = (uint8_t)((request->source_address >> 8) & 0xFFu);
    buffer[1] = (uint8_t)(request->source_address & 0xFFu);
    buffer[2] = request->activation_type;
    buffer[3] = 0x00u;
    buffer[4] = 0x00u;
    buffer[5] = 0x00u;
    buffer[6] = 0x00u;

    return DOIP_RESULT_OK;
}

doip_result_t doip_routing_activation_request_decode(
    const uint8_t *buffer,
    size_t buffer_size,
    doip_routing_activation_request_t *request)
{
    doip_routing_activation_request_t decoded;

    if ((buffer == NULL) || (request == NULL)) {
        return DOIP_RESULT_INVALID_ARGUMENT;
    }

    if (buffer_size < DOIP_ROUTING_ACTIVATION_REQUEST_PAYLOAD_SIZE) {
        return DOIP_RESULT_BUFFER_TOO_SMALL;
    }

    if (buffer_size != DOIP_ROUTING_ACTIVATION_REQUEST_PAYLOAD_SIZE) {
        return DOIP_RESULT_UNSUPPORTED;
    }

    if ((buffer[3] != 0x00u) ||
        (buffer[4] != 0x00u) ||
        (buffer[5] != 0x00u) ||
        (buffer[6] != 0x00u)) {
        return DOIP_RESULT_INVALID_HEADER;
    }

    decoded.source_address = (uint16_t)
        (((uint16_t)buffer[0] << 8) | (uint16_t)buffer[1]);
    decoded.activation_type = buffer[2];

    request->source_address = decoded.source_address;
    request->activation_type = decoded.activation_type;

    return DOIP_RESULT_OK;
}
