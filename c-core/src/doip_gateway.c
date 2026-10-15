#include "doip_gateway.h"

#include "doip_header.h"
#include "doip_message.h"
#include "doip_payload_type.h"
#include "doip_routing_activation.h"
#include "doip_routing_activation_request.h"
#include "doip_routing_activation_response.h"

#define DOIP_GATEWAY_RESPONSE_MESSAGE_SIZE \
    (DOIP_GENERIC_HEADER_SIZE + DOIP_ROUTING_ACTIVATION_RESPONSE_PAYLOAD_SIZE)

doip_result_t doip_gateway_init(
    doip_gateway_t *gateway,
    uint16_t doip_entity_logical_address,
    uint8_t conflict_response_code)
{
    doip_result_t result;

    if (gateway == NULL) {
        return DOIP_RESULT_INVALID_ARGUMENT;
    }

    result = doip_routing_activation_state_reset(&gateway->routing_activation);
    if (result != DOIP_RESULT_OK) {
        return result;
    }

    gateway->doip_entity_logical_address = doip_entity_logical_address;
    gateway->conflict_response_code = conflict_response_code;

    return DOIP_RESULT_OK;
}

doip_result_t doip_gateway_reset(doip_gateway_t *gateway)
{
    if (gateway == NULL) {
        return DOIP_RESULT_INVALID_ARGUMENT;
    }

    return doip_routing_activation_state_reset(&gateway->routing_activation);
}

doip_result_t doip_gateway_process_message(
    doip_gateway_t *gateway,
    const uint8_t *input_message,
    size_t input_message_size,
    uint8_t *output_message,
    size_t output_message_capacity,
    size_t *output_size)
{
    doip_message_view_t request_message;
    doip_routing_activation_request_t request;
    doip_routing_activation_response_parameters_t response_parameters;
    uint8_t response_code;
    doip_result_t result;

    if (output_size != NULL) {
        *output_size = 0u;
    }

    if ((gateway == NULL) ||
        (input_message == NULL) ||
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

    /*
     * The response size is fixed. Check capacity before processing the state
     * so a short output buffer cannot activate or otherwise alter the client.
     */
    if (output_message_capacity < DOIP_GATEWAY_RESPONSE_MESSAGE_SIZE) {
        return DOIP_RESULT_BUFFER_TOO_SMALL;
    }

    result = doip_routing_activation_state_process(
        &gateway->routing_activation,
        &request,
        gateway->conflict_response_code,
        &response_code);
    if (result != DOIP_RESULT_OK) {
        return result;
    }

    response_parameters.doip_entity_logical_address =
        gateway->doip_entity_logical_address;
    response_parameters.response_code = response_code;

    result = doip_routing_activation_encode_response(
        &request,
        &response_parameters,
        output_message,
        output_message_capacity,
        output_size);
    if (result != DOIP_RESULT_OK) {
        return result;
    }

    return DOIP_RESULT_OK;
}
