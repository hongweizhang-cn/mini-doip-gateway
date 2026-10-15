#ifndef DOIP_ROUTING_ACTIVATION_STATE_H
#define DOIP_ROUTING_ACTIVATION_STATE_H

#include <stdint.h>

#include "doip_routing_activation_request.h"
#include "doip_types.h"

/*
 * Routing Activation Response Code for successful activation.
 * This value is already used by the project's Routing Activation tests.
 */
#define DOIP_ROUTING_ACTIVATION_RESPONSE_CODE_SUCCESS ((uint8_t)0x10u)

typedef enum {
    DOIP_ROUTING_ACTIVATION_STATE_INACTIVE = 0,
    DOIP_ROUTING_ACTIVATION_STATE_ACTIVE
} doip_routing_activation_state_t;

/*
 * State for one logical client context only. Initialize the context before
 * any other operation. Inactive contexts have client_logical_address == 0.
 */
typedef struct {
    doip_routing_activation_state_t state;
    uint16_t client_logical_address;
} doip_routing_activation_context_t;

/*
 * Initialize or reset one context to the inactive state and clear its logical
 * address. Repeated resets are safe.
 */
doip_result_t doip_routing_activation_state_reset(
    doip_routing_activation_context_t *activation_context);

/*
 * Process an already-decoded request. A first request activates the context;
 * a repeated request from the same address is idempotently successful. If a
 * different address requests activation while active, reject it using the
 * caller-supplied conflict_response_code without changing the context.
 *
 * The activation type is not validated by this simplified state component.
 * response_code is set to zero on entry when non-NULL, and receives the
 * protocol Response Code only when the function returns DOIP_RESULT_OK.
 * response_code must not overlap activation_context or activation_request.
 */
doip_result_t doip_routing_activation_state_process_request(
    doip_routing_activation_context_t *activation_context,
    const doip_routing_activation_request_t *activation_request,
    uint8_t conflict_response_code,
    uint8_t *response_code);

/*
 * Query the context. Output values are initialized to INACTIVE and address 0
 * before validation. For an inactive context, the returned address is zero.
 */
doip_result_t doip_routing_activation_state_get(
    const doip_routing_activation_context_t *activation_context,
    doip_routing_activation_state_t *state,
    uint16_t *client_logical_address);

#endif /* DOIP_ROUTING_ACTIVATION_STATE_H */
