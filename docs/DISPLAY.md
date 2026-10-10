# Nextion display – building the HMI project

Display: **NX3224T028 / NX3224K028** (2.8", 320×240, landscape).

## Who does what

| Who | Does |
|---|---|
| **You, in Nextion Editor** | the whole look: backgrounds, photos, logo, icons, fonts, layout, extra labels |
| **Pico** | writes values into components with fixed **names**, changes state colours, switches pages |
| **HMI buttons** | send one line only: `printh 65 <page> <id> 01` |

Key rules:

1. **The component name (`objname`) must match exactly** – the Pico addresses components by name.
2. **Button code** goes into **Touch Press Event**, one line from the tables below.
   Leave Touch Release Event empty and untick “Send Component ID”.
3. **The Pico switches pages.** Do not put `page 1` etc. into buttons – `printh` is enough.
4. **Everything else is up to you**: photos, backgrounds, labels, colours, positions and sizes.

## Ready-made backgrounds

`display/ui_1kolba/` contains generated backgrounds and helper files:

| File | Purpose |
|---|---|
| `pic0_main.png`, `pic1_main_pressed.png` | main page: normal / pressed |
| `pic2_settings.png`, `pic3_settings_pressed.png` | settings page |
| `pic4_system.png`, `pic5_system_pressed.png` | system page |
| `preview_*.png` | preview with sample values (for viewing only) |
| `nazwy_*.png` | name maps – every component outlined and labelled |
| `components.md` | full table: positions, sizes, fonts, colours, codes |

![Main](../display/ui_1kolba/preview_main.png)
![Settings](../display/ui_1kolba/preview_settings.png)
![System](../display/ui_1kolba/preview_system.png)

The labels drawn on the backgrounds are in Polish. `components.md` is generated with Polish headings.
Regenerate everything with `python firmware/tools/gen_nextion_ui.py`.

## Step by step

### 1. Project

- Device: **NX3224T028** (or K028 – see the sticker on the back)
- Display direction: **90°** (landscape, 320×240)
- Character encoding: **iso-8859-2** (needed for Polish characters)

In `Program.s` change the first line `baud=9600` to `baud=115200` (optional – the Pico auto-detects the baud rate).

### 2. Fonts

Add them in the **Fonts** panel in this order – the order sets the font ID:

| ID | File | Size | Used for |
|---|---|---|---|
| 0 | `display/font0.zi` | 16 | labels, mode, status |
| 1 | `display/font1.zi` | 24 bold | buttons, values |
| 2 | `display/font2.zi` | 48 bold | spare |
| 3 | `display/Arial_96_bold.zi` | 96 bold | big temperature `t_cur1` |

Your own sizes: **Tools → Font Generator**, encoding iso-8859-2. A font must contain digits, `-`, `.`, `%`, `+`,
`(`, `)` and capital letters.

### 3. Pictures

Add `pic0 … pic5` in this order so that their IDs are 0–5. Add your own photos and logos **after** them.

### 4. Pages

| No. | Name (free) | Contents |
|---|---|---|
| 0 | `main` | main screen |
| 1 | `settings` | presets and standby settings |
| 2 | `system` | offset, power limit, brightness, setpoint step, max temperature, heating-time counter |
| 3 | `pid` | optional – PID gains; without it, tune PID from the USB console |

The page number matters – it is the second byte in `printh`.

### 5. Transparent buttons with a pressed effect

- **Page:** `sta = image`, `pic` = background of the page (0, 2 or 4).
- **Button:** `sta = crop image`, `picc` = page background, `picc2` = pressed background (1, 3 or 5), `txt` empty.
  The button shows the matching part of the picture and switches to the pressed picture while touched.
- **Value field:** `sta = crop image`, `picc` = page background.
- **Exceptions** – keep `sta = solid color`, the Pico sets their colours: `t_mode1`, `b_p1…3`, `j_pwr1`.

Positions must match `components.md` exactly, otherwise the cropped part is shifted.

### 6. Compile and upload

`Compile` → `File → TFT file output` → copy the `.tft` to a FAT32 microSD card (only that file) →
insert it into the display and power up → after “Update successed” remove the card and restart.
**Restart the Pico together with the display** after uploading a new `.tft`.

## Component list – the contract with the Pico

“Pico sets” lists attributes that are overwritten at runtime – do not rely on their editor values.

### Page 0 – `main`

| objname | Type | Pico sets | Touch Press Event |
|---|---|---|---|
| `t_cur1` | Text, font 3 | `txt` (`357`, `---`), `pco` by state | – |
| `t_set1` | Text | `txt` (`SET 360`, `SET 150 (360)`) | – |
| `t_mode1` | Text, **solid color** | `txt`, `bco`, `pco` | `printh 65 00 16 01` (iron on/off) |
| `j_pwr1` | Progress bar, **solid color** | `val` 0–100, `pco` | – |
| `t_vin` | Text | `txt` (`24.1V`), `pco` (red below 18 V) | – |
| `b_p1` / `b_p2` / `b_p3` | Button, **solid color** | `txt` (preset temp.), `bco`, `pco` (active = red digits) | `printh 65 00 01 01` / `02` / `03` |
| `b_minus` / `b_plus` | Button (optional) | – | `printh 65 00 04 01` / `printh 65 00 05 01` |
| `b_cfg` | Button | – | `printh 65 00 0A 01` (→ settings) |

### Page 1 – `settings`

| objname | Type | Pico sets | Touch Press Event |
|---|---|---|---|
| `n_prof1` / `n_prof2` / `n_prof3` | Number | `val` [°C] | – |
| `b_prof1p` / `b_prof1m` | Button | – | `printh 65 01 01 01` / `printh 65 01 02 01` |
| `b_prof2p` / `b_prof2m` | Button | – | `printh 65 01 03 01` / `printh 65 01 04 01` |
| `b_prof3p` / `b_prof3m` | Button | – | `printh 65 01 05 01` / `printh 65 01 06 01` |
| `n_sleep` | Number | `val` [s] – time to hibernation | – |
| `b_sleepp` / `b_sleepm` | Button | – | `printh 65 01 07 01` / `printh 65 01 08 01` |
| `n_hib` | Number | `val` [°C] – hibernation temperature | – |
| `b_hibp` / `b_hibm` | Button | – | `printh 65 01 09 01` / `printh 65 01 0A 01` |
| `n_aoff` | Number (optional) | `val` [min] – auto-off, 0 = never | – |
| `b_aoffp` / `b_aoffm` | Button (optional) | – | `printh 65 01 0B 01` / `printh 65 01 0C 01` |
| `t_kp` / `t_ki` / `t_kd` | **Text** (optional) | `txt` – PID preview, read-only | – |
| `t_status` | Text | `txt` („ZAPISANO” = saved, clears after 1.5 s) | – |
| `b_sys` | Button | – | `printh 65 01 0D 01` (→ system) |
| `b_save` | Button | – | `printh 65 01 14 01` (save) |
| `b_back` | Button | – | `printh 65 01 15 01` (→ main) |

### Page 2 – `system`

| objname | Type | Pico sets | Touch Press Event |
|---|---|---|---|
| `t_off` | **Text** | `txt` (`+0`) – temperature offset, ±50 °C, step 1 | – |
| `b_offp` / `b_offm` | Button | – | `printh 65 02 01 01` / `printh 65 02 02 01` |
| `n_pwr` | Number | `val` [%] – power limit, 10–100, step 5 | – |
| `b_pwrp` / `b_pwrm` | Button | – | `printh 65 02 03 01` / `printh 65 02 04 01` |
| `n_bri` | Number | `val` [%] – brightness, 10–100, step 10 | – |
| `b_brip` / `b_brim` | Button | – | `printh 65 02 05 01` / `printh 65 02 06 01` |
| `n_step` | Number | `val` [°C] – setpoint step: 1, 2, 5, 10, 20, 50 | – |
| `b_stepp` / `b_stepm` | Button | – | `printh 65 02 07 01` / `printh 65 02 08 01` |
| `n_tmax` | Number | `val` [°C] – max tip temperature, 200–480, step 10 | – |
| `b_tmaxp` / `b_tmaxm` | Button | – | `printh 65 02 09 01` / `printh 65 02 0A 01` |
| `t_hours` | **Text** | `txt` (`12 h 05 min`) – heating-time counter | – |
| `t_live` | **Text** | `txt` (`T=357/360 P=45%`) – live reading for calibration | – |
| `t_status` | Text | `txt` („ZAPISANO”) | – |
| `b_pid` | Button (optional) | – | `printh 65 02 0B 01` (→ PID page) |
| `b_save` | Button | – | `printh 65 02 14 01` (save) |
| `b_back` | Button | – | `printh 65 02 15 01` (→ settings) |

The setpoint step applies to `b_minus` / `b_plus` on the main page and to the preset +/− buttons.
Lowering the max temperature immediately clips presets and the setpoint.

### Page 3 – `pid` (optional, no ready background)

| objname | Type | Pico sets | Touch Press Event |
|---|---|---|---|
| `t_ch` | Text (optional) | `txt` (`KOLBA 1`) | – |
| `t_kp` / `t_ki` / `t_kd` | **Text** | `txt` (`15.0`, `2.50`, `8.0`) | – |
| `b_kpp` / `b_kpm` | Button | – | `printh 65 03 01 01` / `printh 65 03 02 01` |
| `b_kip` / `b_kim` | Button | – | `printh 65 03 03 01` / `printh 65 03 04 01` |
| `b_kdp` / `b_kdm` | Button | – | `printh 65 03 05 01` / `printh 65 03 06 01` |
| `t_live`, `t_status` | Text | `txt` | – |
| `b_dflt` | Button | – | `printh 65 03 0D 01` (PID defaults) |
| `b_save` | Button | – | `printh 65 03 14 01` (save) |
| `b_back` | Button | – | `printh 65 03 15 01` (→ system) |

## Colours

The Pico sets colours only where they carry meaning. Everywhere else set `pco` yourself –
the editor default is black, invisible on a dark background.

| Components | `pco` | Colour |
|---|---|---|
| all values on pages 1 and 2 (`n_*`, `t_kp/ki/kd`, `t_off`, `t_hours`) | 61310 | white |
| `t_set1` | 36020 | grey |
| `t_live` | 64451 | orange |
| `t_status` | 11886 | green |
| `b_p1…3` pressed: `bco2` / `pco2` | 14890 / 64451 | lighter grey / orange |
| `j_pwr1` background: `bco` | 10631 | dark grey |

State colours set by the Pico (RGB565):

| State (on-screen text) | `t_mode1` background | Text |
|---|---|---|
| PRACA (work) | 11886 green | black |
| CZUWANIE (standby) | 64739 orange | black |
| HIBERNACJA (hibernation) | 15455 blue | white |
| WYM. GROTU (tip change) | 63011 yellow | black |
| WYLACZONA / BRAK KOLBY (off / no tip) | 21261 grey | white |
| PRZEGRZANIE / BLAD GRZANIA (over-temp. / heating fault) | 63942 red | white |

`t_cur1`: white = at setpoint, orange = heating, blue = cooling, red = fault, grey = off.
The palette is in `firmware/src/ui.h`.

## Troubleshooting

| Symptom | Cause |
|---|---|
| a field stays empty | wrong `objname`, missing font ID, `txt_maxl` too small (set 20), or `isbr = True` with a big font |
| `t_kp` / `t_ki` / `t_kd` show `0` | they are Number components – change them to Text |
| buttons do nothing | code in Touch Release instead of Press, wrong page byte (e.g. copied `65 01` on page 2), or “Send Component ID” ticked |
| values change but are invisible | `pco` is black on a dark background |
| screen looks stale after a new `.tft` | restart the Pico together with the display |
| check a button code | Editor → Debug → press the button; “Simulator Return” shows e.g. `65 02 03 01` |

The `nx` console command shows whether the Pico receives anything from the display, and every touch is logged
as `[TOUCH]`.
