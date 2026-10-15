#include "doip_tcp_server.h"

#include <stdio.h>
#include <string.h>

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include "doip_header.h"
#include "doip_message.h"
#include "doip_payload_type.h"
#include "doip_routing_activation_request.h"
#include "doip_routing_activation_response.h"

#define TEST_REQUEST_SIZE \
    (DOIP_GENERIC_HEADER_SIZE + DOIP_ROUTING_ACTIVATION_REQUEST_PAYLOAD_SIZE)
#define TEST_RESPONSE_SIZE \
    (DOIP_GENERIC_HEADER_SIZE + DOIP_ROUTING_ACTIVATION_RESPONSE_PAYLOAD_SIZE)

typedef struct {
    doip_tcp_server_t *server;
    doip_result_t result;
} server_thread_arguments_t;

static int check_result(
    const char *test_name,
    doip_result_t expected,
    doip_result_t actual)
{
    if (expected != actual) {
        printf("FAIL: %s: expected result %d, got %d\n",
            test_name, (int)expected, (int)actual);
        return 1;
    }
    return 0;
}

static int build_request_message(
    uint16_t client_address,
    uint8_t *message,
    size_t message_capacity)
{
    doip_routing_activation_request_t request;
    doip_header_t header;
    uint8_t request_body[DOIP_ROUTING_ACTIVATION_REQUEST_PAYLOAD_SIZE];
    doip_result_t result;

    request.source_address = client_address;
    request.activation_type = 0x01u;
    result = doip_routing_activation_request_encode(
        &request, request_body, sizeof(request_body));
    if (result != DOIP_RESULT_OK) {
        return 1;
    }

    header.protocol_version = 0x02u;
    header.inverse_protocol_version = 0xFDu;
    header.payload_type = DOIP_PAYLOAD_TYPE_ROUTING_ACTIVATION_REQUEST;
    header.payload_length = (uint32_t)sizeof(request_body);
    return doip_message_encode(
        &header, request_body, sizeof(request_body),
        message, message_capacity) == DOIP_RESULT_OK ? 0 : 1;
}

static int send_all(SOCKET socket_handle, const uint8_t *bytes, size_t byte_count)
{
    size_t sent_total = 0u;

    while (sent_total < byte_count) {
        int sent_now = send(
            socket_handle,
            (const char *)bytes + sent_total,
            (int)(byte_count - sent_total),
            0);
        if (sent_now <= 0) {
            return 1;
        }
        sent_total += (size_t)sent_now;
    }
    return 0;
}

static int receive_exact(SOCKET socket_handle, uint8_t *bytes, size_t byte_count)
{
    size_t received_total = 0u;

    while (received_total < byte_count) {
        int received_now = recv(
            socket_handle,
            (char *)bytes + received_total,
            (int)(byte_count - received_total),
            0);
        if (received_now <= 0) {
            return 1;
        }
        received_total += (size_t)received_now;
    }
    return 0;
}

static SOCKET connect_to_server(uint16_t port)
{
    struct sockaddr_in server_address;
    SOCKET client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (client_socket == INVALID_SOCKET) {
        return INVALID_SOCKET;
    }

    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    InetPtonA(AF_INET, "127.0.0.1", &server_address.sin_addr);
    if (connect(
            client_socket,
            (const struct sockaddr *)&server_address,
            (int)sizeof(server_address)) == SOCKET_ERROR) {
        if (client_socket != INVALID_SOCKET) {
            closesocket(client_socket);
        }
        return INVALID_SOCKET;
    }

    {
        DWORD timeout_ms = 2000u;
        setsockopt(
            client_socket,
            SOL_SOCKET,
            SO_RCVTIMEO,
            (const char *)&timeout_ms,
            (int)sizeof(timeout_ms));
        setsockopt(
            client_socket,
            SOL_SOCKET,
            SO_SNDTIMEO,
            (const char *)&timeout_ms,
            (int)sizeof(timeout_ms));
    }
    return client_socket;
}

static DWORD WINAPI run_server_thread(LPVOID argument)
{
    server_thread_arguments_t *thread_arguments =
        (server_thread_arguments_t *)argument;

    thread_arguments->result = doip_tcp_server_run(thread_arguments->server);
    return 0u;
}

static int wait_for_server_thread(HANDLE thread, DWORD timeout_ms)
{
    DWORD wait_result = WaitForSingleObject(thread, timeout_ms);

    if (wait_result != WAIT_OBJECT_0) {
        printf("FAIL: server thread did not stop within %lu ms\n",
            (unsigned long)timeout_ms);
        return 1;
    }
    return 0;
}

static int wait_until_server_loop_running(
    const doip_tcp_server_t *server,
    DWORD timeout_ms)
{
    DWORD started_at = GetTickCount();

    while ((DWORD)(GetTickCount() - started_at) < timeout_ms) {
        if (InterlockedCompareExchange(
                (volatile LONG *)&server->lifecycle_state, 0, 0) == 2L) {
            return 0;
        }
        Sleep(10u);
    }

    printf("FAIL: service loop did not enter running state within %lu ms\n",
        (unsigned long)timeout_ms);
    return 1;
}

static int expect_connection_closed_after_header(
    uint16_t server_port,
    const uint8_t generic_header[DOIP_GENERIC_HEADER_SIZE],
    const char *test_name)
{
    SOCKET client_socket = connect_to_server(server_port);
    uint8_t unexpected_response_byte;
    int receive_result;

    if (client_socket == INVALID_SOCKET) {
        printf("FAIL: %s: could not connect to server\n", test_name);
        return 1;
    }
    Sleep(150u);
    if (send_all(
            client_socket, generic_header, DOIP_GENERIC_HEADER_SIZE) != 0) {
        printf("FAIL: %s: could not send Generic Header\n", test_name);
        closesocket(client_socket);
        return 1;
    }
    shutdown(client_socket, SD_SEND);

    receive_result = recv(
        client_socket, (char *)&unexpected_response_byte, 1, 0);
    {
        int receive_error = receive_result == SOCKET_ERROR
            ? WSAGetLastError()
            : 0;

        closesocket(client_socket);
        if ((receive_result == 0) ||
            ((receive_result == SOCKET_ERROR) &&
             ((receive_error == WSAECONNRESET) ||
              (receive_error == WSAECONNABORTED)))) {
            return 0;
        }
        printf("FAIL: %s: server did not close rejected connection "
            "(recv=%d, Winsock error=%d)\n",
            test_name, receive_result, receive_error);
        return 1;
    }
}

static int check_response(
    const uint8_t *response_message,
    size_t response_message_size,
    uint16_t expected_client_address)
{
    doip_message_view_t decoded_message;
    doip_routing_activation_response_t response_body;
    doip_result_t result;

    if (response_message_size != TEST_RESPONSE_SIZE) {
        printf("FAIL: response has unexpected complete-message length\n");
        return 1;
    }

    result = doip_message_decode(
        response_message, response_message_size, &decoded_message);
    if (check_result("decode_response_message", DOIP_RESULT_OK, result) != 0) {
        return 1;
    }
    if ((decoded_message.header.payload_type !=
            DOIP_PAYLOAD_TYPE_ROUTING_ACTIVATION_RESPONSE) ||
        (decoded_message.header.payload_length !=
            DOIP_ROUTING_ACTIVATION_RESPONSE_PAYLOAD_SIZE)) {
        printf("FAIL: response Generic Header fields mismatch\n");
        return 1;
    }

    result = doip_routing_activation_response_decode(
        decoded_message.payload,
        decoded_message.header.payload_length,
        &response_body);
    if (check_result("decode_response_body", DOIP_RESULT_OK, result) != 0) {
        return 1;
    }
    if ((response_body.client_logical_address != expected_client_address) ||
        (response_body.doip_entity_logical_address != 0x1000u) ||
        (response_body.response_code != 0x10u)) {
        printf("FAIL: response Body fields mismatch\n");
        return 1;
    }
    return 0;
}

static int send_fragmented_request(
    SOCKET client_socket,
    const uint8_t request_message[TEST_REQUEST_SIZE])
{
    if ((send_all(client_socket, request_message, 3u) != 0)) {
        return 1;
    }
    Sleep(30u);
    if (send_all(client_socket, request_message + 3u, 5u) != 0) {
        return 1;
    }
    Sleep(30u);
    if (send_all(
            client_socket,
            request_message + DOIP_GENERIC_HEADER_SIZE,
            2u) != 0) {
        return 1;
    }
    Sleep(30u);
    return send_all(
        client_socket,
        request_message + DOIP_GENERIC_HEADER_SIZE + 2u,
        DOIP_ROUTING_ACTIVATION_REQUEST_PAYLOAD_SIZE - 2u);
}

static int test_server_lifecycle_and_stream_handling(void)
{
    const char *name = "server_lifecycle_and_stream_handling";
    doip_tcp_server_t server;
    server_thread_arguments_t thread_arguments;
    HANDLE server_thread;
    SOCKET client_socket = INVALID_SOCKET;
    uint8_t request_message[TEST_REQUEST_SIZE];
    uint8_t response_message[TEST_RESPONSE_SIZE];
    uint8_t partial_header[3] = {0x02u, 0xFDu, 0x00u};
    uint8_t invalid_version_header[DOIP_GENERIC_HEADER_SIZE];
    uint8_t invalid_inverse_header[DOIP_GENERIC_HEADER_SIZE];
    uint8_t oversized_header[DOIP_GENERIC_HEADER_SIZE];
    doip_header_t header;
    doip_result_t result;
    int failures = 0;

    if (doip_tcp_server_init(
            NULL, "127.0.0.1", 0u, 0x1000u, 0x03u) !=
        DOIP_RESULT_INVALID_ARGUMENT) {
        printf("FAIL: null_server initialization was not rejected\n");
        return 1;
    }
    if (doip_tcp_server_init(
            &server, "not-an-ip", 0u, 0x1000u, 0x03u) != DOIP_RESULT_OK) {
        printf("FAIL: server config initialization failed\n");
        return 1;
    }

    result = doip_tcp_server_start(&server);
    if (check_result("invalid_bind_address", DOIP_RESULT_INVALID_ARGUMENT, result) != 0) {
        return 1;
    }
    result = doip_tcp_server_stop(&server);
    if (check_result("stop_after_start_failure", DOIP_RESULT_OK, result) != 0) {
        return 1;
    }
    if (doip_tcp_server_init(
            &server, "127.0.0.1", 0u, 0x1000u, 0x03u) != DOIP_RESULT_OK) {
        printf("FAIL: valid server config initialization failed\n");
        return 1;
    }

    result = doip_tcp_server_stop(&server);
    if (check_result("stop_after_init", DOIP_RESULT_OK, result) != 0) {
        return 1;
    }

    result = doip_tcp_server_start(&server);
    if (check_result(name, DOIP_RESULT_OK, result) != 0) {
        printf("  startup socket error: %d\n", server.last_socket_error);
        return 1;
    }

    result = doip_tcp_server_stop(&server);
    if (check_result("stop_before_run", DOIP_RESULT_OK, result) != 0) {
        return 1;
    }
    result = doip_tcp_server_start(&server);
    if (check_result("restart_after_stop_before_run", DOIP_RESULT_OK, result) != 0) {
        printf("  startup socket error: %d\n", server.last_socket_error);
        return 1;
    }
    if (server.bound_port == 0u) {
        printf("FAIL: dynamic port was not assigned\n");
        doip_tcp_server_stop(&server);
        return 1;
    }

    thread_arguments.server = &server;
    thread_arguments.result = DOIP_RESULT_UNSUPPORTED;
    server_thread = CreateThread(
        NULL, 0u, run_server_thread, &thread_arguments, 0u, NULL);
    if (server_thread == NULL) {
        printf("FAIL: could not create service-loop test thread\n");
        doip_tcp_server_stop(&server);
        return 1;
    }

    if (build_request_message(
            0x0E00u, request_message, sizeof(request_message)) != 0) {
        printf("FAIL: request message preparation failed\n");
        failures++;
        goto stop_server;
    }

    client_socket = connect_to_server(server.bound_port);
    if (client_socket == INVALID_SOCKET) {
        printf("FAIL: client could not connect to listening server\n");
        failures++;
        goto stop_server;
    }
    if (send_fragmented_request(client_socket, request_message) != 0 ||
        receive_exact(client_socket, response_message, sizeof(response_message)) != 0) {
        printf("FAIL: fragmented Request did not produce a complete Response\n");
        failures++;
        if (client_socket != INVALID_SOCKET) {
            closesocket(client_socket);
        }
        goto stop_server;
    }
    failures += check_response(
        response_message, sizeof(response_message), 0x0E00u);
    closesocket(client_socket);
    client_socket = INVALID_SOCKET;

    client_socket = connect_to_server(server.bound_port);
    if (client_socket == INVALID_SOCKET) {
        printf("FAIL: client could not connect for incomplete-header case\n");
        failures++;
        goto stop_server;
    }
    send_all(client_socket, partial_header, sizeof(partial_header));
    closesocket(client_socket);
    client_socket = INVALID_SOCKET;

    client_socket = connect_to_server(server.bound_port);
    if (client_socket == INVALID_SOCKET) {
        printf("FAIL: client could not connect for incomplete-Payload case\n");
        failures++;
        goto stop_server;
    }
    send_all(
        client_socket,
        request_message,
        DOIP_GENERIC_HEADER_SIZE + 2u);
    shutdown(client_socket, SD_SEND);
    closesocket(client_socket);
    Sleep(150u);

    client_socket = connect_to_server(server.bound_port);
    if (client_socket == INVALID_SOCKET) {
        printf("FAIL: client could not reconnect after incomplete Payload\n");
        failures++;
        goto stop_server;
    }
    send_all(
        client_socket,
        request_message,
        DOIP_GENERIC_HEADER_SIZE + 2u);
    shutdown(client_socket, SD_SEND);
    closesocket(client_socket);
    client_socket = INVALID_SOCKET;
    Sleep(50u);

    header.protocol_version = DOIP_PROTOCOL_VERSION;
    header.inverse_protocol_version = DOIP_PROTOCOL_VERSION_INVERSE;
    header.payload_type = DOIP_PAYLOAD_TYPE_ROUTING_ACTIVATION_REQUEST;
    header.payload_length = 0u;
    if (doip_header_encode(
            &header, invalid_version_header, sizeof(invalid_version_header)) !=
        DOIP_RESULT_OK) {
        printf("FAIL: invalid-version Header preparation failed\n");
        failures++;
        goto stop_server;
    }
    invalid_version_header[0] = 0x03u;
    invalid_version_header[1] = 0xFCu;
    failures += expect_connection_closed_after_header(
        server.bound_port, invalid_version_header, "unsupported_protocol_version");

    if (doip_header_encode(
            &header, invalid_inverse_header, sizeof(invalid_inverse_header)) !=
        DOIP_RESULT_OK) {
        printf("FAIL: invalid-inverse Header preparation failed\n");
        failures++;
        goto stop_server;
    }
    invalid_inverse_header[1] = 0xFCu;
    failures += expect_connection_closed_after_header(
        server.bound_port, invalid_inverse_header, "mismatched_version_inverse");

    header.payload_length =
        (uint32_t)(DOIP_TCP_SERVER_MAX_PAYLOAD_SIZE + 1u);
    if (doip_header_encode(
            &header, oversized_header, sizeof(oversized_header)) !=
        DOIP_RESULT_OK) {
        printf("FAIL: oversized Header preparation failed\n");
        failures++;
        goto stop_server;
    }
    failures += expect_connection_closed_after_header(
        server.bound_port, oversized_header, "oversized_payload_length");

    client_socket = connect_to_server(server.bound_port);
    if (client_socket == INVALID_SOCKET) {
        printf("FAIL: server did not accept a new client after invalid frames\n");
        failures++;
        goto stop_server;
    }
    if (send_all(client_socket, request_message, sizeof(request_message)) != 0 ||
        receive_exact(client_socket, response_message, sizeof(response_message)) != 0) {
        printf("FAIL: reconnected client did not receive a complete Response\n");
        failures++;
        closesocket(client_socket);
        goto stop_server;
    }
    failures += check_response(
        response_message, sizeof(response_message), 0x0E00u);
    closesocket(client_socket);
    client_socket = INVALID_SOCKET;

    client_socket = connect_to_server(server.bound_port);
    if (client_socket == INVALID_SOCKET) {
        printf("FAIL: client could not connect for stop-during-read case\n");
        failures++;
        goto stop_server;
    }
    send_all(
        client_socket,
        request_message,
        DOIP_GENERIC_HEADER_SIZE + 2u);
    Sleep(50u);

stop_server:
    result = doip_tcp_server_request_stop(&server);
    if (check_result("request_stop", DOIP_RESULT_OK, result) != 0) {
        failures++;
    }
    failures += wait_for_server_thread(server_thread, 3000u);
    if (thread_arguments.result != DOIP_RESULT_OK) {
        printf("FAIL: service loop returned %d\n", (int)thread_arguments.result);
        failures++;
    }
    if (client_socket != INVALID_SOCKET) {
        closesocket(client_socket);
    }
    CloseHandle(server_thread);
    return failures;
}

static int test_stop_while_waiting_for_connection(void)
{
    const char *name = "stop_while_waiting_for_connection";
    doip_tcp_server_t server;
    server_thread_arguments_t thread_arguments;
    HANDLE server_thread;
    doip_result_t result;
    int failures = 0;

    result = doip_tcp_server_init(
        &server, "127.0.0.1", 0u, 0x1000u, 0x03u);
    if (check_result(name, DOIP_RESULT_OK, result) != 0) {
        return 1;
    }
    result = doip_tcp_server_start(&server);
    if (check_result(name, DOIP_RESULT_OK, result) != 0) {
        return 1;
    }

    thread_arguments.server = &server;
    thread_arguments.result = DOIP_RESULT_UNSUPPORTED;
    server_thread = CreateThread(
        NULL, 0u, run_server_thread, &thread_arguments, 0u, NULL);
    if (server_thread == NULL) {
        printf("FAIL: %s: could not start service-loop thread\n", name);
        doip_tcp_server_stop(&server);
        return 1;
    }
    if (wait_until_server_loop_running(&server, 2000u) != 0) {
        failures++;
    }

    result = doip_tcp_server_request_stop(&server);
    if (check_result(name, DOIP_RESULT_OK, result) != 0) {
        failures++;
    }
    failures += wait_for_server_thread(server_thread, 3000u);
    if (thread_arguments.result != DOIP_RESULT_OK) {
        printf("FAIL: %s: service loop returned %d\n",
            name, (int)thread_arguments.result);
        failures++;
    }
    CloseHandle(server_thread);
    return failures;
}

int main(void)
{
    WSADATA winsock_data;
    int failures;
    int startup_result = WSAStartup(MAKEWORD(2, 2), &winsock_data);

    if (startup_result != 0) {
        printf("FAIL: test WSAStartup returned %d\n", startup_result);
        return 1;
    }

    failures = test_stop_while_waiting_for_connection();
    failures += test_server_lifecycle_and_stream_handling();
    WSACleanup();
    if (failures != 0) {
        printf("%d TCP Server test(s) failed.\n", failures);
        return 1;
    }

    printf("All test_doip_tcp_server tests passed.\n");
    return 0;
}
