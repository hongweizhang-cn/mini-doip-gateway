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

#define DOIP_TCP_SERVER_POLL_INTERVAL_MS 100
#define DOIP_TCP_SERVER_MAX_MESSAGE_SIZE \
    (DOIP_GENERIC_HEADER_SIZE + DOIP_TCP_SERVER_MAX_PAYLOAD_SIZE)
#define DOIP_TCP_SERVER_STATE_STOPPED 0L
#define DOIP_TCP_SERVER_STATE_LISTENING 1L
#define DOIP_TCP_SERVER_STATE_RUNNING 2L
#define DOIP_TCP_SERVER_STATE_STOPPING 3L

typedef enum {
    DOIP_TCP_READ_COMPLETE = 0,
    DOIP_TCP_READ_DISCONNECTED,
    DOIP_TCP_READ_STOPPED,
    DOIP_TCP_READ_FAILED
} doip_tcp_read_result_t;

static SOCKET server_socket(uintptr_t socket_value)
{
    return (SOCKET)socket_value;
}

static void close_socket(uintptr_t *socket_value)
{
    if (*socket_value != (uintptr_t)INVALID_SOCKET) {
        closesocket(server_socket(*socket_value));
        *socket_value = (uintptr_t)INVALID_SOCKET;
    }
}

static void release_server_resources(doip_tcp_server_t *server)
{
    close_socket(&server->client_socket);
    close_socket(&server->listen_socket);
    if (server->winsock_started) {
        WSACleanup();
        server->winsock_started = 0;
    }
}

static int stop_is_requested(const doip_tcp_server_t *server)
{
    return InterlockedCompareExchange(
        (volatile LONG *)&server->stop_requested, 0, 0) != 0;
}

static int wait_for_socket(
    doip_tcp_server_t *server,
    SOCKET socket_handle,
    int wait_for_write)
{
    fd_set socket_set;
    struct timeval timeout;
    int select_result;

    while (!stop_is_requested(server)) {
        FD_ZERO(&socket_set);
        FD_SET(socket_handle, &socket_set);
        timeout.tv_sec = 0;
        timeout.tv_usec = DOIP_TCP_SERVER_POLL_INTERVAL_MS * 1000;

        if (wait_for_write) {
            select_result = select(0, NULL, &socket_set, NULL, &timeout);
        } else {
            select_result = select(0, &socket_set, NULL, NULL, &timeout);
        }

        if (select_result > 0) {
            return 1;
        }
        if (select_result == 0) {
            continue;
        }

        server->last_socket_error = WSAGetLastError();
        if (server->last_socket_error == WSAEINTR) {
            continue;
        }
        return -1;
    }

    return 0;
}

static doip_tcp_read_result_t receive_exact(
    doip_tcp_server_t *server,
    SOCKET client_socket,
    uint8_t *destination,
    size_t byte_count)
{
    size_t received_total = 0u;

    while (received_total < byte_count) {
        int wait_result = wait_for_socket(server, client_socket, 0);
        int receive_result;

        if (wait_result == 0) {
            return DOIP_TCP_READ_STOPPED;
        }
        if (wait_result < 0) {
            return DOIP_TCP_READ_FAILED;
        }

        receive_result = recv(
            client_socket,
            (char *)destination + received_total,
            (int)(byte_count - received_total),
            0);
        if (receive_result > 0) {
            received_total += (size_t)receive_result;
            continue;
        }
        if (receive_result == 0) {
            return DOIP_TCP_READ_DISCONNECTED;
        }

        server->last_socket_error = WSAGetLastError();
        if ((server->last_socket_error == WSAEWOULDBLOCK) ||
            (server->last_socket_error == WSAEINTR)) {
            continue;
        }
        return DOIP_TCP_READ_FAILED;
    }

    return DOIP_TCP_READ_COMPLETE;
}

static int send_all(
    doip_tcp_server_t *server,
    SOCKET client_socket,
    const uint8_t *source,
    size_t byte_count)
{
    size_t sent_total = 0u;

    while (sent_total < byte_count) {
        int wait_result = wait_for_socket(server, client_socket, 1);
        int send_result;

        if (wait_result <= 0) {
            return wait_result;
        }

        send_result = send(
            client_socket,
            (const char *)source + sent_total,
            (int)(byte_count - sent_total),
            0);
        if (send_result > 0) {
            sent_total += (size_t)send_result;
            continue;
        }
        if (send_result == 0) {
            return -1;
        }

        server->last_socket_error = WSAGetLastError();
        if ((server->last_socket_error == WSAEWOULDBLOCK) ||
            (server->last_socket_error == WSAEINTR)) {
            continue;
        }
        return -1;
    }

    return 1;
}

static int process_client_messages(
    doip_tcp_server_t *server,
    SOCKET client_socket)
{
    uint8_t input_message[DOIP_TCP_SERVER_MAX_MESSAGE_SIZE];
    uint8_t output_message[DOIP_TCP_SERVER_MAX_MESSAGE_SIZE];

    while (!stop_is_requested(server)) {
        doip_header_t decoded_header;
        size_t complete_message_size;
        size_t output_message_size = 0u;
        doip_result_t result;
        doip_tcp_read_result_t read_result;
        int send_result;

        read_result = receive_exact(
            server, client_socket, input_message, DOIP_GENERIC_HEADER_SIZE);
        if (read_result == DOIP_TCP_READ_DISCONNECTED) {
            return 0;
        }
        if (read_result == DOIP_TCP_READ_STOPPED) {
            return 1;
        }
        if (read_result == DOIP_TCP_READ_FAILED) {
            return -1;
        }

        result = doip_header_decode(
            input_message, DOIP_GENERIC_HEADER_SIZE, &decoded_header);
        if (result != DOIP_RESULT_OK) {
            printf("Rejecting client message: invalid Generic Header (result %d).\n",
                (int)result);
            return -1;
        }
        if ((size_t)decoded_header.payload_length >
            DOIP_TCP_SERVER_MAX_PAYLOAD_SIZE) {
            printf("Rejecting client message: Payload Length %lu exceeds limit %lu.\n",
                (unsigned long)decoded_header.payload_length,
                (unsigned long)DOIP_TCP_SERVER_MAX_PAYLOAD_SIZE);
            return -1;
        }

        complete_message_size = DOIP_GENERIC_HEADER_SIZE +
            (size_t)decoded_header.payload_length;
        read_result = receive_exact(
            server,
            client_socket,
            input_message + DOIP_GENERIC_HEADER_SIZE,
            (size_t)decoded_header.payload_length);
        if (read_result == DOIP_TCP_READ_DISCONNECTED) {
            return 0;
        }
        if (read_result == DOIP_TCP_READ_STOPPED) {
            return 1;
        }
        if (read_result == DOIP_TCP_READ_FAILED) {
            return -1;
        }

        result = doip_gateway_process_message(
            &server->gateway,
            input_message,
            complete_message_size,
            output_message,
            sizeof(output_message),
            &output_message_size);
        if (result != DOIP_RESULT_OK) {
            printf("Gateway rejected complete client message (result %d).\n",
                (int)result);
            return -1;
        }

        send_result = send_all(
            server, client_socket, output_message, output_message_size);
        if (send_result <= 0) {
            return send_result;
        }
    }

    return 1;
}

doip_result_t doip_tcp_server_init(
    doip_tcp_server_t *server,
    const char *bind_address,
    uint16_t port,
    uint16_t doip_entity_logical_address,
    uint8_t conflict_response_code)
{
    size_t address_length;
    doip_result_t result;

    if ((server == NULL) || (bind_address == NULL)) {
        return DOIP_RESULT_INVALID_ARGUMENT;
    }

    address_length = strlen(bind_address);
    if ((address_length == 0u) ||
        (address_length >= DOIP_TCP_SERVER_ADDRESS_SIZE)) {
        return DOIP_RESULT_INVALID_ARGUMENT;
    }

    memset(server, 0, sizeof(*server));
    memcpy(server->bind_address, bind_address, address_length + 1u);
    server->configured_port = port;
    server->doip_entity_logical_address = doip_entity_logical_address;
    server->conflict_response_code = conflict_response_code;
    server->listen_socket = (uintptr_t)INVALID_SOCKET;
    server->client_socket = (uintptr_t)INVALID_SOCKET;

    result = doip_gateway_init(
        &server->gateway,
        doip_entity_logical_address,
        conflict_response_code);
    return result;
}

doip_result_t doip_tcp_server_start(doip_tcp_server_t *server)
{
    WSADATA winsock_data;
    struct sockaddr_in server_address;
    int address_result;
    int exclusive_address_use = 1;
    int result;
    int address_length = (int)sizeof(server_address);

    if ((server == NULL) ||
        (InterlockedCompareExchange(
            &server->lifecycle_state,
            DOIP_TCP_SERVER_STATE_STOPPED,
            DOIP_TCP_SERVER_STATE_STOPPED) != DOIP_TCP_SERVER_STATE_STOPPED)) {
        return DOIP_RESULT_INVALID_ARGUMENT;
    }

    result = WSAStartup(MAKEWORD(2, 2), &winsock_data);
    if (result != 0) {
        server->last_socket_error = result;
        return DOIP_RESULT_UNSUPPORTED;
    }
    server->winsock_started = 1;

    memset(&server_address, 0, sizeof(server_address));
    address_result = InetPtonA(AF_INET, server->bind_address, &server_address.sin_addr);
    if (address_result != 1) {
        server->last_socket_error =
            address_result == 0 ? WSAEINVAL : WSAGetLastError();
        release_server_resources(server);
        return DOIP_RESULT_INVALID_ARGUMENT;
    }

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(server->configured_port);

    {
        SOCKET listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        u_long nonblocking = 1u;
        int ioctl_result;

        if (listen_socket == INVALID_SOCKET) {
            server->last_socket_error = WSAGetLastError();
            release_server_resources(server);
            return DOIP_RESULT_UNSUPPORTED;
        }
        server->listen_socket = (uintptr_t)listen_socket;

        result = setsockopt(
            listen_socket,
            SOL_SOCKET,
            SO_EXCLUSIVEADDRUSE,
            (const char *)&exclusive_address_use,
            (int)sizeof(exclusive_address_use));
        if (result == SOCKET_ERROR) {
            server->last_socket_error = WSAGetLastError();
            release_server_resources(server);
            return DOIP_RESULT_UNSUPPORTED;
        }
        result = bind(
            listen_socket,
            (const struct sockaddr *)&server_address,
            (int)sizeof(server_address));
        if (result == SOCKET_ERROR) {
            server->last_socket_error = WSAGetLastError();
            release_server_resources(server);
            return DOIP_RESULT_UNSUPPORTED;
        }

        result = listen(listen_socket, SOMAXCONN);
        if (result == SOCKET_ERROR) {
            server->last_socket_error = WSAGetLastError();
            release_server_resources(server);
            return DOIP_RESULT_UNSUPPORTED;
        }

        ioctl_result = ioctlsocket(listen_socket, FIONBIO, &nonblocking);
        if (ioctl_result == SOCKET_ERROR) {
            server->last_socket_error = WSAGetLastError();
            release_server_resources(server);
            return DOIP_RESULT_UNSUPPORTED;
        }
    }

    if (getsockname(
            server_socket(server->listen_socket),
            (struct sockaddr *)&server_address,
            &address_length) == SOCKET_ERROR) {
        server->last_socket_error = WSAGetLastError();
        release_server_resources(server);
        return DOIP_RESULT_UNSUPPORTED;
    }

    server->bound_port = ntohs(server_address.sin_port);
    server->stop_requested = 0;
    InterlockedExchange(
        &server->lifecycle_state, DOIP_TCP_SERVER_STATE_LISTENING);
    server->last_socket_error = 0;

    printf("DoIP TCP Server listening on %s:%u.\n",
        server->bind_address, (unsigned int)server->bound_port);
    return DOIP_RESULT_OK;
}

doip_result_t doip_tcp_server_run(doip_tcp_server_t *server)
{
    SOCKET listen_socket;

    if (server == NULL) {
        return DOIP_RESULT_INVALID_ARGUMENT;
    }

    if (InterlockedCompareExchange(
            &server->lifecycle_state,
            DOIP_TCP_SERVER_STATE_RUNNING,
            DOIP_TCP_SERVER_STATE_LISTENING) !=
        DOIP_TCP_SERVER_STATE_LISTENING) {
        return DOIP_RESULT_INVALID_ARGUMENT;
    }

    listen_socket = server_socket(server->listen_socket);
    while (!stop_is_requested(server)) {
        int wait_result = wait_for_socket(server, listen_socket, 0);

        if (wait_result == 0) {
            break;
        }
        if (wait_result < 0) {
            release_server_resources(server);
            InterlockedExchange(
                &server->lifecycle_state, DOIP_TCP_SERVER_STATE_STOPPED);
            return DOIP_RESULT_UNSUPPORTED;
        }

        {
            struct sockaddr_in client_address;
            int client_address_size = (int)sizeof(client_address);
            SOCKET client_socket = accept(
                listen_socket,
                (struct sockaddr *)&client_address,
                &client_address_size);
            u_long nonblocking = 1u;
            int client_result;
            int client_process_result;

            if (client_socket == INVALID_SOCKET) {
                server->last_socket_error = WSAGetLastError();
                if ((server->last_socket_error == WSAEWOULDBLOCK) ||
                    (server->last_socket_error == WSAEINTR)) {
                    continue;
                }
                release_server_resources(server);
                InterlockedExchange(
                    &server->lifecycle_state, DOIP_TCP_SERVER_STATE_STOPPED);
                return DOIP_RESULT_UNSUPPORTED;
            }

            server->client_socket = (uintptr_t)client_socket;
            server->last_socket_error = 0;
            client_result = ioctlsocket(client_socket, FIONBIO, &nonblocking);
            if (client_result == SOCKET_ERROR) {
                server->last_socket_error = WSAGetLastError();
                close_socket(&server->client_socket);
                continue;
            }

            printf("DoIP TCP client connected.\n");
            client_process_result =
                process_client_messages(server, client_socket);
            if (client_process_result < 0) {
                printf("DoIP TCP client closed after a receive, protocol, or send error.\n");
                if (server->last_socket_error != 0) {
                    printf("  Winsock error: %d\n", server->last_socket_error);
                }
            } else {
                printf("DoIP TCP client disconnected.\n");
            }
            close_socket(&server->client_socket);
            doip_gateway_reset_routing_activation(&server->gateway);

            if (client_process_result == 1) {
                break;
            }
        }
    }

    release_server_resources(server);
    InterlockedExchange(
        &server->lifecycle_state, DOIP_TCP_SERVER_STATE_STOPPED);
    return DOIP_RESULT_OK;
}

doip_result_t doip_tcp_server_request_stop(doip_tcp_server_t *server)
{
    if (server == NULL) {
        return DOIP_RESULT_INVALID_ARGUMENT;
    }

    InterlockedExchange((volatile LONG *)&server->stop_requested, 1);
    return DOIP_RESULT_OK;
}

doip_result_t doip_tcp_server_stop(doip_tcp_server_t *server)
{
    long lifecycle_state;

    if (server == NULL) {
        return DOIP_RESULT_INVALID_ARGUMENT;
    }

    doip_tcp_server_request_stop(server);
    lifecycle_state = InterlockedCompareExchange(
        &server->lifecycle_state,
        DOIP_TCP_SERVER_STATE_STOPPED,
        DOIP_TCP_SERVER_STATE_STOPPED);

    if (lifecycle_state == DOIP_TCP_SERVER_STATE_LISTENING) {
        if (InterlockedCompareExchange(
                &server->lifecycle_state,
                DOIP_TCP_SERVER_STATE_STOPPING,
                DOIP_TCP_SERVER_STATE_LISTENING) ==
            DOIP_TCP_SERVER_STATE_LISTENING) {
            release_server_resources(server);
            InterlockedExchange(
                &server->lifecycle_state, DOIP_TCP_SERVER_STATE_STOPPED);
        }
    }

    return DOIP_RESULT_OK;
}
