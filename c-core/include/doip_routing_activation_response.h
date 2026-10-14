#ifndef DOIP_ROUTING_ACTIVATION_RESPONSE_H
#define DOIP_ROUTING_ACTIVATION_RESPONSE_H

#include <stddef.h>
#include <stdint.h>

#include "doip_types.h"

/*
 * Basic DoIP Routing Activation Response Payload Body (9 bytes):
 *   bytes 0-1: client logical address, big-endian
 *   bytes 2-3: DoIP entity logical address, big-endian
 *   byte 4: response code, preserved without interpreting its meaning
 *   bytes 5-8: ISO Reserved, encoded as zero and required to be zero on decode
 *
 * This module does not support the 4-byte OEM-specific Data extension,
 * Routing Activation state handling, or connection management.
 * The source response object and destination buffer must not overlap.
 */
#define DOIP_ROUTING_ACTIVATION_RESPONSE_PAYLOAD_SIZE ((size_t)9u)

typedef struct {
    uint16_t client_logical_address;
    uint16_t doip_entity_logical_address;
    uint8_t response_code;
} doip_routing_activation_response_t;

doip_result_t doip_routing_activation_response_encode(
    const doip_routing_activation_response_t *response,
    uint8_t *buffer,
    size_t buffer_size);

doip_result_t doip_routing_activation_response_decode(
    const uint8_t *buffer,
    size_t buffer_size,
    doip_routing_activation_response_t *response);

#endif /* DOIP_ROUTING_ACTIVATION_RESPONSE_H */
