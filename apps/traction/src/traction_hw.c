#include "traction_hw.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(traction_hw, LOG_LEVEL_INF);

void traction_hw_init(void)
{
    LOG_INF("Traction hardware initialized (stub)");
}

void traction_hw_set_output(uint16_t speed, int16_t accel, uint16_t torque_limit)
{
    // Stub: send command to actual motor controller (e.g. Kelly KEB)
    // LOG_DBG("HW SET: speed=%u, accel=%d, torque=%u", speed, accel, torque_limit);
}

void traction_hw_set_gear(deos_traction_gear_t gear)
{
    LOG_INF("HW GEAR SET: %d", gear);
}

void traction_hw_emergency_stop(void)
{
    LOG_ERR("HW EMERGENCY STOP");
    // Hard stop output
}
