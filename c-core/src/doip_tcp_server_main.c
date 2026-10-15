#include "doip_tcp_server.h"

#include <stdio.h>
#include <stdlib.h>

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#include <windows.h>

#define DEFAULT_BIND_ADDRESS "127.0.0.1"
#define DEFAULT_PORT 13400u
#define DEFAULT_ENTITY_LOGICAL_ADDRESS 0x1000u
#define DEFAULT_CONFLICT_RESPONSE_CODE 0x03u

static doip_tcp_server_t *active_server;

static BOOL WINAPI console_control_handler(DWORD control_type)
{
    if ((control_type == CTRL_C_EVENT) ||
        (control_type == CTRL_BREAK_EVENT) ||
        (control_type == CTRL_CLOSE_EVENT)) {
        if (active_server != NULL) {
            doip_tcp_server_request_stop(active_server);
        }
        return TRUE;
    }
    return FALSE;
}

static int parse_port(const char *text, uint16_t *port)
{
    char *end_pointer;
    unsigned long parsed_port;

    parsed_port = strtoul(text, &end_pointer, 10);
    if ((*text == '\0') || (*end_pointer != '\0') ||
        (parsed_port > 65535ul)) {
        return 0;
    }

    *port = (uint16_t)parsed_port;
    return 1;
}

int main(int argc, char **argv)
{
    doip_tcp_server_t server;
    const char *bind_address = DEFAULT_BIND_ADDRESS;
    uint16_t port = (uint16_t)DEFAULT_PORT;
    doip_result_t result;

    if (argc > 3) {
        fprintf(stderr, "Usage: %s [IPv4-address] [port]\n", argv[0]);
        return EXIT_FAILURE;
    }
    if (argc >= 2) {
        bind_address = argv[1];
    }
    if ((argc == 3) && !parse_port(argv[2], &port)) {
        fprintf(stderr, "Invalid TCP port: %s\n", argv[2]);
        return EXIT_FAILURE;
    }

    result = doip_tcp_server_init(
        &server,
        bind_address,
        port,
        (uint16_t)DEFAULT_ENTITY_LOGICAL_ADDRESS,
        (uint8_t)DEFAULT_CONFLICT_RESPONSE_CODE);
    if (result != DOIP_RESULT_OK) {
        fprintf(stderr, "Server configuration is invalid (result %d).\n", (int)result);
        return EXIT_FAILURE;
    }

    result = doip_tcp_server_start(&server);
    if (result != DOIP_RESULT_OK) {
        fprintf(stderr,
            "Could not start DoIP TCP Server (result %d, Winsock error %d).\n",
            (int)result, server.last_socket_error);
        return EXIT_FAILURE;
    }

    active_server = &server;
    if (!SetConsoleCtrlHandler(console_control_handler, TRUE)) {
        fprintf(stderr, "Could not register console stop handler (error %lu).\n",
            (unsigned long)GetLastError());
        doip_tcp_server_stop(&server);
        return EXIT_FAILURE;
    }

    printf("Press Ctrl+C to stop the server.\n");
    result = doip_tcp_server_run(&server);
    SetConsoleCtrlHandler(console_control_handler, FALSE);
    active_server = NULL;

    if (result != DOIP_RESULT_OK) {
        fprintf(stderr,
            "DoIP TCP Server stopped after a socket error %d (result %d).\n",
            server.last_socket_error, (int)result);
        return EXIT_FAILURE;
    }

    printf("DoIP TCP Server stopped.\n");
    return EXIT_SUCCESS;
}
