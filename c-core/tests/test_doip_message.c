#include "doip_message.h"

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

static int check_header(
    const char *test_name,
    const doip_header_t *actual,
    const doip_header_t *expected)
{
    if ((actual->protocol_version != expected->protocol_version) ||
        (actual->inverse_protocol_version != expected->inverse_protocol_version) ||
        (actual->payload_type != expected->payload_type) ||
        (actual->payload_length != expected->payload_length)) {
        printf("FAIL: %s: decoded Header fields differ\n", test_name);
        return 1;
    }

    return 0;
}

static int test_encode_nonempty_message(void)
{
    const char *name = "encode_nonempty_message";
    const doip_header_t header = {0x02u, 0xFDu, 0x8001u, 3u};
    const uint8_t payload[3] = {0x10u, 0x20u, 0x30u};
    uint8_t buffer[11];
    const uint8_t expected[11] = {
        0x02u, 0xFDu, 0x80u, 0x01u,
        0x00u, 0x00u, 0x00u, 0x03u,
        0x10u, 0x20u, 0x30u
    };
    doip_result_t result;

    result = doip_message_encode(&header, payload, sizeof(payload), buffer, sizeof(buffer));
    if (check_result(name, DOIP_RESULT_OK, result) != 0) {
        return 1;
    }

    return check_bytes(name, buffer, expected, sizeof(expected));
}

static int test_encode_preserves_extra_capacity(void)
{
    const char *name = "encode_preserves_extra_capacity";
    const doip_header_t header = {0x02u, 0xFDu, 0x0005u, 1u};
    const uint8_t payload[1] = {0xA5u};
    uint8_t buffer[12] = {
        0xCCu, 0xCCu, 0xCCu, 0xCCu, 0xCCu, 0xCCu,
        0xCCu, 0xCCu, 0xCCu, 0xCCu, 0xCCu, 0xCCu
    };
    const uint8_t expected[12] = {
        0x02u, 0xFDu, 0x00u, 0x05u,
        0x00u, 0x00u, 0x00u, 0x01u,
        0xA5u, 0xCCu, 0xCCu, 0xCCu
    };
    doip_result_t result;

    result = doip_message_encode(&header, payload, sizeof(payload), buffer, sizeof(buffer));
    if (check_result(name, DOIP_RESULT_OK, result) != 0) {
        return 1;
    }

    return check_bytes(name, buffer, expected, sizeof(expected));
}

static int test_encode_length_mismatch_no_write(void)
{
    const char *name = "encode_length_mismatch_no_write";
    const doip_header_t header = {0x02u, 0xFDu, 0x8001u, 2u};
    const uint8_t payload[3] = {0x10u, 0x20u, 0x30u};
    uint8_t buffer[11];
    uint8_t expected[11];
    doip_result_t result;
    size_t i;

    for (i = 0u; i < sizeof(buffer); ++i) {
        buffer[i] = 0xCCu;
        expected[i] = 0xCCu;
    }

    result = doip_message_encode(&header, payload, sizeof(payload), buffer, sizeof(buffer));
    if (check_result(name, DOIP_RESULT_INVALID_LENGTH, result) != 0) {
        return 1;
    }

    return check_bytes(name, buffer, expected, sizeof(buffer));
}

static int test_encode_capacity_too_small_no_write(void)
{
    const char *name = "encode_capacity_too_small_no_write";
    const doip_header_t header = {0x02u, 0xFDu, 0x8001u, 3u};
    const uint8_t payload[3] = {0x10u, 0x20u, 0x30u};
    uint8_t buffer[10];
    uint8_t expected[10];
    doip_result_t result;
    size_t i;

    for (i = 0u; i < sizeof(buffer); ++i) {
        buffer[i] = 0xCCu;
        expected[i] = 0xCCu;
    }

    result = doip_message_encode(&header, payload, sizeof(payload), buffer, sizeof(buffer));
    if (check_result(name, DOIP_RESULT_BUFFER_TOO_SMALL, result) != 0) {
        return 1;
    }

    return check_bytes(name, buffer, expected, sizeof(buffer));
}

static int test_encode_null_arguments(void)
{
    const doip_header_t header = {0x02u, 0xFDu, 0x8001u, 1u};
    const uint8_t payload[1] = {0x11u};
    uint8_t buffer[9];
    doip_result_t result;

    result = doip_message_encode(NULL, payload, sizeof(payload), buffer, sizeof(buffer));
    if (check_result("encode_null_header", DOIP_RESULT_INVALID_ARGUMENT, result) != 0) {
        return 1;
    }

    result = doip_message_encode(&header, payload, sizeof(payload), NULL, sizeof(buffer));
    if (check_result("encode_null_destination", DOIP_RESULT_INVALID_ARGUMENT, result) != 0) {
        return 1;
    }

    result = doip_message_encode(&header, NULL, sizeof(payload), buffer, sizeof(buffer));
    if (check_result("encode_null_nonempty_payload", DOIP_RESULT_INVALID_ARGUMENT, result) != 0) {
        return 1;
    }

    return 0;
}

static int test_encode_payload_size_exceeds_uint32(void)
{
    const char *name = "encode_payload_size_exceeds_uint32";
    const doip_header_t header = {0x02u, 0xFDu, 0x8001u, UINT32_MAX};
    uint8_t buffer[DOIP_GENERIC_HEADER_SIZE] = {
        0xCCu, 0xCCu, 0xCCu, 0xCCu, 0xCCu, 0xCCu, 0xCCu, 0xCCu
    };
    const uint8_t expected[DOIP_GENERIC_HEADER_SIZE] = {
        0xCCu, 0xCCu, 0xCCu, 0xCCu, 0xCCu, 0xCCu, 0xCCu, 0xCCu
    };
    doip_result_t result;

    if (SIZE_MAX <= UINT32_MAX) {
        return 0;
    }

    result = doip_message_encode(
        &header,
        NULL,
        (size_t)UINT32_MAX + 1u,
        buffer,
        sizeof(buffer));
    if (check_result(name, DOIP_RESULT_INVALID_LENGTH, result) != 0) {
        return 1;
    }

    return check_bytes(name, buffer, expected, sizeof(buffer));
}

static int test_decode_nonempty_message(void)
{
    const char *name = "decode_nonempty_message";
    const uint8_t buffer[11] = {
        0x02u, 0xFDu, 0x80u, 0x01u,
        0x00u, 0x00u, 0x00u, 0x03u,
        0x10u, 0x20u, 0x30u
    };
    const doip_header_t expected_header = {0x02u, 0xFDu, 0x8001u, 3u};
    const uint8_t expected_payload[3] = {0x10u, 0x20u, 0x30u};
    doip_message_view_t message;
    doip_result_t result;

    result = doip_message_decode(buffer, sizeof(buffer), &message);
    if (check_result(name, DOIP_RESULT_OK, result) != 0) {
        return 1;
    }

    if (check_header(name, &message.header, &expected_header) != 0) {
        return 1;
    }
    if (message.payload != &buffer[DOIP_GENERIC_HEADER_SIZE]) {
        printf("FAIL: %s: payload view points to the wrong location\n", name);
        return 1;
    }

    return check_bytes(name, message.payload, expected_payload, sizeof(expected_payload));
}

static int test_zero_length_round_trip(void)
{
    const char *name = "zero_length_round_trip";
    const doip_header_t header = {0x02u, 0xFDu, 0x0005u, 0u};
    const uint8_t expected[DOIP_GENERIC_HEADER_SIZE] = {
        0x02u, 0xFDu, 0x00u, 0x05u,
        0x00u, 0x00u, 0x00u, 0x00u
    };
    uint8_t buffer[DOIP_GENERIC_HEADER_SIZE];
    doip_message_view_t message;
    doip_result_t result;

    result = doip_message_encode(&header, NULL, 0u, buffer, sizeof(buffer));
    if (check_result(name, DOIP_RESULT_OK, result) != 0) {
        return 1;
    }
    if (check_bytes(name, buffer, expected, sizeof(expected)) != 0) {
        return 1;
    }

    result = doip_message_decode(buffer, sizeof(buffer), &message);
    if (check_result(name, DOIP_RESULT_OK, result) != 0) {
        return 1;
    }
    if (check_header(name, &message.header, &header) != 0) {
        return 1;
    }
    if (message.payload != buffer + DOIP_GENERIC_HEADER_SIZE) {
        printf("FAIL: %s: empty payload view is not one-past the header\n", name);
        return 1;
    }

    return 0;
}

static int check_message_unchanged(
    const char *test_name,
    const doip_message_view_t *message,
    const doip_message_view_t *expected)
{
    if ((message->header.protocol_version != expected->header.protocol_version) ||
        (message->header.inverse_protocol_version != expected->header.inverse_protocol_version) ||
        (message->header.payload_type != expected->header.payload_type) ||
        (message->header.payload_length != expected->header.payload_length) ||
        (message->payload != expected->payload)) {
        printf("FAIL: %s: output view changed on failure\n", test_name);
        return 1;
    }

    return 0;
}

static int test_decode_short_header_preserves_output(void)
{
    const char *name = "decode_short_header_preserves_output";
    const uint8_t buffer[7] = {0x02u, 0xFDu, 0x80u, 0x01u, 0u, 0u, 0u};
    const doip_message_view_t initial = {{0x11u, 0xEEu, 0x2222u, 0x33333333u}, buffer};
    doip_message_view_t message = initial;
    doip_result_t result;

    result = doip_message_decode(buffer, sizeof(buffer), &message);
    if (check_result(name, DOIP_RESULT_BUFFER_TOO_SMALL, result) != 0) {
        return 1;
    }

    return check_message_unchanged(name, &message, &initial);
}

static int test_decode_truncated_payload_preserves_output(void)
{
    const char *name = "decode_truncated_payload_preserves_output";
    const uint8_t buffer[10] = {
        0x02u, 0xFDu, 0x80u, 0x01u,
        0x00u, 0x00u, 0x00u, 0x03u,
        0x10u, 0x20u
    };
    const doip_message_view_t initial = {{0x11u, 0xEEu, 0x2222u, 0x33333333u}, buffer};
    doip_message_view_t message = initial;
    doip_result_t result;

    result = doip_message_decode(buffer, sizeof(buffer), &message);
    if (check_result(name, DOIP_RESULT_BUFFER_TOO_SMALL, result) != 0) {
        return 1;
    }

    return check_message_unchanged(name, &message, &initial);
}

static int test_decode_trailing_data_preserves_output(void)
{
    const char *name = "decode_trailing_data_preserves_output";
    const uint8_t buffer[10] = {
        0x02u, 0xFDu, 0x80u, 0x01u,
        0x00u, 0x00u, 0x00u, 0x01u,
        0x10u, 0x20u
    };
    const doip_message_view_t initial = {{0x11u, 0xEEu, 0x2222u, 0x33333333u}, buffer};
    doip_message_view_t message = initial;
    doip_result_t result;

    result = doip_message_decode(buffer, sizeof(buffer), &message);
    if (check_result(name, DOIP_RESULT_INVALID_LENGTH, result) != 0) {
        return 1;
    }

    return check_message_unchanged(name, &message, &initial);
}

static int test_decode_null_arguments(void)
{
    const char *name = "decode_null_arguments";
    const uint8_t buffer[DOIP_GENERIC_HEADER_SIZE] = {0u};
    doip_message_view_t message = {{0u, 0u, 0u, 0u}, NULL};
    doip_result_t result;

    result = doip_message_decode(NULL, sizeof(buffer), &message);
    if (check_result("decode_null_input", DOIP_RESULT_INVALID_ARGUMENT, result) != 0) {
        return 1;
    }

    result = doip_message_decode(buffer, sizeof(buffer), NULL);
    if (check_result("decode_null_output", DOIP_RESULT_INVALID_ARGUMENT, result) != 0) {
        return 1;
    }

    return check_message_unchanged(name, &message, &(doip_message_view_t){{0u, 0u, 0u, 0u}, NULL});
}

static int test_decode_unknown_type_and_large_length(void)
{
    const char *name = "decode_unknown_type_and_large_length";
    const uint8_t unknown_type_message[9] = {
        0x02u, 0xFDu, 0xFEu, 0xDCu,
        0x00u, 0x00u, 0x00u, 0x01u,
        0x55u
    };
    const uint8_t large_length_header[DOIP_GENERIC_HEADER_SIZE] = {
        0x02u, 0xFDu, 0xFEu, 0xDCu,
        0x01u, 0x02u, 0x03u, 0x04u
    };
    doip_message_view_t message;
    doip_message_view_t initial = {{0x11u, 0xEEu, 0x2222u, 0x33333333u}, NULL};
    doip_header_t decoded_header;
    doip_result_t result;

    result = doip_header_decode(
        large_length_header, sizeof(large_length_header), &decoded_header);
    if (check_result(name, DOIP_RESULT_OK, result) != 0) {
        return 1;
    }
    if (decoded_header.payload_length != 0x01020304u) {
        printf("FAIL: %s: large payload length was not decoded in big-endian order\n", name);
        return 1;
    }

    result = doip_message_decode(
        unknown_type_message, sizeof(unknown_type_message), &message);
    if (check_result(name, DOIP_RESULT_OK, result) != 0) {
        return 1;
    }
    if ((message.header.payload_type != 0xFEDCu) ||
        (message.header.payload_length != 1u) ||
        (message.payload != &unknown_type_message[DOIP_GENERIC_HEADER_SIZE]) ||
        (message.payload[0] != 0x55u)) {
        printf("FAIL: %s: unknown Payload Type was not preserved\n", name);
        return 1;
    }

    message = initial;
    result = doip_message_decode(
        large_length_header, sizeof(large_length_header), &message);
    if (check_result(name, DOIP_RESULT_BUFFER_TOO_SMALL, result) != 0) {
        return 1;
    }
    if (message.header.payload_type != initial.header.payload_type ||
        message.header.payload_length != initial.header.payload_length ||
        message.payload != initial.payload) {
        printf("FAIL: %s: large declared length changed output view\n", name);
        return 1;
    }

    return 0;
}

int main(void)
{
    int failures = 0;

    failures += test_encode_nonempty_message();
    failures += test_encode_preserves_extra_capacity();
    failures += test_encode_length_mismatch_no_write();
    failures += test_encode_capacity_too_small_no_write();
    failures += test_encode_null_arguments();
    failures += test_encode_payload_size_exceeds_uint32();
    failures += test_decode_nonempty_message();
    failures += test_zero_length_round_trip();
    failures += test_decode_short_header_preserves_output();
    failures += test_decode_truncated_payload_preserves_output();
    failures += test_decode_trailing_data_preserves_output();
    failures += test_decode_null_arguments();
    failures += test_decode_unknown_type_and_large_length();

    if (failures != 0) {
        printf("%d test(s) failed.\n", failures);
        return 1;
    }

    printf("All test_doip_message tests passed.\n");
    return 0;
}
