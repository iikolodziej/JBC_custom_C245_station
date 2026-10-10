#pragma once
#include "config.h"

void  pid_init(PID_t *pid, float kp, float ki, float kd);
void  pid_reset(PID_t *pid);
float pid_compute(PID_t *pid, float setpoint, float current, float dt);
