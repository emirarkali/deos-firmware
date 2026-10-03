#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <deos/deos.h>
#include "eth_uros.h"

LOG_MODULE_REGISTER(eth_uros, LOG_LEVEL_INF);

/* 
 * TODO: micro-ROS integration is pending.
 * Similar to eth_textual.c, this will manage the connection to the 
 * micro-ROS agent or bridge ROS messages to DEOS frames.
 */

int eth_uros_init(void)
{
    /* TODO: Register transport and start threads when implemented */
    // deos_router_register_transport(DEOS_TRANSPORT_ETH_UROS, uros_tx_callback);
    
    LOG_INF("micro-ROS Ethernet interface initialized (TODO)");
    return 0;
}
