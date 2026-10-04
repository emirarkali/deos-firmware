#include <deos/deos.h>
#include "deos_internal.h"
#include <zephyr/logging/log.h>
#include <errno.h>

LOG_MODULE_REGISTER(deos_network, LOG_LEVEL_INF);

int deos_send_ping(
    deos_node_id_t destination,
    uint32_t ping_id)
{
    if (destination == DEOS_NODE_BROADCAST) {
        LOG_ERR("Broadcast PING rejected");
        return -EINVAL; /* Do not allow broadcast ping to avoid response storm */
    }

    uint8_t payload[4];
    deos_put_u32_le(payload, ping_id);

    return deos_send(
        destination,
        DEOS_PRIO_NETWORK,
        DEOS_CLASS_NETWORK,
        DEOS_SERVICE_SYSTEM,
        DEOS_CMD_SYSTEM_PING,
        payload,
        sizeof(payload));
}

int deos_send_heartbeat_from_node(deos_node_id_t source_node, deos_state_t current_state)
{
    if (!deos_is_local_node(source_node)) {
        LOG_ERR("Cannot send heartbeat from unregistered node 0x%02X", source_node);
        return -EPERM;
    }

    struct deos_heartbeat_payload payload;
    payload.current_state = current_state;

    return deos_send_from_node(
        source_node,
        DEOS_NODE_BROADCAST,
        DEOS_PRIO_NETWORK,
        DEOS_CLASS_NETWORK,
        DEOS_SERVICE_SYSTEM,
        DEOS_CMD_SYSTEM_HEARTBEAT,
        &payload,
        sizeof(payload));
}

int deos_send_heartbeat(deos_state_t current_state)
{
    const struct deos_config *config = deos_get_config();
    if (!config) return -ENODEV;

    return deos_send_heartbeat_from_node(config->node_id, current_state);
}

void deos_handle_ping(const deos_message_t *msg)
{
    if (msg->destination == DEOS_NODE_BROADCAST) {
        LOG_WRN("Broadcast PING from 0x%02X ignored", msg->source);
        return;
    }

    if (msg->payload_len != 4) {
        LOG_WRN("Invalid PING payload length: %d", msg->payload_len);
        return;
    }

    uint32_t ping_id = deos_get_u32_le(msg->payload);
    LOG_DBG("Received PING from 0x%02X with ID 0x%08X", msg->source, ping_id);

    /* Automatically generate PONG */
    /* PONG sequence will use normal node sequence via deos_send_from_node */
    int ret = deos_send_from_node(
        msg->destination, /* Reply from the targeted local node */
        msg->source,
        DEOS_PRIO_NETWORK,
        DEOS_CLASS_NETWORK,
        DEOS_SERVICE_SYSTEM,
        DEOS_CMD_SYSTEM_PONG,
        msg->payload, /* Reflect Ping ID */
        msg->payload_len);

    if (ret != 0) {
        LOG_ERR("Failed to send PONG to 0x%02X: %d", msg->source, ret);
    }
}

void deos_handle_pong(const deos_message_t *msg)
{
    if (msg->payload_len != 4) {
        LOG_WRN("Invalid PONG payload length: %d", msg->payload_len);
        return;
    }

    uint32_t ping_id = deos_get_u32_le(msg->payload);
    LOG_DBG("Received PONG from 0x%02X for ID 0x%08X", msg->source, ping_id);
    
    /* Transaction correlation is provisional and can be added here in the future. */
}
