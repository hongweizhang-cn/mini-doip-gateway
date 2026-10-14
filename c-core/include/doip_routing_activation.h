#ifndef DOIP_ROUTING_ACTIVATION_H
#define DOIP_ROUTING_ACTIVATION_H

#include <stddef.h>
#include <stdint.h>

#include "doip_types.h"

typedef struct {
    uint16_t doip_entity_logical_address;
    uint8_t response_code;
} doip_routing_activation_response_parameters_t;

/*
 * Handle one complete Routing Activation Request message and encode its
 * corresponding Response message. The Request and Response bodies are
 * processed by their existing codecs; the Generic Header and full message are
 * processed by the existing message codec.
 *
 * input_message is borrowed for the duration of the call. output_message is
 * caller-owned. The output may overlap the input because the response is
 * assembled in a local buffer before being written. output_size must point to
 * a separate size_t object and must not overlap any other argument.
 *
 * output_size is set to zero on entry when non-NULL. On failure the output
 * message buffer is unchanged. On success output_size receives the complete
 * encoded message size.
 */
doip_result_t doip_routing_activation_handle_request(
    const uint8_t *input_message,
    size_t input_message_size,
    const doip_routing_activation_response_parameters_t *response_parameters,
    uint8_t *output_message,
    size_t output_message_capacity,
    size_t *output_size);

#endif /* DOIP_ROUTING_ACTIVATION_H */
