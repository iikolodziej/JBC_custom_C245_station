<!-- wygenerowane przez tools/gen_nextion_ui.py - nie edytuj ręcznie -->

Wspólne zasady:
- strona: `sta=image`, `pic` = numer tła strony
- `crop image`: `picc` = tło strony, przyciski dodatkowo `picc2` = tło wciśnięte
- przyciski: w Touch Press Event tylko linijka `printh`, bez `page`, odznacz „Send Component ID”
- czcionki: 0 = 16 px, 1 = 24 bold, 2 = 48 bold, 3 = 96 bold (kolejność dodania do projektu)
- `xcen`: 0 = do lewej, 1 = środek, 2 = do prawej; `ycen` = 1 (środek) dla wszystkich

### Strona 0 `main`  (tło: pic 0, wciśnięte: pic 1)

| objname | typ | x | y | w | h | sta | font | xcen | kolory i inne | Touch Press Event |
|---|---|---|---|---|---|---|---|---|---|---|
| `t_vin` | Text | 236 | 2 | 80 | 22 | crop image (picc=0) | 0 | 2 | kolor: Pico; `txt_maxl=20` |  |
| `t_cur1` | Text | 8 | 30 | 226 | 100 | crop image (picc=0) | 3 | 2 | kolor: Pico; `txt_maxl=20` |  |
| `t_set1` | Text | 8 | 134 | 146 | 24 | crop image (picc=0) | 0 | 0 | `pco=36020` `txt_maxl=20` |  |
| `t_mode1` | Text | 160 | 132 | 152 | 28 | **solid color** | 0 | 1 | kolor: Pico; `txt_maxl=20` | `printh 65 00 16 01` |
| `j_pwr1` | Progress bar | 12 | 164 | 296 | 10 | **solid color** | - | - | `bco=10631` (tło paska); kolor paska: Pico |  |
| `b_p1` | Button | 4 | 184 | 78 | 52 | **solid color** | 1 | 1 | kolor: Pico; `bco2=14890` `pco2=64451` | `printh 65 00 01 01` |
| `b_p2` | Button | 86 | 184 | 78 | 52 | **solid color** | 1 | 1 | kolor: Pico; `bco2=14890` `pco2=64451` | `printh 65 00 02 01` |
| `b_p3` | Button | 168 | 184 | 78 | 52 | **solid color** | 1 | 1 | kolor: Pico; `bco2=14890` `pco2=64451` | `printh 65 00 03 01` |
| `b_cfg` | Button | 250 | 184 | 66 | 52 | crop image (picc=0, picc2=1) | 0 | 1 | `txt` puste (napis jest na tle) | `printh 65 00 0A 01` |

### Strona 1 `settings`  (tło: pic 2, wciśnięte: pic 3)

| objname | typ | x | y | w | h | sta | font | xcen | kolory i inne | Touch Press Event |
|---|---|---|---|---|---|---|---|---|---|---|
| `b_back` | Button | 0 | 0 | 64 | 26 | crop image (picc=2, picc2=3) | 0 | 1 | `txt` puste (napis jest na tle) | `printh 65 01 15 01` |
| `b_prof1m` | Button | 176 | 30 | 40 | 27 | crop image (picc=2, picc2=3) | 0 | 1 | `txt` puste (napis jest na tle) | `printh 65 01 02 01` |
| `n_prof1` | Number | 218 | 30 | 56 | 27 | crop image (picc=2) | 1 | 1 | `pco=61310` |  |
| `b_prof1p` | Button | 276 | 30 | 40 | 27 | crop image (picc=2, picc2=3) | 0 | 1 | `txt` puste (napis jest na tle) | `printh 65 01 01 01` |
| `b_prof2m` | Button | 176 | 59 | 40 | 27 | crop image (picc=2, picc2=3) | 0 | 1 | `txt` puste (napis jest na tle) | `printh 65 01 04 01` |
| `n_prof2` | Number | 218 | 59 | 56 | 27 | crop image (picc=2) | 1 | 1 | `pco=61310` |  |
| `b_prof2p` | Button | 276 | 59 | 40 | 27 | crop image (picc=2, picc2=3) | 0 | 1 | `txt` puste (napis jest na tle) | `printh 65 01 03 01` |
| `b_prof3m` | Button | 176 | 88 | 40 | 27 | crop image (picc=2, picc2=3) | 0 | 1 | `txt` puste (napis jest na tle) | `printh 65 01 06 01` |
| `n_prof3` | Number | 218 | 88 | 56 | 27 | crop image (picc=2) | 1 | 1 | `pco=61310` |  |
| `b_prof3p` | Button | 276 | 88 | 40 | 27 | crop image (picc=2, picc2=3) | 0 | 1 | `txt` puste (napis jest na tle) | `printh 65 01 05 01` |
| `b_sleepm` | Button | 176 | 117 | 40 | 27 | crop image (picc=2, picc2=3) | 0 | 1 | `txt` puste (napis jest na tle) | `printh 65 01 08 01` |
| `n_sleep` | Number | 218 | 117 | 56 | 27 | crop image (picc=2) | 1 | 1 | `pco=61310` |  |
| `b_sleepp` | Button | 276 | 117 | 40 | 27 | crop image (picc=2, picc2=3) | 0 | 1 | `txt` puste (napis jest na tle) | `printh 65 01 07 01` |
| `b_hibm` | Button | 176 | 146 | 40 | 27 | crop image (picc=2, picc2=3) | 0 | 1 | `txt` puste (napis jest na tle) | `printh 65 01 0A 01` |
| `n_hib` | Number | 218 | 146 | 56 | 27 | crop image (picc=2) | 1 | 1 | `pco=61310` |  |
| `b_hibp` | Button | 276 | 146 | 40 | 27 | crop image (picc=2, picc2=3) | 0 | 1 | `txt` puste (napis jest na tle) | `printh 65 01 09 01` |
| `t_kp` | Text | 36 | 175 | 72 | 27 | crop image (picc=2) | 1 | 1 | `pco=61310` `txt_maxl=20` |  |
| `t_ki` | Text | 140 | 175 | 72 | 27 | crop image (picc=2) | 1 | 1 | `pco=61310` `txt_maxl=20` |  |
| `t_kd` | Text | 244 | 175 | 72 | 27 | crop image (picc=2) | 1 | 1 | `pco=61310` `txt_maxl=20` |  |
| `b_sys` | Button | 4 | 206 | 120 | 32 | crop image (picc=2, picc2=3) | 0 | 1 | `txt` puste (napis jest na tle) | `printh 65 01 0D 01` |
| `t_status` | Text | 126 | 206 | 98 | 32 | crop image (picc=2) | 0 | 1 | `pco=11886` `txt_maxl=20` |  |
| `b_save` | Button | 228 | 206 | 88 | 32 | crop image (picc=2, picc2=3) | 0 | 1 | `txt` puste (napis jest na tle) | `printh 65 01 14 01` |

### Strona 2 `system`  (tło: pic 4, wciśnięte: pic 5)

| objname | typ | x | y | w | h | sta | font | xcen | kolory i inne | Touch Press Event |
|---|---|---|---|---|---|---|---|---|---|---|
| `b_back` | Button | 0 | 0 | 64 | 26 | crop image (picc=4, picc2=5) | 0 | 1 | `txt` puste (napis jest na tle) | `printh 65 02 15 01` |
| `b_offm` | Button | 176 | 30 | 40 | 27 | crop image (picc=4, picc2=5) | 0 | 1 | `txt` puste (napis jest na tle) | `printh 65 02 02 01` |
| `t_off` | Text | 218 | 30 | 56 | 27 | crop image (picc=4) | 1 | 1 | `pco=61310` `txt_maxl=20` |  |
| `b_offp` | Button | 276 | 30 | 40 | 27 | crop image (picc=4, picc2=5) | 0 | 1 | `txt` puste (napis jest na tle) | `printh 65 02 01 01` |
| `b_pwrm` | Button | 176 | 59 | 40 | 27 | crop image (picc=4, picc2=5) | 0 | 1 | `txt` puste (napis jest na tle) | `printh 65 02 04 01` |
| `n_pwr` | Number | 218 | 59 | 56 | 27 | crop image (picc=4) | 1 | 1 | `pco=61310` |  |
| `b_pwrp` | Button | 276 | 59 | 40 | 27 | crop image (picc=4, picc2=5) | 0 | 1 | `txt` puste (napis jest na tle) | `printh 65 02 03 01` |
| `b_brim` | Button | 176 | 88 | 40 | 27 | crop image (picc=4, picc2=5) | 0 | 1 | `txt` puste (napis jest na tle) | `printh 65 02 06 01` |
| `n_bri` | Number | 218 | 88 | 56 | 27 | crop image (picc=4) | 1 | 1 | `pco=61310` |  |
| `b_brip` | Button | 276 | 88 | 40 | 27 | crop image (picc=4, picc2=5) | 0 | 1 | `txt` puste (napis jest na tle) | `printh 65 02 05 01` |
| `b_stepm` | Button | 176 | 117 | 40 | 27 | crop image (picc=4, picc2=5) | 0 | 1 | `txt` puste (napis jest na tle) | `printh 65 02 08 01` |
| `n_step` | Number | 218 | 117 | 56 | 27 | crop image (picc=4) | 1 | 1 | `pco=61310` |  |
| `b_stepp` | Button | 276 | 117 | 40 | 27 | crop image (picc=4, picc2=5) | 0 | 1 | `txt` puste (napis jest na tle) | `printh 65 02 07 01` |
| `b_tmaxm` | Button | 176 | 146 | 40 | 27 | crop image (picc=4, picc2=5) | 0 | 1 | `txt` puste (napis jest na tle) | `printh 65 02 0A 01` |
| `n_tmax` | Number | 218 | 146 | 56 | 27 | crop image (picc=4) | 1 | 1 | `pco=61310` |  |
| `b_tmaxp` | Button | 276 | 146 | 40 | 27 | crop image (picc=4, picc2=5) | 0 | 1 | `txt` puste (napis jest na tle) | `printh 65 02 09 01` |
| `t_hours` | Text | 176 | 175 | 140 | 27 | crop image (picc=4) | 1 | 1 | `pco=61310` `txt_maxl=20` |  |
| `t_live` | Text | 4 | 206 | 150 | 32 | crop image (picc=4) | 0 | 1 | `pco=64451` `txt_maxl=20` |  |
| `t_status` | Text | 156 | 206 | 68 | 32 | crop image (picc=4) | 0 | 1 | `pco=11886` `txt_maxl=20` |  |
| `b_save` | Button | 228 | 206 | 88 | 32 | crop image (picc=4, picc2=5) | 0 | 1 | `txt` puste (napis jest na tle) | `printh 65 02 14 01` |
