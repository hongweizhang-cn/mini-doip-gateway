#ifndef DOIP_PAYLOAD_TYPE_H
#define DOIP_PAYLOAD_TYPE_H

#include <stdint.h>

/*
 * DoIP Payload Type constants for the Generic Header payload_type field.
 * This header defines the known payload type values only; it does not parse or
 * interpret payload content.
 */

#define DOIP_PAYLOAD_TYPE_VEHICLE_IDENTIFICATION_REQUEST ((uint16_t)0x0001u)
#define DOIP_PAYLOAD_TYPE_VEHICLE_IDENTIFICATION_REQUEST_WITH_EID ((uint16_t)0x0002u)
#define DOIP_PAYLOAD_TYPE_VEHICLE_IDENTIFICATION_REQUEST_WITH_VIN ((uint16_t)0x0003u)
#define DOIP_PAYLOAD_TYPE_VEHICLE_ANNOUNCEMENT ((uint16_t)0x0004u)
#define DOIP_PAYLOAD_TYPE_ROUTING_ACTIVATION_REQUEST ((uint16_t)0x0005u)
#define DOIP_PAYLOAD_TYPE_ROUTING_ACTIVATION_RESPONSE ((uint16_t)0x0006u)

#endif /* DOIP_PAYLOAD_TYPE_H */
