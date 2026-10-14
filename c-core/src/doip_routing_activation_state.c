#include "doip_routing_activation_state.h"

doip_result_t doip_routing_activation_state_reset(
    doip_routing_activation_context_t *context)
{
    if (context == NULL) {
        return DOIP_RESULT_INVALID_ARGUMENT;
    }

    context->state = DOIP_ROUTING_ACTIVATION_STATE_INACTIVE;
    context->client_logical_address = 0u;

    return DOIP_RESULT_OK;
}

doip_result_t doip_routing_activation_state_process(
    doip_routing_activation_context_t *context,
    const doip_routing_activation_request_t *request,
    uint8_t conflict_response_code,
    uint8_t *response_code)
{
    if (response_code != NULL) {
        *response_code = 0u;
    }

    if ((context == NULL) || (request == NULL) || (response_code == NULL)) {
        return DOIP_RESULT_INVALID_ARGUMENT;
    }

    if ((context->state != DOIP_ROUTING_ACTIVATION_STATE_INACTIVE) &&
        (context->state != DOIP_ROUTING_ACTIVATION_STATE_ACTIVE)) {
        return DOIP_RESULT_INVALID_ARGUMENT;
    }

    if (context->state == DOIP_ROUTING_ACTIVATION_STATE_INACTIVE) {
        context->client_logical_address = request->source_address;
        context->state = DOIP_ROUTING_ACTIVATION_STATE_ACTIVE;
        *response_code = DOIP_ROUTING_ACTIVATION_RESPONSE_CODE_SUCCESS;
        return DOIP_RESULT_OK;
    }

    if (context->client_logical_address == request->source_address) {
        *response_code = DOIP_ROUTING_ACTIVATION_RESPONSE_CODE_SUCCESS;
        return DOIP_RESULT_OK;
    }

    *response_code = conflict_response_code;
    return DOIP_RESULT_OK;
}

doip_result_t doip_routing_activation_state_get(
    const doip_routing_activation_context_t *context,
    doip_routing_activation_state_t *state,
    uint16_t *client_logical_address)
{
    if (state != NULL) {
        *state = DOIP_ROUTING_ACTIVATION_STATE_INACTIVE;
    }
    if (client_logical_address != NULL) {
        *client_logical_address = 0u;
    }

    if ((context == NULL) || (state == NULL) ||
        (client_logical_address == NULL)) {
        return DOIP_RESULT_INVALID_ARGUMENT;
    }

    if ((context->state != DOIP_ROUTING_ACTIVATION_STATE_INACTIVE) &&
        (context->state != DOIP_ROUTING_ACTIVATION_STATE_ACTIVE)) {
        return DOIP_RESULT_INVALID_ARGUMENT;
    }

    *state = context->state;
    if (context->state == DOIP_ROUTING_ACTIVATION_STATE_ACTIVE) {
        *client_logical_address = context->client_logical_address;
    }

    return DOIP_RESULT_OK;
}
