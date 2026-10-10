"""Generates docs/wiring_diagram.drawio (open/edit in draw.io, export PNG to docs/images/).

Run:  python docs/gen_wiring_drawio.py
"""
import os
from xml.sax.saxutils import escape

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "wiring_diagram.drawio")

cells = []
_id = [1]


def nid():
    _id[0] += 1
    return f"c{_id[0]}"


BOX = ("rounded=1;whiteSpace=wrap;html=1;arcSize=6;fontFamily=Helvetica;fontSize=12;"
       "align=center;verticalAlign=middle;spacing=6;strokeWidth=1.5;")
COL = {
    "power": "fillColor=#fde2e2;strokeColor=#c0392b;",
    "mcu": "fillColor=#e3f2e1;strokeColor=#2e7d32;",
    "sense": "fillColor=#e1ecfb;strokeColor=#1f5fbf;",
    "ui": "fillColor=#fff3d6;strokeColor=#b7791f;",
    "iron": "fillColor=#efe3f7;strokeColor=#6a1b9a;",
    "unused": "fillColor=#f2f2f2;strokeColor=#9e9e9e;dashed=1;fontColor=#757575;",
    "gnd": "fillColor=#333333;strokeColor=#000000;fontColor=#ffffff;",
    "note": "fillColor=#fffbe6;strokeColor=#d4b106;align=left;verticalAlign=top;",
}


def box(x, y, w, h, html, kind):
    i = nid()
    cells.append(f'<mxCell id="{i}" value="{escape(html, {chr(34): "&quot;"})}" style="{BOX}{COL[kind]}" '
                 f'vertex="1" parent="1"><mxGeometry x="{x}" y="{y}" width="{w}" height="{h}" as="geometry"/></mxCell>')
    return i


WIRE = {
    "pwr": "strokeColor=#c0392b;strokeWidth=2;",
    "heat": "strokeColor=#c0392b;strokeWidth=4;",
    "gnd": "strokeColor=#000000;strokeWidth=3;",
    "sig": "strokeColor=#1f5fbf;strokeWidth=1.5;",
    "tc": "strokeColor=#6a1b9a;strokeWidth=1.5;",
    "unused": "strokeColor=#9e9e9e;strokeWidth=1.2;dashed=1;",
}


def wire(src, dst, label, kind, exit_=None, entry=None, points=None):
    i = nid()
    st = ("edgeStyle=orthogonalEdgeStyle;rounded=1;html=1;endArrow=none;fontSize=11;"
          "labelBackgroundColor=#ffffff;" + WIRE[kind])
    if exit_:
        st += f"exitX={exit_[0]};exitY={exit_[1]};exitDx=0;exitDy=0;"
    if entry:
        st += f"entryX={entry[0]};entryY={entry[1]};entryDx=0;entryDy=0;"
    cells.append(f'<mxCell id="{i}" value="{escape(label, {chr(34): "&quot;"})}" style="{st}" edge="1" '
                 f'parent="1" source="{src}" target="{dst}"><mxGeometry relative="1" as="geometry">'
                 + (('<Array as="points">' + "".join(f'<mxPoint x="{x}" y="{y}"/>' for x, y in points) + '</Array>')
                    if points else '') + '</mxGeometry></mxCell>')


# ---------------------------------------------------------------- blocks
title = box(40, 10, 1520, 44,
            "<b style='font-size:20px'>JBC C245 station – wiring</b>"
            "&nbsp;&nbsp;&nbsp;<span style='font-size:12px'>red = 24 V power · black = ground (star point) · "
            "blue = signals · purple = thermocouple · dashed = fitted, not used by the firmware</span>", "note")

psu = box(40, 90, 200, 80, "<b>24 V PSU</b><br>Omron, ≥ 150 W", "power")
vsens = box(40, 230, 200, 80, "<b>Voltage sensor</b><br>input voltage → GP26<br>(ratio V_DIVIDER_RATIO)", "power")
dc1 = box(40, 360, 200, 80, "<b>DC-DC #1</b><br>24 V → 5 V<br>Pico, Nextion, modules", "power")
dc2 = box(40, 490, 200, 80, "<b>DC-DC #2</b><br>24 V → LED voltage<br>button LEDs", "power")
uln = box(330, 420, 170, 80, "<b>ULN2803A</b><br>LED sink driver", "ui")
btns = box(330, 560, 170, 110, "<b>3 preset buttons</b><br>with LEDs<br>buttons to GND<br>LED cathodes → ULN", "ui")
star = box(40, 700, 200, 70, "<b>STAR GROUND POINT</b><br>one bolt / terminal", "gnd")

pico = box(620, 160, 300, 600,
           "<b style='font-size:16px'>Raspberry Pi Pico 2</b><br>(or Pico – same pinout)<br><br>"
           "<div style='text-align:left;font-family:monospace;font-size:11px;line-height:1.5'>"
           "pin 1 &nbsp;GP0 &nbsp;UART TX → Nextion RX<br>"
           "pin 2 &nbsp;GP1 &nbsp;UART RX ← Nextion TX<br>"
           "pin 4 &nbsp;GP2 &nbsp;LED 3 → ULN<br>"
           "pin 6 &nbsp;GP4 &nbsp;RTC SDA (spare)<br>"
           "pin 7 &nbsp;GP5 &nbsp;RTC SCL (spare)<br>"
           "pin 9 &nbsp;GP6 &nbsp;ADS1115 SDA<br>"
           "pin 10 GP7 &nbsp;ADS1115 SCL<br>"
           "pin 12-16 GP9-12 SD card (spare)<br>"
           "pin 22 GP17 heater PWM → RPWM<br>"
           "pin 24 GP18 button 1<br>"
           "pin 25 GP19 button 2<br>"
           "pin 26 GP20 button 3<br>"
           "pin 27 GP21 LED 1 → ULN<br>"
           "pin 29 GP22 LED 2 → ULN<br>"
           "pin 31 GP26 supply voltage (ADC0)<br>"
           "pin 32 GP27 stand sensor<br>"
           "pin 34 GP28 tip-change contact<br>"
           "pin 36 3V3 → ADS, BTS EN, pull-up<br>"
           "pin 39 VSYS ← 5 V<br>"
           "GND → star point</div>", "mcu")

bts = box(1060, 90, 220, 130,
          "<b>BTS7960 (IBT-2)</b><br>high-side switch<br>B+ ← 24 V · B− → star<br>M+ → heater · <b>M− n/c</b><br>"
          "RPWM ← GP17 · LPWM → GND<br>R_EN, L_EN, VCC ← 3.3 V", "power")
iron = box(1380, 90, 180, 150,
           "<b>T245 handle</b><br>C245 cartridge<br><br>blue = heater +<br>red = common<br>green = thermocouple",
           "iron")
prot = box(1310, 300, 200, 150,
           "<b>Input protection</b><br>green ─ 2.2 kΩ ─ A0<br>red (at socket) ─ 2.2 kΩ ─ A1<br>"
           "100 nF between A0 and A1<br>1 MΩ green → 3.3 V<br><i>(iron side of the 2.2 kΩ)</i>", "sense")
ads = box(1060, 320, 220, 110, "<b>ADS1115</b><br>VDD ← 3.3 V · GND → star<br>SDA/SCL ← GP6/GP7<br>ADDR → GND / open (0x48)",
          "sense")
nx = box(1060, 480, 220, 80, "<b>Nextion NX3224T028</b><br>2.8&quot; touch screen<br>5 V ← DC-DC #1", "ui")
stand = box(1060, 600, 220, 90, "<b>Stand</b><br>dock contact → GP27<br>tip-change contact → GP28<br>(closes to GND)",
            "iron")
rtc = box(620, 800, 140, 60, "<b>RTC module</b><br>spare", "unused")
sd = box(780, 800, 140, 60, "<b>microSD module</b><br>spare", "unused")

# ---------------------------------------------------------------- wires
wire(psu, bts, "+24 V", "heat", (1, 0.3), (0, 0.3))
wire(psu, vsens, "+24 V", "pwr", (0.5, 1), (0.5, 0))
wire(vsens, dc1, "", "pwr", (0.5, 1), (0.5, 0))
wire(dc1, dc2, "+24 V in", "pwr", (0.5, 1), (0.5, 0))
wire(psu, star, "0 V", "gnd", (0, 0.7), (0, 0.5))
wire(vsens, pico, "S → GP26", "sig", (1, 0.5), (0, 0.15))
wire(dc1, pico, "5 V → VSYS", "pwr", (1, 0.5), (0, 0.4))
wire(dc2, uln, "LED +", "pwr", (1, 0.5), (0, 0.5))
wire(uln, btns, "LED −", "sig", (0.5, 1), (0.5, 0))
wire(pico, uln, "GP21 / GP22 / GP2", "sig", (0, 0.6), (1, 0.5))
wire(btns, pico, "GP18 / GP19 / GP20", "sig", (1, 0.5), (0, 0.85))
wire(star, pico, "GND", "gnd", (1, 0.5), (0, 0.96))
wire(bts, iron, "M+ → blue", "heat", (1, 0.3), (0, 0.3))
wire(pico, bts, "GP17 → RPWM", "sig", (1, 0.12), (0, 0.75))
wire(iron, star, "red → star (thick wire)", "gnd", (0.95, 1), (0.5, 1), [(1551, 900), (140, 900)])
wire(iron, prot, "green + red", "tc", (0.3, 1), (0.66, 0))
wire(prot, ads, "A0 / A1", "tc", (0, 0.5), (1, 0.5))
wire(ads, pico, "SDA / SCL  GP6 / GP7", "sig", (0, 0.5), (1, 0.33))
wire(nx, pico, "TX/RX  GP1 / GP0", "sig", (0, 0.5), (1, 0.55))
wire(stand, pico, "GP27 / GP28", "sig", (0, 0.5), (1, 0.8))
wire(rtc, pico, "GP4 / GP5", "unused", (0.5, 0), (0.25, 1))
wire(sd, pico, "GP9–GP12", "unused", (0.5, 0), (0.75, 1))

note = box(1060, 730, 500, 100,
           "<b>Rules</b><br>• all grounds meet in one star point (PSU −, BTS B−, iron red, Pico, ADS)<br>"
           "• ADS1115 and BTS7960 logic from 3.3 V, never 5 V<br>"
           "• A1 taken with its own thin wire at the handle socket<br>"
           "• measure the green wire while heating before fitting the ADS1115", "note")

xml = ('<mxfile host="drawio"><diagram id="wiring" name="Wiring">'
       '<mxGraphModel dx="1600" dy="900" grid="1" gridSize="10" guides="1" tooltips="1" connect="1" arrows="1" '
       'fold="1" page="1" pageScale="1" pageWidth="1600" pageHeight="940" background="#ffffff" math="0" shadow="0">'
       '<root><mxCell id="0"/><mxCell id="1" parent="0"/>' + "".join(cells) +
       '</root></mxGraphModel></diagram></mxfile>')

with open(OUT, "w", encoding="utf8") as f:
    f.write(xml)
print("written", OUT)
