#pragma once
#include <zephyr/drivers/gpio.h>

class SafetyManager {
public:
    SafetyManager();
    int init();
    bool is_estop_triggered() const;

private:
    struct gpio_dt_spec button;
};
