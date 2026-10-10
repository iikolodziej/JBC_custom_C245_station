#pragma once
#include "config.h"

void  config_set_defaults(Config_t *cfg);
void  config_load(Config_t *cfg);
void  config_save(Config_t *cfg);
void  config_sanitize(Config_t *cfg);
void  config_pid_defaults(ChannelConfig_t *cc);
bool  config_temp_step_valid(int step);
uint8_t config_temp_step_next(uint8_t step, int dir);

void  adc_voltage_init(void);
float adc_read_voltage(void);
