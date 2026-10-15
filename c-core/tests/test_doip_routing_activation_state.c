#include "doip_routing_activation_state.h"

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

static int check_context(
    const char *test_name,
    const doip_routing_activation_context_t *context,
    doip_routing_activation_state_t expected_state,
    uint16_t expected_address)
{
    doip_routing_activation_state_t state;
    uint16_t client_address;
    doip_result_t result;

    result = doip_routing_activation_state_get(context, &state, &client_address);
    if (check_result(test_name, DOIP_RESULT_OK, result) != 0) {
        return 1;
    }

    if (state != expected_state) {
        printf("FAIL: %s: activation state mismatch\n", test_name);
        return 1;
    }
    if (client_address != expected_address) {
        printf("FAIL: %s: client logical address mismatch (expected 0x%04X, got 0x%04X)\n",
            test_name, (unsigned int)expected_address, (unsigned int)client_address);
        return 1;
    }

    return 0;
}

static int test_initialization_and_queries(void)
{
    const char *name = "initialization_and_queries";
    doip_routing_activation_context_t context;
    doip_routing_activation_state_t state;
    uint16_t address;
    doip_result_t result;

    result = doip_routing_activation_state_reset(&context);
    if (check_result(name, DOIP_RESULT_OK, result) != 0) {
        return 1;
    }
    if (check_context(name, &context, DOIP_ROUTING_ACTIVATION_STATE_INACTIVE, 0u) != 0) {
        return 1;
    }

    state = DOIP_ROUTING_ACTIVATION_STATE_ACTIVE;
    address = 0xFFFFu;
    result = doip_routing_activation_state_get(&context, &state, &address);
    if (check_result(name, DOIP_RESULT_OK, result) != 0) {
        return 1;
    }
    if ((state != DOIP_ROUTING_ACTIVATION_STATE_INACTIVE) || (address != 0u)) {
        printf("FAIL: %s: inactive query did not return inactive/zero\n", name);
        return 1;
    }

    return 0;
}

static int test_first_activation(void)
{
    const char *name = "first_activation";
    doip_routing_activation_context_t context;
    const doip_routing_activation_request_t request = {0x0E00u, 0x01u};
    uint8_t response_code = 0xFFu;
    doip_result_t result;

    if (doip_routing_activation_state_reset(&context) != DOIP_RESULT_OK) {
        printf("FAIL: %s: context reset failed\n", name);
        return 1;
    }

    result = doip_routing_activation_state_process_request(
        &context, &request, 0x00u, &response_code);
    if (check_result(name, DOIP_RESULT_OK, result) != 0) {
        return 1;
    }
    if (response_code != DOIP_ROUTING_ACTIVATION_RESPONSE_CODE_SUCCESS) {
        printf("FAIL: %s: wrong success Response Code 0x%02X\n",
            name, (unsigned int)response_code);
        return 1;
    }

    return check_context(name, &context, DOIP_ROUTING_ACTIVATION_STATE_ACTIVE, 0x0E00u);
}

static int test_same_client_reactivation_is_idempotent(void)
{
    const char *name = "same_client_reactivation_is_idempotent";
    doip_routing_activation_context_t context;
    const doip_routing_activation_request_t first = {0x1234u, 0x01u};
    const doip_routing_activation_request_t repeated = {0x1234u, 0x06u};
    uint8_t response_code = 0u;
    doip_result_t result;

    if (doip_routing_activation_state_reset(&context) != DOIP_RESULT_OK ||
        doip_routing_activation_state_process_request(
            &context, &first, 0x00u, &response_code) != DOIP_RESULT_OK) {
        printf("FAIL: %s: setup failed\n", name);
        return 1;
    }

    response_code = 0u;
    result = doip_routing_activation_state_process_request(
        &context, &repeated, 0x00u, &response_code);
    if (check_result(name, DOIP_RESULT_OK, result) != 0) {
        return 1;
    }
    if (response_code != DOIP_ROUTING_ACTIVATION_RESPONSE_CODE_SUCCESS) {
        printf("FAIL: %s: repeated client did not receive success Response Code\n", name);
        return 1;
    }

    return check_context(name, &context, DOIP_ROUTING_ACTIVATION_STATE_ACTIVE, 0x1234u);
}

static int test_different_client_rejected_without_overwrite(void)
{
    const char *name = "different_client_rejected_without_overwrite";
    doip_routing_activation_context_t context;
    const doip_routing_activation_request_t active_client = {0x1234u, 0x01u};
    const doip_routing_activation_request_t other_client = {0xABCDu, 0x01u};
    const uint8_t conflict_response_code = 0x03u;
    uint8_t response_code = 0u;
    doip_result_t result;

    if (doip_routing_activation_state_reset(&context) != DOIP_RESULT_OK ||
        doip_routing_activation_state_process_request(
            &context, &active_client, conflict_response_code, &response_code) != DOIP_RESULT_OK) {
        printf("FAIL: %s: setup failed\n", name);
        return 1;
    }

    response_code = 0u;
    result = doip_routing_activation_state_process_request(
        &context, &other_client, conflict_response_code, &response_code);
    if (check_result(name, DOIP_RESULT_OK, result) != 0) {
        return 1;
    }
    if (response_code != conflict_response_code) {
        printf("FAIL: %s: caller policy Response Code not preserved\n", name);
        return 1;
    }

    return check_context(name, &context, DOIP_ROUTING_ACTIVATION_STATE_ACTIVE, 0x1234u);
}

static int test_parameter_errors_preserve_context(void)
{
    const char *name = "parameter_errors_preserve_context";
    doip_routing_activation_context_t context;
    const doip_routing_activation_request_t request = {0x1234u, 0x01u};
    uint8_t response_code = 0xFFu;
    doip_result_t result;

    if (doip_routing_activation_state_reset(&context) != DOIP_RESULT_OK ||
        doip_routing_activation_state_process_request(
            &context, &request, 0x00u, &response_code) != DOIP_RESULT_OK) {
        printf("FAIL: %s: setup failed\n", name);
        return 1;
    }

    response_code = 0xFFu;
    result = doip_routing_activation_state_process_request(
        &context, NULL, 0x00u, &response_code);
    if (check_result("null_request", DOIP_RESULT_INVALID_ARGUMENT, result) != 0 ||
        response_code != 0u) {
        printf("FAIL: null_request: result or initialized Response Code incorrect\n");
        return 1;
    }

    if (check_context(name, &context, DOIP_ROUTING_ACTIVATION_STATE_ACTIVE, 0x1234u) != 0) {
        return 1;
    }

    result = doip_routing_activation_state_process_request(
        NULL, &request, 0x00u, &response_code);
    if (check_result("null_context", DOIP_RESULT_INVALID_ARGUMENT, result) != 0 ||
        response_code != 0u) {
        printf("FAIL: null_context: result or initialized Response Code incorrect\n");
        return 1;
    }

    result = doip_routing_activation_state_process_request(
        &context, &request, 0x00u, NULL);
    if (check_result("null_response_code", DOIP_RESULT_INVALID_ARGUMENT, result) != 0) {
        return 1;
    }

    result = doip_routing_activation_state_process_request(
        &context, &request, 0u, &response_code);
    if (check_result(name, DOIP_RESULT_OK, result) != 0) {
        return 1;
    }

    result = doip_routing_activation_state_get(&context, NULL, &(uint16_t){0u});
    if (check_result("null_state_output", DOIP_RESULT_INVALID_ARGUMENT, result) != 0) {
        return 1;
    }

    result = doip_routing_activation_state_reset(NULL);
    if (check_result("reset_null_context", DOIP_RESULT_INVALID_ARGUMENT, result) != 0) {
        return 1;
    }

    return check_context(name, &context, DOIP_ROUTING_ACTIVATION_STATE_ACTIVE, 0x1234u);
}

static int test_reset_and_reactivate(void)
{
    const char *name = "reset_and_reactivate";
    doip_routing_activation_context_t context;
    const doip_routing_activation_request_t request = {0x2222u, 0x01u};
    uint8_t response_code = 0u;
    doip_result_t result;

    if (doip_routing_activation_state_reset(&context) != DOIP_RESULT_OK ||
        doip_routing_activation_state_process_request(
            &context, &request, 0x00u, &response_code) != DOIP_RESULT_OK) {
        printf("FAIL: %s: setup failed\n", name);
        return 1;
    }

    result = doip_routing_activation_state_reset(&context);
    if (check_result(name, DOIP_RESULT_OK, result) != 0) {
        return 1;
    }
    result = doip_routing_activation_state_reset(&context);
    if (check_result(name, DOIP_RESULT_OK, result) != 0) {
        return 1;
    }
    if (check_context(name, &context, DOIP_ROUTING_ACTIVATION_STATE_INACTIVE, 0u) != 0) {
        return 1;
    }

    response_code = 0u;
    result = doip_routing_activation_state_process_request(
        &context, &request, 0x00u, &response_code);
    if (check_result(name, DOIP_RESULT_OK, result) != 0) {
        return 1;
    }
    if (response_code != DOIP_ROUTING_ACTIVATION_RESPONSE_CODE_SUCCESS) {
        printf("FAIL: %s: reset context did not accept first activation\n", name);
        return 1;
    }

    return check_context(name, &context, DOIP_ROUTING_ACTIVATION_STATE_ACTIVE, 0x2222u);
}

static int test_invalid_context_state_is_rejected(void)
{
    const char *name = "invalid_context_state_is_rejected";
    doip_routing_activation_context_t context = {
        (doip_routing_activation_state_t)99,
        0x1234u
    };
    const doip_routing_activation_request_t request = {0x5678u, 0x01u};
    uint8_t response_code = 0xFFu;
    doip_result_t result;

    result = doip_routing_activation_state_process_request(
        &context, &request, 0x00u, &response_code);
    if (check_result(name, DOIP_RESULT_INVALID_ARGUMENT, result) != 0) {
        return 1;
    }
    if ((response_code != 0u) ||
        (context.state != (doip_routing_activation_state_t)99) ||
        (context.client_logical_address != 0x1234u)) {
        printf("FAIL: %s: invalid state or context was modified\n", name);
        return 1;
    }

    return 0;
}

int main(void)
{
    int failures = 0;

    failures += test_initialization_and_queries();
    failures += test_first_activation();
    failures += test_same_client_reactivation_is_idempotent();
    failures += test_different_client_rejected_without_overwrite();
    failures += test_parameter_errors_preserve_context();
    failures += test_reset_and_reactivate();
    failures += test_invalid_context_state_is_rejected();

    if (failures != 0) {
        printf("%d test(s) failed.\n", failures);
        return 1;
    }

    printf("All test_doip_routing_activation_state tests passed.\n");
    return 0;
}
