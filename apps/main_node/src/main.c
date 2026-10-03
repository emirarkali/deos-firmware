#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <deos/deos.h>
#include <deos/deos_fault.h>

#include "eth_textual.h"
#include "eth_uros.h"
#include "uart_lora.h"

LOG_MODULE_REGISTER(main_node, LOG_LEVEL_INF);

/* Application defined routing policy */
static deos_transport_t main_routing_policy(deos_node_id_t destination)
{
    if (destination == DEOS_NODE_TEXTUAL) {
        return DEOS_TRANSPORT_ETH_TEXTUAL;
    }
    if (destination == DEOS_NODE_MICRO_ROS) {
        return DEOS_TRANSPORT_ETH_UROS;
    }
    if (destination == DEOS_NODE_GROUND_CONTROL) {
        return DEOS_TRANSPORT_UART_LORA;
    }
    
    /* Route everything else to the CAN-FD network by default */
    return DEOS_TRANSPORT_CAN_FD;
}

int main(void)
{
    LOG_INF("Starting DEOS Main STM32 Gateway...");

    /* Initialize DEOS Configuration */
    struct deos_config config = {
        .node_id = DEOS_NODE_MAIN_STM32,
        .can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_can_primary)),
        .router_enabled = true,
        .hosted_nodes_enabled = true
    };

    if (!device_is_ready(config.can_dev)) {
        LOG_ERR("CAN device not ready");
        return -1;
    }

    /* Set our custom routing lookup function */
    deos_router_set_lookup_fn(main_routing_policy);

    /* Initialize DEOS Core */
    int ret = deos_init(&config);
    if (ret != 0) {
        LOG_ERR("DEOS init failed: %d", ret);
        return ret;
    }

    /* Initialize External Ethernet Interfaces */
    eth_textual_init();
    eth_uros_init(); /* TODO: Implement micro-ROS */
    uart_lora_init();

    /* Start DEOS threads */
    deos_start();

    while (1) {
        k_sleep(K_MSEC(1000));
        /* Main application background tasks */
    }

    return 0;
}
