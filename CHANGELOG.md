# Changelog

## v2.3.1 – 2026-10-10

### Added
- Raspberry Pi Pico 2 (RP2350) build – same pinout, `jbc_station_v2.3.1_1kolba_pico2.uf2`.
- `i2c` console command: SDA/SCL line test and address scan.
- I2C bus recovery (9 × SCL + STOP + re-init) whenever the ADS1115 stops answering.

### Changed
- PID: default Kd 8 → 14 and integral band ±30 → ±10 °C – removes the 15 °C overshoot on warm-up.

### Fixed
- After the line test the I2C block stayed hung until a reboot, so the ADS1115 looked dead.
- Line test no longer relies on the internal pull-down (RP2350 erratum E9).

### Docs
- License: GNU GPL v3.0.
- Wiring diagram (`docs/wiring_diagram.drawio` + PNG), photos of the station and the controller board.
- Hardware list: voltage sensor module, two DC-DC converters, ULN2803A LED driver, RTC and microSD (spare).
- Thermocouple input protection (2 × 2.2 kΩ, 100 nF, 1 MΩ pull-up on the iron side, optional BAT54S)
  and star grounding.

## v2.3 – 2026-10-02

### Changed
- Heating is switched off after 15 min of hibernation by default (was 30 min).
- No automatic screen dimming – the brightness from the system page is always used.

### Added
- `aoff <min>` console command; `status` shows standby, hibernation and auto-off settings.

## v2.2 – 2026-10-02

### Added
- ADS1115 temperature measurement (`tempsense.c`): differential C245 thermocouple, AxxSolder curve,
  cold-junction compensation from the Pico internal sensor, optional NTC on A2, automatic I2C address scan.
- `tc` console command.
- Sensor selection with `TEMP_SENSOR` – the MAX6675 driver is kept as an option.

### Changed
- Measurement gap shortened from 235 ms to 20 ms – maximum heater power rises from about 60 % to 94 %.
- Heater driver: BTS7960 (IBT-2) high-side module; no firmware change was needed.

### Fixed
- Reversed thermocouple polarity of the C245 (`ADS_TC_INVERT`).

## v2.1 – 2026-09-24

### Added
- System page: temperature offset, power limit, brightness, setpoint step, max tip temperature,
  heating-time counter.
- Pico on-board LED heartbeat (slow = running, fast = fault).
- USB console command `nx` (display link diagnostics, longest main-loop pass) and `[TOUCH]` logging.
- Name maps of all screen components (`display/ui_1kolba/nazwy_*.png`).

### Changed
- “Heating but not rising” protection is now relative to the power limit.
- A missing tip overrides a latched heating fault, so “no tip” is always shown.
- The main screen dims only in hibernation or when the iron is off.
- Active preset is shown with red digits.
- Settings layout changed – the first start after the update loads default settings.

### Fixed
- Touch lag of up to 2 s: `printh` frames without the `FF FF FF` terminator are now handled immediately.
- Spurious display re-initialisation caused by the same issue.
- Physical button pins (GP18/19/20 = buttons 1/2/3).
- Display RX pin now has a pull-up, so a disconnected display does not produce garbage.

## v2.0 – 2026-09-22

- Firmware rewritten from scratch: synchronous heat/measure cycle, PID with anti-windup,
  standby → hibernation → auto-off, tip-change contact, protection, watchdog.
- Touch UI designed in Nextion Editor; the Pico only writes values by component name.
- Settings stored in flash with CRC.
- USB console for diagnostics and PID tuning.

## v1

- First Raspberry Pi Pico firmware (not published).
