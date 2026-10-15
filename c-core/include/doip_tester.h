#ifndef DOIP_TESTER_H
#define DOIP_TESTER_H

#include <stdint.h>

#include "doip_routing_activation_response.h"
#include "doip_types.h"

/*
 * Connect to one IPv4 DoIP server, perform Routing Activation, and verify the
 * response. The call initializes and cleans up its own Winsock reference.
 * The response output is zeroed on entry and populated after successful Body
 * decoding, including when the Response Code indicates activation failure.
 * DOIP_RESULT_OK means the response code is the project's success code 0x10;
 * other valid response codes return DOIP_RESULT_UNSUPPORTED.
 *
 * This implementation is available on Windows and supports only the basic
 * 9-byte Routing Activation Response Body. Network operations have bounded
 * timeouts; the 4096-byte received-Payload limit is a local safety limit, not
 * a claim about the DoIP standard's general maximum. response must not overlap
 * any other argument storage.
 */
doip_result_t doip_tester_run(
    const char *server_ipv4,
    uint16_t server_port,
    uint16_t tester_logical_address,
    uint16_t doip_entity_logical_address,
    doip_routing_activation_response_t *response);

#endif /* DOIP_TESTER_H */
