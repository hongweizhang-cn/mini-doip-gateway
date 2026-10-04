#ifndef DOIP_TYPES_H
#define DOIP_TYPES_H

/*
 * Shared C Core types.
 * Keep this header free of Header/payload/transport/UDS definitions so later
 * modules can reuse doip_result_t without depending on doip_header.h.
 */

typedef enum {
    DOIP_RESULT_OK = 0,
    DOIP_RESULT_INVALID_ARGUMENT,
    DOIP_RESULT_BUFFER_TOO_SMALL,
    DOIP_RESULT_INVALID_HEADER,
    DOIP_RESULT_INVALID_LENGTH,
    DOIP_RESULT_UNSUPPORTED
} doip_result_t;

#endif /* DOIP_TYPES_H */
