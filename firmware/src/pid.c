#include "pid.h"
#include <math.h>

// Groty JBC C245 mają niską bezwładność termiczną:
// - duże Kp (szybka reakcja)
// - małe Ki (tylko dociąga uchyb ustalony)
// - małe Kd (tłumienie przeregulowania)
#define PID_INTEGRATE_BAND  10.0f   // całkuj tylko blisko nastawy (anti-windup przy nagrzewaniu)

void pid_init(PID_t *pid, float kp, float ki, float kd) {
    pid->kp             = kp;
    pid->ki             = ki;
    pid->kd             = kd;
    pid->integral_limit = 500.0f;
    pid->output_limit   = (float)PWM_MAX_DUTY;
    pid_reset(pid);
}

void pid_reset(PID_t *pid) {
    pid->integral  = 0.0f;
    pid->prev_meas = 0.0f;
    pid->has_prev  = 0;
}

// Zwraca 0..output_limit (duty 0-1000)
float pid_compute(PID_t *pid, float setpoint, float current, float dt) {
    if (dt <= 0.0f) return 0.0f;

    float error  = setpoint - current;
    float p_term = pid->kp * error;

    // Pochodna liczona z pomiaru, nie z uchybu - zmiana nastawy nie daje "kopnięcia"
    float d_term = 0.0f;
    if (pid->has_prev) {
        d_term = -pid->kd * (current - pid->prev_meas) / dt;
    }
    pid->prev_meas = current;
    pid->has_prev  = 1;

    // Całka trzymana w jednostkach wyjścia => zmiana Ki nie powoduje skoku
    if (fabsf(error) < PID_INTEGRATE_BAND) {
        float candidate = pid->integral + pid->ki * error * dt;
        float out_try   = p_term + candidate + d_term;
        // Warunkowe całkowanie: nie pompuj całki, gdy wyjście i tak jest nasycone
        bool saturating = (out_try > pid->output_limit && error > 0) ||
                          (out_try < 0.0f && error < 0);
        if (!saturating) pid->integral = candidate;
    }
    if (pid->integral >  pid->integral_limit) pid->integral =  pid->integral_limit;
    if (pid->integral < -pid->integral_limit) pid->integral = -pid->integral_limit;

    float output = p_term + pid->integral + d_term;
    if (output < 0.0f)              output = 0.0f;   // grzałka nie chłodzi
    if (output > pid->output_limit) output = pid->output_limit;
    return output;
}
