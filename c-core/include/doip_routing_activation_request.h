#ifndef DOIP_ROUTING_ACTIVATION_REQUEST_H
#define DOIP_ROUTING_ACTIVATION_REQUEST_H

#include <stddef.h>
#include <stdint.h>

#include "doip_types.h"

/*
 * Minimal DoIP Routing Activation Request payload definition for the basic
 * 7-byte payload form without OEM-specific data.
 *
 * Wire layout (payload body only, big-endian):
 *   bytes 0-1: source_address
 *   byte 2: activation_type
 *   bytes 3-6: ISO reserved, fixed zero bytes for this implementation
 *
 * This module intentionally does not support OEM-specific data extensions or
 * higher-level Routing Activation state handling.
 */
#define DOIP_ROUTING_ACTIVATION_REQUEST_PAYLOAD_SIZE ((size_t)7u)

typedef struct {
    uint16_t source_address;
    uint8_t activation_type;
} doip_routing_activation_request_t;

doip_result_t doip_routing_activation_request_encode(
    const doip_routing_activation_request_t *request,
    uint8_t *buffer,
    size_t buffer_size);

doip_result_t doip_routing_activation_request_decode(
    const uint8_t *buffer,
    size_t buffer_size,
    doip_routing_activation_request_t *request);

#endif /* DOIP_ROUTING_ACTIVATION_REQUEST_H */
