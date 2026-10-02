#include <deos/deos.h>
#include <deos/deos_fault.h>
#include "deos_internal.h"
#include <zephyr/logging/log.h>
#include <zephyr/drivers/can.h>
#include <errno.h>

LOG_MODULE_REGISTER(deos_tx, LOG_LEVEL_INF);

int deos_send_from_node(
    deos_node_id_t source_node,
    deos_node_id_t destination,
    deos_priority_t priority,
    deos_message_class_t message_class,
    deos_service_id_t service,
    uint8_t command,
    const void *payload,
    size_t payload_len)
{
    const struct deos_config *config = deos_get_config();
    if (!config || !config->can_dev) {
        return -ENODEV;
    }

    if (!deos_is_local_node(source_node)) {
        return -EPERM; /* Prevent spoofing */
    }

    if (payload_len > DEOS_MAX_PAYLOAD_LEN) {
        return -EMSGSIZE;
    }

    if (payload_len > 0 && payload == NULL) {
        return -EINVAL;
    }

    deos_message_t msg = {
        .priority      = priority,
        .message_class = message_class,
        .service       = service,
        .destination   = destination,
        .source        = source_node,
        .version       = DEOS_PROTOCOL_VERSION,
        .sequence      = deos_next_sequence_for_node(source_node),
        .command       = command,
        .payload_len   = payload_len
    };

    if (payload_len > 0) {
        memcpy(msg.payload, payload, payload_len);
    }

    struct can_frame frame = {0};
    int ret = deos_encode_frame(&msg, &frame);
    if (ret != 0) {
        LOG_ERR("Failed to encode frame: %d", ret);
        return ret;
    }

    /* Send via CAN API (non-blocking or small timeout could be used, here blocking) */
    ret = can_send(config->can_dev, &frame, K_MSEC(100), NULL, NULL);
    if (ret != 0) {
        LOG_ERR("CAN send failed: %d", ret);
        deos_fault_raise(DEOS_FAULT_TX_FAILURE, DEOS_FAULT_SEVERITY_WARNING);
        return ret;
    }

    LOG_DBG("Sent MSG (Cmd: 0x%02X) Seq: %u", command, msg.sequence);
    return 0;
}

int deos_send(
    deos_node_id_t destination,
    deos_priority_t priority,
    deos_message_class_t message_class,
    deos_service_id_t service,
    uint8_t command,
    const void *payload,
    size_t payload_len)
{
    const struct deos_config *config = deos_get_config();
    if (!config) {
        return -ENODEV;
    }

    return deos_send_from_node(
        config->node_id,
        destination,
        priority,
        message_class,
        service,
        command,
        payload,
        payload_len
    );
}
