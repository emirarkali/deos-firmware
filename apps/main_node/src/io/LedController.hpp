#pragma once
#include <deos/deos.h>
#include <zephyr/drivers/gpio.h>

class LedController {
public:
    LedController();
    int init();
    void update(deos_state_t current_state, uint32_t ms_counter);

private:
    struct gpio_dt_spec led_green; // led0 (Right Signal)
    struct gpio_dt_spec led_blue;  // led1 (Left Signal)
    struct gpio_dt_spec led_red;   // led2 (Brake Light)
};
