#include "eth_uros.hpp"
#include <zephyr/logging/log.h>
#include <deos/deos.h>

LOG_MODULE_REGISTER(eth_uros, LOG_LEVEL_INF);

EthUros& EthUros::getInstance() {
    static EthUros instance;
    return instance;
}

int EthUros::tx_callback(const deos_message_t *msg) {
    return getInstance().send(msg);
}

int EthUros::send(const deos_message_t *msg) {
    // TODO: Implement micro-ROS message transmission logic here
    return 0;
}

int EthUros::init() {
    /* TODO: Register transport and start threads when implemented */
    // deos_router_register_transport(DEOS_TRANSPORT_ETH_UROS, tx_callback);
    
    LOG_INF("micro-ROS Ethernet interface initialized (OOP TODO)");
    return 0;
}
