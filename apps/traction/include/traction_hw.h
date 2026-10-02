#ifndef TRACTION_HW_H
#define TRACTION_HW_H

#include <stdint.h>
#include <deos/deos_icd.h>

void traction_hw_init(void);

/* Stubs for actual motor controller hardware interaction */
void traction_hw_set_output(uint16_t speed, int16_t accel, uint16_t torque_limit);
void traction_hw_set_gear(deos_traction_gear_t gear);
void traction_hw_emergency_stop(void);

#endif /* TRACTION_HW_H */
