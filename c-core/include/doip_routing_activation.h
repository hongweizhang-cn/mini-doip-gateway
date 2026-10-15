#ifndef DOIP_ROUTING_ACTIVATION_H
#define DOIP_ROUTING_ACTIVATION_H

#include <stddef.h>
#include <stdint.h>

#include "doip_routing_activation_request.h"
#include "doip_types.h"

typedef struct {
    uint16_t doip_entity_logical_address;
    uint8_t response_code;
} doip_routing_activation_response_parameters_t;

/*
 * Encode a Routing Activation Response for an already-decoded Request.
 * The Request and Response bodies are processed by their existing codecs;
 * the Generic Header and full message are processed by the existing message
 * codec. This function does not parse or validate a request message.
 *
 * output_message is caller-owned. output_size must point to a separate size_t
 * object and must not overlap any other argument.
 *
 * output_size is set to zero on entry when non-NULL. On failure the output
 * message buffer is unchanged. On success output_size receives the complete
 * encoded message size.
 */
doip_result_t doip_routing_activation_encode_response(
    const doip_routing_activation_request_t *request,
    const doip_routing_activation_response_parameters_t *response_parameters,
    uint8_t *output_message,
    size_t output_message_capacity,
    size_t *output_size);

/*
 * Compatibility/convenience entry point for callers with a complete Request
 * message. It decodes the message and Request Body once, then delegates
 * response construction to doip_routing_activation_encode_response().
 * Input and output message regions may overlap because the Request is decoded
 * before output is written. output_size must remain separate from all other
 * arguments.
 */
doip_result_t doip_routing_activation_handle_request(
    const uint8_t *input_message,
    size_t input_message_size,
    const doip_routing_activation_response_parameters_t *response_parameters,
    uint8_t *output_message,
    size_t output_message_capacity,
    size_t *output_size);

#endif /* DOIP_ROUTING_ACTIVATION_H */
