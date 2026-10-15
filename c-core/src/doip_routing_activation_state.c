#include "doip_routing_activation_state.h"

doip_result_t doip_routing_activation_state_reset(
    doip_routing_activation_context_t *activation_context)
{
    if (activation_context == NULL) {
        return DOIP_RESULT_INVALID_ARGUMENT;
    }

    activation_context->state = DOIP_ROUTING_ACTIVATION_STATE_INACTIVE;
    activation_context->client_logical_address = 0u;

    return DOIP_RESULT_OK;
}

doip_result_t doip_routing_activation_state_process_request(
    doip_routing_activation_context_t *activation_context,
    const doip_routing_activation_request_t *activation_request,
    uint8_t conflict_response_code,
    uint8_t *response_code)
{
    if (response_code != NULL) {
        *response_code = 0u;
    }

    if ((activation_context == NULL) ||
        (activation_request == NULL) ||
        (response_code == NULL)) {
        return DOIP_RESULT_INVALID_ARGUMENT;
    }

    if ((activation_context->state != DOIP_ROUTING_ACTIVATION_STATE_INACTIVE) &&
        (activation_context->state != DOIP_ROUTING_ACTIVATION_STATE_ACTIVE)) {
        return DOIP_RESULT_INVALID_ARGUMENT;
    }

    if (activation_context->state == DOIP_ROUTING_ACTIVATION_STATE_INACTIVE) {
        activation_context->client_logical_address =
            activation_request->source_address;
        activation_context->state = DOIP_ROUTING_ACTIVATION_STATE_ACTIVE;
        *response_code = DOIP_ROUTING_ACTIVATION_RESPONSE_CODE_SUCCESS;
        return DOIP_RESULT_OK;
    }

    if (activation_context->client_logical_address ==
        activation_request->source_address) {
        *response_code = DOIP_ROUTING_ACTIVATION_RESPONSE_CODE_SUCCESS;
        return DOIP_RESULT_OK;
    }

    *response_code = conflict_response_code;
    return DOIP_RESULT_OK;
}

doip_result_t doip_routing_activation_state_get(
    const doip_routing_activation_context_t *activation_context,
    doip_routing_activation_state_t *state,
    uint16_t *client_logical_address)
{
    if (state != NULL) {
        *state = DOIP_ROUTING_ACTIVATION_STATE_INACTIVE;
    }
    if (client_logical_address != NULL) {
        *client_logical_address = 0u;
    }

    if ((activation_context == NULL) || (state == NULL) ||
        (client_logical_address == NULL)) {
        return DOIP_RESULT_INVALID_ARGUMENT;
    }

    if ((activation_context->state != DOIP_ROUTING_ACTIVATION_STATE_INACTIVE) &&
        (activation_context->state != DOIP_ROUTING_ACTIVATION_STATE_ACTIVE)) {
        return DOIP_RESULT_INVALID_ARGUMENT;
    }

    *state = activation_context->state;
    if (activation_context->state == DOIP_ROUTING_ACTIVATION_STATE_ACTIVE) {
        *client_logical_address = activation_context->client_logical_address;
    }

    return DOIP_RESULT_OK;
}
