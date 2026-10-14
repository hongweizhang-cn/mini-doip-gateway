#include "doip_routing_activation_response.h"

#include <stdio.h>
#include <stdint.h>

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

static int check_result(
    const char *test_name,
    doip_result_t expected,
    doip_result_t actual)
{
    if (actual != expected) {
        printf("FAIL: %s\n", test_name);
        printf("  expected: %s (%d)\n", result_name(expected), (int)expected);
        printf("  actual:   %s (%d)\n", result_name(actual), (int)actual);
        return 1;
    }
    return 0;
}

static int check_bytes(
    const char *test_name,
    const uint8_t *actual,
    const uint8_t *expected,
    size_t count)
{
    size_t i;

    for (i = 0u; i < count; ++i) {
        if (actual[i] != expected[i]) {
            printf("FAIL: %s\n", test_name);
            printf("  expected byte[%lu]: 0x%02X\n",
                (unsigned long)i, (unsigned int)expected[i]);
            printf("  actual byte[%lu]:   0x%02X\n",
                (unsigned long)i, (unsigned int)actual[i]);
            return 1;
        }
    }

    return 0;
}

static int check_response(
    const char *test_name,
    const doip_routing_activation_response_t *actual,
    uint16_t client_address,
    uint16_t entity_address,
    uint8_t response_code)
{
    if (actual->client_logical_address != client_address) {
        printf("FAIL: %s: client logical address mismatch\n", test_name);
        return 1;
    }
    if (actual->doip_entity_logical_address != entity_address) {
        printf("FAIL: %s: DoIP entity logical address mismatch\n", test_name);
        return 1;
    }
    if (actual->response_code != response_code) {
        printf("FAIL: %s: response code mismatch\n", test_name);
        return 1;
    }

    return 0;
}

static int check_response_unchanged(
    const char *test_name,
    const doip_routing_activation_response_t *actual,
    const doip_routing_activation_response_t *expected)
{
    return check_response(
        test_name,
        actual,
        expected->client_logical_address,
        expected->doip_entity_logical_address,
        expected->response_code);
}

static int test_encode_success_response(void)
{
    const char *name = "encode_success_response";
    const doip_routing_activation_response_t response = {0x0E00u, 0x1000u, 0x10u};
    const uint8_t expected[DOIP_ROUTING_ACTIVATION_RESPONSE_PAYLOAD_SIZE] = {
        0x0Eu, 0x00u, 0x10u, 0x00u, 0x10u, 0x00u, 0x00u, 0x00u, 0x00u
    };
    uint8_t buffer[DOIP_ROUTING_ACTIVATION_RESPONSE_PAYLOAD_SIZE];
    doip_result_t result;

    result = doip_routing_activation_response_encode(&response, buffer, sizeof(buffer));
    if (check_result(name, DOIP_RESULT_OK, result) != 0) {
        return 1;
    }

    return check_bytes(name, buffer, expected, sizeof(expected));
}

static int test_encode_failure_and_unknown_codes(void)
{
    const char *name = "encode_failure_and_unknown_codes";
    const uint8_t codes[3] = {0x00u, 0x06u, 0x7Fu};
    const uint8_t common_expected[3][DOIP_ROUTING_ACTIVATION_RESPONSE_PAYLOAD_SIZE] = {
        {0x12u, 0x34u, 0xABu, 0xCDu, 0x00u, 0u, 0u, 0u, 0u},
        {0x12u, 0x34u, 0xABu, 0xCDu, 0x06u, 0u, 0u, 0u, 0u},
        {0x12u, 0x34u, 0xABu, 0xCDu, 0x7Fu, 0u, 0u, 0u, 0u}
    };
    size_t i;

    for (i = 0u; i < 3u; ++i) {
        doip_routing_activation_response_t response = {0x1234u, 0xABCDu, codes[i]};
        uint8_t buffer[DOIP_ROUTING_ACTIVATION_RESPONSE_PAYLOAD_SIZE];
        doip_result_t result;

        result = doip_routing_activation_response_encode(&response, buffer, sizeof(buffer));
        if (check_result(name, DOIP_RESULT_OK, result) != 0) {
            return 1;
        }
        if (check_bytes(name, buffer, common_expected[i], sizeof(buffer)) != 0) {
            return 1;
        }
    }

    return 0;
}

static int test_encode_preserves_tail_and_capacity_failure(void)
{
    const char *name = "encode_preserves_tail_and_capacity_failure";
    const doip_routing_activation_response_t response = {0x0E00u, 0x1000u, 0x10u};
    const uint8_t expected_large[11] = {
        0x0Eu, 0x00u, 0x10u, 0x00u, 0x10u, 0u, 0u, 0u, 0u, 0xCCu, 0xCCu
    };
    uint8_t large_buffer[11] = {
        0xCCu, 0xCCu, 0xCCu, 0xCCu, 0xCCu, 0xCCu,
        0xCCu, 0xCCu, 0xCCu, 0xCCu, 0xCCu
    };
    uint8_t small_buffer[8] = {0xCCu, 0xCCu, 0xCCu, 0xCCu, 0xCCu, 0xCCu, 0xCCu, 0xCCu};
    const uint8_t unchanged[8] = {0xCCu, 0xCCu, 0xCCu, 0xCCu, 0xCCu, 0xCCu, 0xCCu, 0xCCu};
    doip_result_t result;

    result = doip_routing_activation_response_encode(&response, large_buffer, sizeof(large_buffer));
    if (check_result(name, DOIP_RESULT_OK, result) != 0) {
        return 1;
    }
    if (check_bytes(name, large_buffer, expected_large, sizeof(large_buffer)) != 0) {
        return 1;
    }

    result = doip_routing_activation_response_encode(&response, small_buffer, sizeof(small_buffer));
    if (check_result(name, DOIP_RESULT_BUFFER_TOO_SMALL, result) != 0) {
        return 1;
    }

    return check_bytes(name, small_buffer, unchanged, sizeof(small_buffer));
}

static int test_encode_null_arguments(void)
{
    const doip_routing_activation_response_t response = {0u, 0u, 0x10u};
    uint8_t buffer[DOIP_ROUTING_ACTIVATION_RESPONSE_PAYLOAD_SIZE];
    doip_result_t result;

    result = doip_routing_activation_response_encode(NULL, buffer, sizeof(buffer));
    if (check_result("encode_null_response", DOIP_RESULT_INVALID_ARGUMENT, result) != 0) {
        return 1;
    }

    result = doip_routing_activation_response_encode(&response, NULL, sizeof(buffer));
    if (check_result("encode_null_buffer", DOIP_RESULT_INVALID_ARGUMENT, result) != 0) {
        return 1;
    }

    return 0;
}

static int test_decode_known_response(void)
{
    const char *name = "decode_known_response";
    const uint8_t buffer[DOIP_ROUTING_ACTIVATION_RESPONSE_PAYLOAD_SIZE] = {
        0x12u, 0x34u, 0xABu, 0xCDu, 0x10u, 0u, 0u, 0u, 0u
    };
    doip_routing_activation_response_t response = {0u, 0u, 0u};
    doip_result_t result;

    result = doip_routing_activation_response_decode(buffer, sizeof(buffer), &response);
    if (check_result(name, DOIP_RESULT_OK, result) != 0) {
        return 1;
    }

    return check_response(name, &response, 0x1234u, 0xABCDu, 0x10u);
}

static int test_decode_unknown_response_code(void)
{
    const char *name = "decode_unknown_response_code";
    const uint8_t buffer[DOIP_ROUTING_ACTIVATION_RESPONSE_PAYLOAD_SIZE] = {
        0x0Eu, 0x00u, 0x10u, 0x00u, 0x7Fu, 0u, 0u, 0u, 0u
    };
    doip_routing_activation_response_t response = {0u, 0u, 0u};
    doip_result_t result;

    result = doip_routing_activation_response_decode(buffer, sizeof(buffer), &response);
    if (check_result(name, DOIP_RESULT_OK, result) != 0) {
        return 1;
    }

    return check_response(name, &response, 0x0E00u, 0x1000u, 0x7Fu);
}

static int test_decode_reserved_nonzero_preserves_output(void)
{
    const char *name = "decode_reserved_nonzero_preserves_output";
    const doip_routing_activation_response_t initial = {0xAAAAu, 0xBBBBu, 0xCCu};
    const uint8_t base[DOIP_ROUTING_ACTIVATION_RESPONSE_PAYLOAD_SIZE] = {
        0x12u, 0x34u, 0xABu, 0xCDu, 0x10u, 0u, 0u, 0u, 0u
    };
    size_t reserved_index;

    for (reserved_index = 5u; reserved_index < 9u; ++reserved_index) {
        uint8_t buffer[DOIP_ROUTING_ACTIVATION_RESPONSE_PAYLOAD_SIZE];
        doip_routing_activation_response_t response = initial;
        doip_result_t result;

        buffer[0] = base[0];
        buffer[1] = base[1];
        buffer[2] = base[2];
        buffer[3] = base[3];
        buffer[4] = base[4];
        buffer[5] = base[5];
        buffer[6] = base[6];
        buffer[7] = base[7];
        buffer[8] = base[8];
        buffer[reserved_index] = 0x01u;

        result = doip_routing_activation_response_decode(buffer, sizeof(buffer), &response);
        if (check_result(name, DOIP_RESULT_INVALID_HEADER, result) != 0) {
            return 1;
        }
        if (check_response_unchanged(name, &response, &initial) != 0) {
            return 1;
        }
    }

    return 0;
}

static int test_decode_length_boundaries_preserves_output(void)
{
    const char *name = "decode_length_boundaries_preserves_output";
    const uint8_t valid[DOIP_ROUTING_ACTIVATION_RESPONSE_PAYLOAD_SIZE] = {
        0x12u, 0x34u, 0xABu, 0xCDu, 0x10u, 0u, 0u, 0u, 0u
    };
    const doip_routing_activation_response_t initial = {0xAAAAu, 0xBBBBu, 0xCCu};
    const size_t invalid_lengths[] = {0u, 1u, 8u, 10u, 12u, 14u};
    size_t i;

    for (i = 0u; i < sizeof(invalid_lengths) / sizeof(invalid_lengths[0]); ++i) {
        uint8_t buffer[14];
        doip_routing_activation_response_t response = initial;
        doip_result_t expected_result;
        doip_result_t result;

        buffer[0] = valid[0];
        buffer[1] = valid[1];
        buffer[2] = valid[2];
        buffer[3] = valid[3];
        buffer[4] = valid[4];
        buffer[5] = valid[5];
        buffer[6] = valid[6];
        buffer[7] = valid[7];
        buffer[8] = valid[8];
        buffer[9] = 0xAAu;
        buffer[10] = 0xBBu;
        buffer[11] = 0xCCu;
        buffer[12] = 0xDDu;
        buffer[13] = 0xEEu;

        expected_result = invalid_lengths[i] < DOIP_ROUTING_ACTIVATION_RESPONSE_PAYLOAD_SIZE
            ? DOIP_RESULT_BUFFER_TOO_SMALL
            : DOIP_RESULT_INVALID_LENGTH;
        result = doip_routing_activation_response_decode(
            buffer, invalid_lengths[i], &response);
        if (check_result(name, expected_result, result) != 0) {
            return 1;
        }
        if (check_response_unchanged(name, &response, &initial) != 0) {
            return 1;
        }
    }

    {
        uint8_t buffer[13] = {
            0x12u, 0x34u, 0xABu, 0xCDu, 0x10u, 0u, 0u,
            0u, 0u, 0xAAu, 0xBBu, 0xCCu, 0xDDu
        };
        doip_routing_activation_response_t response = initial;
        doip_result_t result;

        result = doip_routing_activation_response_decode(buffer, sizeof(buffer), &response);
        if (check_result(name, DOIP_RESULT_UNSUPPORTED, result) != 0) {
            return 1;
        }
        if (check_response_unchanged(name, &response, &initial) != 0) {
            return 1;
        }
    }

    {
        doip_routing_activation_response_t response = initial;
        doip_result_t result;

        result = doip_routing_activation_response_decode(valid, sizeof(valid), &response);
        if (check_result(name, DOIP_RESULT_OK, result) != 0) {
            return 1;
        }
        return check_response(name, &response, 0x1234u, 0xABCDu, 0x10u);
    }
}

static int test_decode_null_arguments(void)
{
    const char *name = "decode_null_arguments";
    const uint8_t buffer[DOIP_ROUTING_ACTIVATION_RESPONSE_PAYLOAD_SIZE] = {
        0x12u, 0x34u, 0xABu, 0xCDu, 0x10u, 0u, 0u, 0u, 0u
    };
    const doip_routing_activation_response_t initial = {0xAAAAu, 0xBBBBu, 0xCCu};
    doip_routing_activation_response_t response = initial;
    doip_result_t result;

    result = doip_routing_activation_response_decode(
        NULL, sizeof(buffer), &response);
    if (check_result("decode_null_buffer", DOIP_RESULT_INVALID_ARGUMENT, result) != 0) {
        return 1;
    }
    if (check_response_unchanged(name, &response, &initial) != 0) {
        return 1;
    }

    result = doip_routing_activation_response_decode(buffer, sizeof(buffer), NULL);
    return check_result("decode_null_response", DOIP_RESULT_INVALID_ARGUMENT, result);
}

int main(void)
{
    int failures = 0;

    failures += test_encode_success_response();
    failures += test_encode_failure_and_unknown_codes();
    failures += test_encode_preserves_tail_and_capacity_failure();
    failures += test_encode_null_arguments();
    failures += test_decode_known_response();
    failures += test_decode_unknown_response_code();
    failures += test_decode_reserved_nonzero_preserves_output();
    failures += test_decode_length_boundaries_preserves_output();
    failures += test_decode_null_arguments();

    if (failures != 0) {
        printf("%d test(s) failed.\n", failures);
        return 1;
    }

    printf("All test_doip_routing_activation_response tests passed.\n");
    return 0;
}
