#include "traction_app.h"
#include <string.h>
#include <zephyr/kernel.h>

struct traction_app_state g_traction_state;

void traction_app_init(void)
{
    memset(&g_traction_state, 0, sizeof(g_traction_state));
    
    // Default safe state
    g_traction_state.target_gear = DEOS_GEAR_PARK;
    g_traction_state.emergency_stop = false;
    g_traction_state.brake_active = false;
    g_traction_state.system_state = DEOS_STATE_INIT;
    g_traction_state.system_mode = DEOS_MODE_NORMAL;
    g_traction_state.last_control_command_ms = k_uptime_get();
}

struct traction_app_state* traction_get_state(void)
{
    return &g_traction_state;
}
