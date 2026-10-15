#ifndef DOIP_PAYLOAD_H
#define DOIP_PAYLOAD_H

#include <stddef.h>
#include <stdint.h>

#include "doip_types.h"

doip_result_t doip_payload_check_length(
    uint32_t payload_length,
    size_t available_size);

doip_result_t doip_payload_copy(
    const uint8_t *source_bytes,
    uint32_t payload_length,
    uint8_t *destination_bytes,
    size_t destination_capacity);

#endif /* DOIP_PAYLOAD_H */
