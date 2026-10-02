#ifndef TRACTION_CONTROL_H
#define TRACTION_CONTROL_H

#include <stdbool.h>

void traction_control_init(void);

/* Safety Logic */
bool traction_is_command_timed_out(void);
bool traction_is_drive_allowed(void);
void traction_apply_safety_limits(void);

#endif /* TRACTION_CONTROL_H */
