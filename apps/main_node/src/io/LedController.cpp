#include "LedController.hpp"
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(main_node_app);

#define LED_GREEN_NODE DT_ALIAS(led0) // Right signal
#define LED_BLUE_NODE  DT_ALIAS(led1) // Left signal
#define LED_RED_NODE   DT_ALIAS(led2) // Brake light

LedController::LedController() : 
    led_green(GPIO_DT_SPEC_GET(LED_GREEN_NODE, gpios)),
    led_blue(GPIO_DT_SPEC_GET(LED_BLUE_NODE, gpios)),
    led_red(GPIO_DT_SPEC_GET(LED_RED_NODE, gpios)) 
{}

int LedController::init() {
    if (!gpio_is_ready_dt(&led_green)) {
        LOG_ERR("Green LED device is not ready");
        return -ENODEV;
    }
    if (!gpio_is_ready_dt(&led_blue)) {
        LOG_ERR("Blue LED device is not ready");
        return -ENODEV;
    }
    if (!gpio_is_ready_dt(&led_red)) {
        LOG_ERR("Red LED device is not ready");
        return -ENODEV;
    }

    gpio_pin_configure_dt(&led_green, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&led_blue, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&led_red, GPIO_OUTPUT_INACTIVE);

    return 0;
}

void LedController::update(deos_state_t current_state, uint32_t ms_counter) {
    bool g = false; // Right signal (Green)
    bool b = false; // Left signal (Blue)
    bool r = false; // Brake light (Red)

    switch (current_state) {
    case DEOS_STATE_INIT:
        // Chaser effect during boot
        g = (ms_counter % 600 < 200);
        b = (ms_counter % 600 >= 200 && ms_counter % 600 < 400);
        r = (ms_counter % 600 >= 400);
        break;

    case DEOS_STATE_STANDBY:
        // Parked / Hazard lights: Both signals blink slowly (dörtlüler yanıyor), Brake OFF
        g = b = (ms_counter % 1000 < 500);
        r = false;
        break;

    case DEOS_STATE_ACTIVE:
        // System running normally. Everything OFF by default.
        // Wait for actual turn commands, but for now keep them off.
        g = false;
        b = false;
        r = false;
        break;

    case DEOS_STATE_SAFE:
        // E-STOP pressed: Hard Brake ON, Hazards flashing rapidly.
        r = true; 
        g = b = (ms_counter % 250 < 125);
        break;

    case DEOS_STATE_FAULT:
        // Critical Error: Alternating left/right signals (police lights), fast blinking brake
        g = (ms_counter % 200 < 100);
        b = !g;
        r = (ms_counter % 200 < 100);
        break;

    default:
        g = b = r = false;
        break;
    }

    gpio_pin_set_dt(&led_green, g);
    gpio_pin_set_dt(&led_blue, b);
    gpio_pin_set_dt(&led_red, r);
}
