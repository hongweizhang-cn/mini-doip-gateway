#include "doip_gateway.h"

#include <stdio.h>
#include <stdint.h>

#include "doip_header.h"
#include "doip_message.h"
#include "doip_payload_type.h"
#include "doip_routing_activation_request.h"
#include "doip_routing_activation_response.h"

#define TEST_REQUEST_SIZE \
    (DOIP_GENERIC_HEADER_SIZE + DOIP_ROUTING_ACTIVATION_REQUEST_PAYLOAD_SIZE)
#define TEST_RESPONSE_SIZE \
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

static int build_request(
    uint16_t client_address,
    uint16_t payload_type,
    uint8_t message[TEST_REQUEST_SIZE])
{
    const doip_routing_activation_request_t request = {client_address, 0x01u};
    doip_header_t header;
    uint8_t body[DOIP_ROUTING_ACTIVATION_REQUEST_PAYLOAD_SIZE];
    doip_result_t result;

    result = doip_routing_activation_request_encode(&request, body, sizeof(body));
    if (result != DOIP_RESULT_OK) {
        return 1;
    }

    header.protocol_version = 0x02u;
    header.inverse_protocol_version = 0xFDu;
    header.payload_type = payload_type;
    header.payload_length = (uint32_t)sizeof(body);
    return doip_message_encode(&header, body, sizeof(body), message, TEST_REQUEST_SIZE)
        == DOIP_RESULT_OK ? 0 : 1;
}

static int check_gateway_state(
    const char *name,
    const doip_gateway_t *gateway,
    doip_routing_activation_state_t expected_state,
    uint16_t expected_client)
{
    doip_routing_activation_state_t actual_state;
    uint16_t actual_client;
    doip_result_t result;

    result = doip_routing_activation_state_get(
        &gateway->routing_activation, &actual_state, &actual_client);
    if (check_result(name, DOIP_RESULT_OK, result) != 0) {
        return 1;
    }
    if ((actual_state != expected_state) || (actual_client != expected_client)) {
        printf("FAIL: %s: activation state/client mismatch\n", name);
        return 1;
    }
    return 0;
}

static int check_response(
    const char *name,
    const uint8_t *buffer,
    size_t buffer_size,
    uint16_t client_address,
    uint16_t entity_address,
    uint8_t response_code)
{
    doip_message_view_t message;
    doip_routing_activation_response_t response;
    doip_result_t result;

    if (buffer_size != TEST_RESPONSE_SIZE) {
        printf("FAIL: %s: response size mismatch\n", name);
        return 1;
    }
    result = doip_message_decode(buffer, buffer_size, &message);
    if (check_result(name, DOIP_RESULT_OK, result) != 0) {
        return 1;
    }
    if ((message.header.payload_type != DOIP_PAYLOAD_TYPE_ROUTING_ACTIVATION_RESPONSE) ||
        (message.header.payload_length != DOIP_ROUTING_ACTIVATION_RESPONSE_PAYLOAD_SIZE)) {
        printf("FAIL: %s: Generic Header fields mismatch\n", name);
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
        printf("FAIL: %s: Response Body fields mismatch\n", name);
        return 1;
    }
    return 0;
}

static int test_init_and_first_activation(void)
{
    const char *name = "init_and_first_activation";
    doip_gateway_t gateway;
    uint8_t request[TEST_REQUEST_SIZE];
    uint8_t output[TEST_RESPONSE_SIZE];
    size_t output_size = 0u;
    doip_result_t result;

    result = doip_gateway_init(&gateway, 0x1000u, 0x03u);
    if (check_result(name, DOIP_RESULT_OK, result) != 0) {
        return 1;
    }
    if (check_gateway_state(name, &gateway, DOIP_ROUTING_ACTIVATION_STATE_INACTIVE, 0u) != 0) {
        return 1;
    }
    if (build_request(0x0E00u, DOIP_PAYLOAD_TYPE_ROUTING_ACTIVATION_REQUEST, request) != 0) {
        printf("FAIL: %s: request creation failed\n", name);
        return 1;
    }

    result = doip_gateway_process_message(
        &gateway, request, sizeof(request), output, sizeof(output), &output_size);
    if (check_result(name, DOIP_RESULT_OK, result) != 0) {
        return 1;
    }
    if (check_response(name, output, output_size, 0x0E00u, 0x1000u, 0x10u) != 0) {
        return 1;
    }
    return check_gateway_state(name, &gateway, DOIP_ROUTING_ACTIVATION_STATE_ACTIVE, 0x0E00u);
}

static int test_repeat_and_conflicting_client(void)
{
    const char *name = "repeat_and_conflicting_client";
    doip_gateway_t gateway;
    uint8_t request[TEST_REQUEST_SIZE];
    uint8_t output[TEST_RESPONSE_SIZE];
    size_t output_size;
    doip_result_t result;

    if (doip_gateway_init(&gateway, 0x1000u, 0x03u) != DOIP_RESULT_OK ||
        build_request(0x1234u, DOIP_PAYLOAD_TYPE_ROUTING_ACTIVATION_REQUEST, request) != 0) {
        printf("FAIL: %s: setup failed\n", name);
        return 1;
    }

    result = doip_gateway_process_message(
        &gateway, request, sizeof(request), output, sizeof(output), &output_size);
    if (check_result(name, DOIP_RESULT_OK, result) != 0) {
        return 1;
    }
    result = doip_gateway_process_message(
        &gateway, request, sizeof(request), output, sizeof(output), &output_size);
    if (check_result(name, DOIP_RESULT_OK, result) != 0 ||
        check_response(name, output, output_size, 0x1234u, 0x1000u, 0x10u) != 0) {
        return 1;
    }

    if (build_request(0xABCDu, DOIP_PAYLOAD_TYPE_ROUTING_ACTIVATION_REQUEST, request) != 0) {
        printf("FAIL: %s: conflicting request creation failed\n", name);
        return 1;
    }
    result = doip_gateway_process_message(
        &gateway, request, sizeof(request), output, sizeof(output), &output_size);
    if (check_result(name, DOIP_RESULT_OK, result) != 0 ||
        check_response(name, output, output_size, 0xABCDu, 0x1000u, 0x03u) != 0) {
        return 1;
    }

    return check_gateway_state(name, &gateway, DOIP_ROUTING_ACTIVATION_STATE_ACTIVE, 0x1234u);
}

static int expect_failure(
    const char *name,
    doip_gateway_t *gateway,
    const uint8_t *input,
    size_t input_size,
    uint8_t *output,
    size_t output_capacity,
    doip_result_t expected_result,
    doip_routing_activation_state_t expected_state,
    uint16_t expected_client)
{
    uint8_t expected_output[TEST_RESPONSE_SIZE];
    size_t output_size = 99u;
    size_t i;
    doip_result_t result;

    for (i = 0u; i < output_capacity; ++i) {
        output[i] = 0xA5u;
        expected_output[i] = 0xA5u;
    }

    result = doip_gateway_process_message(
        gateway, input, input_size, output, output_capacity, &output_size);
    if (check_result(name, expected_result, result) != 0) {
        return 1;
    }
    if (output_size != 0u) {
        printf("FAIL: %s: output size was not reset to zero\n", name);
        return 1;
    }
    if (check_bytes(name, output, expected_output, output_capacity) != 0) {
        return 1;
    }
    return check_gateway_state(name, gateway, expected_state, expected_client);
}

static int test_invalid_messages_leave_state_and_output_unchanged(void)
{
    const char *name = "invalid_messages_leave_state_and_output_unchanged";
    doip_gateway_t gateway;
    uint8_t request[TEST_REQUEST_SIZE + 1u];
    uint8_t output[TEST_RESPONSE_SIZE];

    if (doip_gateway_init(&gateway, 0x1000u, 0x03u) != DOIP_RESULT_OK ||
        build_request(0x1234u, DOIP_PAYLOAD_TYPE_ROUTING_ACTIVATION_REQUEST, request) != 0) {
        printf("FAIL: %s: setup failed\n", name);
        return 1;
    }

    if (expect_failure("truncated_header", &gateway, request, 7u,
            output, sizeof(output), DOIP_RESULT_BUFFER_TOO_SMALL,
            DOIP_ROUTING_ACTIVATION_STATE_INACTIVE, 0u) != 0) {
        return 1;
    }
    if (expect_failure("truncated_body", &gateway, request, TEST_REQUEST_SIZE - 1u,
            output, sizeof(output), DOIP_RESULT_BUFFER_TOO_SMALL,
            DOIP_ROUTING_ACTIVATION_STATE_INACTIVE, 0u) != 0) {
        return 1;
    }

    request[7] = (uint8_t)(request[7] + 1u);
    if (expect_failure("declared_length_too_large", &gateway, request, TEST_REQUEST_SIZE,
            output, sizeof(output), DOIP_RESULT_BUFFER_TOO_SMALL,
            DOIP_ROUTING_ACTIVATION_STATE_INACTIVE, 0u) != 0) {
        return 1;
    }

    if (build_request(0x1234u, DOIP_PAYLOAD_TYPE_ROUTING_ACTIVATION_REQUEST, request) != 0) {
        printf("FAIL: %s: request rebuild failed\n", name);
        return 1;
    }
    request[TEST_REQUEST_SIZE] = 0xEEu;
    if (expect_failure("trailing_bytes", &gateway, request, sizeof(request),
            output, sizeof(output), DOIP_RESULT_INVALID_LENGTH,
            DOIP_ROUTING_ACTIVATION_STATE_INACTIVE, 0u) != 0) {
        return 1;
    }

    if (build_request(0x1234u, 0x8001u, request) != 0) {
        printf("FAIL: %s: unsupported type request build failed\n", name);
        return 1;
    }
    if (expect_failure("unsupported_payload_type", &gateway, request, TEST_REQUEST_SIZE,
            output, sizeof(output), DOIP_RESULT_UNSUPPORTED,
            DOIP_ROUTING_ACTIVATION_STATE_INACTIVE, 0u) != 0) {
        return 1;
    }

    if (build_request(0x1234u, DOIP_PAYLOAD_TYPE_ROUTING_ACTIVATION_REQUEST, request) != 0) {
        printf("FAIL: %s: request rebuild failed\n", name);
        return 1;
    }
    request[DOIP_GENERIC_HEADER_SIZE + 3u] = 0x01u;
    return expect_failure("invalid_request_reserved", &gateway, request, TEST_REQUEST_SIZE,
        output, sizeof(output), DOIP_RESULT_INVALID_HEADER,
        DOIP_ROUTING_ACTIVATION_STATE_INACTIVE, 0u);
}

static int test_capacity_check_precedes_state_change(void)
{
    const char *name = "capacity_check_precedes_state_change";
    doip_gateway_t gateway;
    uint8_t request[TEST_REQUEST_SIZE];
    uint8_t output[TEST_RESPONSE_SIZE - 1u];

    if (doip_gateway_init(&gateway, 0x1000u, 0x03u) != DOIP_RESULT_OK ||
        build_request(0x1234u, DOIP_PAYLOAD_TYPE_ROUTING_ACTIVATION_REQUEST, request) != 0) {
        printf("FAIL: %s: setup failed\n", name);
        return 1;
    }

    return expect_failure(name, &gateway, request, sizeof(request), output, sizeof(output),
        DOIP_RESULT_BUFFER_TOO_SMALL,
        DOIP_ROUTING_ACTIVATION_STATE_INACTIVE, 0u);
}

static int test_null_arguments_and_empty_input(void)
{
    const char *name = "null_arguments_and_empty_input";
    doip_gateway_t gateway;
    uint8_t request[TEST_REQUEST_SIZE];
    uint8_t output[TEST_RESPONSE_SIZE];
    const uint8_t unchanged[TEST_RESPONSE_SIZE] = {
        0xA5u, 0xA5u, 0xA5u, 0xA5u, 0xA5u, 0xA5u,
        0xA5u, 0xA5u, 0xA5u, 0xA5u, 0xA5u, 0xA5u,
        0xA5u, 0xA5u, 0xA5u, 0xA5u, 0xA5u
    };
    size_t output_size = 99u;
    doip_result_t result;

    if (doip_gateway_init(&gateway, 0x1000u, 0x03u) != DOIP_RESULT_OK ||
        build_request(0x1234u, DOIP_PAYLOAD_TYPE_ROUTING_ACTIVATION_REQUEST, request) != 0) {
        printf("FAIL: %s: setup failed\n", name);
        return 1;
    }

    if (expect_failure("empty_input", &gateway, request, 0u, output, sizeof(output),
            DOIP_RESULT_BUFFER_TOO_SMALL, DOIP_ROUTING_ACTIVATION_STATE_INACTIVE, 0u) != 0) {
        return 1;
    }

    result = doip_gateway_process_message(
        NULL, request, sizeof(request), output, sizeof(output), &output_size);
    if (check_result("null_gateway", DOIP_RESULT_INVALID_ARGUMENT, result) != 0 ||
        output_size != 0u ||
        check_bytes("null_gateway", output, unchanged, sizeof(output)) != 0) {
        printf("FAIL: null_gateway: failure outputs incorrect\n");
        return 1;
    }

    output_size = 99u;
    result = doip_gateway_process_message(
        &gateway, NULL, sizeof(request), output, sizeof(output), &output_size);
    if (check_result("null_input", DOIP_RESULT_INVALID_ARGUMENT, result) != 0 ||
        output_size != 0u ||
        check_bytes("null_input", output, unchanged, sizeof(output)) != 0) {
        printf("FAIL: null_input: failure outputs incorrect\n");
        return 1;
    }

    output_size = 99u;
    result = doip_gateway_process_message(
        &gateway, request, sizeof(request), NULL, sizeof(output), &output_size);
    if (check_result("null_output", DOIP_RESULT_INVALID_ARGUMENT, result) != 0 ||
        output_size != 0u) {
        printf("FAIL: null_output: result or output size incorrect\n");
        return 1;
    }

    result = doip_gateway_process_message(
        &gateway, request, sizeof(request), output, sizeof(output), NULL);
    if (check_result("null_output_size", DOIP_RESULT_INVALID_ARGUMENT, result) != 0 ||
        check_bytes("null_output_size", output, unchanged, sizeof(output)) != 0) {
        return 1;
    }
    return check_gateway_state(name, &gateway, DOIP_ROUTING_ACTIVATION_STATE_INACTIVE, 0u);
}

static int test_reset_allows_new_activation(void)
{
    const char *name = "reset_allows_new_activation";
    doip_gateway_t gateway;
    uint8_t request[TEST_REQUEST_SIZE];
    uint8_t output[TEST_RESPONSE_SIZE];
    size_t output_size;
    doip_result_t result;

    if (doip_gateway_init(&gateway, 0x1000u, 0x03u) != DOIP_RESULT_OK ||
        build_request(0x1111u, DOIP_PAYLOAD_TYPE_ROUTING_ACTIVATION_REQUEST, request) != 0) {
        printf("FAIL: %s: setup failed\n", name);
        return 1;
    }
    result = doip_gateway_process_message(
        &gateway, request, sizeof(request), output, sizeof(output), &output_size);
    if (check_result(name, DOIP_RESULT_OK, result) != 0) {
        return 1;
    }
    if (doip_gateway_reset_routing_activation(&gateway) != DOIP_RESULT_OK ||
        doip_gateway_reset_routing_activation(&gateway) != DOIP_RESULT_OK) {
        printf("FAIL: %s: reset failed\n", name);
        return 1;
    }
    if (check_gateway_state(name, &gateway, DOIP_ROUTING_ACTIVATION_STATE_INACTIVE, 0u) != 0) {
        return 1;
    }

    if (build_request(0x2222u, DOIP_PAYLOAD_TYPE_ROUTING_ACTIVATION_REQUEST, request) != 0) {
        printf("FAIL: %s: second request build failed\n", name);
        return 1;
    }
    result = doip_gateway_process_message(
        &gateway, request, sizeof(request), output, sizeof(output), &output_size);
    if (check_result(name, DOIP_RESULT_OK, result) != 0 ||
        check_response(name, output, output_size, 0x2222u, 0x1000u, 0x10u) != 0) {
        return 1;
    }

    return check_gateway_state(name, &gateway, DOIP_ROUTING_ACTIVATION_STATE_ACTIVE, 0x2222u);
}

static int test_input_output_buffer_alias(void)
{
    const char *name = "input_output_buffer_alias";
    doip_gateway_t gateway;
    uint8_t message_buffer[TEST_RESPONSE_SIZE];
    size_t output_size = 0u;
    doip_result_t result;

    if (doip_gateway_init(&gateway, 0x1000u, 0x03u) != DOIP_RESULT_OK ||
        build_request(
            0x3456u,
            DOIP_PAYLOAD_TYPE_ROUTING_ACTIVATION_REQUEST,
            message_buffer) != 0) {
        printf("FAIL: %s: setup failed\n", name);
        return 1;
    }

    result = doip_gateway_process_message(
        &gateway,
        message_buffer,
        TEST_REQUEST_SIZE,
        message_buffer,
        sizeof(message_buffer),
        &output_size);
    if (check_result(name, DOIP_RESULT_OK, result) != 0) {
        return 1;
    }
    if (check_response(name, message_buffer, output_size, 0x3456u, 0x1000u, 0x10u) != 0) {
        return 1;
    }

    return check_gateway_state(
        name, &gateway, DOIP_ROUTING_ACTIVATION_STATE_ACTIVE, 0x3456u);
}

int main(void)
{
    int failures = 0;

    failures += test_init_and_first_activation();
    failures += test_repeat_and_conflicting_client();
    failures += test_invalid_messages_leave_state_and_output_unchanged();
    failures += test_capacity_check_precedes_state_change();
    failures += test_null_arguments_and_empty_input();
    failures += test_reset_allows_new_activation();
    failures += test_input_output_buffer_alias();

    if (failures != 0) {
        printf("%d test(s) failed.\n", failures);
        return 1;
    }

    printf("All test_doip_gateway tests passed.\n");
    return 0;
}
