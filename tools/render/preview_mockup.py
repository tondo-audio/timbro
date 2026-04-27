"""Compose a static mockup of the JUCE plugin look.

Layout (logical px on 720x280 horizontal window, scaled 2x):
- Title baked into panel.png
- 4 controls in a row at y=148, x=(105, 275, 445, 615), diameter 110:
  INPUT knob, TONE knob (with 0..10 scale), OUTPUT knob, BYPASS switch
- Static labels baked into panel.png

Output: tools/render/_preview.png  (review only, not shipped)
"""

import os
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
PANEL = os.path.abspath(os.path.join(HERE, "..", "..", "resources", "ui", "panel.png"))
KNOB_STRIP = os.path.abspath(os.path.join(HERE, "..", "..", "resources", "ui", "knob_filmstrip.png"))
SWITCH_STRIP = os.path.abspath(os.path.join(HERE, "..", "..", "resources", "ui", "switch_strip.png"))
OUT = os.path.join(HERE, "_preview.png")

SCALE = 2

panel = Image.open(PANEL).convert("RGBA")

CTRL_DIAMETER = 156
CTRL_ROW_CY   = 148
# Order left → right: BYPASS, INPUT, OUTPUT, VOICE (main)
BYPASS_CX, INPUT_CX, OUTPUT_CX, MAIN_CX = (105, 275, 445, 615)

# --- Knobs ---------------------------------------------------------------
knob_strip = Image.open(KNOB_STRIP).convert("RGBA")
kfw = knob_strip.width
kfh = kfw
knob_n = knob_strip.height // kfh


def knob_frame(value):
    idx = int(round((value / 10.0) * (knob_n - 1)))
    return knob_strip.crop((0, idx * kfh, kfw, (idx + 1) * kfh))


knob_size_retina = CTRL_DIAMETER * SCALE
# INPUT, OUTPUT, VOICE — sample values for the mockup
for cx_logical, val in zip((INPUT_CX, OUTPUT_CX, MAIN_CX), (4, 7, 6)):
    img = knob_frame(val).resize((knob_size_retina, knob_size_retina), Image.LANCZOS)
    cx = cx_logical * SCALE
    cy = CTRL_ROW_CY * SCALE
    panel.alpha_composite(img, (cx - knob_size_retina // 2,
                                 cy - knob_size_retina // 2))

# --- Toggle switch (in 4th position) -------------------------------------
if os.path.exists(SWITCH_STRIP):
    switch_strip = Image.open(SWITCH_STRIP).convert("RGBA")
    sfw = switch_strip.width
    sfh = sfw
    sw = switch_strip.crop((0, 0, sfw, sfh))   # frame 0 = UP state
    sw_size_retina = CTRL_DIAMETER * SCALE
    sw = sw.resize((sw_size_retina, sw_size_retina), Image.LANCZOS)
    cx = BYPASS_CX * SCALE
    # No display-shift needed: the toggle's pivot is sunk below the panel in
    # the render so the lever's visual centre is already at the image centre,
    # which composites onto CTRL_ROW_CY.
    cy = CTRL_ROW_CY * SCALE
    panel.alpha_composite(sw, (cx - sw_size_retina // 2,
                                cy - sw_size_retina // 2))
else:
    print(f"[preview] {SWITCH_STRIP} not yet rendered — switch will be missing")

panel.save(OUT)
print(f"[preview] wrote {OUT}")
