#!/usr/bin/env python3
"""Generate Timbro app icon - a minimal dial/knob design."""

from PIL import Image, ImageDraw, ImageFont
import math
import subprocess
import os

SIZE = 1024
CENTER = SIZE // 2
ICON_DIR = os.path.join(os.path.dirname(os.path.dirname(__file__)), "resources", "icon")

# Colors - warm analog aesthetic
BG_COLOR = (30, 28, 26)           # Dark charcoal background
DIAL_OUTER = (55, 50, 45)         # Dark ring
DIAL_FACE = (45, 42, 38)         # Knob face
INDICATOR = (232, 168, 56)        # Warm amber/gold indicator
TICK_COLOR = (120, 110, 100)      # Subtle tick marks
HIGHLIGHT = (70, 65, 58)          # Subtle highlight on knob
SHADOW = (20, 18, 16)             # Shadow


def draw_icon(size=SIZE):
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    cx, cy = size // 2, size // 2

    # Rounded square background
    margin = int(size * 0.08)
    radius = int(size * 0.18)
    draw.rounded_rectangle(
        [margin, margin, size - margin, size - margin],
        radius=radius,
        fill=BG_COLOR,
    )

    # Outer ring / bezel
    knob_radius = int(size * 0.30)
    bezel_radius = knob_radius + int(size * 0.06)

    # Shadow under bezel
    draw.ellipse(
        [cx - bezel_radius + 4, cy - bezel_radius + 6,
         cx + bezel_radius + 4, cy + bezel_radius + 6],
        fill=(10, 9, 8, 180),
    )

    # Bezel ring
    draw.ellipse(
        [cx - bezel_radius, cy - bezel_radius,
         cx + bezel_radius, cy + bezel_radius],
        fill=DIAL_OUTER,
    )

    # Tick marks around the dial (like zone positions)
    # Arc from 135° (7 o'clock, CLEAN) to 405° (5 o'clock, LEAD)
    arc_start = 135
    arc_end = 405
    num_ticks = 11  # 0 through 10
    tick_inner = bezel_radius + int(size * 0.02)
    tick_outer = bezel_radius + int(size * 0.05)

    for i in range(num_ticks):
        angle_deg = arc_start + (arc_end - arc_start) * i / (num_ticks - 1)
        angle_rad = math.radians(angle_deg)
        x1 = cx + tick_inner * math.cos(angle_rad)
        y1 = cy + tick_inner * math.sin(angle_rad)
        x2 = cx + tick_outer * math.cos(angle_rad)
        y2 = cy + tick_outer * math.sin(angle_rad)

        # Major ticks at zone boundaries (0, 2, 4, 6, 8, 10)
        if i % 2 == 0:
            width = 3
            color = TICK_COLOR
        else:
            width = 2
            color = (90, 82, 75)

        draw.line([(x1, y1), (x2, y2)], fill=color, width=width)

    # Knob face with subtle gradient effect (concentric circles)
    for r in range(knob_radius, 0, -1):
        t = r / knob_radius
        # Slight gradient: lighter toward top-left
        shade = int(45 + (1 - t) * 15)
        c = (shade, shade - 3, shade - 6)
        draw.ellipse(
            [cx - r, cy - r, cx + r, cy + r],
            fill=c,
        )

    # Subtle highlight arc on top-left of knob
    highlight_r = knob_radius - int(size * 0.02)
    for angle in range(200, 320):
        angle_rad = math.radians(angle)
        for dr in range(3):
            r = highlight_r - dr
            x = cx + r * math.cos(angle_rad)
            y = cy + r * math.sin(angle_rad)
            alpha = int(25 * (1 - dr / 3))
            if 0 <= x < size and 0 <= y < size:
                px = img.getpixel((int(x), int(y)))
                blended = tuple(min(255, c + alpha) for c in px[:3]) + (px[3],)
                img.putpixel((int(x), int(y)), blended)

    # Indicator line - pointing roughly at 7-8 position (DRIVE zone, ~330°)
    # Default position: pointing up-right, around the 2 o'clock position
    indicator_angle = math.radians(315)  # 315° = upper-right, suggesting high gain
    ind_inner = int(knob_radius * 0.25)
    ind_outer = int(knob_radius * 0.85)
    ix1 = cx + ind_inner * math.cos(indicator_angle)
    iy1 = cy + ind_inner * math.sin(indicator_angle)
    ix2 = cx + ind_outer * math.cos(indicator_angle)
    iy2 = cy + ind_outer * math.sin(indicator_angle)

    # Glow effect for indicator
    for w in range(8, 0, -1):
        alpha = int(40 * (1 - w / 8))
        glow_color = (232, 168, 56, alpha)
        # Draw on separate layer for glow
        draw.line([(ix1, iy1), (ix2, iy2)], fill=INDICATOR, width=max(1, w))

    # Sharp indicator line on top
    draw.line([(ix1, iy1), (ix2, iy2)], fill=INDICATOR, width=4)

    # Small dot at the tip
    dot_r = int(size * 0.012)
    draw.ellipse(
        [ix2 - dot_r, iy2 - dot_r, ix2 + dot_r, iy2 + dot_r],
        fill=INDICATOR,
    )

    # Center cap
    cap_r = int(knob_radius * 0.15)
    draw.ellipse(
        [cx - cap_r, cy - cap_r, cx + cap_r, cy + cap_r],
        fill=(35, 32, 28),
    )

    return img


def create_icns(png_path, icns_path):
    """Create .icns file from a 1024x1024 PNG using macOS iconutil."""
    iconset_dir = icns_path.replace(".icns", ".iconset")
    os.makedirs(iconset_dir, exist_ok=True)

    img = Image.open(png_path)
    sizes = [16, 32, 64, 128, 256, 512, 1024]

    for s in sizes:
        resized = img.resize((s, s), Image.LANCZOS)
        if s <= 512:
            resized.save(os.path.join(iconset_dir, f"icon_{s}x{s}.png"))
        if s <= 1024 and s >= 32:
            # @2x version
            half = s // 2
            if half >= 16:
                resized_2x = img.resize((s, s), Image.LANCZOS)
                resized_2x.save(os.path.join(iconset_dir, f"icon_{half}x{half}@2x.png"))

    # 512@2x = 1024
    img.save(os.path.join(iconset_dir, "icon_512x512@2x.png"))

    subprocess.run(["iconutil", "-c", "icns", iconset_dir, "-o", icns_path], check=True)
    # Clean up iconset
    import shutil
    shutil.rmtree(iconset_dir)
    print(f"Created {icns_path}")


if __name__ == "__main__":
    os.makedirs(ICON_DIR, exist_ok=True)

    icon = draw_icon()
    png_path = os.path.join(ICON_DIR, "timbro_icon.png")
    icon.save(png_path)
    print(f"Saved PNG: {png_path}")

    icns_path = os.path.join(ICON_DIR, "Timbro.icns")
    create_icns(png_path, icns_path)
