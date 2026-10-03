#ifndef DEOS_TYPES_H
#define DEOS_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <deos/deos_icd.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Maximum theoretical command payload for DEOS
 * (64 bytes CAN-FD payload - 4 bytes common header)
 */
#define DEOS_MAX_PAYLOAD_LEN 60

/*
 * Core DEOS Message Structure
 * Reusable container for DEOS protocol messages independent of CAN transport.
 */
typedef struct {
    deos_priority_t priority;
    deos_message_class_t message_class;
    deos_service_id_t service;

    deos_node_id_t destination;
    deos_node_id_t source;

    uint8_t version;
    uint8_t sequence;
    uint8_t command;

    uint8_t payload[DEOS_MAX_PAYLOAD_LEN];
    uint8_t payload_len;
} deos_message_t;

struct device;

/*
 * Configuration Struct
 */
struct deos_config {
    deos_node_id_t node_id;
    const struct device *can_dev;
    bool router_enabled;
    bool hosted_nodes_enabled;
};

/*
 * Message Handler Type
 */
typedef int (*deos_message_handler_t)(const deos_message_t *message, void *user_data);

/*
 * Transport Type
 */
typedef enum {
    DEOS_TRANSPORT_LOCAL = 0,
    DEOS_TRANSPORT_CAN_FD,
    DEOS_TRANSPORT_ETH_TEXTUAL,
    DEOS_TRANSPORT_ETH_UROS,
    DEOS_TRANSPORT_COUNT
} deos_transport_t;

/*
 * TRACTION Command Payloads
 */
struct deos_traction_target_payload {
    uint16_t target_speed; /* 0.01 m/s */
    int16_t  target_accel; /* 0.01 m/s^2 */
} __attribute__((packed));

struct deos_traction_gear_payload {
    uint8_t gear; /* deos_traction_gear_t */
} __attribute__((packed));

struct deos_traction_torque_limit_payload {
    uint16_t torque_limit; /* 0.01 % (0..10000) */
} __attribute__((packed));

#ifdef __cplusplus
}
#endif

#endif /* DEOS_TYPES_H */
