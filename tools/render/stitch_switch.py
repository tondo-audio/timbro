"""Stitch the two toggle-switch frames (up.png, down.png) into a vertical strip.
Frame 0 = UP (bypass off / signal active, LED ON), frame 1 = DOWN (bypass engaged, LED OFF).

Adds a soft red glow around the LED area on the ON frame so it reads as a
real lit indicator (Eevee Next removed the built-in bloom node).
"""

import os
from PIL import Image, ImageFilter, ImageChops

HERE = os.path.dirname(os.path.abspath(__file__))
FRAMES_DIR = os.path.join(HERE, "_switch_frames")
OUT = os.path.abspath(os.path.join(HERE, "..", "..", "resources", "ui", "switch_strip.png"))


def add_green_glow(img: Image.Image, glow_strength=1.4, blur_radius=14) -> Image.Image:
    """Detect bright green pixels and add a soft additive glow around them."""
    r, g, b, a = img.split()
    # Mask = bright green minus other channels (highlights only the LED)
    bright = ImageChops.subtract(g, r)
    bright = ImageChops.subtract(bright, b)
    bright = bright.point(lambda p: min(255, int(p * glow_strength)))
    glow = bright.filter(ImageFilter.GaussianBlur(blur_radius))
    green_layer = Image.new("RGBA", img.size, (40, 255, 80, 0))
    green_layer.putalpha(glow)
    out = Image.new("RGBA", img.size, (0, 0, 0, 0))
    out.alpha_composite(green_layer)
    out.alpha_composite(img)
    return out


up = Image.open(os.path.join(FRAMES_DIR, "up.png")).convert("RGBA")
down = Image.open(os.path.join(FRAMES_DIR, "down.png")).convert("RGBA")

up = add_green_glow(up)

w, h = up.size
strip = Image.new("RGBA", (w, h * 2), (0, 0, 0, 0))
strip.paste(up, (0, 0))
strip.paste(down, (0, h))
strip.save(OUT, optimize=True)
print(f"[stitch_switch] wrote {OUT} ({w}x{h*2}, 2 frames, green glow on UP)")
