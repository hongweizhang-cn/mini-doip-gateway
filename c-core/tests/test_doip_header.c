#include "doip_header.h"

#include <stdio.h>
#include <stdint.h>

static const uint8_t k_expected_wire[DOIP_GENERIC_HEADER_SIZE] = {
    0x02u, 0xFDu, 0x80u, 0x01u, 0x01u, 0x02u, 0x03u, 0x04u
};

static void fill_sample_header(doip_header_t *header)
{
    header->protocol_version = 0x02u;
    header->inverse_protocol_version = 0xFDu;
    header->payload_type = 0x8001u;
    header->payload_length = 0x01020304u;
}

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

static int fail_u8(const char *test_name, const char *field, uint8_t expected, uint8_t actual)
{
    printf("FAIL: %s\n", test_name);
    printf("  expected %s: 0x%02X\n", field, (unsigned int)expected);
    printf("  actual %s:   0x%02X\n", field, (unsigned int)actual);
    return 1;
}

static int fail_u16(const char *test_name, const char *field, uint16_t expected, uint16_t actual)
{
    printf("FAIL: %s\n", test_name);
    printf("  expected %s: 0x%04X\n", field, (unsigned int)expected);
    printf("  actual %s:   0x%04X\n", field, (unsigned int)actual);
    return 1;
}

static int fail_u32(const char *test_name, const char *field, uint32_t expected, uint32_t actual)
{
    printf("FAIL: %s\n", test_name);
    printf("  expected %s: 0x%08X\n", field, (unsigned int)expected);
    printf("  actual %s:   0x%08X\n", field, (unsigned int)actual);
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

static int check_header_fields(
    const char *test_name,
    const doip_header_t *header,
    uint8_t protocol_version,
    uint8_t inverse_protocol_version,
    uint16_t payload_type,
    uint32_t payload_length)
{
    if (header->protocol_version != protocol_version) {
        return fail_u8(test_name, "protocol_version", protocol_version, header->protocol_version);
    }
    if (header->inverse_protocol_version != inverse_protocol_version) {
        return fail_u8(
            test_name,
            "inverse_protocol_version",
            inverse_protocol_version,
            header->inverse_protocol_version);
    }
    if (header->payload_type != payload_type) {
        return fail_u16(test_name, "payload_type", payload_type, header->payload_type);
    }
    if (header->payload_length != payload_length) {
        return fail_u32(test_name, "payload_length", payload_length, header->payload_length);
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

    for (i = 0; i < count; ++i) {
        if (buffer[i] != expected[i]) {
            printf("FAIL: %s\n", test_name);
            printf("  expected buffer[%u]: 0x%02X\n", (unsigned int)i, (unsigned int)expected[i]);
            printf("  actual buffer[%u]:   0x%02X\n", (unsigned int)i, (unsigned int)buffer[i]);
            return 1;
        }
    }
    return 0;
}

static int test_encode_ok(void)
{
    const char *name = "encode_ok";
    doip_header_t header;
    uint8_t buffer[DOIP_GENERIC_HEADER_SIZE];
    doip_result_t result;

    fill_sample_header(&header);
    result = doip_header_encode(&header, buffer, sizeof(buffer));
    if (check_result(name, DOIP_RESULT_OK, result) != 0) {
        return 1;
    }
    return check_wire_bytes(name, buffer, k_expected_wire, DOIP_GENERIC_HEADER_SIZE);
}

static int test_decode_ok(void)
{
    const char *name = "decode_ok";
    doip_header_t header;
    doip_result_t result;

    result = doip_header_decode(k_expected_wire, DOIP_GENERIC_HEADER_SIZE, &header);
    if (check_result(name, DOIP_RESULT_OK, result) != 0) {
        return 1;
    }
    return check_header_fields(name, &header, 0x02u, 0xFDu, 0x8001u, 0x01020304u);
}

static int test_encode_decode_round_trip(void)
{
    const char *name = "encode_decode_round_trip";
    doip_header_t original;
    doip_header_t decoded;
    uint8_t buffer[DOIP_GENERIC_HEADER_SIZE];
    doip_result_t result;

    fill_sample_header(&original);

    result = doip_header_encode(&original, buffer, sizeof(buffer));
    if (check_result(name, DOIP_RESULT_OK, result) != 0) {
        return 1;
    }

    result = doip_header_decode(buffer, sizeof(buffer), &decoded);
    if (check_result(name, DOIP_RESULT_OK, result) != 0) {
        return 1;
    }

    return check_header_fields(
        name,
        &decoded,
        original.protocol_version,
        original.inverse_protocol_version,
        original.payload_type,
        original.payload_length);
}

static int test_encode_null_arguments(void)
{
    const char *name;
    doip_header_t header;
    uint8_t buffer[DOIP_GENERIC_HEADER_SIZE];
    doip_result_t result;

    fill_sample_header(&header);

    name = "encode_null_header";
    result = doip_header_encode(NULL, buffer, sizeof(buffer));
    if (check_result(name, DOIP_RESULT_INVALID_ARGUMENT, result) != 0) {
        return 1;
    }

    name = "encode_null_buffer";
    result = doip_header_encode(&header, NULL, sizeof(buffer));
    if (check_result(name, DOIP_RESULT_INVALID_ARGUMENT, result) != 0) {
        return 1;
    }

    name = "encode_null_header_and_buffer";
    result = doip_header_encode(NULL, NULL, DOIP_GENERIC_HEADER_SIZE);
    if (check_result(name, DOIP_RESULT_INVALID_ARGUMENT, result) != 0) {
        return 1;
    }

    return 0;
}

static int test_decode_null_arguments(void)
{
    const char *name;
    doip_header_t header;
    doip_result_t result;

    name = "decode_null_buffer";
    result = doip_header_decode(NULL, DOIP_GENERIC_HEADER_SIZE, &header);
    if (check_result(name, DOIP_RESULT_INVALID_ARGUMENT, result) != 0) {
        return 1;
    }

    name = "decode_null_header";
    result = doip_header_decode(k_expected_wire, DOIP_GENERIC_HEADER_SIZE, NULL);
    if (check_result(name, DOIP_RESULT_INVALID_ARGUMENT, result) != 0) {
        return 1;
    }

    name = "decode_null_buffer_and_header";
    result = doip_header_decode(NULL, DOIP_GENERIC_HEADER_SIZE, NULL);
    if (check_result(name, DOIP_RESULT_INVALID_ARGUMENT, result) != 0) {
        return 1;
    }

    return 0;
}

static int test_encode_buffer_too_small(void)
{
    const char *name;
    doip_header_t header;
    uint8_t buffer[DOIP_GENERIC_HEADER_SIZE];
    doip_result_t result;

    fill_sample_header(&header);

    name = "encode_buffer_size_0";
    result = doip_header_encode(&header, buffer, 0u);
    if (check_result(name, DOIP_RESULT_BUFFER_TOO_SMALL, result) != 0) {
        return 1;
    }

    name = "encode_buffer_size_7";
    result = doip_header_encode(&header, buffer, 7u);
    if (check_result(name, DOIP_RESULT_BUFFER_TOO_SMALL, result) != 0) {
        return 1;
    }

    return 0;
}

static int test_decode_buffer_too_small(void)
{
    const char *name;
    doip_header_t header;
    doip_result_t result;

    name = "decode_buffer_size_0";
    result = doip_header_decode(k_expected_wire, 0u, &header);
    if (check_result(name, DOIP_RESULT_BUFFER_TOO_SMALL, result) != 0) {
        return 1;
    }

    name = "decode_buffer_size_7";
    result = doip_header_decode(k_expected_wire, 7u, &header);
    if (check_result(name, DOIP_RESULT_BUFFER_TOO_SMALL, result) != 0) {
        return 1;
    }

    return 0;
}

static int test_big_endian_layout(void)
{
    const char *name = "big_endian_layout";
    doip_header_t header;
    uint8_t buffer[DOIP_GENERIC_HEADER_SIZE];
    doip_result_t result;

    fill_sample_header(&header);
    result = doip_header_encode(&header, buffer, sizeof(buffer));
    if (check_result(name, DOIP_RESULT_OK, result) != 0) {
        return 1;
    }

    if (buffer[2] != 0x80u) {
        return fail_u8(name, "buffer[2] (payload_type high)", 0x80u, buffer[2]);
    }
    if (buffer[3] != 0x01u) {
        return fail_u8(name, "buffer[3] (payload_type low)", 0x01u, buffer[3]);
    }
    if (buffer[4] != 0x01u) {
        return fail_u8(name, "buffer[4] (payload_length b0)", 0x01u, buffer[4]);
    }
    if (buffer[5] != 0x02u) {
        return fail_u8(name, "buffer[5] (payload_length b1)", 0x02u, buffer[5]);
    }
    if (buffer[6] != 0x03u) {
        return fail_u8(name, "buffer[6] (payload_length b2)", 0x03u, buffer[6]);
    }
    if (buffer[7] != 0x04u) {
        return fail_u8(name, "buffer[7] (payload_length b3)", 0x04u, buffer[7]);
    }

    return 0;
}

static int test_encode_buffer_larger_than_header(void)
{
    const char *name = "encode_buffer_size_16";
    doip_header_t header;
    uint8_t buffer[16];
    size_t i;
    doip_result_t result;

    fill_sample_header(&header);
    for (i = 0; i < sizeof(buffer); ++i) {
        buffer[i] = 0xAAu;
    }

    result = doip_header_encode(&header, buffer, sizeof(buffer));
    if (check_result(name, DOIP_RESULT_OK, result) != 0) {
        return 1;
    }
    if (check_wire_bytes(name, buffer, k_expected_wire, DOIP_GENERIC_HEADER_SIZE) != 0) {
        return 1;
    }

    for (i = DOIP_GENERIC_HEADER_SIZE; i < sizeof(buffer); ++i) {
        if (buffer[i] != 0xAAu) {
            return fail_u8(name, "byte beyond header", 0xAAu, buffer[i]);
        }
    }

    return 0;
}

int main(void)
{
    int failures = 0;

    failures += test_encode_ok();
    failures += test_decode_ok();
    failures += test_encode_decode_round_trip();
    failures += test_encode_null_arguments();
    failures += test_decode_null_arguments();
    failures += test_encode_buffer_too_small();
    failures += test_decode_buffer_too_small();
    failures += test_big_endian_layout();
    failures += test_encode_buffer_larger_than_header();

    if (failures != 0) {
        printf("%d test(s) failed.\n", failures);
        return 1;
    }

    printf("All test_doip_header tests passed.\n");
    return 0;
}
