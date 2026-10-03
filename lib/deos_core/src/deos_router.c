#include <deos/deos.h>
#include "deos_internal.h"
#include <zephyr/logging/log.h>
#include <errno.h>

LOG_MODULE_REGISTER(deos_router, LOG_LEVEL_INF);

/* Registry for transport TX callbacks */
static deos_transport_tx_fn_t transport_tx_registry[DEOS_TRANSPORT_COUNT];

int deos_router_init(void)
{
    for (int i = 0; i < DEOS_TRANSPORT_COUNT; i++) {
        transport_tx_registry[i] = NULL;
    }
    LOG_INF("Router functionality initialized");
    return 0;
}

int deos_router_register_transport(deos_transport_t transport, deos_transport_tx_fn_t tx_fn)
{
    if (transport >= DEOS_TRANSPORT_COUNT) {
        return -EINVAL;
    }
    transport_tx_registry[transport] = tx_fn;
    LOG_DBG("Registered TX callback for transport %d", transport);
    return 0;
}

void deos_route_message(const deos_message_t *msg, deos_transport_t incoming_transport)
{
    if (msg->destination == DEOS_NODE_BROADCAST) {
        /* Route to all registered transports EXCEPT the one it came from (Loop Prevention) */
        for (int i = 1; i < DEOS_TRANSPORT_COUNT; i++) { /* Start at 1 to skip INTERNAL */
            deos_transport_t target_transport = (deos_transport_t)i;
            if (target_transport != incoming_transport && transport_tx_registry[target_transport] != NULL) {
                transport_tx_registry[target_transport](msg);
            }
        }
        return;
    }

    /* Unicast Routing Table */
    deos_transport_t target_transport = DEOS_TRANSPORT_UNKNOWN;

    switch (msg->destination) {
        case DEOS_NODE_TEXTUAL:
            target_transport = DEOS_TRANSPORT_ETH_TEXTUAL;
            break;
        case DEOS_NODE_MICRO_ROS:
            target_transport = DEOS_TRANSPORT_ETH_UROS;
            break;
        default:
            /* For now, assume all other ECUs are on CAN-FD */
            target_transport = DEOS_TRANSPORT_CAN_FD;
            break;
    }

    /* Prevent Loopback and check if transport is registered */
    if (target_transport != DEOS_TRANSPORT_UNKNOWN && 
        target_transport != incoming_transport &&
        transport_tx_registry[target_transport] != NULL) {
        
        LOG_DBG("Routing message from 0x%02X to 0x%02X via %d",
                 msg->source, msg->destination, target_transport);
        transport_tx_registry[target_transport](msg);
    } else {
        if (target_transport == incoming_transport) {
            LOG_DBG("Router: Skipping loopback for destination 0x%02X", msg->destination);
        } else {
            LOG_WRN("Router: Dropping message to 0x%02X (No TX callback for transport %d)", 
                    msg->destination, target_transport);
        }
    }
}

