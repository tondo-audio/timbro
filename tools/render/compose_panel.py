"""Bake static typography and decorations onto panel_base.png.

Reads:  resources/ui/panel_base.png  (Blender brass+tolex render, 920x1200)
Writes: resources/ui/panel.png       (final panel with logo, labels, scale, screws)

Static elements baked here (no need to render at runtime):
- "TIMBRO" script logo (top centre)
- "NEURAL AMP MODELER" subtitle
- 0..10 scale numbers around the main dial
- "INPUT" / "BYPASS" / "OUTPUT" labels in the bottom row
- Four decorative screws at the faceplate corners
- A subtle "TONDO AUDIO" maker stamp at the bottom

JUCE still renders dynamically: the rotating knobs (filmstrip), the zone-strip
lamps, the active zone name, and the bypass toggle lamp.
"""

import os
import math
from PIL import Image, ImageDraw, ImageFont, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.abspath(os.path.join(HERE, "..", "..", "resources", "ui", "panel_base.png"))
DST = os.path.abspath(os.path.join(HERE, "..", "..", "resources", "ui", "panel.png"))

# Plugin window is 720x280 logical (horizontal amp-head); 2x retina = 1440x560.
W, H = 1440, 560
SCALE = 2
WIN_W = 720
WIN_H = 280

# =============================================================================
# Boutique amp-head layout — 4 controls in a single horizontal row:
# INPUT knob, TONE knob (with 0..10 scale), OUTPUT knob, BYPASS toggle switch.
# All coordinates are in logical pixels; s() scales to retina.
# =============================================================================

# Title band — single line "Timbro" at top, no subtitle
LOGO_Y = 42

# Control row: 4 elements equally spaced, all centred on the same Y
CTRL_DIAMETER = 156              # logical px (same for knobs and switch)
CTRL_RADIUS   = CTRL_DIAMETER // 2
CTRL_ROW_CY   = 148
# Order, left → right: BYPASS / INPUT / OUTPUT / VOICE (main)
CTRL_CXS = (105, 275, 445, 615)
BYPASS_CX, INPUT_CX, OUTPUT_CX, MAIN_CX = CTRL_CXS
bypass_cx = BYPASS_CX

# Tight scale ring — measured from the VISUAL knob edge, not the container.
# The rendered knob occupies only the central ~46% of the filmstrip frame, so
# the visual radius is ~30 logical even when CTRL_RADIUS (the container) is 65.
VISUAL_KNOB_R = int(CTRL_RADIUS * 0.49)   # ~49% of container = visible knob edge
TICK_R  = VISUAL_KNOB_R + 3
LABEL_R = TICK_R + 12

# Screen-polar convention (Y-down): 135° = 7:30, 270° = 12 o'clock, 45° = 4:30
start_angle = math.pi * 0.75   # 135° → 7:30 (min)
sweep       = math.pi * 1.5    # 270° sweep ending at 4:30 (max)

# Static silkscreen labels under each control (and ON/OFF flanking the switch)
CTRL_LABEL_Y = 232
SWITCH_ON_Y  = 118       # "ON" silkscreen just above the lever cap (UP)
SWITCH_OFF_Y = 178       # "OFF" silkscreen below the lever (symmetric)

# The toggle is rendered on the asset's vertical axis, so all three silkscreen
# labels (ON, OFF, BYPASS) line up under it at BYPASS_CX. The LED is off to
# the right inside the same asset.
SWITCH_TOGGLE_X = BYPASS_CX

# All three knobs share the same tick/scale ring (the switch in pos 1 has none)
KNOB_CXS = (INPUT_CX, OUTPUT_CX, MAIN_CX)


# ---------------------------------------------------------------------------
# Font loading — Mac system fonts, fall back gracefully
# ---------------------------------------------------------------------------
def load_font(candidates, size):
    for path in candidates:
        if os.path.exists(path):
            try:
                return ImageFont.truetype(path, size * SCALE)
            except Exception:
                pass
    return ImageFont.load_default(size * SCALE)


SCRIPT = "/System/Library/Fonts/Supplemental/SnellRoundhand.ttc"
ARIAL_BOLD = "/System/Library/Fonts/Supplemental/Arial Bold.ttf"
IMPACT = "/System/Library/Fonts/Supplemental/Impact.ttf"

font_logo = load_font([SCRIPT], 30)        # logo at top of horizontal panel
font_subtitle = load_font([ARIAL_BOLD], 7)
font_label = load_font([ARIAL_BOLD], 11)
font_scale = load_font([ARIAL_BOLD], 9)    # small numbers hugging the knob
font_switch = load_font([ARIAL_BOLD], 8)   # ON / OFF flanking the toggle
font_maker = load_font([ARIAL_BOLD], 7)


# ---------------------------------------------------------------------------
# Load panel base
# ---------------------------------------------------------------------------
panel = Image.open(SRC).convert("RGBA")
assert panel.size == (W, H), f"panel_base.png is {panel.size}, expected {(W, H)}"
WIN_W, WIN_H  # ensure constants accessible (already defined above)
draw = ImageDraw.Draw(panel)


def s(v):
    """Scale logical -> retina pixels."""
    return int(round(v * SCALE))


# ---------------------------------------------------------------------------
# Typography colours (sit on top of brushed brass — choose for contrast)
# ---------------------------------------------------------------------------
# Boutique silkscreen: warm cream on matte black faceplate
LOGO_WHITE = (240, 230, 208, 245)
LOGO_SHADOW = (0, 0, 0, 200)
LABEL_BLACK = (220, 208, 182, 245)   # silkscreen cream
LABEL_HALO = (0, 0, 0, 120)          # dark halo for separation
SCALE_BLACK = (220, 208, 182, 245)
MAKER_DIM = (160, 145, 115, 200)


def draw_text_with_shadow(xy, text, font, fill, shadow=LOGO_SHADOW, offset=(2, 2),
                           anchor="mm"):
    sx, sy = xy
    ox, oy = offset
    draw.text((sx + ox, sy + oy), text, font=font, fill=shadow, anchor=anchor)
    draw.text((sx, sy), text, font=font, fill=fill, anchor=anchor)


# ---------------------------------------------------------------------------
# 1) TIMBRO logo — single line, no subtitle (boutique amp head face)
# ---------------------------------------------------------------------------
logo_cx = s(WIN_W // 2)
logo_cy = s(LOGO_Y)
draw_text_with_shadow((logo_cx, logo_cy), "Timbro", font_logo,
                       fill=LOGO_WHITE, shadow=LOGO_SHADOW, offset=(s(1.0), s(1.0)),
                       anchor="mm")


# ---------------------------------------------------------------------------
# 2) Tick marks around all three knobs + scale 0..10 around the centre knob
# ---------------------------------------------------------------------------
def draw_ticks(cx, cy, radius_inner, radius_outer_major, radius_outer_minor,
               width_major=1.5, width_minor=1.0):
    for i in range(11):
        angle = start_angle + (i / 10.0) * sweep
        x1 = cx + math.cos(angle) * radius_inner
        y1 = cy + math.sin(angle) * radius_inner
        x2 = cx + math.cos(angle) * radius_outer_major
        y2 = cy + math.sin(angle) * radius_outer_major
        draw.line([(s(x1), s(y1)), (s(x2), s(y2))],
                  fill=LABEL_BLACK, width=max(2, s(width_major)))
    for i in range(1, 20, 2):
        angle = start_angle + (i / 20.0) * sweep
        x1 = cx + math.cos(angle) * radius_inner
        y1 = cy + math.sin(angle) * radius_inner
        x2 = cx + math.cos(angle) * radius_outer_minor
        y2 = cy + math.sin(angle) * radius_outer_minor
        draw.line([(s(x1), s(y1)), (s(x2), s(y2))],
                  fill=LABEL_BLACK, width=max(1, s(width_minor)))


# Tick ring + 0..10 numbers around all three knobs (the switch has neither)
for cx in KNOB_CXS:
    draw_ticks(cx, CTRL_ROW_CY,
               radius_inner=TICK_R,
               radius_outer_major=TICK_R + 6,
               radius_outer_minor=TICK_R + 3,
               width_major=1.2,
               width_minor=0.8)
    for i in range(11):
        angle = start_angle + (i / 10.0) * sweep
        nx = cx + math.cos(angle) * LABEL_R
        ny = CTRL_ROW_CY + math.sin(angle) * LABEL_R
        draw_text_with_shadow((s(nx), s(ny)), str(i), font_scale,
                               fill=SCALE_BLACK,
                               shadow=(0, 0, 0, 160), offset=(s(0.5), s(0.5)),
                               anchor="mm")


# ---------------------------------------------------------------------------
# 3) Static silkscreen labels — BYPASS / INPUT / OUTPUT / VOICE.
#    Plus ON / OFF silkscreen flanking the toggle switch.
# ---------------------------------------------------------------------------
for cx, text in [(BYPASS_CX, "BYPASS"),
                  (INPUT_CX,  "INPUT"),
                  (OUTPUT_CX, "OUTPUT"),
                  (MAIN_CX,   "VOICE")]:
    draw_text_with_shadow((s(cx), s(CTRL_LABEL_Y)), text, font_label,
                           fill=LABEL_BLACK,
                           shadow=(0, 0, 0, 180), offset=(s(0.7), s(0.7)),
                           anchor="mm")

# ON above the switch (UP position = bypass off / signal active)
draw_text_with_shadow((s(SWITCH_TOGGLE_X), s(SWITCH_ON_Y)), "ON", font_switch,
                       fill=LABEL_BLACK,
                       shadow=(0, 0, 0, 160), offset=(s(0.5), s(0.5)),
                       anchor="mm")
# OFF below the switch (DOWN position = bypass engaged)
draw_text_with_shadow((s(SWITCH_TOGGLE_X), s(SWITCH_OFF_Y)), "OFF", font_switch,
                       fill=LABEL_BLACK,
                       shadow=(0, 0, 0, 160), offset=(s(0.5), s(0.5)),
                       anchor="mm")


# Screws are 3D geometry now (rendered in render_panel.py) — no Pillow draw here.


# ---------------------------------------------------------------------------
# 4) Maker stamp at the very bottom
# ---------------------------------------------------------------------------
draw_text_with_shadow((s(WIN_W // 2), s(WIN_H - 11)),
                       "TONDO AUDIO   —   MADE IN ITALY",
                       font_maker, fill=MAKER_DIM,
                       shadow=(255, 255, 255, 30), offset=(s(0.5), s(0.5)),
                       anchor="mm")


# ---------------------------------------------------------------------------
# Save
# ---------------------------------------------------------------------------
panel.save(DST, optimize=True)
print(f"[compose_panel] wrote {DST}")
