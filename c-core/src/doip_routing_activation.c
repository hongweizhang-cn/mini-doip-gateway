#include "doip_routing_activation.h"

#include "doip_header.h"
#include "doip_message.h"
#include "doip_payload.h"
#include "doip_payload_type.h"
#include "doip_routing_activation_request.h"
#include "doip_routing_activation_response.h"

#define DOIP_ROUTING_ACTIVATION_RESPONSE_MESSAGE_SIZE \
    (DOIP_GENERIC_HEADER_SIZE + DOIP_ROUTING_ACTIVATION_RESPONSE_PAYLOAD_SIZE)

doip_result_t doip_routing_activation_handle_request(
    const uint8_t *input_message,
    size_t input_message_size,
    const doip_routing_activation_response_parameters_t *response_parameters,
    uint8_t *output_message,
    size_t output_message_capacity,
    size_t *output_size)
{
    doip_message_view_t request_message;
    doip_routing_activation_request_t request;
    doip_routing_activation_response_t response;
    doip_header_t response_header;
    uint8_t response_payload[DOIP_ROUTING_ACTIVATION_RESPONSE_PAYLOAD_SIZE];
    uint8_t encoded_message[DOIP_ROUTING_ACTIVATION_RESPONSE_MESSAGE_SIZE];
    doip_result_t result;

    if (output_size != NULL) {
        *output_size = 0u;
    }

    if ((input_message == NULL) ||
        (response_parameters == NULL) ||
        (output_message == NULL) ||
        (output_size == NULL)) {
        return DOIP_RESULT_INVALID_ARGUMENT;
    }

    result = doip_message_decode(input_message, input_message_size, &request_message);
    if (result != DOIP_RESULT_OK) {
        return result;
    }

    if (request_message.header.payload_type !=
        DOIP_PAYLOAD_TYPE_ROUTING_ACTIVATION_REQUEST) {
        return DOIP_RESULT_UNSUPPORTED;
    }

    result = doip_routing_activation_request_decode(
        request_message.payload,
        request_message.header.payload_length,
        &request);
    if (result != DOIP_RESULT_OK) {
        return result;
    }

    response.client_logical_address = request.source_address;
    response.doip_entity_logical_address =
        response_parameters->doip_entity_logical_address;
    response.response_code = response_parameters->response_code;

    result = doip_routing_activation_response_encode(
        &response,
        response_payload,
        sizeof(response_payload));
    if (result != DOIP_RESULT_OK) {
        return result;
    }

    response_header.protocol_version = 0x02u;
    response_header.inverse_protocol_version = 0xFDu;
    response_header.payload_type = DOIP_PAYLOAD_TYPE_ROUTING_ACTIVATION_RESPONSE;
    response_header.payload_length = (uint32_t)sizeof(response_payload);

    result = doip_message_encode(
        &response_header,
        response_payload,
        sizeof(response_payload),
        encoded_message,
        sizeof(encoded_message));
    if (result != DOIP_RESULT_OK) {
        return result;
    }

    if (output_message_capacity < sizeof(encoded_message)) {
        return DOIP_RESULT_BUFFER_TOO_SMALL;
    }

    result = doip_payload_copy(
        encoded_message,
        (uint32_t)sizeof(encoded_message),
        output_message,
        output_message_capacity);
    if (result != DOIP_RESULT_OK) {
        return result;
    }

    *output_size = sizeof(encoded_message);
    return DOIP_RESULT_OK;
}
