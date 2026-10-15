#include "doip_tester.h"

#include <stdio.h>
#include <string.h>

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include "doip_gateway.h"
#include "doip_header.h"
#include "doip_message.h"
#include "doip_payload_type.h"
#include "doip_routing_activation_request.h"
#include "doip_routing_activation_response.h"
#include "doip_tcp_server.h"

#define TESTER_LOGICAL_ADDRESS ((uint16_t)0x0E00u)
#define ENTITY_LOGICAL_ADDRESS ((uint16_t)0x1000u)
#define RESPONSE_MESSAGE_SIZE \
    (DOIP_GENERIC_HEADER_SIZE + DOIP_ROUTING_ACTIVATION_RESPONSE_PAYLOAD_SIZE)
#define TEST_REQUEST_MESSAGE_SIZE \
    (DOIP_GENERIC_HEADER_SIZE + DOIP_ROUTING_ACTIVATION_REQUEST_PAYLOAD_SIZE)

typedef enum {
    FAKE_PEER_VALID_REJECTION = 0,
    FAKE_PEER_INVALID_HEADER,
    FAKE_PEER_OVERSIZED_PAYLOAD,
    FAKE_PEER_PARTIAL_HEADER,
    FAKE_PEER_PARTIAL_PAYLOAD,
    FAKE_PEER_INVALID_PAYLOAD_LENGTH,
    FAKE_PEER_WRONG_PAYLOAD_TYPE,
    FAKE_PEER_MISMATCHED_ADDRESS
} fake_peer_mode_t;

typedef struct {
    SOCKET listen_socket;
    uint16_t port;
    HANDLE thread;
    fake_peer_mode_t mode;
    uint8_t response_message[RESPONSE_MESSAGE_SIZE];
    size_t response_size;
    int thread_result;
} fake_peer_t;

typedef struct {
    doip_tcp_server_t server;
    doip_result_t result;
} gateway_server_thread_t;

static int send_all(SOCKET socket_handle, const uint8_t *bytes, size_t size)
{
    size_t sent = 0u;

    while (sent < size) {
        int amount = send(
            socket_handle, (const char *)bytes + sent, (int)(size - sent), 0);
        if (amount <= 0) {
            return 1;
        }
        sent += (size_t)amount;
    }
    return 0;
}

static int receive_exact(SOCKET socket_handle, uint8_t *bytes, size_t size)
{
    size_t received = 0u;

    while (received < size) {
        int amount = recv(
            socket_handle, (char *)bytes + received, (int)(size - received), 0);
        if (amount <= 0) {
            return 1;
        }
        received += (size_t)amount;
    }
    return 0;
}

static int make_response(
    uint8_t response_code,
    uint16_t payload_type,
    uint8_t *message,
    size_t capacity,
    size_t *message_size)
{
    doip_routing_activation_response_t body;
    doip_header_t header;
    uint8_t payload[DOIP_ROUTING_ACTIVATION_RESPONSE_PAYLOAD_SIZE];

    body.client_logical_address = TESTER_LOGICAL_ADDRESS;
    body.doip_entity_logical_address = ENTITY_LOGICAL_ADDRESS;
    body.response_code = response_code;
    if (doip_routing_activation_response_encode(
            &body, payload, sizeof(payload)) != DOIP_RESULT_OK) {
        return 1;
    }

    header.protocol_version = DOIP_PROTOCOL_VERSION;
    header.inverse_protocol_version = DOIP_PROTOCOL_VERSION_INVERSE;
    header.payload_type = payload_type;
    header.payload_length = (uint32_t)sizeof(payload);
    if (doip_message_encode(
            &header, payload, sizeof(payload), message, capacity) !=
        DOIP_RESULT_OK) {
        return 1;
    }
    *message_size = RESPONSE_MESSAGE_SIZE;
    return 0;
}

static DWORD WINAPI run_fake_peer(LPVOID argument)
{
    fake_peer_t *peer = (fake_peer_t *)argument;
    SOCKET client_socket;
    uint8_t request_message[TEST_REQUEST_MESSAGE_SIZE];
    size_t response_bytes;
    fd_set listen_set;
    struct timeval accept_timeout;
    DWORD io_timeout = 5000u;

    FD_ZERO(&listen_set);
    FD_SET(peer->listen_socket, &listen_set);
    accept_timeout.tv_sec = 5;
    accept_timeout.tv_usec = 0;
    if (select(0, &listen_set, NULL, NULL, &accept_timeout) <= 0) {
        peer->thread_result = 1;
        return 1u;
    }
    client_socket = accept(peer->listen_socket, NULL, NULL);
    if (client_socket == INVALID_SOCKET) {
        peer->thread_result = 2;
        return 2u;
    }
    setsockopt(
        client_socket, SOL_SOCKET, SO_RCVTIMEO,
        (const char *)&io_timeout, (int)sizeof(io_timeout));

    if (receive_exact(client_socket, request_message, sizeof(request_message)) != 0) {
        peer->thread_result = 3;
        closesocket(client_socket);
        return 3u;
    }

    response_bytes = peer->response_size;
    if (peer->mode == FAKE_PEER_PARTIAL_HEADER) {
        response_bytes = 4u;
    } else if (peer->mode == FAKE_PEER_PARTIAL_PAYLOAD) {
        response_bytes = DOIP_GENERIC_HEADER_SIZE + 3u;
    }
    if (send_all(client_socket, peer->response_message, response_bytes) != 0) {
        peer->thread_result = 4;
    }
    shutdown(client_socket, SD_BOTH);
    closesocket(client_socket);
    return (DWORD)peer->thread_result;
}

static int start_fake_peer(fake_peer_t *peer, fake_peer_mode_t mode)
{
    struct sockaddr_in address;
    int address_size = (int)sizeof(address);
    int reuse_address = 1;
    doip_header_t header;

    memset(peer, 0, sizeof(*peer));
    peer->listen_socket = INVALID_SOCKET;
    peer->mode = mode;
    peer->listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (peer->listen_socket == INVALID_SOCKET) {
        return 1;
    }
    setsockopt(
        peer->listen_socket, SOL_SOCKET, SO_REUSEADDR,
        (const char *)&reuse_address, (int)sizeof(reuse_address));
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = 0u;
    InetPtonA(AF_INET, "127.0.0.1", &address.sin_addr);
    if ((bind(peer->listen_socket, (const struct sockaddr *)&address,
              (int)sizeof(address)) == SOCKET_ERROR) ||
        (listen(peer->listen_socket, 1) == SOCKET_ERROR) ||
        (getsockname(peer->listen_socket, (struct sockaddr *)&address,
                     &address_size) == SOCKET_ERROR)) {
        closesocket(peer->listen_socket);
        peer->listen_socket = INVALID_SOCKET;
        return 1;
    }
    peer->port = ntohs(address.sin_port);

    header.protocol_version = DOIP_PROTOCOL_VERSION;
    header.inverse_protocol_version = DOIP_PROTOCOL_VERSION_INVERSE;
    header.payload_type = DOIP_PAYLOAD_TYPE_ROUTING_ACTIVATION_RESPONSE;
    header.payload_length = (uint32_t)DOIP_ROUTING_ACTIVATION_RESPONSE_PAYLOAD_SIZE;

    if (mode == FAKE_PEER_INVALID_HEADER) {
        memset(peer->response_message, 0, DOIP_GENERIC_HEADER_SIZE);
        peer->response_message[0] = 0x03u;
        peer->response_message[1] = 0xFCu;
        peer->response_message[2] = 0x00u;
        peer->response_message[3] = 0x06u;
        peer->response_size = DOIP_GENERIC_HEADER_SIZE;
    } else if (mode == FAKE_PEER_OVERSIZED_PAYLOAD) {
        header.payload_length = 4097u;
        if (doip_header_encode(
                &header, peer->response_message,
                DOIP_GENERIC_HEADER_SIZE) != DOIP_RESULT_OK) {
            return 1;
        }
        peer->response_size = DOIP_GENERIC_HEADER_SIZE;
    } else if (mode == FAKE_PEER_INVALID_PAYLOAD_LENGTH) {
        header.payload_length = 8u;
        if (doip_header_encode(
                &header, peer->response_message,
                DOIP_GENERIC_HEADER_SIZE) != DOIP_RESULT_OK) {
            return 1;
        }
        peer->response_size = DOIP_GENERIC_HEADER_SIZE;
    } else {
        uint16_t payload_type = mode == FAKE_PEER_WRONG_PAYLOAD_TYPE
            ? DOIP_PAYLOAD_TYPE_ROUTING_ACTIVATION_REQUEST
            : DOIP_PAYLOAD_TYPE_ROUTING_ACTIVATION_RESPONSE;
        uint8_t code = mode == FAKE_PEER_VALID_REJECTION ? 0x03u : 0x10u;
        if (make_response(
                code, payload_type, peer->response_message,
                sizeof(peer->response_message), &peer->response_size) != 0) {
            return 1;
        }
        if (mode == FAKE_PEER_MISMATCHED_ADDRESS) {
            peer->response_message[DOIP_GENERIC_HEADER_SIZE + 1u] = 0x01u;
        }
    }

    peer->thread = CreateThread(NULL, 0u, run_fake_peer, peer, 0u, NULL);
    return peer->thread == NULL ? 1 : 0;
}

static int finish_fake_peer(fake_peer_t *peer)
{
    DWORD wait_result = WaitForSingleObject(peer->thread, 7000u);
    int failures = 0;

    if (wait_result != WAIT_OBJECT_0) {
        printf("FAIL: fake peer did not finish before timeout\n");
        failures++;
    } else if (peer->thread_result != 0) {
        printf("FAIL: fake peer reported error %d\n", peer->thread_result);
        failures++;
    }
    if (wait_result != WAIT_OBJECT_0) {
        shutdown(peer->listen_socket, SD_BOTH);
    }
    CloseHandle(peer->thread);
    closesocket(peer->listen_socket);
    return failures;
}

static doip_result_t run_tester(uint16_t port, doip_routing_activation_response_t *response)
{
    return doip_tester_run(
        "127.0.0.1", port, TESTER_LOGICAL_ADDRESS,
        ENTITY_LOGICAL_ADDRESS, response);
}

static DWORD WINAPI run_gateway_server(LPVOID argument)
{
    gateway_server_thread_t *thread_data =
        (gateway_server_thread_t *)argument;
    thread_data->result = doip_tcp_server_run(&thread_data->server);
    return 0u;
}

static int test_end_to_end_with_gateway(void)
{
    gateway_server_thread_t thread_data;
    HANDLE server_thread;
    doip_routing_activation_response_t response;
    doip_result_t result;
    int failures = 0;

    result = doip_tcp_server_init(
        &thread_data.server, "127.0.0.1", 0u,
        ENTITY_LOGICAL_ADDRESS, 0x03u);
    if (result != DOIP_RESULT_OK ||
        doip_tcp_server_start(&thread_data.server) != DOIP_RESULT_OK) {
        printf("FAIL: could not start Gateway TCP Server for Tester integration\n");
        return 1;
    }

    thread_data.result = DOIP_RESULT_UNSUPPORTED;
    server_thread = CreateThread(
        NULL, 0u, run_gateway_server, &thread_data, 0u, NULL);
    if (server_thread == NULL) {
        printf("FAIL: could not start Gateway service thread\n");
        doip_tcp_server_stop(&thread_data.server);
        return 1;
    }

    result = DOIP_RESULT_UNSUPPORTED;
    {
        DWORD start = GetTickCount();
        while ((DWORD)(GetTickCount() - start) < 2000u) {
            if (InterlockedCompareExchange(
                    &thread_data.server.lifecycle_state, 0, 0) == 2L) {
                break;
            }
            Sleep(10u);
        }
        if (InterlockedCompareExchange(
                &thread_data.server.lifecycle_state, 0, 0) != 2L) {
            printf("FAIL: Gateway service loop did not start\n");
            failures++;
        } else {
            result = run_tester(thread_data.server.bound_port, &response);
            if ((result != DOIP_RESULT_OK) ||
                (response.client_logical_address != TESTER_LOGICAL_ADDRESS) ||
                (response.doip_entity_logical_address != ENTITY_LOGICAL_ADDRESS) ||
                (response.response_code !=
                    DOIP_ROUTING_ACTIVATION_RESPONSE_CODE_SUCCESS)) {
                printf("FAIL: Tester did not verify Gateway activation response\n");
                failures++;
            }
        }
    }

    doip_tcp_server_request_stop(&thread_data.server);
    if (WaitForSingleObject(server_thread, 3000u) != WAIT_OBJECT_0) {
        printf("FAIL: Gateway service thread did not stop\n");
        failures++;
    } else if (thread_data.result != DOIP_RESULT_OK) {
        printf("FAIL: Gateway service returned %d\n", (int)thread_data.result);
        failures++;
    }
    CloseHandle(server_thread);
    return failures;
}

static int test_fake_response(
    fake_peer_mode_t mode,
    doip_result_t expected_result,
    uint8_t expected_response_code,
    const char *test_name)
{
    fake_peer_t peer;
    doip_routing_activation_response_t response;
    doip_result_t result;
    int failures = 0;

    if (start_fake_peer(&peer, mode) != 0) {
        printf("FAIL: %s: could not create fake TCP peer\n", test_name);
        return 1;
    }
    memset(&response, 0xA5, sizeof(response));
    result = run_tester(peer.port, &response);
    if (result != expected_result) {
        printf("FAIL: %s: expected result %d, got %d\n",
            test_name, (int)expected_result, (int)result);
        failures++;
    }
    if ((expected_response_code != 0u) &&
        (response.response_code != expected_response_code)) {
        printf("FAIL: %s: decoded response code was not retained\n", test_name);
        failures++;
    }
    failures += finish_fake_peer(&peer);
    return failures;
}

static int test_connect_failure(void)
{
    fake_peer_t unused_peer;
    SOCKET probe_socket;
    struct sockaddr_in address;
    int address_size = (int)sizeof(address);
    uint16_t unused_port;
    doip_routing_activation_response_t response;
    doip_result_t result;

    memset(&unused_peer, 0, sizeof(unused_peer));
    probe_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (probe_socket == INVALID_SOCKET) {
        return 1;
    }
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = 0u;
    InetPtonA(AF_INET, "127.0.0.1", &address.sin_addr);
    if ((bind(probe_socket, (const struct sockaddr *)&address,
              (int)sizeof(address)) == SOCKET_ERROR) ||
        (getsockname(probe_socket, (struct sockaddr *)&address,
                     &address_size) == SOCKET_ERROR)) {
        closesocket(probe_socket);
        printf("FAIL: could not reserve a dynamic test port\n");
        return 1;
    }
    unused_port = ntohs(address.sin_port);
    closesocket(probe_socket);

    result = run_tester(unused_port, &response);
    if (result == DOIP_RESULT_OK) {
        printf("FAIL: Tester succeeded without a listening server\n");
        return 1;
    }
    return 0;
}

int main(void)
{
    WSADATA winsock_data;
    int startup_result = WSAStartup(MAKEWORD(2, 2), &winsock_data);
    int failures = 0;

    if (startup_result != 0) {
        printf("FAIL: test WSAStartup returned %d\n", startup_result);
        return 1;
    }

    failures += test_end_to_end_with_gateway();
    failures += test_fake_response(
        FAKE_PEER_VALID_REJECTION, DOIP_RESULT_UNSUPPORTED, 0x03u,
        "non-success-response-code");
    failures += test_fake_response(
        FAKE_PEER_INVALID_HEADER, DOIP_RESULT_INVALID_HEADER, 0u,
        "invalid-header");
    failures += test_fake_response(
        FAKE_PEER_OVERSIZED_PAYLOAD, DOIP_RESULT_BUFFER_TOO_SMALL, 0u,
        "oversized-payload");
    failures += test_fake_response(
        FAKE_PEER_PARTIAL_HEADER, DOIP_RESULT_INVALID_LENGTH, 0u,
        "server-closed-during-header");
    failures += test_fake_response(
        FAKE_PEER_PARTIAL_PAYLOAD, DOIP_RESULT_INVALID_LENGTH, 0u,
        "server-closed-during-payload");
    failures += test_fake_response(
        FAKE_PEER_INVALID_PAYLOAD_LENGTH, DOIP_RESULT_INVALID_LENGTH, 0u,
        "invalid-payload-length");
    failures += test_fake_response(
        FAKE_PEER_WRONG_PAYLOAD_TYPE, DOIP_RESULT_UNSUPPORTED, 0u,
        "unexpected-payload-type");
    failures += test_fake_response(
        FAKE_PEER_MISMATCHED_ADDRESS, DOIP_RESULT_INVALID_HEADER, 0x10u,
        "mismatched-logical-address");
    failures += test_connect_failure();

    WSACleanup();
    if (failures != 0) {
        printf("%d Tester test(s) failed.\n", failures);
        return 1;
    }

    printf("All test_doip_tester tests passed.\n");
    return 0;
}
