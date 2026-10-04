#include "MainApp.hpp"
#include "transport/eth_textual.hpp"
#include "transport/eth_uros.hpp"
#include "transport/uart_lora.hpp"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main_node_app, LOG_LEVEL_INF);

MainApp& MainApp::getInstance() {
    static MainApp instance;
    return instance;
}

extern "C" {
    void heartbeat_rx_callback(const deos_message_t *msg, void *user_data) {
        MainApp& app = MainApp::getInstance();
        
        deos_state_t reported_state = DEOS_STATE_UNKNOWN;
        if (msg->payload_len == sizeof(struct deos_heartbeat_payload)) {
            const struct deos_heartbeat_payload *pl = (const struct deos_heartbeat_payload *)msg->payload;
            reported_state = (deos_state_t)pl->current_state;
        }
        
        app.network_monitor.update_heartbeat(msg->source, reported_state, app.current_state);
    }

    deos_transport_t c_routing_policy(deos_node_id_t destination) {
        if (destination == DEOS_NODE_GROUND_CONTROL) {
            return DEOS_TRANSPORT_ETH_TEXTUAL;
        } else if (destination == DEOS_NODE_MICRO_ROS) {
            return DEOS_TRANSPORT_ETH_UROS;
        } else if (destination >= DEOS_NODE_TRACTION && destination <= DEOS_NODE_DIAG_TOOL) {
            return DEOS_TRANSPORT_CAN_FD;
        }
        return DEOS_TRANSPORT_LOCAL;
    }
}

int MainApp::init() {
    int ret = led_controller.init();
    if (ret != 0) return ret;

    ret = safety_manager.init();
    if (ret != 0) return ret;

    struct deos_config config = {
        .node_id = DEOS_NODE_MAIN_STM32,
        .can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_can_primary)),
        .router_enabled = true,
        .hosted_nodes_enabled = true
    };

    if (!device_is_ready(config.can_dev)) {
        LOG_ERR("CAN device not ready");
        return -ENODEV;
    }

    ret = deos_init(&config);
    if (ret != 0) {
        LOG_ERR("DEOS init failed: %d", ret);
        return ret;
    }

    deos_register_router_policy(c_routing_policy);
    
    /* DEOS çekirdeğine Heartbeat paketlerini nereye düşüreceğini söylüyoruz */
    deos_register_handler(DEOS_SERVICE_SYSTEM, DEOS_CMD_SYSTEM_HEARTBEAT, heartbeat_rx_callback, NULL);

    EthTextual::getInstance().init();
    EthUros::getInstance().init();
    UartLora::getInstance().init();

    return 0;
}

void MainApp::set_state(deos_state_t new_state) {
    if (current_state == new_state) return;

    current_state = new_state;

    /* Durum değiştiğinde ağdaki tüm nodlara bildir (Broadcast) */
    deos_priority_t prio = (new_state == DEOS_STATE_SAFE || new_state == DEOS_STATE_FAULT) 
                            ? DEOS_PRIO_EMERGENCY : DEOS_PRIO_CONTROL;

    deos_send(DEOS_NODE_BROADCAST, prio, DEOS_CLASS_COMMAND, DEOS_SERVICE_SYSTEM, 
              DEOS_CMD_SYSTEM_SET_STATE, &current_state, sizeof(current_state));

    LOG_INF("System State changed to: 0x%02X", current_state);
}

void MainApp::run() {
    deos_start();

    LOG_INF("Main System Controller (Brain) loop started.");

    /* Sistemi hazır (STANDBY) moduna geçir */
    set_state(DEOS_STATE_STANDBY);
    
    uint32_t ms_counter = 0;

    while (1) {
        bool estop_pressed = safety_manager.is_estop_triggered();

        /* --- DONANIMSAL KESME / GÜVENLİK KONTROLÜ --- */
        if (estop_pressed && current_state != DEOS_STATE_SAFE) {
            LOG_ERR("!!! EMERGENCY STOP TRIGGERED !!!");
            set_state(DEOS_STATE_SAFE);
        } else if (!estop_pressed && current_state == DEOS_STATE_SAFE) {
            LOG_INF("E-STOP Released.");
            set_state(DEOS_STATE_STANDBY);
        }

        /* --- NETWORK (NODE) WATCHDOG & HEARTBEAT --- */
        network_monitor.check_watchdog(*this);
        network_monitor.broadcast_heartbeat(current_state, ms_counter);

        /* --- STATE MACHINE (DURUM MAKİNESİ) Görselleri --- */
        led_controller.update(current_state, ms_counter);

        ms_counter += 10;
        k_sleep(K_MSEC(10));
    }
}
