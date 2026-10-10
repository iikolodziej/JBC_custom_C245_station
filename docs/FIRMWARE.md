# Firmware – how it works (v2.3.1)

Raspberry Pi Pico firmware written in C on Pico SDK 1.5.1. The source is in `firmware/src/`.

The same source also builds for the **Pico 2 (RP2350)** with Pico SDK 2.1.1 and picotool 2.1.1, using the same pinout.
The result is `bin/jbc_station_v2.3.1_1kolba_pico2.uf2`:

```
cmake -S firmware -B firmware/build_pico2 -G Ninja -DPICO_BOARD=pico2 -DPICO_PLATFORM=rp2350-arm-s -Dpicotool_DIR=<picotool dir>
ninja -C firmware/build_pico2
```
Comments in the code are in Polish.

## Files

| File | Contents |
|---|---|
| `main.c` | main loop, physical buttons, USB console, heating-time counter, status LED |
| `config.h` | **pins**, constants, ranges, data structures – change hardware here |
| `config.c` | defaults, flash load/save with CRC, supply voltage measurement |
| `channel.c` | heat/measure cycle, iron modes, protection |
| `pid.c` | PID controller |
| `tempsense.c` | tip temperature: ADS1115 driver, C245 curve, cold-junction compensation; MAX6675 as an option |
| `max6675.c` | legacy MAX6675 driver (bit-banged SPI), used when `TEMP_SENSOR` = MAX6675 |
| `buttons.c` | short/long button presses and their LEDs |
| `nextion.c` | display UART driver: baud auto-detection, cache of sent values, touch events |
| `ui.c`, `ui.h` | screen page logic, button event codes |

## Number of irons

```c
#define HW_CHANNELS   2   // irons supported by the board
#define NUM_CHANNELS  1   // irons handled by the firmware and the screen (1 or 2)
```

With one iron the heater output of iron 2 (GP16) is held low.
Two irons also need a different main screen – the graphics generator has a 2-iron layout in `define_main_2()`.

## Heat / measure cycle

In the C245 cartridge the thermocouple and the heater share one wire, so a reading taken while
heater current flows is wrong.

```
|<--- heating 350 ms --->|<10 ms>|<-- ADS1115 10 ms -->|  -> read, PID, next cycle
|  duty = PID output     | 0     | 0                   |
```

The maximum average power is about 94 % of the heater power (60 % with the legacy MAX6675). The PID output range is 0–1000,
further limited by the power limit on the system page.

## Temperature measurement (ADS1115)

`TEMP_SENSOR` in `config.h` selects `TEMP_SENSOR_ADS1115` (default) or `TEMP_SENSOR_MAX6675`.

- **Input:** differential A0–A1, ±0.256 V range, 7.8 µV per step, 128 samples/s (one conversion ≈ 8 ms).
- **Curve:** the C245 thermocouple is not type K. The firmware uses the curve measured by the
  [AxxSolder](https://github.com/AxxAxx/AxxSolder) project: ΔT = 0.035747·U − 4.1975·10⁻⁸·U² (U in µV),
  about 28 µV/°C, 9.2 mV at 350 °C.
- **Cold junction:** the Pico internal temperature sensor (`CJC_SOURCE`). An optional 10 kΩ NTC on A2
  (`CJC_NTC_A2`) is more accurate; it is only possible with one iron.
- **Polarity:** `ADS_TC_INVERT 1` – in the C245 the green wire is negative relative to the red one.
- **Missing tip:** a 1 MΩ resistor from the green wire (iron side of the 2.2 kΩ series resistor) to 3.3 V
  drives the input to full scale → `BRAK KOLBY`.
- **I2C recovery:** whenever the module stops answering, the firmware clocks the bus free
  (9 × SCL + STOP), re-initialises the I2C block and scans 0x48–0x4B again.
- **Address:** 0x48–0x4B is scanned at start-up and again while the module is missing.

Measured on real hardware with default PID: 360 °C held within ±0.3 °C at about 7 % average power.

## PID

Defaults: Kp = 15, Ki = 2.5, Kd = 14 – tuned on a real C245 cartridge on 24 V.
With Kd = 8 a warm-up from 130 to 300 °C overshot to 315 °C (the heater keeps delivering heat after the
power is cut); with Kd = 14 it settles at 300 °C without overshoot, and a 300 → 380 °C step stays within +3 °C.

- The integral works only within ±10 °C of the setpoint, so it does not wind up while heating.
- The derivative is taken from the measurement, so a setpoint change does not cause a power spike.

Tuning from the console: `log on`, then `pid 1 <kp> <ki> <kd>`.

| Symptom | Change |
|---|---|
| temperature oscillates | lower Kp or raise Kd |
| reaches the setpoint too slowly | raise Kp |
| steady error under load | raise Ki |
| overshoot | lower Ki or raise Kd |

## Iron modes

The on-screen names are in Polish; English meaning in brackets.

```
lifted ───────────────────────────────────────► PRACA (work, setpoint)
in stand ─► CZUWANIE (standby, keeps setpoint) ─[time to hibernation]─► HIBERNACJA (hibernation temp.)
          ─[auto-off]─► WYLACZONA (off)
lifting from standby, hibernation or auto-off ─► PRACA immediately
switched off by the user ─► waits for a touch or a button (lifting does not wake it)
tip-change contact ─► WYM. GROTU (tip change, heating blocked for at least 5 s)
```

The screen always uses the brightness from the system page (no automatic dimming since v2.3).

## Protection

| Situation | Reaction | Reset |
|---|---|---|
| no tip / open thermocouple | heating off, “BRAK KOLBY” | automatic when the tip is back |
| temperature above 520 °C | heating off, “PRZEGRZANIE” | preset or mode touch after cooling down |
| ≥ 80 % of allowed power for 15 s and rise below 15 °C | heating off, “BLAD GRZANIA” | preset or mode touch |
| firmware hang | 2 s watchdog, restart with heaters off | – |
| supply below 18 V | voltage shown in red | – |

## Settings

| Parameter | Range | Step | Default |
|---|---|---|---|
| presets 1 / 2 / 3 | 150 °C – max temperature | setpoint step | 300 / 360 / 450 °C |
| time to hibernation | 5–300 s | 5 s | 10 s |
| hibernation temperature | 50–250 °C | 5 °C | 150 °C |
| auto-off (from the start of hibernation) | 0–120 min (0 = never) | 5 min | 15 min |
| temperature offset | ±50 °C | 1 °C | 0 |
| power limit | 10–100 % | 5 % | 100 % |
| brightness | 10–100 % | 10 % | 80 % |
| setpoint step | 1, 2, 5, 10, 20, 50 °C | – | 5 °C |
| max tip temperature | 200–480 °C | 10 °C | 450 °C |

Settings are saved automatically 20 s after the last change, or immediately with “ZAPISZ” (save).
They live in the last flash sector with a CRC. When the data layout changes (`FLASH_CONFIG_MAGIC`),
the defaults are loaded. The heating-time counter is saved every 10 minutes of heating.

## USB console (115200)

```
help                  list of commands
status                iron state, settings, display link
log on | log off      CSV per reading: ms,ch,temp,target,duty,mode
set 1 350             setpoint of iron 1
pid 1 15 2.5 14       Kp Ki Kd of iron 1
offset 1 -5           reading offset of iron 1
on 1 | off 1          switch iron 1 on / off
aoff 15               heating off after 15 min of hibernation (0 = never)
save                  save to flash
defaults              factory settings (the heating-time counter is kept)
nx                    display link diagnostics and the longest main-loop pass
tc                    thermocouple: ADS1115 address, cold junction, raw value, voltage, temperature
i2c                   SDA/SCL line test (held up by the module / open / shorted) and address scan
```

Every screen touch is printed as `[TOUCH] <time> ms: strona <page> id <id>`.

## Pico on-board LED (GP25)

| LED | Meaning |
|---|---|
| blinks once per second | firmware is running |
| blinks four times per second | over-temperature or heating fault |
| does not blink | firmware stalled – the watchdog will restart it |
