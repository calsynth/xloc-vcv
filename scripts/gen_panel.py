#!/usr/bin/env python3
# Regenerate res/XLOC2.svg from the original XLOC2 aluminium-panel artwork.
#
# The original Illustrator/PDF export (panel.svg, produced with
# `pdftocairo -svg xloc2_alum_panel_01.ai panel.svg`) is parsed and its real
# vector glyphs (the XLOC 2 wordmark, calsynth bear logo, all labels and
# numerals) are re-placed onto a Rack-sized 22HP panel. The OLED aperture is
# enlarged as much as the flanking buttons allow while keeping the 128x64 2:1
# aspect. Geometry here must stay in sync with src/XLOC2.cpp.
#
# Usage: python3 scripts/gen_panel.py [path/to/panel.svg]
import sys
from svgelements import SVG, Shape, Path

SRC = sys.argv[1] if len(sys.argv) > 1 else "panel.svg"
PT2MM = 25.4 / 72.0

doc = SVG.parse(SRC, reify=True)
shapes, seen = [], set()
for e in doc.elements():
    if isinstance(e, Shape):
        p = e if isinstance(e, Path) else Path(e)
        if not len(p):
            continue
        bb = p.bbox()
        if not bb:
            continue
        key = (round(bb[0], 2), round(bb[1], 2), round(bb[2], 2), round(bb[3], 2),
               str(p.fill), p.d()[:50])
        if key in seen:
            continue
        seen.add(key)
        shapes.append((p, bb))


def grab(x0, y0, x1, y1):
    return [(p, b) for p, b in shapes
            if x0 <= (b[0] + b[2]) / 2 <= x1 and y0 <= (b[1] + b[3]) / 2 <= y1]


INK = "#1d1d1b"
out = []


def emit(items, tx, ty, scale=1.0, fill=None):
    if not items:
        print("WARN empty group at", tx, ty)
        return
    x0 = min(b[0] for _, b in items); y0 = min(b[1] for _, b in items)
    x1 = max(b[2] for _, b in items); y1 = max(b[3] for _, b in items)
    cx = (x0 + x1) / 2; cy = (y0 + y1) / 2
    for p, b in items:
        q = Path(p)
        q *= f"translate({-cx},{-cy})"
        q *= f"scale({PT2MM * scale})"
        q *= f"translate({tx},{ty})"
        q.reify()
        f = fill or (p.fill.hexrgb if p.fill and p.fill.value is not None else INK)
        out.append((q.d(), f))


# ---- target geometry (mm) — keep in sync with src/XLOC2.cpp ----
W, H = 111.76, 128.5                       # 22HP x 3U
SX0, SY0, SX1, SY1 = 12.0, 7.6, 99.76, 51.5   # screen 87.76 x 43.9 (2:1)
C = dict(usb=7.6, trig=23.0, cv1=39.0, cv2=52.0, co1=71.0, co2=84.0, aud=101.0)
R = [77.5, 89.5, 101.5, 113.5]
LO = 6.0
HDR = 68.6
ENCL, ENCR, ENCY = 22.0, 89.76, 58.5
ZX, ZY = 55.88, 57.0

emit(grab(250, 246, 340, 264), W / 2, 4.9, 1.12)     # XLOC 2 wordmark
emit(grab(248, 581, 338, 602), W / 2, 123.6, 1.15)   # calsynth logo
emit(grab(204, 292, 222, 304), 6.2, 20.8)            # A
emit(grab(362, 292, 380, 304), W - 6.2, 20.8)        # B
emit(grab(204, 335, 222, 347), 6.2, 48.8)            # X
emit(grab(362, 335, 380, 347), W - 6.2, 48.8)        # Y
emit(grab(284, 396, 300, 408), ZX, ZY + 7.2)         # Z
emit(grab(218, 424, 239, 434), C['trig'], HDR)
emit(grab(265, 424, 289, 434), (C['cv1'] + C['cv2']) / 2, HDR)
emit(grab(324, 424, 359, 434), (C['co1'] + C['co2']) / 2, HDR)
emit(grab(374, 424, 403, 434), C['aud'], HDR)
emit(grab(184, 450, 201, 460), C['usb'], R[0] + LO)              # USB
emit(grab(202, 424, 210, 446), C['usb'] + 5.6, R[0] - 2.4)       # DEV (vert.)
emit(grab(202, 470, 210, 494), C['usb'] + 5.8, (R[1] + R[2]) / 2 - 4.6)  # HOST
emit(grab(182, 535, 201, 545), C['usb'], R[2] + LO)              # MIDI
emit(grab(202, 516, 210, 528), C['usb'] + 5.4, R[2] - 0.6)       # IN
emit(grab(202, 548, 210, 566), C['usb'] + 5.6, R[3] - 0.6)       # OUT
numbox = dict(trig=(217, 233), cv1=(252, 268), cv2=(281, 297),
              co1=(316, 332), co2=(345, 361))
rowlab = [(464, 478), (499, 512), (534, 547), (568, 581)]
for col, (x0, x1) in numbox.items():
    for i, (y0, y1) in enumerate(rowlab):
        emit(grab(x0, y0, x1, y1), C[col], R[i] + LO)
emit(grab(372, 449, 381, 459), C['aud'] - 6.6, R[0])            # L
emit(grab(372, 483, 382, 493), C['aud'] - 6.6, R[1])            # R
emit(grab(372, 517, 381, 528), C['aud'] - 6.6, R[2])            # L
emit(grab(372, 551, 382, 562), C['aud'] - 6.6, R[3])            # R
emit(grab(386, 462, 398, 480), C['aud'] + 6.4, (R[0] + R[1]) / 2)  # IN (vert.)
emit(grab(380, 531, 402, 548), C['aud'] + 6.4, (R[2] + R[3]) / 2)  # OUT (vert.)
emit(grab(180, 266, 193, 280), 5.0, 12.4)                       # indicator dot
emit(grab(391, 266, 404, 280), W - 5.0, 12.4)

# jack holes (ports drawn on top by widgets)
holes = []
for i in range(4):
    holes.append((C['trig'], R[i]))
for i in range(4):
    holes += [(C['cv1'], R[i]), (C['cv2'], R[i])]
for i in range(4):
    holes += [(C['co1'], R[i]), (C['co2'], R[i])]
for i in range(4):
    holes.append((C['aud'], R[i]))
hsvg = "".join(f'<circle cx="{x:.2f}" cy="{y:.2f}" r="3.4" fill="#0c0c0c"/>'
               for x, y in holes)

# control mounting circles (widgets drawn on top)
ctl = (
    f'<circle cx="6.2" cy="14.5" r="3.6" fill="#161616"/>'
    f'<circle cx="{W-6.2}" cy="14.5" r="3.6" fill="#161616"/>'
    f'<circle cx="6.2" cy="42.5" r="3.6" fill="#161616"/>'
    f'<circle cx="{W-6.2}" cy="42.5" r="3.6" fill="#161616"/>'
    f'<circle cx="{ZX}" cy="{ZY}" r="3.6" fill="#161616"/>'
    f'<circle cx="{ENCL}" cy="{ENCY}" r="6.2" fill="#2a2a2a" stroke="#0c0c0c" stroke-width="0.5"/>'
    f'<circle cx="{ENCR}" cy="{ENCY}" r="6.2" fill="#2a2a2a" stroke="#0c0c0c" stroke-width="0.5"/>'
)
# decorative USB (DEV/HOST) + MIDI jacks (non-functional in VCV)
dec = (
    f'<rect x="{C["usb"]-1.9}" y="{R[0]-6.5}" width="3.8" height="9.5" rx="1.9" fill="#0c0c0c"/>'
    f'<rect x="{C["usb"]-2.6}" y="{(R[1]+R[2])/2-7.5}" width="5.2" height="15" rx="0.6" fill="#0c0c0c"/>'
    f'<circle cx="{C["usb"]:.2f}" cy="{R[2]:.2f}" r="3.4" fill="#0c0c0c"/>'
    f'<circle cx="{C["usb"]:.2f}" cy="{R[3]:.2f}" r="3.4" fill="#0c0c0c"/>'
)

body = "".join(f'<path d="{d}" fill="{f}"/>' for d, f in out)
svg = f'''<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">
<rect width="{W}" height="{H}" fill="#e9e6df"/>
<rect x="0" y="0" width="{W}" height="{H}" fill="none" stroke="#c9c5bd" stroke-width="0.5"/>
<rect x="{SX0}" y="{SY0}" width="{SX1-SX0:.2f}" height="{SY1-SY0:.2f}" rx="1.4" fill="#050505"/>
<rect x="{SX0-0.7:.2f}" y="{SY0-0.7:.2f}" width="{SX1-SX0+1.4:.2f}" height="{SY1-SY0+1.4:.2f}" rx="1.8" fill="none" stroke="#b9b5ad" stroke-width="0.5"/>
{hsvg}{ctl}{dec}
{body}
</svg>'''
import os
_here = os.path.dirname(os.path.abspath(__file__))
_dest = os.path.join(_here, "..", "res", "XLOC2.svg")
open(_dest, "w").write(svg)
print("wrote", os.path.normpath(_dest), "—", len(out), "glyph paths; screen",
      round(SX1 - SX0, 2), "x", round(SY1 - SY0, 2), "mm")
