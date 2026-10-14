#include "doip_routing_activation_response.h"

doip_result_t doip_routing_activation_response_encode(
    const doip_routing_activation_response_t *response,
    uint8_t *buffer,
    size_t buffer_size)
{
    if ((response == NULL) || (buffer == NULL)) {
        return DOIP_RESULT_INVALID_ARGUMENT;
    }

    if (buffer_size < DOIP_ROUTING_ACTIVATION_RESPONSE_PAYLOAD_SIZE) {
        return DOIP_RESULT_BUFFER_TOO_SMALL;
    }

    buffer[0] = (uint8_t)((response->client_logical_address >> 8) & 0xFFu);
    buffer[1] = (uint8_t)(response->client_logical_address & 0xFFu);
    buffer[2] = (uint8_t)((response->doip_entity_logical_address >> 8) & 0xFFu);
    buffer[3] = (uint8_t)(response->doip_entity_logical_address & 0xFFu);
    buffer[4] = response->response_code;
    buffer[5] = 0x00u;
    buffer[6] = 0x00u;
    buffer[7] = 0x00u;
    buffer[8] = 0x00u;

    return DOIP_RESULT_OK;
}

doip_result_t doip_routing_activation_response_decode(
    const uint8_t *buffer,
    size_t buffer_size,
    doip_routing_activation_response_t *response)
{
    doip_routing_activation_response_t decoded;

    if ((buffer == NULL) || (response == NULL)) {
        return DOIP_RESULT_INVALID_ARGUMENT;
    }

    if (buffer_size < DOIP_ROUTING_ACTIVATION_RESPONSE_PAYLOAD_SIZE) {
        return DOIP_RESULT_BUFFER_TOO_SMALL;
    }

    if (buffer_size == 13u) {
        return DOIP_RESULT_UNSUPPORTED;
    }

    if (buffer_size != DOIP_ROUTING_ACTIVATION_RESPONSE_PAYLOAD_SIZE) {
        return DOIP_RESULT_INVALID_LENGTH;
    }

    if ((buffer[5] != 0x00u) ||
        (buffer[6] != 0x00u) ||
        (buffer[7] != 0x00u) ||
        (buffer[8] != 0x00u)) {
        return DOIP_RESULT_INVALID_HEADER;
    }

    decoded.client_logical_address = (uint16_t)
        (((uint16_t)buffer[0] << 8) | (uint16_t)buffer[1]);
    decoded.doip_entity_logical_address = (uint16_t)
        (((uint16_t)buffer[2] << 8) | (uint16_t)buffer[3]);
    decoded.response_code = buffer[4];

    *response = decoded;
    return DOIP_RESULT_OK;
}
