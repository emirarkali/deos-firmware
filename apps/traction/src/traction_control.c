#include "traction_control.h"
#include "traction_app.h"
#include "traction_hw.h"
#include "traction_config.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(traction_ctrl, LOG_LEVEL_INF);

#define TRACTION_CONTROL_THREAD_STACK_SIZE 1024
#define TRACTION_CONTROL_THREAD_PRIORITY 5

K_THREAD_STACK_DEFINE(traction_ctrl_stack, TRACTION_CONTROL_THREAD_STACK_SIZE);
static struct k_thread traction_ctrl_thread_data;

bool traction_is_command_timed_out(void)
{
    int64_t now = k_uptime_get();
    if ((now - g_traction_state.last_control_command_ms) > g_traction_config.cmd_timeout_ms) {
        return true;
    }
    return false;
}

bool traction_is_drive_allowed(void)
{
    if (g_traction_state.emergency_stop) {
        return false;
    }
    
    if (g_traction_state.brake_active) {
        return false;
    }
    
    if (traction_is_command_timed_out()) {
        return false;
    }
    
    if (g_traction_state.target_gear == DEOS_GEAR_PARK || 
        g_traction_state.target_gear == DEOS_GEAR_NEUTRAL) {
        return false;
    }

    return true;
}

void traction_apply_safety_limits(void)
{
    // For example, if SOC > 95%, limit regen
    if (g_traction_state.bms_soc_raw > 9500) { // 95.00%
        // Disable or reduce regen torque limit
    }
    
    // TODO: Need proper torque-to-current mapping model before clamping torque limit with max_current.
    // torque_limit_raw is 0.01 % and max_current is 0.1 A, so direct comparison is invalid.
}

static void traction_control_loop(void *arg1, void *arg2, void *arg3)
{
    LOG_INF("Traction control loop started");
    
    while (1) {
        // 1. Safety Checks
        bool drive_allowed = traction_is_drive_allowed();
        
        // 2. Apply Limits
        traction_apply_safety_limits();
        
        // 3. Hardware Output Placeholder
        if (drive_allowed) {
            traction_hw_set_output(
                g_traction_state.target_speed_raw,
                g_traction_state.target_accel_raw,
                g_traction_state.torque_limit_raw
            );
        } else {
            // Send 0 output or neutral command to HW
            traction_hw_set_output(0, 0, 0);
        }
        
        k_sleep(K_MSEC(20)); // 50 Hz control loop
    }
}

void traction_control_init(void)
{
    k_thread_create(&traction_ctrl_thread_data, traction_ctrl_stack,
                    K_THREAD_STACK_SIZEOF(traction_ctrl_stack),
                    traction_control_loop,
                    NULL, NULL, NULL,
                    TRACTION_CONTROL_THREAD_PRIORITY, 0, K_NO_WAIT);
}
