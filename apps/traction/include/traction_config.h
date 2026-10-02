#ifndef TRACTION_CONFIG_H
#define TRACTION_CONFIG_H

#include <stdint.h>
#include <stddef.h>
#include <deos/deos_icd.h>

struct traction_config {
    uint16_t pid_kp;
    uint16_t pid_ki;
    uint16_t pid_kd;
    uint16_t max_current;
    uint16_t max_speed;
    uint16_t accel_limit;
    uint16_t cmd_timeout_ms;
    uint8_t pole_pairs;
    uint16_t gear_ratio;
    uint16_t wheel_circum_mm;
    uint16_t regen_cutoff_speed;
};

extern struct traction_config g_traction_config;

void traction_config_init(void);

/* Parameter handlers */
int traction_config_get_param(deos_traction_parameter_t param, void *out_val, size_t *out_len);
int traction_config_set_param(deos_traction_parameter_t param, const void *val, size_t len);

#endif /* TRACTION_CONFIG_H */
