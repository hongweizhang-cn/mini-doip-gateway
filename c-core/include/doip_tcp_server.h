#ifndef DOIP_TCP_SERVER_H
#define DOIP_TCP_SERVER_H

#include <stddef.h>
#include <stdint.h>

#include "doip_gateway.h"
#include "doip_types.h"

#define DOIP_TCP_SERVER_MAX_PAYLOAD_SIZE ((size_t)4096u)
#define DOIP_TCP_SERVER_ADDRESS_SIZE ((size_t)16u)

/*
 * Windows-only, single-threaded DoIP TCP server state. Socket handles and
 * Gateway state are owned by this object after a successful start.
 */
typedef struct {
    char bind_address[DOIP_TCP_SERVER_ADDRESS_SIZE];
    uint16_t configured_port;
    uint16_t bound_port;
    uint16_t doip_entity_logical_address;
    uint8_t conflict_response_code;
    uintptr_t listen_socket;
    uintptr_t client_socket;
    volatile long stop_requested;
    volatile long lifecycle_state;
    int winsock_started;
    int last_socket_error;
    doip_gateway_t gateway;
} doip_tcp_server_t;

/*
 * Initialize server configuration. bind_address must be a numeric IPv4
 * address; port zero requests an OS-assigned port. No sockets are opened.
 */
doip_result_t doip_tcp_server_init(
    doip_tcp_server_t *server,
    const char *bind_address,
    uint16_t port,
    uint16_t doip_entity_logical_address,
    uint8_t conflict_response_code);

/*
 * Create, bind, and listen on the configured IPv4 endpoint. On success,
 * bound_port contains the selected port. A socket startup/bind/listen failure
 * returns DOIP_RESULT_UNSUPPORTED and records the Winsock error in
 * server->last_socket_error.
 */
doip_result_t doip_tcp_server_start(doip_tcp_server_t *server);

/*
 * Run the blocking, single-client service loop. It reads complete DoIP
 * messages with bounded buffers and dispatches them to Gateway Core.
 * Returns DOIP_RESULT_OK after a stop request; fatal listener errors return
 * DOIP_RESULT_UNSUPPORTED. This function owns cleanup of listening and
 * client sockets and Winsock startup state.
 */
doip_result_t doip_tcp_server_run(doip_tcp_server_t *server);

/*
 * Set the stop request without performing cleanup. Safe to call from a
 * Windows console control handler or another thread while run is active.
 */
doip_result_t doip_tcp_server_request_stop(doip_tcp_server_t *server);

/*
 * Request shutdown. If run is active, it performs cleanup before run returns;
 * this call itself does not wait for that cleanup. If start succeeded but run
 * has not begun, this call releases resources immediately. Do not call
 * concurrently with init or start.
 */
doip_result_t doip_tcp_server_stop(doip_tcp_server_t *server);

#endif
