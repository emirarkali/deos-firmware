#include <deos/deos.h>
#include "deos_internal.h"
#include <zephyr/logging/log.h>
#include <errno.h>

LOG_MODULE_REGISTER(deos_router, LOG_LEVEL_INF);

int deos_router_init(void)
{
    LOG_INF("Router functionality initialized");
    return 0;
}

void deos_route_message(const deos_message_t *msg, deos_transport_t incoming_transport)
{
    /* Router loop prevention */
    if (incoming_transport == DEOS_TRANSPORT_CAN_FD) {
        /*
         * If it came from CAN-FD, and the node is on the same shared CAN-FD segment,
         * we shouldn't route it back to CAN-FD. Since currently CAN-FD is our
         * only transport, we just log and return.
         * In the future, this would lookup the destination in a routing table
         * and send it out the appropriate transport.
         */
         LOG_DBG("Router: Skipping loopback for destination 0x%02X", msg->destination);
         return;
    }

    /* Provisional implementation */
    LOG_DBG("Routing message from 0x%02X to 0x%02X via %d",
             msg->source, msg->destination, incoming_transport);
}
