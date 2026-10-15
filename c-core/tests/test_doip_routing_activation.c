#include "doip_routing_activation.h"

#include <stdio.h>
#include <stdint.h>

#include "doip_header.h"
#include "doip_message.h"
#include "doip_payload_type.h"
#include "doip_routing_activation_request.h"
#include "doip_routing_activation_response.h"

#define TEST_REQUEST_MESSAGE_SIZE \
    (DOIP_GENERIC_HEADER_SIZE + DOIP_ROUTING_ACTIVATION_REQUEST_PAYLOAD_SIZE)
#define TEST_RESPONSE_MESSAGE_SIZE \
    (DOIP_GENERIC_HEADER_SIZE + DOIP_ROUTING_ACTIVATION_RESPONSE_PAYLOAD_SIZE)

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

static int build_request_message(
    uint16_t client_address,
    uint8_t payload_type_selector,
    uint8_t message[TEST_REQUEST_MESSAGE_SIZE])
{
    doip_routing_activation_request_t request;
    doip_header_t header;
    uint8_t body[DOIP_ROUTING_ACTIVATION_REQUEST_PAYLOAD_SIZE];
    doip_result_t result;

    request.source_address = client_address;
    request.activation_type = 0x01u;
    result = doip_routing_activation_request_encode(&request, body, sizeof(body));
    if (result != DOIP_RESULT_OK) {
        return 1;
    }

    header.protocol_version = 0x02u;
    header.inverse_protocol_version = 0xFDu;
    header.payload_type = payload_type_selector == 0u
        ? DOIP_PAYLOAD_TYPE_ROUTING_ACTIVATION_REQUEST
        : 0x8001u;
    header.payload_length = (uint32_t)sizeof(body);

    return doip_message_encode(&header, body, sizeof(body), message,
        TEST_REQUEST_MESSAGE_SIZE) == DOIP_RESULT_OK ? 0 : 1;
}

static int process_request(
    const uint8_t *request_message,
    size_t request_message_size,
    uint16_t doip_entity_logical_address,
    uint8_t response_code,
    uint8_t *response_message,
    size_t response_message_capacity,
    size_t *response_message_size)
{
    doip_routing_activation_response_parameters_t response_parameters;

    response_parameters.doip_entity_logical_address = doip_entity_logical_address;
    response_parameters.response_code = response_code;
    return doip_routing_activation_process_request_message(
        request_message,
        request_message_size,
        &response_parameters,
        response_message,
        response_message_capacity,
        response_message_size) ==
        DOIP_RESULT_OK ? 0 : 1;
}

static int check_successful_response(
    const char *name,
    const uint8_t *output,
    size_t output_size,
    uint16_t client_address,
    uint16_t entity_address,
    uint8_t response_code)
{
    doip_message_view_t message;
    doip_routing_activation_response_t response;
    doip_result_t result;

    if (output_size != TEST_RESPONSE_MESSAGE_SIZE) {
        printf("FAIL: %s: wrong complete response size\n", name);
        return 1;
    }

    result = doip_message_decode(output, output_size, &message);
    if (check_result(name, DOIP_RESULT_OK, result) != 0) {
        return 1;
    }
    if ((message.header.protocol_version != 0x02u) ||
        (message.header.inverse_protocol_version != 0xFDu) ||
        (message.header.payload_type != DOIP_PAYLOAD_TYPE_ROUTING_ACTIVATION_RESPONSE) ||
        (message.header.payload_length != DOIP_ROUTING_ACTIVATION_RESPONSE_PAYLOAD_SIZE)) {
        printf("FAIL: %s: response Generic Header mismatch\n", name);
        return 1;
    }

    result = doip_routing_activation_response_decode(
        message.payload, message.header.payload_length, &response);
    if (check_result(name, DOIP_RESULT_OK, result) != 0) {
        return 1;
    }
    if ((response.client_logical_address != client_address) ||
        (response.doip_entity_logical_address != entity_address) ||
        (response.response_code != response_code)) {
        printf("FAIL: %s: response Body fields mismatch\n", name);
        return 1;
    }

    return 0;
}

static int test_success_response_flow(void)
{
    const char *name = "success_response_flow";
    uint8_t input[TEST_REQUEST_MESSAGE_SIZE];
    uint8_t output[TEST_RESPONSE_MESSAGE_SIZE];
    size_t output_size = 0u;

    if (build_request_message(0x0E00u, 0u, input) != 0) {
        printf("FAIL: %s: could not prepare Request message\n", name);
        return 1;
    }

    if (process_request(input, sizeof(input), 0x1000u, 0x10u,
            output, sizeof(output), &output_size) != 0) {
        printf("FAIL: %s: handler rejected valid Request\n", name);
        return 1;
    }

    return check_successful_response(
        name, output, output_size, 0x0E00u, 0x1000u, 0x10u);
}

static int test_rejection_response_flow(void)
{
    const char *name = "rejection_response_flow";
    uint8_t input[TEST_REQUEST_MESSAGE_SIZE];
    uint8_t output[TEST_RESPONSE_MESSAGE_SIZE];
    size_t output_size = 0u;

    if (build_request_message(0x1234u, 0u, input) != 0) {
        printf("FAIL: %s: could not prepare Request message\n", name);
        return 1;
    }

    if (process_request(input, sizeof(input), 0xABCDu, 0x06u,
            output, sizeof(output), &output_size) != 0) {
        printf("FAIL: %s: handler rejected valid Request\n", name);
        return 1;
    }

    return check_successful_response(
        name, output, output_size, 0x1234u, 0xABCDu, 0x06u);
}

static int test_build_response_message_from_decoded_request(void)
{
    const char *name = "build_response_message_from_decoded_request";
    const doip_routing_activation_request_t request = {0x1234u, 0x01u};
    const doip_routing_activation_response_parameters_t response_parameters = {
        0xABCDu, 0x7Fu
    };
    uint8_t output[TEST_RESPONSE_MESSAGE_SIZE];
    size_t output_size = 0u;
    doip_result_t result;

    result = doip_routing_activation_encode_response_message(
        &request, &response_parameters, output, sizeof(output), &output_size);
    if (check_result(name, DOIP_RESULT_OK, result) != 0) {
        return 1;
    }

    return check_successful_response(
        name, output, output_size, 0x1234u, 0xABCDu, 0x7Fu);
}

static int test_raw_api_allows_input_output_overlap(void)
{
    const char *name = "raw_api_allows_input_output_overlap";
    const doip_routing_activation_response_parameters_t parameters = {0x1000u, 0x10u};
    uint8_t message[TEST_RESPONSE_MESSAGE_SIZE];
    size_t output_size = 0u;
    doip_result_t result;

    if (build_request_message(0x0E00u, 0u, message) != 0) {
        printf("FAIL: %s: could not prepare Request message\n", name);
        return 1;
    }

    result = doip_routing_activation_process_request_message(
        message,
        TEST_REQUEST_MESSAGE_SIZE,
        &parameters,
        message,
        sizeof(message),
        &output_size);
    if (check_result(name, DOIP_RESULT_OK, result) != 0) {
        return 1;
    }

    return check_successful_response(
        name, message, output_size, 0x0E00u, 0x1000u, 0x10u);
}

static int expect_failure_unchanged(
    const char *name,
    const uint8_t *input,
    size_t input_size,
    const doip_routing_activation_response_parameters_t *parameters,
    uint8_t *output,
    size_t output_capacity,
    size_t *output_size,
    doip_result_t expected_result)
{
    uint8_t expected_output[TEST_RESPONSE_MESSAGE_SIZE + 4u];
    size_t i;
    doip_result_t result;

    for (i = 0u; i < output_capacity; ++i) {
        output[i] = 0xA5u;
        expected_output[i] = 0xA5u;
    }
    *output_size = 99u;

    result = doip_routing_activation_process_request_message(
        input, input_size, parameters, output, output_capacity, output_size);
    if (check_result(name, expected_result, result) != 0) {
        return 1;
    }
    if (*output_size != 0u) {
        printf("FAIL: %s: output_size was not reset on failure\n", name);
        return 1;
    }

    return check_bytes(name, output, expected_output, output_capacity);
}

static int test_invalid_payload_type(void)
{
    const char *name = "invalid_payload_type";
    uint8_t input[TEST_REQUEST_MESSAGE_SIZE];
    uint8_t output[TEST_RESPONSE_MESSAGE_SIZE];
    size_t output_size;
    const doip_routing_activation_response_parameters_t parameters = {0x1000u, 0x10u};

    if (build_request_message(0x0E00u, 1u, input) != 0) {
        printf("FAIL: %s: could not prepare input message\n", name);
        return 1;
    }

    return expect_failure_unchanged(name, input, sizeof(input), &parameters,
        output, sizeof(output), &output_size, DOIP_RESULT_UNSUPPORTED);
}

static int test_malformed_message_lengths(void)
{
    const char *name = "malformed_message_lengths";
    uint8_t input[TEST_REQUEST_MESSAGE_SIZE + 1u];
    uint8_t output[TEST_RESPONSE_MESSAGE_SIZE];
    size_t output_size;
    const doip_routing_activation_response_parameters_t parameters = {0x1000u, 0x10u};

    if (build_request_message(0x0E00u, 0u, input) != 0) {
        printf("FAIL: %s: could not prepare input message\n", name);
        return 1;
    }

    if (expect_failure_unchanged("truncated_header", input, 7u, &parameters,
            output, sizeof(output), &output_size, DOIP_RESULT_BUFFER_TOO_SMALL) != 0) {
        return 1;
    }
    if (expect_failure_unchanged("truncated_request_body", input,
            DOIP_GENERIC_HEADER_SIZE + DOIP_ROUTING_ACTIVATION_REQUEST_PAYLOAD_SIZE - 1u,
            &parameters, output, sizeof(output), &output_size,
            DOIP_RESULT_BUFFER_TOO_SMALL) != 0) {
        return 1;
    }

    input[7] = (uint8_t)(input[7] + 1u);
    if (expect_failure_unchanged("declared_length_too_large", input,
            TEST_REQUEST_MESSAGE_SIZE, &parameters, output, sizeof(output),
            &output_size, DOIP_RESULT_BUFFER_TOO_SMALL) != 0) {
        return 1;
    }

    if (build_request_message(0x0E00u, 0u, input) != 0) {
        printf("FAIL: %s: could not rebuild input message\n", name);
        return 1;
    }
    input[TEST_REQUEST_MESSAGE_SIZE] = 0xEEu;
    return expect_failure_unchanged("trailing_data", input,
        sizeof(input), &parameters, output, sizeof(output), &output_size,
        DOIP_RESULT_INVALID_LENGTH);
}

static int test_malformed_request_body(void)
{
    const char *name = "malformed_request_body";
    uint8_t input[TEST_REQUEST_MESSAGE_SIZE];
    uint8_t output[TEST_RESPONSE_MESSAGE_SIZE];
    size_t output_size;
    const doip_routing_activation_response_parameters_t parameters = {0x1000u, 0x10u};

    if (build_request_message(0x0E00u, 0u, input) != 0) {
        printf("FAIL: %s: could not prepare input message\n", name);
        return 1;
    }
    input[DOIP_GENERIC_HEADER_SIZE + 3u] = 0x01u;
    return expect_failure_unchanged(name, input, sizeof(input), &parameters,
        output, sizeof(output), &output_size, DOIP_RESULT_INVALID_HEADER);
}

static int test_invalid_arguments_and_empty_input(void)
{
    const char *name = "invalid_arguments_and_empty_input";
    uint8_t input[TEST_REQUEST_MESSAGE_SIZE];
    uint8_t output[TEST_RESPONSE_MESSAGE_SIZE];
    const uint8_t unchanged_output[TEST_RESPONSE_MESSAGE_SIZE] = {
        0xA5u, 0xA5u, 0xA5u, 0xA5u, 0xA5u, 0xA5u,
        0xA5u, 0xA5u, 0xA5u, 0xA5u, 0xA5u, 0xA5u,
        0xA5u, 0xA5u, 0xA5u, 0xA5u, 0xA5u
    };
    size_t output_size;
    const doip_routing_activation_response_parameters_t parameters = {0x1000u, 0x10u};
    doip_result_t result;

    if (build_request_message(0x0E00u, 0u, input) != 0) {
        printf("FAIL: %s: could not prepare input message\n", name);
        return 1;
    }

    if (expect_failure_unchanged("empty_input", input, 0u, &parameters,
            output, sizeof(output), &output_size, DOIP_RESULT_BUFFER_TOO_SMALL) != 0) {
        return 1;
    }

    output_size = 99u;
    result = doip_routing_activation_process_request_message(
        NULL, sizeof(input), &parameters, output, sizeof(output), &output_size);
    if (check_result("null_input", DOIP_RESULT_INVALID_ARGUMENT, result) != 0 ||
        output_size != 0u) {
        printf("FAIL: null_input: output_size not reset or wrong result\n");
        return 1;
    }
    if (check_bytes("null_input", output, unchanged_output, sizeof(output)) != 0) {
        return 1;
    }

    output_size = 99u;
    result = doip_routing_activation_process_request_message(
        input, sizeof(input), NULL, output, sizeof(output), &output_size);
    if (check_result("null_response_parameters", DOIP_RESULT_INVALID_ARGUMENT, result) != 0 ||
        output_size != 0u) {
        printf("FAIL: null_response_parameters: output_size not reset or wrong result\n");
        return 1;
    }
    if (check_bytes("null_response_parameters", output, unchanged_output, sizeof(output)) != 0) {
        return 1;
    }

    output_size = 99u;
    result = doip_routing_activation_process_request_message(
        input, sizeof(input), &parameters, NULL, sizeof(output), &output_size);
    if (check_result("null_output_buffer", DOIP_RESULT_INVALID_ARGUMENT, result) != 0 ||
        output_size != 0u) {
        printf("FAIL: null_output_buffer: output_size not reset or wrong result\n");
        return 1;
    }

    result = doip_routing_activation_process_request_message(
        input, sizeof(input), &parameters, output, sizeof(output), NULL);
    if (check_result("null_output_size", DOIP_RESULT_INVALID_ARGUMENT, result) != 0) {
        return 1;
    }

    return check_bytes("null_output_size", output, unchanged_output, sizeof(output));
}

static int test_output_capacity_too_small(void)
{
    const char *name = "output_capacity_too_small";
    uint8_t input[TEST_REQUEST_MESSAGE_SIZE];
    uint8_t output[TEST_RESPONSE_MESSAGE_SIZE - 1u];
    size_t output_size;
    const doip_routing_activation_response_parameters_t parameters = {0x1000u, 0x10u};

    if (build_request_message(0x0E00u, 0u, input) != 0) {
        printf("FAIL: %s: could not prepare input message\n", name);
        return 1;
    }

    return expect_failure_unchanged(name, input, sizeof(input), &parameters,
        output, sizeof(output), &output_size, DOIP_RESULT_BUFFER_TOO_SMALL);
}

int main(void)
{
    int failures = 0;

    failures += test_success_response_flow();
    failures += test_rejection_response_flow();
    failures += test_build_response_message_from_decoded_request();
    failures += test_raw_api_allows_input_output_overlap();
    failures += test_invalid_payload_type();
    failures += test_malformed_message_lengths();
    failures += test_malformed_request_body();
    failures += test_invalid_arguments_and_empty_input();
    failures += test_output_capacity_too_small();

    if (failures != 0) {
        printf("%d test(s) failed.\n", failures);
        return 1;
    }

    printf("All test_doip_routing_activation tests passed.\n");
    return 0;
}
