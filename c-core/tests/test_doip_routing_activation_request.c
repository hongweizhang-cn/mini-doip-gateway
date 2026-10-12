#include "doip_routing_activation_request.h"

#include <stdint.h>
#include <stdio.h>

static const char *result_name(doip_result_t result)
{
    switch (result) {
    case DOIP_RESULT_OK:
        return "DOIP_RESULT_OK";
    case DOIP_RESULT_INVALID_ARGUMENT:
        return "DOIP_RESULT_INVALID_ARGUMENT";
    case DOIP_RESULT_BUFFER_TOO_SMALL:
        return "DOIP_RESULT_BUFFER_TOO_SMALL";
    case DOIP_RESULT_INVALID_HEADER:
        return "DOIP_RESULT_INVALID_HEADER";
    case DOIP_RESULT_INVALID_LENGTH:
        return "DOIP_RESULT_INVALID_LENGTH";
    case DOIP_RESULT_UNSUPPORTED:
        return "DOIP_RESULT_UNSUPPORTED";
    default:
        return "DOIP_RESULT_<unknown>";
    }
}

static int fail_result(
    const char *test_name,
    doip_result_t expected,
    doip_result_t actual)
{
    printf("FAIL: %s\n", test_name);
    printf("  expected: %s (%d)\n", result_name(expected), (int)expected);
    printf("  actual:   %s (%d)\n", result_name(actual), (int)actual);
    return 1;
}

static int check_result(
    const char *test_name,
    doip_result_t expected,
    doip_result_t actual)
{
    if (actual != expected) {
        return fail_result(test_name, expected, actual);
    }
    return 0;
}

static int check_wire_bytes(
    const char *test_name,
    const uint8_t *buffer,
    const uint8_t *expected,
    size_t count)
{
    size_t i;

    for (i = 0u; i < count; ++i) {
        if (buffer[i] != expected[i]) {
            printf("FAIL: %s\n", test_name);
            printf("  expected buffer[%lu]: 0x%02X\n", (unsigned long)i, (unsigned int)expected[i]);
            printf("  actual buffer[%lu]:   0x%02X\n", (unsigned long)i, (unsigned int)buffer[i]);
            return 1;
        }
    }

    return 0;
}

static int check_request_fields(
    const char *test_name,
    const doip_routing_activation_request_t *request,
    uint16_t source_address,
    uint8_t activation_type)
{
    if (request->source_address != source_address) {
        printf("FAIL: %s\n", test_name);
        printf("  expected source_address: 0x%04X\n", (unsigned int)source_address);
        printf("  actual source_address:   0x%04X\n", (unsigned int)request->source_address);
        return 1;
    }

    if (request->activation_type != activation_type) {
        printf("FAIL: %s\n", test_name);
        printf("  expected activation_type: 0x%02X\n", (unsigned int)activation_type);
        printf("  actual activation_type:   0x%02X\n", (unsigned int)request->activation_type);
        return 1;
    }

    return 0;
}

static int test_encode_ok(void)
{
    const char *name = "encode_ok";
    doip_routing_activation_request_t request;
    uint8_t buffer[DOIP_ROUTING_ACTIVATION_REQUEST_PAYLOAD_SIZE];
    const uint8_t expected[DOIP_ROUTING_ACTIVATION_REQUEST_PAYLOAD_SIZE] = {
        0x12u, 0x34u, 0x01u, 0x00u, 0x00u, 0x00u, 0x00u
    };
    doip_result_t result;

    request.source_address = 0x1234u;
    request.activation_type = 0x01u;

    result = doip_routing_activation_request_encode(&request, buffer, sizeof(buffer));
    if (check_result(name, DOIP_RESULT_OK, result) != 0) {
        return 1;
    }

    return check_wire_bytes(name, buffer, expected, sizeof(expected));
}

static int test_encode_exact_capacity_keeps_remainder(void)
{
    const char *name = "encode_exact_capacity_keeps_remainder";
    doip_routing_activation_request_t request;
    uint8_t buffer[8] = {0xAAu, 0xBBu, 0xCCu, 0xDDu, 0xEEu, 0xFFu, 0x11u, 0x22u};
    const uint8_t expected[8] = {
        0x12u, 0x34u, 0x01u, 0x00u, 0x00u, 0x00u, 0x00u, 0x22u
    };
    doip_result_t result;

    request.source_address = 0x1234u;
    request.activation_type = 0x01u;

    result = doip_routing_activation_request_encode(&request, buffer, sizeof(buffer));
    if (check_result(name, DOIP_RESULT_OK, result) != 0) {
        return 1;
    }

    return check_wire_bytes(name, buffer, expected, sizeof(expected));
}

static int test_encode_buffer_too_small_no_write(void)
{
    const char *name = "encode_buffer_too_small_no_write";
    doip_routing_activation_request_t request;
    uint8_t buffer[6] = {0xAAu, 0xAAu, 0xAAu, 0xAAu, 0xAAu, 0xAAu};
    doip_result_t result;
    size_t i;

    request.source_address = 0x1234u;
    request.activation_type = 0x01u;

    result = doip_routing_activation_request_encode(&request, buffer, sizeof(buffer));
    if (check_result(name, DOIP_RESULT_BUFFER_TOO_SMALL, result) != 0) {
        return 1;
    }

    for (i = 0u; i < sizeof(buffer); ++i) {
        if (buffer[i] != 0xAAu) {
            printf("FAIL: %s\n", name);
            printf("  expected buffer[%lu] to remain 0xAA\n", (unsigned long)i);
            printf("  actual buffer[%lu]:   0x%02X\n", (unsigned long)i, (unsigned int)buffer[i]);
            return 1;
        }
    }

    return 0;
}

static int test_encode_null_request(void)
{
    const char *name = "encode_null_request";
    uint8_t buffer[DOIP_ROUTING_ACTIVATION_REQUEST_PAYLOAD_SIZE];
    doip_result_t result;

    result = doip_routing_activation_request_encode(NULL, buffer, sizeof(buffer));
    return check_result(name, DOIP_RESULT_INVALID_ARGUMENT, result);
}

static int test_decode_ok(void)
{
    const char *name = "decode_ok";
    const uint8_t buffer[DOIP_ROUTING_ACTIVATION_REQUEST_PAYLOAD_SIZE] = {
        0x12u, 0x34u, 0x01u, 0x00u, 0x00u, 0x00u, 0x00u
    };
    doip_routing_activation_request_t request = {0u, 0u};
    doip_result_t result;

    result = doip_routing_activation_request_decode(buffer, sizeof(buffer), &request);
    if (check_result(name, DOIP_RESULT_OK, result) != 0) {
        return 1;
    }

    return check_request_fields(name, &request, 0x1234u, 0x01u);
}

static int test_decode_null_buffer(void)
{
    const char *name = "decode_null_buffer";
    doip_routing_activation_request_t request = {0u, 0u};
    doip_result_t result;

    result = doip_routing_activation_request_decode(NULL, DOIP_ROUTING_ACTIVATION_REQUEST_PAYLOAD_SIZE, &request);
    return check_result(name, DOIP_RESULT_INVALID_ARGUMENT, result);
}

static int test_decode_null_request(void)
{
    const char *name = "decode_null_request";
    const uint8_t buffer[DOIP_ROUTING_ACTIVATION_REQUEST_PAYLOAD_SIZE] = {
        0x12u, 0x34u, 0x01u, 0x00u, 0x00u, 0x00u, 0x00u
    };
    doip_result_t result;

    result = doip_routing_activation_request_decode(buffer, sizeof(buffer), NULL);
    return check_result(name, DOIP_RESULT_INVALID_ARGUMENT, result);
}

static int test_decode_buffer_too_small(void)
{
    const char *name = "decode_buffer_too_small";
    const uint8_t buffer[6] = {0x12u, 0x34u, 0x01u, 0x00u, 0x00u, 0x00u};
    doip_routing_activation_request_t request = {0xFFFFu, 0xFFu};
    doip_result_t result;

    result = doip_routing_activation_request_decode(buffer, sizeof(buffer), &request);
    if (check_result(name, DOIP_RESULT_BUFFER_TOO_SMALL, result) != 0) {
        return 1;
    }

    return check_request_fields(name, &request, 0xFFFFu, 0xFFu);
}

static int test_decode_unsupported_oem_length(void)
{
    const char *name = "decode_unsupported_oem_length";
    const uint8_t buffer[11] = {
        0x12u, 0x34u, 0x01u, 0x00u, 0x00u, 0x00u, 0x00u,
        0xAAu, 0xBBu, 0xCCu, 0xDDu
    };
    doip_routing_activation_request_t request = {0xFFFFu, 0xFFu};
    doip_result_t result;

    result = doip_routing_activation_request_decode(buffer, sizeof(buffer), &request);
    if (check_result(name, DOIP_RESULT_UNSUPPORTED, result) != 0) {
        return 1;
    }

    return check_request_fields(name, &request, 0xFFFFu, 0xFFu);
}

static int test_decode_reserved_nonzero(void)
{
    const char *name = "decode_reserved_nonzero";
    const uint8_t buffer[DOIP_ROUTING_ACTIVATION_REQUEST_PAYLOAD_SIZE] = {
        0x12u, 0x34u, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u
    };
    doip_routing_activation_request_t request = {0xFFFFu, 0xFFu};
    doip_result_t result;

    result = doip_routing_activation_request_decode(buffer, sizeof(buffer), &request);
    if (check_result(name, DOIP_RESULT_INVALID_HEADER, result) != 0) {
        return 1;
    }

    return check_request_fields(name, &request, 0xFFFFu, 0xFFu);
}

int main(void)
{
    int failures = 0;

    failures += test_encode_ok();
    failures += test_encode_exact_capacity_keeps_remainder();
    failures += test_encode_buffer_too_small_no_write();
    failures += test_encode_null_request();
    failures += test_decode_ok();
    failures += test_decode_null_buffer();
    failures += test_decode_null_request();
    failures += test_decode_buffer_too_small();
    failures += test_decode_unsupported_oem_length();
    failures += test_decode_reserved_nonzero();

    if (failures != 0) {
        printf("%d test(s) failed.\n", failures);
        return 1;
    }

    printf("All test_doip_routing_activation_request tests passed.\n");
    return 0;
}
