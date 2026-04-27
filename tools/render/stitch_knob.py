"""Stitch the per-frame PNGs from render_knob.py into a single vertical strip,
with a soft drop shadow composited under each frame.

Run after `blender --background --python render_knob.py`.
Uses system Python (Pillow), since Blender's bundled Python lacks PIL.
"""

import os
import sys
from PIL import Image, ImageDraw, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
FRAMES_DIR = os.path.join(HERE, "_knob_frames")
OUT_PATH = os.path.abspath(
    os.path.join(HERE, "..", "..", "resources", "ui", "knob_filmstrip.png")
)


def make_shadow(size, radius_frac=0.26, alpha=120, offset=(2, 3), blur=7):
    """Soft circular shadow on a transparent canvas of `size`x`size`."""
    s = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(s)
    cx = cy = size // 2
    r = int(size * radius_frac)
    d.ellipse([(cx - r + offset[0], cy - r + offset[1]),
               (cx + r + offset[0], cy + r + offset[1])],
              fill=(0, 0, 0, alpha))
    return s.filter(ImageFilter.GaussianBlur(blur))


frames = sorted(f for f in os.listdir(FRAMES_DIR) if f.endswith(".png"))
if not frames:
    sys.exit(f"no frames found in {FRAMES_DIR}")

first = Image.open(os.path.join(FRAMES_DIR, frames[0]))
w, h = first.size
shadow = make_shadow(w)

strip = Image.new("RGBA", (w, h * len(frames)), (0, 0, 0, 0))
for i, name in enumerate(frames):
    knob = Image.open(os.path.join(FRAMES_DIR, name)).convert("RGBA")
    cell = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    cell.alpha_composite(shadow)
    cell.alpha_composite(knob)
    strip.paste(cell, (0, i * h))

strip.save(OUT_PATH, optimize=True)
print(f"[stitch_knob] wrote {OUT_PATH} ({w}x{h * len(frames)}, {len(frames)} frames)")
