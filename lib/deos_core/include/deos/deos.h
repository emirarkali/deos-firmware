#ifndef DEOS_H
#define DEOS_H

#include <deos/deos_icd.h>
#include <deos/deos_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the DEOS protocol stack.
 *
 * @param config The configuration structure for DEOS core.
 * @return 0 on success, negative error code on failure.
 */
int deos_init(const struct deos_config *config);

/**
 * @brief Start the DEOS protocol operations (e.g. start threads).
 *
 * @return 0 on success, negative error code on failure.
 */
int deos_start(void);

/**
 * @brief Send a DEOS message.
 *
 * @param destination Target node ID.
 * @param priority Message priority.
 * @param message_class Message class.
 * @param service Service ID.
 * @param command Command ID.
 * @param payload Pointer to the command-specific payload.
 * @param payload_len Length of the payload (max 60 bytes).
 * @return 0 on success, negative error code on failure.
 */
int deos_send(
    deos_node_id_t destination,
    deos_priority_t priority,
    deos_message_class_t message_class,
    deos_service_id_t service,
    uint8_t command,
    const void *payload,
    size_t payload_len);

/**
 * @brief Send a PING message.
 *
 * @param destination Target node ID (Broadcast rejected).
 * @param ping_id 32-bit correlation ID.
 * @return 0 on success, negative error code on failure.
 */
int deos_send_ping(
    deos_node_id_t destination,
    uint32_t ping_id);

/**
 * @brief Register an application message handler.
 *
 * @param message_class Message class to listen for.
 * @param service Service ID to listen for.
 * @param command Command ID to listen for.
 * @param handler Callback function to invoke.
 * @param user_data User data passed to the callback.
 * @return 0 on success, negative error code on failure.
 */
int deos_register_handler(
    deos_message_class_t message_class,
    deos_service_id_t service,
    uint8_t command,
    deos_message_handler_t handler,
    void *user_data);

/**
 * @brief Register an additional local logical node.
 * Must be called before deos_start().
 */
int deos_register_local_node(deos_node_id_t node_id);

/**
 * @brief Register a handler for a specific local node.
 */
int deos_register_handler_for_node(
    deos_node_id_t local_node,
    deos_message_class_t message_class,
    deos_service_id_t service,
    uint8_t command,
    deos_message_handler_t handler,
    void *user_data);

/**
 * @brief Send a DEOS message from a specific local node.
 */
int deos_send_from_node(
    deos_node_id_t source_node,
    deos_node_id_t destination,
    deos_priority_t priority,
    deos_message_class_t message_class,
    deos_service_id_t service,
    uint8_t command,
    const void *payload,
    size_t payload_len);

/**
 * @brief Send a HEARTBEAT message.
 *
 * @return 0 on success, negative error code on failure.
 */
int deos_send_heartbeat(void);

/**
 * @brief Send a HEARTBEAT message from a specific local node.
 *
 * @param source_node The local node ID sending the heartbeat.
 * @return 0 on success, negative error code on failure.
 */
int deos_send_heartbeat_from_node(deos_node_id_t source_node);

#ifdef __cplusplus
}
#endif

#endif /* DEOS_H */
