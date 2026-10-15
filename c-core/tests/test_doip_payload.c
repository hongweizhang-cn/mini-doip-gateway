#include "doip_payload.h"

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
            printf("  expected[%lu]: 0x%02X\n", (unsigned long)i, (unsigned int)expected[i]);
            printf("  actual[%lu]:   0x%02X\n", (unsigned long)i, (unsigned int)actual[i]);
            return 1;
        }
    }

    return 0;
}

static int test_check_length_equal(void)
{
    const char *name = "check_length_equal";
    doip_result_t result;

    result = doip_payload_check_length(8u, 8u);
    return check_result(name, DOIP_RESULT_OK, result);
}

static int test_check_length_greater(void)
{
    const char *name = "check_length_greater";
    doip_result_t result;

    result = doip_payload_check_length(8u, 16u);
    return check_result(name, DOIP_RESULT_OK, result);
}

static int test_check_length_smaller(void)
{
    const char *name = "check_length_smaller";
    doip_result_t result;

    result = doip_payload_check_length(8u, 7u);
    return check_result(name, DOIP_RESULT_BUFFER_TOO_SMALL, result);
}

static int test_check_length_zero_available_zero(void)
{
    const char *name = "check_length_zero_available_zero";
    doip_result_t result;

    result = doip_payload_check_length(0u, 0u);
    return check_result(name, DOIP_RESULT_OK, result);
}

static int test_check_length_zero_available_positive(void)
{
    const char *name = "check_length_zero_available_positive";
    doip_result_t result;

    result = doip_payload_check_length(0u, 4u);
    return check_result(name, DOIP_RESULT_OK, result);
}

static int test_copy_success_known_bytes(void)
{
    const char *name = "copy_success_known_bytes";
    const uint8_t source_bytes[4] = {0x10u, 0x20u, 0x30u, 0x40u};
    uint8_t destination_bytes[4] = {0x00u, 0x00u, 0x00u, 0x00u};
    doip_result_t result;

    result = doip_payload_copy(
        source_bytes, 4u, destination_bytes, sizeof(destination_bytes));
    if (check_result(name, DOIP_RESULT_OK, result) != 0) {
        return 1;
    }

    return check_bytes(
        name, destination_bytes, source_bytes, sizeof(source_bytes));
}

static int test_copy_success_exact_capacity(void)
{
    const char *name = "copy_success_exact_capacity";
    const uint8_t source_bytes[5] = {0xAAu, 0xBBu, 0xCCu, 0xDDu, 0xEEu};
    uint8_t destination_bytes[5] = {0u};
    doip_result_t result;

    result = doip_payload_copy(
        source_bytes, 5u, destination_bytes, sizeof(destination_bytes));
    if (check_result(name, DOIP_RESULT_OK, result) != 0) {
        return 1;
    }

    return check_bytes(
        name, destination_bytes, source_bytes, sizeof(source_bytes));
}

static int test_copy_success_larger_buffer_keeps_remainder(void)
{
    const char *name = "copy_success_larger_buffer_keeps_remainder";
    const uint8_t source_bytes[3] = {0x01u, 0x02u, 0x03u};
    uint8_t destination_bytes[6] = {
        0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu
    };
    const uint8_t expected[6] = {0x01u, 0x02u, 0x03u, 0xFFu, 0xFFu, 0xFFu};
    doip_result_t result;

    result = doip_payload_copy(
        source_bytes, 3u, destination_bytes, sizeof(destination_bytes));
    if (check_result(name, DOIP_RESULT_OK, result) != 0) {
        return 1;
    }

    return check_bytes(name, destination_bytes, expected, sizeof(expected));
}

static int test_copy_buffer_too_small_no_write(void)
{
    const char *name = "copy_buffer_too_small_no_write";
    const uint8_t source_bytes[4] = {0x01u, 0x02u, 0x03u, 0x04u};
    uint8_t destination_bytes[3] = {0xAAu, 0xAAu, 0xAAu};
    doip_result_t result;
    size_t i;

    result = doip_payload_copy(
        source_bytes, 4u, destination_bytes, sizeof(destination_bytes));
    if (check_result(name, DOIP_RESULT_BUFFER_TOO_SMALL, result) != 0) {
        return 1;
    }

    for (i = 0u; i < sizeof(destination_bytes); ++i) {
        if (destination_bytes[i] != 0xAAu) {
            printf("FAIL: %s\n", name);
            printf("  expected destination_bytes[%lu] to remain 0xAA\n",
                (unsigned long)i);
            printf("  actual destination_bytes[%lu]:   0x%02X\n",
                (unsigned long)i, (unsigned int)destination_bytes[i]);
            return 1;
        }
    }

    return 0;
}

static int test_copy_src_null_nonzero(void)
{
    const char *name = "copy_src_null_nonzero";
    uint8_t destination_bytes[4] = {0u};
    doip_result_t result;

    result = doip_payload_copy(
        NULL, 4u, destination_bytes, sizeof(destination_bytes));
    return check_result(name, DOIP_RESULT_INVALID_ARGUMENT, result);
}

static int test_copy_dst_null_nonzero(void)
{
    const char *name = "copy_dst_null_nonzero";
    const uint8_t source_bytes[4] = {0x01u, 0x02u, 0x03u, 0x04u};
    doip_result_t result;

    result = doip_payload_copy(source_bytes, 4u, NULL, 4u);
    return check_result(name, DOIP_RESULT_INVALID_ARGUMENT, result);
}

static int test_copy_zero_length_nulls(void)
{
    const char *name = "copy_zero_length_nulls";
    doip_result_t result;

    result = doip_payload_copy(NULL, 0u, NULL, 0u);
    return check_result(name, DOIP_RESULT_OK, result);
}

static int test_copy_zero_length_nonnull_pointers(void)
{
    const char *name = "copy_zero_length_nonnull_pointers";
    const uint8_t source_bytes[4] = {0x11u, 0x22u, 0x33u, 0x44u};
    uint8_t destination_bytes[4] = {0xFFu, 0xFFu, 0xFFu, 0xFFu};
    const uint8_t expected[4] = {0xFFu, 0xFFu, 0xFFu, 0xFFu};
    doip_result_t result;

    result = doip_payload_copy(
        source_bytes, 0u, destination_bytes, sizeof(destination_bytes));
    if (check_result(name, DOIP_RESULT_OK, result) != 0) {
        return 1;
    }

    return check_bytes(name, destination_bytes, expected, sizeof(expected));
}

static int test_copy_one_byte_success(void)
{
    const char *name = "copy_one_byte_success";
    const uint8_t source_bytes[1] = {0x7Fu};
    uint8_t destination_bytes[1] = {0u};
    doip_result_t result;

    result = doip_payload_copy(
        source_bytes, 1u, destination_bytes, sizeof(destination_bytes));
    if (check_result(name, DOIP_RESULT_OK, result) != 0) {
        return 1;
    }

    return check_bytes(
        name, destination_bytes, source_bytes, sizeof(source_bytes));
}

static int test_copy_source_not_modified(void)
{
    const char *name = "copy_source_not_modified";
    uint8_t source_bytes[4] = {0x01u, 0x02u, 0x03u, 0x04u};
    uint8_t destination_bytes[4] = {0u};
    const uint8_t expected[4] = {0x01u, 0x02u, 0x03u, 0x04u};
    doip_result_t result;

    result = doip_payload_copy(
        source_bytes, sizeof(source_bytes),
        destination_bytes, sizeof(destination_bytes));
    if (check_result(name, DOIP_RESULT_OK, result) != 0) {
        return 1;
    }

    return check_bytes(name, source_bytes, expected, sizeof(expected));
}

int main(void)
{
    int failures = 0;

    failures += test_check_length_equal();
    failures += test_check_length_greater();
    failures += test_check_length_smaller();
    failures += test_check_length_zero_available_zero();
    failures += test_check_length_zero_available_positive();
    failures += test_copy_success_known_bytes();
    failures += test_copy_success_exact_capacity();
    failures += test_copy_success_larger_buffer_keeps_remainder();
    failures += test_copy_buffer_too_small_no_write();
    failures += test_copy_src_null_nonzero();
    failures += test_copy_dst_null_nonzero();
    failures += test_copy_zero_length_nulls();
    failures += test_copy_zero_length_nonnull_pointers();
    failures += test_copy_one_byte_success();
    failures += test_copy_source_not_modified();

    if (failures != 0) {
        printf("%d test(s) failed.\n", failures);
        return 1;
    }

    printf("All test_doip_payload tests passed.\n");
    return 0;
}
