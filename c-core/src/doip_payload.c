#include "doip_payload.h"

doip_result_t doip_payload_check_length(
    uint32_t payload_length,
    size_t available_size)
{
    if ((size_t)payload_length <= available_size) {
        return DOIP_RESULT_OK;
    }

    return DOIP_RESULT_BUFFER_TOO_SMALL;
}

doip_result_t doip_payload_copy(
    const uint8_t *src,
    uint32_t payload_length,
    uint8_t *dst,
    size_t dst_size)
{
    uint32_t i;

    if (payload_length == 0u) {
        return DOIP_RESULT_OK;
    }

    if (src == NULL) {
        return DOIP_RESULT_INVALID_ARGUMENT;
    }

    if (dst == NULL) {
        return DOIP_RESULT_INVALID_ARGUMENT;
    }

    if ((size_t)payload_length > dst_size) {
        return DOIP_RESULT_BUFFER_TOO_SMALL;
    }

    for (i = 0u; i < payload_length; ++i) {
        dst[i] = src[i];
    }

    return DOIP_RESULT_OK;
}
