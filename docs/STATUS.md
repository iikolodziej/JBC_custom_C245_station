# Project status

*Last update: 2026-10-10 – v2.3.1, complete*

The station is finished and in daily use. Everything below is verified on the real hardware
(Raspberry Pi Pico 2, 24 V supply, JBC T245 handle with C245 cartridges).

## Firmware v2.3.1

Files: `firmware/bin/jbc_station_v2.3.1_1kolba.uf2` (Pico), `firmware/bin/jbc_station_v2.3.1_1kolba_pico2.uf2` (Pico 2).

- Temperature measured with an ADS1115: C245 curve, cold-junction compensation, reversed polarity handled.
- Heater switched on the positive side by a BTS7960 (IBT-2) module.
- Synchronous heat/measure cycle (350 ms heat + 20 ms measure), PID, iron modes.
- Stand sensor with standby, hibernation and auto-off; tip-change contact.
- Protection: no tip, over-temperature, “heating but not rising”, watchdog, I2C bus recovery.
- System page: offset, power limit, brightness, setpoint step, max temperature, heating-time counter.
- USB console: `status`, `log`, `pid`, `tc`, `i2c`, `nx`, touch logging.
- Runs on the Pico 2 (RP2350) and the Pico (RP2040).

## Verified on hardware

| Area | Result |
|---|---|
| Temperature control | warm-up 200 → 300 °C without overshoot, setpoint held within ±0.3 °C |
| Setpoint step | 300 → 380 °C within +3 °C, then stable |
| Measurement | ADS1115 at 0x48 behind 2.2 kΩ + 100 nF input protection, missing-tip detection with the 1 MΩ pull-up |
| Stand | standby, hibernation, auto-off, immediate wake-up on lifting |
| Tip change | heating blocked during the change, back to work 5 s after release |
| Supply monitoring | 24 V input shown on the screen (24.3 V) |
| Display | link both ways, immediate touch response, three-page Nextion UI |
| Buttons and LEDs | GP18/19/20 and GP21/22/2 = presets 1/2/3, LEDs through the ULN2803A |
| Protection | “heating but not rising” trips without 24 V; I2C recovers after a bus fault |
