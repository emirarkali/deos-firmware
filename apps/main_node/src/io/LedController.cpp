#include "LedController.hpp"
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(main_node_app);

#define LED0_NODE DT_ALIAS(led0)

LedController::LedController() : led(GPIO_DT_SPEC_GET(LED0_NODE, gpios)) {}

int LedController::init() {
    if (!gpio_is_ready_dt(&led)) {
        LOG_ERR("LED device %s is not ready", led.port->name);
        return -ENODEV;
    }
    int ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
    if (ret != 0) {
        LOG_ERR("Error %d: failed to configure LED pin", ret);
        return ret;
    }
    return 0;
}

void LedController::update(deos_state_t current_state, uint32_t ms_counter) {
    switch (current_state) {
    case DEOS_STATE_INIT:
        gpio_pin_set_dt(&led, 0);
        break;
    case DEOS_STATE_STANDBY:
        if (ms_counter % 1100 < 100) gpio_pin_set_dt(&led, 1);
        else gpio_pin_set_dt(&led, 0);
        break;
    case DEOS_STATE_ACTIVE:
        gpio_pin_set_dt(&led, 1);
        break;
    case DEOS_STATE_SAFE:
        if (ms_counter % 2000 < 1000) gpio_pin_set_dt(&led, 1);
        else gpio_pin_set_dt(&led, 0);
        break;
    case DEOS_STATE_FAULT:
        if (ms_counter % 200 < 100) gpio_pin_set_dt(&led, 1);
        else gpio_pin_set_dt(&led, 0);
        break;
    default:
        gpio_pin_set_dt(&led, 0);
        break;
    }
}
