#include "traction_config.h"
#include <string.h>

struct traction_config g_traction_config;

void traction_config_init(void)
{
    // Load defaults
    memset(&g_traction_config, 0, sizeof(g_traction_config));
    
    g_traction_config.pid_kp = 100;
    g_traction_config.pid_ki = 10;
    g_traction_config.pid_kd = 5;
    g_traction_config.max_current = 2000;  // 200.0 A (scale: 0.1A)
    g_traction_config.max_speed = 3000;    // 30 m/s
    g_traction_config.accel_limit = 500;   // 5 m/s^2
    g_traction_config.cmd_timeout_ms = 200;
    g_traction_config.pole_pairs = 4;
    g_traction_config.gear_ratio = 1000;   // 10.00
    g_traction_config.wheel_circum_mm = 2000;
    g_traction_config.regen_cutoff_speed = 200; // 2 m/s
}

int traction_config_get_param(deos_traction_parameter_t param, void *out_val, size_t *out_len)
{
    if (!out_val || !out_len) return -1;
    
    switch (param) {
        case DEOS_PARAM_TRACTION_PID_KP:
            memcpy(out_val, &g_traction_config.pid_kp, sizeof(uint16_t));
            *out_len = sizeof(uint16_t);
            return 0;
        case DEOS_PARAM_TRACTION_PID_KI:
            memcpy(out_val, &g_traction_config.pid_ki, sizeof(uint16_t));
            *out_len = sizeof(uint16_t);
            return 0;
        case DEOS_PARAM_TRACTION_PID_KD:
            memcpy(out_val, &g_traction_config.pid_kd, sizeof(uint16_t));
            *out_len = sizeof(uint16_t);
            return 0;
        case DEOS_PARAM_TRACTION_MAX_CURRENT:
            memcpy(out_val, &g_traction_config.max_current, sizeof(uint16_t));
            *out_len = sizeof(uint16_t);
            return 0;
        case DEOS_PARAM_TRACTION_MAX_SPEED:
            memcpy(out_val, &g_traction_config.max_speed, sizeof(uint16_t));
            *out_len = sizeof(uint16_t);
            return 0;
        case DEOS_PARAM_TRACTION_ACCEL_LIMIT:
            memcpy(out_val, &g_traction_config.accel_limit, sizeof(uint16_t));
            *out_len = sizeof(uint16_t);
            return 0;
        case DEOS_PARAM_TRACTION_CMD_TIMEOUT_MS:
            memcpy(out_val, &g_traction_config.cmd_timeout_ms, sizeof(uint16_t));
            *out_len = sizeof(uint16_t);
            return 0;
        case DEOS_PARAM_TRACTION_POLE_PAIRS:
            memcpy(out_val, &g_traction_config.pole_pairs, sizeof(uint8_t));
            *out_len = sizeof(uint8_t);
            return 0;
        case DEOS_PARAM_TRACTION_GEAR_RATIO:
            memcpy(out_val, &g_traction_config.gear_ratio, sizeof(uint16_t));
            *out_len = sizeof(uint16_t);
            return 0;
        case DEOS_PARAM_TRACTION_WHEEL_CIRCUM_MM:
            memcpy(out_val, &g_traction_config.wheel_circum_mm, sizeof(uint16_t));
            *out_len = sizeof(uint16_t);
            return 0;
        case DEOS_PARAM_TRACTION_REGEN_CUTOFF_SPD:
            memcpy(out_val, &g_traction_config.regen_cutoff_speed, sizeof(uint16_t));
            *out_len = sizeof(uint16_t);
            return 0;
        default:
            return -1;
    }
}

int traction_config_set_param(deos_traction_parameter_t param, const void *val, size_t len)
{
    if (!val) return -1;
    
    switch (param) {
        case DEOS_PARAM_TRACTION_PID_KP:
            if (len != sizeof(uint16_t)) return -1;
            memcpy(&g_traction_config.pid_kp, val, len);
            return 0;
        case DEOS_PARAM_TRACTION_PID_KI:
            if (len != sizeof(uint16_t)) return -1;
            memcpy(&g_traction_config.pid_ki, val, len);
            return 0;
        case DEOS_PARAM_TRACTION_PID_KD:
            if (len != sizeof(uint16_t)) return -1;
            memcpy(&g_traction_config.pid_kd, val, len);
            return 0;
        case DEOS_PARAM_TRACTION_MAX_CURRENT:
            if (len != sizeof(uint16_t)) return -1;
            memcpy(&g_traction_config.max_current, val, len);
            return 0;
        case DEOS_PARAM_TRACTION_MAX_SPEED:
            if (len != sizeof(uint16_t)) return -1;
            memcpy(&g_traction_config.max_speed, val, len);
            return 0;
        case DEOS_PARAM_TRACTION_ACCEL_LIMIT:
            if (len != sizeof(uint16_t)) return -1;
            memcpy(&g_traction_config.accel_limit, val, len);
            return 0;
        case DEOS_PARAM_TRACTION_CMD_TIMEOUT_MS:
            if (len != sizeof(uint16_t)) return -1;
            memcpy(&g_traction_config.cmd_timeout_ms, val, len);
            return 0;
        case DEOS_PARAM_TRACTION_POLE_PAIRS:
            if (len != sizeof(uint8_t)) return -1;
            memcpy(&g_traction_config.pole_pairs, val, len);
            return 0;
        case DEOS_PARAM_TRACTION_GEAR_RATIO:
            if (len != sizeof(uint16_t)) return -1;
            memcpy(&g_traction_config.gear_ratio, val, len);
            return 0;
        case DEOS_PARAM_TRACTION_WHEEL_CIRCUM_MM:
            if (len != sizeof(uint16_t)) return -1;
            memcpy(&g_traction_config.wheel_circum_mm, val, len);
            return 0;
        case DEOS_PARAM_TRACTION_REGEN_CUTOFF_SPD:
            if (len != sizeof(uint16_t)) return -1;
            memcpy(&g_traction_config.regen_cutoff_speed, val, len);
            return 0;
        default:
            return -1; // Unsupported
    }
}
