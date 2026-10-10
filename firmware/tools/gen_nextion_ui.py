#!/usr/bin/env python3
"""
Generator grafiki UI dla Nextion NX3224T028/K028 (320x240) - stacja JBC C245.

Tworzy w "display/ui_1kolba/":
  pic0_main.png      pic1_main_pressed.png
  pic2_settings.png  pic3_settings_pressed.png
  pic4_system.png    pic5_system_pressed.png
  preview_*.png      - podgląd z przykładowymi danymi (tylko do oglądania)
  components.md      - tabela komponentów (nazwy, pozycje, kod zdarzeń)

Jedno źródło prawdy: tabela COMPONENTS poniżej. Nazwy i ID zdarzeń muszą
się zgadzać z src/ui.h (firmware).

Użycie:  python tools/gen_nextion_ui.py
Wymaga:  pip install pillow
"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

W, H = 320, 240
ROOT = Path(__file__).resolve().parents[2]
OUT_BASE = ROOT / "display" if (ROOT / "display").exists() else ROOT / "projekt nextion"
LAYOUTS = [(1, "ui_1kolba")]   # układ na 2 kolby: dopisz (2, "ui_2kolby")

# ---------------------------------------------------------------- paleta
# Te same wartości co CLR_* w src/ui.h
BG        = (0x0B, 0x0F, 0x14)
PANEL     = (0x16, 0x1C, 0x24)
PANEL_HDR = (0x23, 0x2B, 0x36)
BTN       = (0x2A, 0x33, 0x3F)
BTN_PRESS = (0x3A, 0x46, 0x55)
ACCENT    = (0xFF, 0x7A, 0x1A)
TEXT      = (0xE8, 0xED, 0xF2)
TEXT_DIM  = (0x8A, 0x96, 0xA3)
GREEN     = (0x2E, 0xCC, 0x71)
ORANGE    = (0xFF, 0x9F, 0x1A)
BLUE      = (0x3D, 0x8B, 0xFF)
GRAY      = (0x56, 0x60, 0x6B)
BLACK     = (0, 0, 0)


def rgb565(c):
    r, g, b = c
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


# ---------------------------------------------------------------- czcionki
FONT_DIR = Path("C:/Windows/Fonts")


def font(size, bold=False):
    name = "arialbd.ttf" if bold else "arial.ttf"
    for p in (FONT_DIR / name, Path("/usr/share/fonts/truetype/dejavu/DejaVuSans%s.ttf" % ("-Bold" if bold else ""))):
        if p.exists():
            return ImageFont.truetype(str(p), size)
    return ImageFont.load_default()


# Odpowiedniki czcionek Nextion (Font Generator, Arial, kodowanie iso-8859-2):
#   font 0 = 16 px,  font 1 = 24 px bold,  font 2 = 48 px bold,  font 3 = 96 px bold
NX_FONTS = {0: font(15), 1: font(21, True), 2: font(46, True), 3: font(92, True)}
F_LABEL = font(13)
F_LABEL_B = font(13, True)
F_TITLE = font(15, True)
F_ICON = font(26, True)

# ---------------------------------------------------------------- komponenty
# type: text | number | button | progress | hotspot
# sta:  crop  = "crop image" (przezroczysty, tło z obrazka strony)
#       solid = "solid color" (kolor ustawia firmware: bco/pco)
# align: 0=lewo 1=środek 2=prawo (xcen)
# ev: ID w printh 65 <page> <ev> 01 (Touch Press Event)
C = []


def comp(page, name, typ, x, y, w, h, sta="crop", font_id=0, align=1, ev=None, txt="", note=""):
    C.append(dict(page=page, name=name, type=typ, x=x, y=y, w=w, h=h, sta=sta,
                  font=font_id, align=align, ev=ev, txt=txt, note=note))


ROW_Y0, ROW_H = 30, 29
SET_ROWS = [
    ("prof1", "Profil 1  [°C]", 1, 2, "300"),
    ("prof2", "Profil 2  [°C]", 3, 4, "360"),
    ("prof3", "Profil 3  [°C]", 5, 6, "450"),
    ("sleep", "Hibernacja po  [s]", 7, 8, "10", "od odłożenia do stojaka"),
    ("hib", "Temp. hibernacji  [°C]", 9, 10, "150", "gdy odłożona"),
]
PID_PREVIEW_Y = ROW_Y0 + 5 * ROW_H     # wiersz z podglądem Kp / Ki / Kd (tylko odczyt)
SYS_ROWS = [
    ("off", "Korekta temp.  [°C]", 1, 2, "+0", "text", "kalibracja pomiaru"),
    ("pwr", "Limit mocy  [%]", 3, 4, "100", "number", "10-100"),
    ("bri", "Jasność ekranu  [%]", 5, 6, "80", "number"),
    ("step", "Krok nastawy  [°C]", 7, 8, "5", "number", "1/2/5/10/20/50"),
    ("tmax", "Maks. temp. grota  [°C]", 9, 10, "450", "number", "200-480"),
]
NCH = 1     # liczba kolb w bieżącym układzie (ustawiane w define_components)


def define_main_1():
    """Ekran główny - jedna kolba, duża temperatura (font 3 = 96 px)."""
    comp(0, "t_vin", "text", 236, 2, 80, 22, align=2, txt="24.0V", note="napięcie zasilania")
    comp(0, "t_cur1", "text", 8, 30, 226, 100, font_id=3, align=2, txt="---",
         note="temperatura, kolor = stan")
    comp(0, "t_set1", "text", 8, 134, 146, 24, align=0, txt="SET 300", note="nastawa")
    comp(0, "t_mode1", "text", 160, 132, 152, 28, sta="solid", align=1, ev=22, txt="---",
         note="tryb; dotyk = wł./wył. kolby")
    comp(0, "j_pwr1", "progress", 12, 164, 296, 10, sta="solid", note="moc 0-100%")


def define_main_2():
    """Ekran główny - dwie kolby obok siebie."""
    comp(0, "t_vin", "text", 236, 2, 80, 22, align=2, txt="24.0V", note="napięcie zasilania")
    for i, px in enumerate((4, 162)):
        n = i + 1
        comp(0, f"t_name{n}", "text", px, 28, 154, 22, sta="solid", align=1, txt=f"KOLBA {n}",
             note="tło = wybrana kolba (firmware)")
        comp(0, f"t_cur{n}", "text", px + 4, 52, 108, 52, font_id=2, align=2, txt="---",
             note="temperatura, kolor = stan")
        comp(0, f"t_set{n}", "text", px + 4, 104, 146, 20, align=1, txt="SET 300", note="nastawa")
        comp(0, f"j_pwr{n}", "progress", px + 8, 128, 138, 10, sta="solid", note="moc 0-100%")
        comp(0, f"t_mode{n}", "text", px + 4, 144, 146, 30, sta="solid", align=1, ev=22 + i, txt="---",
             note="tryb; dotyk = wł./wył. kolby")


def define_components(nch):
    global NCH
    NCH = nch
    C.clear()

    # ===== STRONA 0: main =====
    (define_main_1 if nch == 1 else define_main_2)()
    comp(0, "b_p1", "button", 4, 184, 78, 52, sta="solid", font_id=1, ev=1, txt="300", note="profil 1")
    comp(0, "b_p2", "button", 86, 184, 78, 52, sta="solid", font_id=1, ev=2, txt="360", note="profil 2")
    comp(0, "b_p3", "button", 168, 184, 78, 52, sta="solid", font_id=1, ev=3, txt="450", note="profil 3")
    comp(0, "b_cfg", "button", 250, 184, 66, 52, ev=10, note="ustawienia")
    if nch == 2:
        # Hotspoty dodaj NA KOŃCU (najwyższe ID = na wierzchu)
        comp(0, "m_ch1", "hotspot", 4, 28, 154, 96, ev=20, note="wybór kolby 1")
        comp(0, "m_ch2", "hotspot", 162, 28, 154, 96, ev=21, note="wybór kolby 2")

    # ===== STRONA 1: ustawienia =====
    comp(1, "b_back", "button", 0, 0, 64, 26, ev=21, note="powrót do ekranu głównego")
    for r, (key, label, ev_up, ev_dn, sample, *_) in enumerate(SET_ROWS):
        y = ROW_Y0 + r * ROW_H
        comp(1, f"b_{key}m", "button", 176, y, 40, 27, ev=ev_dn, note=f"{label} -")
        comp(1, f"n_{key}", "number", 218, y, 56, 27, font_id=1, align=1, txt=sample, note=label)
        comp(1, f"b_{key}p", "button", 276, y, 40, 27, ev=ev_up, note=f"{label} +")
    for key, x in (("kp", 36), ("ki", 140), ("kd", 244)):
        comp(1, f"t_{key}", "text", x, PID_PREVIEW_Y, 72, 27, font_id=1, align=1,
             txt={"kp": "15.0", "ki": "2.50", "kd": "8.0"}[key], note=f"podgląd {key.upper()} (Text!)")
    comp(1, "b_sys", "button", 4, 206, 120, 32, ev=13, note="strona system")
    comp(1, "t_status", "text", 126, 206, 98, 32, align=1, txt="", note="komunikat ZAPISANO")
    comp(1, "b_save", "button", 228, 206, 88, 32, ev=20, note="zapis do flash")

    # ===== STRONA 2: system =====
    comp(2, "b_back", "button", 0, 0, 64, 26, ev=21, note="powrót do ustawień")
    for r, (key, label, ev_up, ev_dn, sample, typ, *_) in enumerate(SYS_ROWS):
        y = ROW_Y0 + r * ROW_H
        prefix = "t" if typ == "text" else "n"
        comp(2, f"b_{key}m", "button", 176, y, 40, 27, ev=ev_dn, note=f"{label} -")
        comp(2, f"{prefix}_{key}", typ, 218, y, 56, 27, font_id=1, align=1, txt=sample, note=label)
        comp(2, f"b_{key}p", "button", 276, y, 40, 27, ev=ev_up, note=f"{label} +")
    comp(2, "t_hours", "text", 176, ROW_Y0 + 5 * ROW_H, 140, 27, font_id=1, align=1,
         txt="12 h 05 min", note="licznik czasu grzania")
    comp(2, "t_live", "text", 4, 206, 150, 32, align=1, txt="T=357/360 P=45%",
         note="podgląd temperatury i mocy")
    comp(2, "t_status", "text", 156, 206, 68, 32, align=1, txt="", note="komunikat")
    comp(2, "b_save", "button", 228, 206, 88, 32, ev=20, note="zapis do flash")


def comps(page):
    return [c for c in C if c["page"] == page]


# ---------------------------------------------------------------- rysowanie
def rrect(d, box, fill, r=6, outline=None, width=1):
    d.rounded_rectangle(box, radius=r, fill=fill, outline=outline, width=width)


def text_in(d, box, s, f, fill, align=1):
    x0, y0, x1, y1 = box
    bb = d.textbbox((0, 0), s, font=f)
    tw, th = bb[2] - bb[0], bb[3] - bb[1]
    if align == 0:
        x = x0 + 4
    elif align == 2:
        x = x1 - tw - 4
    else:
        x = x0 + (x1 - x0 - tw) / 2
    y = y0 + (y1 - y0 - th) / 2 - bb[1]
    d.text((x, y), s, font=f, fill=fill)


def box_of(c):
    return (c["x"], c["y"], c["x"] + c["w"] - 1, c["y"] + c["h"] - 1)


def draw_gear(d, cx, cy, r, color):
    import math
    for k in range(8):
        a = k * math.pi / 4
        x, y = cx + math.cos(a) * r, cy + math.sin(a) * r
        d.ellipse((x - 3.5, y - 3.5, x + 3.5, y + 3.5), fill=color)
    d.ellipse((cx - r + 1, cy - r + 1, cx + r - 1, cy + r - 1), fill=color)
    d.ellipse((cx - r / 2.6, cy - r / 2.6, cx + r / 2.6, cy + r / 2.6), fill=BTN)


def button_face(d, c, pressed, label=None, f=F_LABEL_B, color=TEXT, icon=None):
    """Tło przycisku typu crop - rysowane na obrazku strony."""
    fill = BTN_PRESS if pressed else BTN
    rrect(d, box_of(c), fill, r=6, outline=ACCENT if pressed else None, width=2)
    cx, cy = c["x"] + c["w"] / 2, c["y"] + c["h"] / 2
    col = ACCENT if pressed else TEXT
    if label in ("−", "+"):          # rysowane kształtem - równo wyśrodkowane
        a = 7 if c["h"] < 40 else 9
        d.rectangle((cx - a, cy - 1.5, cx + a, cy + 1.5), fill=col)
        if label == "+":
            d.rectangle((cx - 1.5, cy - a, cx + 1.5, cy + a), fill=col)
        return
    if icon == "gear":
        draw_gear(d, c["x"] + c["w"] / 2, c["y"] + c["h"] / 2, 10, ACCENT if pressed else TEXT)
        d.ellipse((c["x"] + c["w"] / 2 - 4, c["y"] + c["h"] / 2 - 4,
                   c["x"] + c["w"] / 2 + 4, c["y"] + c["h"] / 2 + 4), fill=fill)
    elif label:
        text_in(d, box_of(c), label, f, ACCENT if pressed else color)


def by_name(page, name):
    return next(c for c in C if c["page"] == page and c["name"] == name)


def header(d, title):
    d.rectangle((0, 0, W, 25), fill=PANEL)
    d.line((0, 26, W, 26), fill=PANEL_HDR)
    text_in(d, (60, 0, 260, 25), title, F_TITLE, TEXT)


def draw_main(pressed):
    im = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(im)
    d.text((8, 5), "JBC", font=F_TITLE, fill=ACCENT)
    d.text((42, 5), "C245  STATION", font=F_TITLE, fill=TEXT)
    d.text((196, 7), "Vin", font=F_LABEL, fill=TEXT_DIM)
    if NCH == 1:
        rrect(d, (4, 28, 315, 179), PANEL, r=8)
        d.text((240, 44), "°C", font=font(26, True), fill=TEXT_DIM)
    else:
        for px in (4, 162):
            rrect(d, (px, 28, px + 153, 177), PANEL, r=6)
            d.text((px + 116, 60), "°C", font=font(18, True), fill=TEXT_DIM)
    button_face(d, by_name(0, "b_cfg"), pressed, icon="gear")
    # tła pod przyciski profili (solid - tylko dla spójności w podglądzie)
    for n in (1, 2, 3):
        rrect(d, box_of(by_name(0, f"b_p{n}")), BTN, r=6)
    return im


def draw_rows(d, page, rows, pressed):
    for r, row in enumerate(rows):
        key, label = row[0], row[1]
        y = ROW_Y0 + r * ROW_H
        if r % 2 == 0:
            d.rectangle((0, y - 1, W, y + ROW_H - 2), fill=PANEL)
        extra = [x for x in row[5:] if x not in ("text", "number")]
        note = extra[0] if extra else None
        d.text((8, y + (1 if note else 6)), label, font=F_LABEL, fill=TEXT)
        if note:
            d.text((8, y + 16), note, font=font(10), fill=TEXT_DIM)
        button_face(d, by_name(page, f"b_{key}m"), pressed, "−", F_ICON)
        button_face(d, by_name(page, f"b_{key}p"), pressed, "+", F_ICON)
        vb = [c for c in comps(page) if c["name"] in (f"n_{key}", f"t_{key}")][0]
        rrect(d, box_of(vb), BG, r=4, outline=PANEL_HDR)


def draw_settings(pressed):
    im = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(im)
    header(d, "USTAWIENIA")
    button_face(d, by_name(1, "b_back"), pressed, "‹ WRÓĆ")
    draw_rows(d, 1, SET_ROWS, pressed)
    y = PID_PREVIEW_Y
    d.rectangle((0, y - 1, W, y + ROW_H - 2), fill=PANEL)
    for lab, key in (("Kp", "kp"), ("Ki", "ki"), ("Kd", "kd")):
        c = by_name(1, f"t_{key}")
        text_in(d, (c["x"] - 30, y, c["x"] - 2, y + ROW_H - 2), lab, F_LABEL_B, TEXT_DIM, 2)
        rrect(d, box_of(c), BG, r=4, outline=PANEL_HDR)
    button_face(d, by_name(1, "b_sys"), pressed, "SYSTEM ›")
    button_face(d, by_name(1, "b_save"), pressed, "ZAPISZ", color=ACCENT)
    return im


def draw_system(pressed):
    im = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(im)
    header(d, "SYSTEM")
    button_face(d, by_name(2, "b_back"), pressed, "‹ WRÓĆ")
    draw_rows(d, 2, SYS_ROWS, pressed)
    y = ROW_Y0 + 5 * ROW_H
    d.rectangle((0, y - 1, W, y + ROW_H - 2), fill=PANEL)
    d.text((8, y + 1), "Czas grzania", font=F_LABEL, fill=TEXT)
    d.text((8, y + 16), "licznik godzin pracy grota", font=font(10), fill=TEXT_DIM)
    rrect(d, box_of(by_name(2, "t_hours")), BG, r=4, outline=PANEL_HDR)
    rrect(d, box_of(by_name(2, "t_live")), PANEL, r=4)
    button_face(d, by_name(2, "b_save"), pressed, "ZAPISZ", color=ACCENT)
    return im


# ---------------------------------------------------------------- podgląd
def render_dynamic(im, page, state):
    """Nakłada przykładowe wartości tak, jak zrobi to firmware."""
    d = ImageDraw.Draw(im)
    for c in comps(page):
        if c["type"] == "hotspot":
            continue
        st = state.get(c["name"], {})
        txt = st.get("txt", c["txt"])
        f = NX_FONTS[c["font"]]
        if c["type"] == "progress":
            val = st.get("val", 0)
            d.rectangle(box_of(c), fill=BTN)
            x1 = c["x"] + int(c["w"] * val / 100)
            if val:
                d.rectangle((c["x"], c["y"], x1, c["y"] + c["h"] - 1), fill=st.get("pco", ACCENT))
            continue
        if c["sta"] == "solid":
            rrect(d, box_of(c), st.get("bco", BTN), r=5 if c["type"] == "button" else 3)
        if txt:
            text_in(d, box_of(c), txt, f, st.get("pco", TEXT), c["align"])
    return im


PREVIEW_MAIN = {
    "t_vin": {"txt": "23.9V", "pco": TEXT_DIM},
    "t_name1": {"bco": ACCENT, "pco": BLACK},
    "t_cur1": {"txt": "358", "pco": TEXT},
    "t_set1": {"txt": "SET 360", "pco": TEXT_DIM},
    "j_pwr1": {"val": 38},
    "t_mode1": {"txt": "PRACA", "bco": GREEN, "pco": BLACK},
    "t_name2": {"bco": PANEL_HDR, "pco": TEXT_DIM},
    "t_cur2": {"txt": "151", "pco": BLUE},
    "t_set2": {"txt": "SET 150 (300)", "pco": TEXT_DIM},
    "j_pwr2": {"val": 6},
    "t_mode2": {"txt": "HIBERNACJA", "bco": BLUE, "pco": TEXT},
    "b_p1": {"bco": BTN, "pco": TEXT},
    "b_p2": {"bco": ACCENT, "pco": BLACK},
    "b_p3": {"bco": BTN, "pco": TEXT},
}
PREVIEW_SETTINGS = {"n_sleep": {"txt": "10"}, "t_status": {"txt": "ZAPISANO", "pco": GREEN}}
PREVIEW_SYSTEM = {"t_live": {"pco": ACCENT}, "t_status": {"txt": "ZAPISANO", "pco": GREEN}}


# ---------------------------------------------------------------- mapa nazw
MAP_COLORS = {"button": (255, 214, 0), "text": (0, 229, 255), "number": (118, 255, 3),
              "progress": (255, 64, 129), "hotspot": (255, 255, 255)}


def render_names(im, page):
    """Obrazek 3x z ramkami komponentów i ich nazwami (objname) - ściąga do edytora."""
    S = 3
    big = im.resize((W * S, H * S), Image.NEAREST).convert("RGB")
    dark = Image.new("RGB", big.size, (0, 0, 0))
    big = Image.blend(big, dark, 0.45)
    d = ImageDraw.Draw(big)
    f = font(15, True)
    for c in comps(page):
        col = MAP_COLORS[c["type"]]
        x0, y0 = c["x"] * S, c["y"] * S
        x1, y1 = (c["x"] + c["w"]) * S - 1, (c["y"] + c["h"]) * S - 1
        d.rectangle((x0, y0, x1, y1), outline=col, width=3)
        label = c["name"]
        bb = d.textbbox((0, 0), label, font=f)
        tw, th = bb[2] - bb[0], bb[3] - bb[1]
        cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
        d.rectangle((cx - tw / 2 - 4, cy - th / 2 - 4, cx + tw / 2 + 4, cy + th / 2 + 5), fill=(0, 0, 0))
        d.text((cx - tw / 2, cy - th / 2 - bb[1]), label, font=f, fill=col)
    # legenda
    y = H * S - 22
    x = 8
    for typ, name in (("button", "Button"), ("text", "Text"), ("number", "Number"), ("progress", "Progress bar")):
        if any(c["type"] == typ for c in comps(page)):
            d.rectangle((x, y, x + 14, y + 14), fill=MAP_COLORS[typ])
            d.text((x + 20, y - 1), name, font=f, fill=(255, 255, 255))
            x += 40 + d.textbbox((0, 0), name, font=f)[2]
    return big


# ---------------------------------------------------------------- tabela
TYPE_PL = {"text": "Text", "number": "Number", "button": "Button", "progress": "Progress bar",
           "hotspot": "Hotspot"}
PAGE_NAMES = {0: "main", 1: "settings", 2: "system"}


# Pola, których kolor ustawia firmware (nie ustawiaj pco/bco na stałe)
PICO_COLORED = {"t_vin", "t_cur1", "t_cur2", "t_mode1", "t_mode2", "t_name1", "t_name2",
                "b_p1", "b_p2", "b_p3", "j_pwr1", "j_pwr2"}
FIXED_PCO = {"t_set1": TEXT_DIM, "t_set2": TEXT_DIM, "t_live": ACCENT, "t_status": GREEN}


def extra_settings(c):
    """Kolory i inne atrybuty do ustawienia ręcznie w Nextion Editor."""
    n, t = c["name"], c["type"]
    if t == "hotspot":
        return "-"
    if n.startswith("b_p") and n[3:].isdigit():
        return f"kolor: Pico; `bco2={rgb565(BTN_PRESS)}` `pco2={rgb565(ACCENT)}`"
    if t == "progress":
        return f"`bco={rgb565(BTN)}` (tło paska); kolor paska: Pico"
    if n in PICO_COLORED:
        return "kolor: Pico" + ("; `txt_maxl=20`" if t == "text" else "")
    if t == "button":
        return "`txt` puste (napis jest na tle)"
    pco = FIXED_PCO.get(n, TEXT)
    out = f"`pco={rgb565(pco)}`"
    if t == "text":
        out += " `txt_maxl=20`"
    return out


def components_md():
    """Tabela komponentów do przewodnika HMI (pozycje pasują do wygenerowanych teł)."""
    out = ["<!-- wygenerowane przez tools/gen_nextion_ui.py - nie edytuj ręcznie -->",
           "",
           "Wspólne zasady:",
           "- strona: `sta=image`, `pic` = numer tła strony",
           "- `crop image`: `picc` = tło strony, przyciski dodatkowo `picc2` = tło wciśnięte",
           "- przyciski: w Touch Press Event tylko linijka `printh`, bez `page`, odznacz „Send Component ID”",
           "- czcionki: 0 = 16 px, 1 = 24 bold, 2 = 48 bold, 3 = 96 bold (kolejność dodania do projektu)",
           "- `xcen`: 0 = do lewej, 1 = środek, 2 = do prawej; `ycen` = 1 (środek) dla wszystkich"]
    for page in (0, 1, 2):
        pic, pic2 = page * 2, page * 2 + 1
        out.append(f"\n### Strona {page} `{PAGE_NAMES[page]}`  (tło: pic {pic}, wciśnięte: pic {pic2})\n")
        out.append("| objname | typ | x | y | w | h | sta | font | xcen | kolory i inne | Touch Press Event |")
        out.append("|---|---|---|---|---|---|---|---|---|---|---|")
        for c in comps(page):
            if c["type"] == "hotspot":
                sta = "-"
            elif c["sta"] == "crop":
                sta = f"crop image (picc={pic}" + (f", picc2={pic2})" if c["type"] == "button" else ")")
            else:
                sta = "**solid color**"
            ev = f"`printh 65 {page:02X} {c['ev']:02X} 01`" if c["ev"] is not None else ""
            fnt = "-" if c["type"] in ("progress", "hotspot") else c["font"]
            xc = "-" if c["type"] in ("progress", "hotspot") else c["align"]
            out.append(f"| `{c['name']}` | {TYPE_PL[c['type']]} | {c['x']} | {c['y']} | {c['w']} | {c['h']} | "
                       f"{sta} | {fnt} | {xc} | {extra_settings(c)} | {ev} |")
    return "\n".join(out) + "\n"


def main():
    pages = [("main", draw_main, PREVIEW_MAIN), ("settings", draw_settings, PREVIEW_SETTINGS),
             ("system", draw_system, PREVIEW_SYSTEM)]
    for nch, folder in LAYOUTS:
        define_components(nch)
        out = OUT_BASE / folder
        out.mkdir(parents=True, exist_ok=True)
        for idx, (name, fn, preview) in enumerate(pages):
            fn(False).save(out / f"pic{idx * 2}_{name}.png")
            fn(True).save(out / f"pic{idx * 2 + 1}_{name}_pressed.png")
            prev = render_dynamic(fn(False), idx, preview)
            prev.resize((W * 2, H * 2), Image.NEAREST).save(out / f"preview_{name}.png")
            render_names(fn(False), idx).save(out / f"nazwy_{name}.png")
        (out / "components.md").write_text(components_md(), encoding="utf-8")
        print(f"Zapisano ({nch} kolb.): {out}")


if __name__ == "__main__":
    main()
