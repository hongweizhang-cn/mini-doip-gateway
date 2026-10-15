#ifndef DOIP_GATEWAY_H
#define DOIP_GATEWAY_H

#include <stddef.h>
#include <stdint.h>

#include "doip_routing_activation_state.h"
#include "doip_types.h"

/*
 * Single-threaded core state for one simplified Gateway instance. The
 * conflict Response Code is explicit configuration; this module does not
 * claim it is the correct response for every standard or deployment.
 */
typedef struct {
    doip_routing_activation_context_t routing_activation;
    uint16_t doip_entity_logical_address;
    uint8_t conflict_response_code;
} doip_gateway_t;

/*
 * Initialize Gateway configuration and reset Routing Activation state.
 * conflict_response_code is returned for a different client while another
 * client is active. No protocol policy is inferred for that value.
 */
doip_result_t doip_gateway_init(
    doip_gateway_t *gateway,
    uint16_t doip_entity_logical_address,
    uint8_t conflict_response_code);

/*
 * Clear only the Routing Activation state, retaining Gateway configuration.
 * Safe to call repeatedly.
 */
doip_result_t doip_gateway_reset(doip_gateway_t *gateway);

/*
 * Process exactly one complete DoIP message. Routing Activation Request is
 * the only supported request type. On success, output_message contains the
 * complete encoded Response and *output_size its byte length. When non-NULL,
 * output_size is set to zero on entry and remains zero on failure.
 *
 * Input is borrowed and may overlap output_message because it is fully
 * decoded before the response is written, but neither message buffer may
 * overlap gateway. output_size must be a separate size_t object and must not
 * overlap gateway or either message buffer. On validation or capacity
 * failure, output and activation state remain unchanged. After successful
 * state processing, the response encoder is called with prevalidated,
 * fixed-size inputs and sufficient capacity.
 *
 * This component is single-threaded, handles one client context, and does not
 * implement transport, connection management, or a complete DoIP state
 * machine.
 */
doip_result_t doip_gateway_process_message(
    doip_gateway_t *gateway,
    const uint8_t *input_message,
    size_t input_message_size,
    uint8_t *output_message,
    size_t output_message_capacity,
    size_t *output_size);

#endif /* DOIP_GATEWAY_H */
