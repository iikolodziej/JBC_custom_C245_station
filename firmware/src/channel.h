#pragma once
#include "config.h"

// Sprzęt: PWM (startuje z wyłączonymi grzałkami) + wejścia stojaka / wymiany grota.
// Wywołaj jako PIERWSZE w main().
void    channel_hw_init(void);

void    channel_init(Channel_t *ch, int idx, const Config_t *cfg);
void    channel_apply_pid_gains(Channel_t *ch, const ChannelConfig_t *cc);

// Maszyna stanów grzanie/pomiar. Zwraca true gdy był nowy pomiar i przeliczenie PID.
bool    control_tick(SystemState_t *sys, uint32_t now_ms);
// Natychmiast wyłącza wszystkie grzałki (np. przed zapisem flash)
void    heaters_all_off(void);

// Akcje użytkownika
void    channel_wake(Channel_t *ch);
void    channel_toggle_power(Channel_t *ch);
void    channel_set_profile(Channel_t *ch, const Config_t *cfg, int profile_idx);
void    channel_adjust_setpoint(Channel_t *ch, const Config_t *cfg, int delta);

// Kopiuje nastawy runtime (setpoint/profil) do struktury Config_t przed zapisem
void    channels_store_to_config(SystemState_t *sys);

uint8_t channel_is_docked(int ch);
uint8_t channel_tip_change_active(int ch);
bool    channel_is_heating_allowed(const Channel_t *ch);
