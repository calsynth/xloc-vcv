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
# No USB/MIDI column (those ports don't exist in VCV); the six functional jack
# columns are spread symmetrically about the panel centre (55.88). The title
# gets a clear strip above the screen and the controls sit off the screen edge
# with margin, so nothing crowds the aperture.
W, H = 111.76, 128.5                       # 22HP x 3U
SX0, SY0, SX1, SY1 = 17.88, 11.0, 93.88, 49.0   # screen 76 x 38 (2:1)
C = dict(trig=14.5, cv1=32.0, cv2=45.0, co1=66.76, co2=79.76, aud=97.26)
R = [77.5, 89.5, 101.5, 113.5]
LO = 6.0
HDR = 68.6
BXL, BXR = 8.5, W - 8.5                     # button columns (flank screen)
BY_TOP, BY_BOT = 19.0, 41.0                 # button rows
ENCL, ENCR, ENCY = 22.0, 89.76, 58.5
ZX, ZY = 55.88, 57.5

emit(grab(250, 246, 340, 264), W / 2, 7.2, 1.0)      # XLOC 2 wordmark
emit(grab(248, 581, 338, 602), W / 2, 123.6, 1.15)   # calsynth logo
emit(grab(204, 292, 222, 304), BXL, BY_TOP + 6.4)    # A
emit(grab(362, 292, 380, 304), BXR, BY_TOP + 6.4)    # B
emit(grab(204, 335, 222, 347), BXL, BY_BOT + 6.4)    # X
emit(grab(362, 335, 380, 347), BXR, BY_BOT + 6.4)    # Y
emit(grab(284, 396, 300, 408), ZX, ZY + 7.0)         # Z
emit(grab(218, 424, 239, 434), C['trig'], HDR)
emit(grab(265, 424, 289, 434), (C['cv1'] + C['cv2']) / 2, HDR)
emit(grab(324, 424, 359, 434), (C['co1'] + C['co2']) / 2, HDR)
emit(grab(374, 424, 403, 434), C['aud'], HDR)
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
    f'<circle cx="{BXL}" cy="{BY_TOP}" r="3.6" fill="#161616"/>'
    f'<circle cx="{BXR}" cy="{BY_TOP}" r="3.6" fill="#161616"/>'
    f'<circle cx="{BXL}" cy="{BY_BOT}" r="3.6" fill="#161616"/>'
    f'<circle cx="{BXR}" cy="{BY_BOT}" r="3.6" fill="#161616"/>'
    f'<circle cx="{ZX}" cy="{ZY}" r="3.6" fill="#161616"/>'
    f'<circle cx="{ENCL}" cy="{ENCY}" r="5.6" fill="#2a2a2a" stroke="#0c0c0c" stroke-width="0.5"/>'
    f'<circle cx="{ENCR}" cy="{ENCY}" r="5.6" fill="#2a2a2a" stroke="#0c0c0c" stroke-width="0.5"/>'
)

body = "".join(f'<path d="{d}" fill="{f}"/>' for d, f in out)
svg = f'''<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">
<rect width="{W}" height="{H}" fill="#e9e6df"/>
<rect x="0" y="0" width="{W}" height="{H}" fill="none" stroke="#c9c5bd" stroke-width="0.5"/>
<rect x="{SX0}" y="{SY0}" width="{SX1-SX0:.2f}" height="{SY1-SY0:.2f}" rx="1.4" fill="#050505"/>
<rect x="{SX0-0.7:.2f}" y="{SY0-0.7:.2f}" width="{SX1-SX0+1.4:.2f}" height="{SY1-SY0+1.4:.2f}" rx="1.8" fill="none" stroke="#b9b5ad" stroke-width="0.5"/>
{hsvg}{ctl}
{body}
</svg>'''
import os
_here = os.path.dirname(os.path.abspath(__file__))
_dest = os.path.join(_here, "..", "res", "XLOC2.svg")
open(_dest, "w").write(svg)
print("wrote", os.path.normpath(_dest), "—", len(out), "glyph paths; screen",
      round(SX1 - SX0, 2), "x", round(SY1 - SY0, 2), "mm")
