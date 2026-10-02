#ifndef TRACTION_APP_H
#define TRACTION_APP_H

#include <stdint.h>
#include <stdbool.h>
#include <deos/deos_icd.h>

/* Application state structure */
struct traction_app_state {
    uint16_t target_speed_raw;
    int16_t target_accel_raw;

    uint8_t target_gear; // deos_traction_gear_t
    uint16_t torque_limit_raw;

    uint16_t current_speed_raw;
    int16_t current_accel_raw;

    bool brake_active;
    bool emergency_stop;

    uint16_t bms_soc_raw;
    uint16_t bms_max_discharge_current_raw;
    uint16_t bms_max_charge_current_raw;

    int64_t last_control_command_ms;
};

/* TODO: This should eventually go to public ICD if standardized */
struct traction_status_response_payload {
    uint16_t current_speed;
    int16_t current_accel;
    uint8_t current_gear;
    uint16_t torque_limit;
    uint8_t state_flags;
} __attribute__((packed));

extern struct traction_app_state g_traction_state;

void traction_app_init(void);
struct traction_app_state* traction_get_state(void);

#endif /* TRACTION_APP_H */
