#include "SafetyManager.hpp"
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(main_node_app);

#define SW0_NODE DT_ALIAS(sw0)

#if !DT_NODE_HAS_STATUS(SW0_NODE, okay)
#error "Unsupported board: sw0 devicetree alias is not defined"
#endif

SafetyManager::SafetyManager() : button(GPIO_DT_SPEC_GET_OR(SW0_NODE, gpios, {0})) {}

int SafetyManager::init() {
    if (!gpio_is_ready_dt(&button)) {
        LOG_ERR("Error: button device %s is not ready", button.port->name);
        return -ENODEV;
    }
    int ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
    if (ret != 0) {
        LOG_ERR("Error %d: failed to configure %s pin %d", ret, button.port->name, button.pin);
        return ret;
    }
    return 0;
}

bool SafetyManager::is_estop_triggered() const {
    return gpio_pin_get_dt(&button) > 0;
}
