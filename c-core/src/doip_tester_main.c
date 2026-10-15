#include "doip_tester.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int parse_unsigned(
    const char *text,
    int base,
    unsigned long maximum,
    unsigned long *value)
{
    char *end_pointer;
    unsigned long parsed;

    if ((text == NULL) || (*text == '\0')) {
        return 0;
    }
    parsed = strtoul(text, &end_pointer, base);
    if ((*end_pointer != '\0') || (parsed > maximum)) {
        return 0;
    }
    *value = parsed;
    return 1;
}

int main(int argc, char **argv)
{
    const char *server_ipv4 = "127.0.0.1";
    unsigned long server_port = 13400ul;
    unsigned long tester_address = 0x0E00ul;
    unsigned long entity_address = 0x1000ul;
    doip_routing_activation_response_t response;
    doip_result_t result;

    if ((argc == 2) &&
        ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0))) {
        printf("Usage: %s [server-ipv4] [port] [tester-logical-address] "
            "[entity-logical-address]\n"
            "Defaults: 127.0.0.1 13400 0x0E00 0x1000; logical addresses "
            "accept decimal or 0x-prefixed hexadecimal.\n",
            argv[0]);
        return EXIT_SUCCESS;
    }

    if (argc > 5) {
        fprintf(stderr,
            "Usage: %s [server-ipv4] [port] [tester-logical-address] "
            "[entity-logical-address]\n"
            "Defaults: 127.0.0.1 13400 0x0E00 0x1000; logical addresses "
            "accept decimal or 0x-prefixed hexadecimal.\n",
            argv[0]);
        return EXIT_FAILURE;
    }
    if ((argc >= 2) && (argv[1][0] != '\0')) {
        server_ipv4 = argv[1];
    }
    if ((argc >= 3) &&
        !parse_unsigned(argv[2], 10, 65535ul, &server_port)) {
        fprintf(stderr, "Invalid TCP port: %s\n", argv[2]);
        return EXIT_FAILURE;
    }
    if ((argc >= 4) &&
        !parse_unsigned(argv[3], 0, 65535ul, &tester_address)) {
        fprintf(stderr, "Invalid Tester Logical Address: %s\n", argv[3]);
        return EXIT_FAILURE;
    }
    if ((argc >= 5) &&
        !parse_unsigned(argv[4], 0, 65535ul, &entity_address)) {
        fprintf(stderr, "Invalid Entity Logical Address: %s\n", argv[4]);
        return EXIT_FAILURE;
    }
    if (server_port == 0ul) {
        fprintf(stderr, "TCP port must be between 1 and 65535.\n");
        return EXIT_FAILURE;
    }

    result = doip_tester_run(
        server_ipv4,
        (uint16_t)server_port,
        (uint16_t)tester_address,
        (uint16_t)entity_address,
        &response);
    if (result != DOIP_RESULT_OK) {
        fprintf(stderr, "Tester failed (DoIP result %d).\n", (int)result);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
