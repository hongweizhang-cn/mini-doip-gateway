#include "doip_tester.h"

#include <stdio.h>
#include <string.h>

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#include <winsock2.h>
#include <ws2tcpip.h>

#include "doip_header.h"
#include "doip_message.h"
#include "doip_payload_type.h"
#include "doip_routing_activation_request.h"
#include "doip_routing_activation_state.h"

#define DOIP_TESTER_CONNECT_TIMEOUT_MS 3000u
#define DOIP_TESTER_IO_TIMEOUT_MS 5000u
#define DOIP_TESTER_MAX_PAYLOAD_SIZE ((size_t)4096u)

static doip_result_t send_all(
    SOCKET socket_handle,
    const uint8_t *bytes,
    size_t byte_count)
{
    size_t sent_total = 0u;

    while (sent_total < byte_count) {
        int sent_now = send(
            socket_handle,
            (const char *)bytes + sent_total,
            (int)(byte_count - sent_total),
            0);
        if (sent_now == SOCKET_ERROR) {
            fprintf(stderr, "Tester send failed (Winsock error %d).\n",
                WSAGetLastError());
            return DOIP_RESULT_UNSUPPORTED;
        }
        if (sent_now == 0) {
            fprintf(stderr, "Tester send failed: connection made no progress.\n");
            return DOIP_RESULT_UNSUPPORTED;
        }
        sent_total += (size_t)sent_now;
    }

    return DOIP_RESULT_OK;
}

static doip_result_t receive_exact(
    SOCKET socket_handle,
    uint8_t *bytes,
    size_t byte_count)
{
    size_t received_total = 0u;

    while (received_total < byte_count) {
        int received_now = recv(
            socket_handle,
            (char *)bytes + received_total,
            (int)(byte_count - received_total),
            0);
        if (received_now == 0) {
            fprintf(stderr, "Server closed the connection before the response was complete.\n");
            return DOIP_RESULT_INVALID_LENGTH;
        }
        if (received_now == SOCKET_ERROR) {
            fprintf(stderr, "Tester receive failed (Winsock error %d).\n",
                WSAGetLastError());
            return DOIP_RESULT_UNSUPPORTED;
        }
        received_total += (size_t)received_now;
    }

    return DOIP_RESULT_OK;
}

static doip_result_t connect_with_timeout(
    const char *server_ipv4,
    uint16_t server_port,
    SOCKET *socket_output)
{
    struct sockaddr_in server_address;
    SOCKET socket_handle;
    u_long nonblocking = 1u;
    fd_set write_set;
    struct timeval timeout;
    int connect_result;
    int socket_error = 0;
    int socket_error_size = (int)sizeof(socket_error);
    DWORD timeout_ms = DOIP_TESTER_IO_TIMEOUT_MS;

    socket_handle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_handle == INVALID_SOCKET) {
        fprintf(stderr, "Could not create Tester socket (Winsock error %d).\n",
            WSAGetLastError());
        return DOIP_RESULT_UNSUPPORTED;
    }

    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(server_port);
    if (InetPtonA(AF_INET, server_ipv4, &server_address.sin_addr) != 1) {
        fprintf(stderr, "Invalid numeric IPv4 address: %s\n", server_ipv4);
        closesocket(socket_handle);
        return DOIP_RESULT_INVALID_ARGUMENT;
    }

    if (ioctlsocket(socket_handle, FIONBIO, &nonblocking) == SOCKET_ERROR) {
        fprintf(stderr, "Could not configure Tester socket (Winsock error %d).\n",
            WSAGetLastError());
        closesocket(socket_handle);
        return DOIP_RESULT_UNSUPPORTED;
    }

    connect_result = connect(
        socket_handle,
        (const struct sockaddr *)&server_address,
        (int)sizeof(server_address));
    if ((connect_result == SOCKET_ERROR) &&
        (WSAGetLastError() != WSAEWOULDBLOCK) &&
        (WSAGetLastError() != WSAEINPROGRESS)) {
        fprintf(stderr, "TCP connection to %s:%u failed (Winsock error %d).\n",
            server_ipv4, (unsigned int)server_port, WSAGetLastError());
        closesocket(socket_handle);
        return DOIP_RESULT_UNSUPPORTED;
    }

    FD_ZERO(&write_set);
    FD_SET(socket_handle, &write_set);
    timeout.tv_sec = (long)(DOIP_TESTER_CONNECT_TIMEOUT_MS / 1000u);
    timeout.tv_usec =
        (long)((DOIP_TESTER_CONNECT_TIMEOUT_MS % 1000u) * 1000u);
    connect_result = select(0, NULL, &write_set, NULL, &timeout);
    if (connect_result <= 0) {
        fprintf(stderr, "TCP connection to %s:%u %s.\n",
            server_ipv4, (unsigned int)server_port,
            connect_result == 0 ? "timed out" : "failed");
        closesocket(socket_handle);
        return DOIP_RESULT_UNSUPPORTED;
    }

    if (getsockopt(
            socket_handle,
            SOL_SOCKET,
            SO_ERROR,
            (char *)&socket_error,
            &socket_error_size) == SOCKET_ERROR ||
        socket_error != 0) {
        fprintf(stderr, "TCP connection to %s:%u failed (Winsock error %d).\n",
            server_ipv4, (unsigned int)server_port,
            socket_error != 0 ? socket_error : WSAGetLastError());
        closesocket(socket_handle);
        return DOIP_RESULT_UNSUPPORTED;
    }

    nonblocking = 0u;
    if ((ioctlsocket(socket_handle, FIONBIO, &nonblocking) == SOCKET_ERROR) ||
        (setsockopt(
            socket_handle, SOL_SOCKET, SO_RCVTIMEO,
            (const char *)&timeout_ms, (int)sizeof(timeout_ms)) == SOCKET_ERROR) ||
        (setsockopt(
            socket_handle, SOL_SOCKET, SO_SNDTIMEO,
            (const char *)&timeout_ms, (int)sizeof(timeout_ms)) == SOCKET_ERROR)) {
        fprintf(stderr, "Could not configure Tester socket timeouts (Winsock error %d).\n",
            WSAGetLastError());
        closesocket(socket_handle);
        return DOIP_RESULT_UNSUPPORTED;
    }

    *socket_output = socket_handle;
    return DOIP_RESULT_OK;
}

doip_result_t doip_tester_run(
    const char *server_ipv4,
    uint16_t server_port,
    uint16_t tester_logical_address,
    uint16_t doip_entity_logical_address,
    doip_routing_activation_response_t *response)
{
    WSADATA winsock_data;
    SOCKET socket_handle = INVALID_SOCKET;
    doip_routing_activation_request_t request;
    doip_header_t request_header;
    doip_header_t response_header;
    doip_routing_activation_response_t decoded_response;
    uint8_t request_body[DOIP_ROUTING_ACTIVATION_REQUEST_PAYLOAD_SIZE];
    uint8_t request_message[
        DOIP_GENERIC_HEADER_SIZE + DOIP_ROUTING_ACTIVATION_REQUEST_PAYLOAD_SIZE];
    uint8_t response_payload[DOIP_TESTER_MAX_PAYLOAD_SIZE];
    uint8_t response_header_bytes[DOIP_GENERIC_HEADER_SIZE];
    doip_result_t result;
    int winsock_result;
    int winsock_started = 0;

    if (response != NULL) {
        memset(response, 0, sizeof(*response));
    }
    if ((server_ipv4 == NULL) || (response == NULL) || (server_port == 0u)) {
        return DOIP_RESULT_INVALID_ARGUMENT;
    }

    winsock_result = WSAStartup(MAKEWORD(2, 2), &winsock_data);
    if (winsock_result != 0) {
        fprintf(stderr, "Tester Winsock initialization failed (error %d).\n",
            winsock_result);
        return DOIP_RESULT_UNSUPPORTED;
    }
    winsock_started = 1;

    printf("Connecting Tester to %s:%u (Tester 0x%04X, Entity 0x%04X).\n",
        server_ipv4, (unsigned int)server_port,
        (unsigned int)tester_logical_address,
        (unsigned int)doip_entity_logical_address);
    result = connect_with_timeout(server_ipv4, server_port, &socket_handle);
    if (result != DOIP_RESULT_OK) {
        goto cleanup;
    }

    request.source_address = tester_logical_address;
    request.activation_type = 0x01u;
    result = doip_routing_activation_request_encode(
        &request, request_body, sizeof(request_body));
    if (result != DOIP_RESULT_OK) {
        goto cleanup;
    }
    request_header.protocol_version = DOIP_PROTOCOL_VERSION;
    request_header.inverse_protocol_version = DOIP_PROTOCOL_VERSION_INVERSE;
    request_header.payload_type = DOIP_PAYLOAD_TYPE_ROUTING_ACTIVATION_REQUEST;
    request_header.payload_length = (uint32_t)sizeof(request_body);
    result = doip_message_encode(
        &request_header, request_body, sizeof(request_body),
        request_message, sizeof(request_message));
    if (result != DOIP_RESULT_OK) {
        goto cleanup;
    }

    result = send_all(socket_handle, request_message, sizeof(request_message));
    if (result != DOIP_RESULT_OK) {
        goto cleanup;
    }
    printf("Routing Activation Request sent (%lu bytes).\n",
        (unsigned long)sizeof(request_message));

    result = receive_exact(
        socket_handle, response_header_bytes, sizeof(response_header_bytes));
    if (result != DOIP_RESULT_OK) {
        goto cleanup;
    }
    result = doip_header_decode(
        response_header_bytes, sizeof(response_header_bytes), &response_header);
    if (result != DOIP_RESULT_OK) {
        fprintf(stderr, "Server returned an invalid DoIP Generic Header (result %d).\n",
            (int)result);
        goto cleanup;
    }
    if (response_header.payload_type !=
        DOIP_PAYLOAD_TYPE_ROUTING_ACTIVATION_RESPONSE) {
        fprintf(stderr, "Unexpected Response Payload Type 0x%04X.\n",
            (unsigned int)response_header.payload_type);
        result = DOIP_RESULT_UNSUPPORTED;
        goto cleanup;
    }
    if ((size_t)response_header.payload_length > DOIP_TESTER_MAX_PAYLOAD_SIZE) {
        fprintf(stderr, "Response Payload Length %lu exceeds Tester limit %lu.\n",
            (unsigned long)response_header.payload_length,
            (unsigned long)DOIP_TESTER_MAX_PAYLOAD_SIZE);
        result = DOIP_RESULT_BUFFER_TOO_SMALL;
        goto cleanup;
    }
    if ((size_t)response_header.payload_length !=
        DOIP_ROUTING_ACTIVATION_RESPONSE_PAYLOAD_SIZE) {
        fprintf(stderr, "Invalid Routing Activation Response Payload Length %lu.\n",
            (unsigned long)response_header.payload_length);
        result = DOIP_RESULT_INVALID_LENGTH;
        goto cleanup;
    }

    result = receive_exact(
        socket_handle, response_payload, (size_t)response_header.payload_length);
    if (result != DOIP_RESULT_OK) {
        goto cleanup;
    }
    result = doip_routing_activation_response_decode(
        response_payload,
        (size_t)response_header.payload_length,
        &decoded_response);
    if (result != DOIP_RESULT_OK) {
        fprintf(stderr, "Could not decode Routing Activation Response Body (result %d).\n",
            (int)result);
        goto cleanup;
    }

    *response = decoded_response;
    printf("Routing Activation Response: Tester=0x%04X, Entity=0x%04X, "
        "ResponseCode=0x%02X.\n",
        (unsigned int)decoded_response.client_logical_address,
        (unsigned int)decoded_response.doip_entity_logical_address,
        (unsigned int)decoded_response.response_code);

    if ((decoded_response.client_logical_address != tester_logical_address) ||
        (decoded_response.doip_entity_logical_address !=
            doip_entity_logical_address)) {
        fprintf(stderr, "Response logical address does not match the requested addresses.\n");
        result = DOIP_RESULT_INVALID_HEADER;
        goto cleanup;
    }
    if (decoded_response.response_code !=
        DOIP_ROUTING_ACTIVATION_RESPONSE_CODE_SUCCESS) {
        fprintf(stderr, "Routing Activation was rejected (Response Code 0x%02X).\n",
            (unsigned int)decoded_response.response_code);
        result = DOIP_RESULT_UNSUPPORTED;
        goto cleanup;
    }

    printf("Routing Activation succeeded.\n");
    result = DOIP_RESULT_OK;

cleanup:
    if (socket_handle != INVALID_SOCKET) {
        shutdown(socket_handle, SD_BOTH);
        closesocket(socket_handle);
    }
    if (winsock_started) {
        WSACleanup();
    }
    return result;
}
