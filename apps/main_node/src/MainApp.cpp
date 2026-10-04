#include "MainApp.hpp"
#include "eth_textual.hpp"
#include "uart_lora.hpp"
#include "eth_uros.hpp"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main_node_app, LOG_LEVEL_INF);

MainApp& MainApp::getInstance() {
    static MainApp instance;
    return instance;
}

deos_transport_t MainApp::routing_policy(deos_node_id_t destination) {
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

int MainApp::init() {
    LOG_INF("Starting DEOS Main STM32 Gateway (C++ OOP Version)...");

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

    deos_router_set_lookup_fn(routing_policy);

    int ret = deos_init(&config);
    if (ret != 0) {
        LOG_ERR("DEOS init failed: %d", ret);
        return ret;
    }

    EthTextual::getInstance().init();
    EthUros::getInstance().init();
    UartLora::getInstance().init();

    return 0;
}

void MainApp::run() {
    deos_start();

    while (1) {
        k_sleep(K_MSEC(1000));
        /* Main application background tasks */
    }
}
