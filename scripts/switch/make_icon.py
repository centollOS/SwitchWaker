#!/usr/bin/env python3
"""Draw SwitchWaker's 256x256 icon (NRO and HOME-menu forwarder): original art.

    uv run --with pillow scripts/switch/make_icon.py [OUTPUT.jpg]

A spider crab (a "centollo") on a rock under a deep-sea gradient, with bubbles, and "SwitchWaker"
on a band below. Drawn from scratch with Pillow primitives, at 4x and downscaled for
anti-aliasing. Deterministic: same Pillow, same bytes. The committed copy is
switch/native/icon/icon.jpg (switch/native/CMakeLists.txt, scripts/switch/build_forwarder.sh);
the default output is build/icon/icon.jpg, to compare before replacing it.
"""
import math
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

SIZE = 256
SS = 4  # supersampling factor
W = SIZE * SS

SHELL = (214, 74, 44)
SHELL_DARK = (150, 42, 30)
SHELL_LIGHT = (240, 128, 84)


def lerp(a, b, t):
    return tuple(round(a[i] + (b[i] - a[i]) * t) for i in range(3))


def gradient(top, bottom):
    img = Image.new("RGB", (W, W))
    px = ImageDraw.Draw(img)
    for y in range(W):
        px.line([(0, y), (W, y)], fill=lerp(top, bottom, y / (W - 1)))
    return img


def s(v):
    return v * SS


def pt(x, y):
    return (s(x), s(y))


def limb(d, points, width, colour):
    """A jointed leg: thick segments with round joints."""
    pts = [pt(x, y) for x, y in points]
    d.line(pts, fill=colour, width=s(width), joint="curve")
    for x, y in pts:
        r = s(width) / 2
        d.ellipse([x - r, y - r, x + r, y + r], fill=colour)


def claw(d, base, tip, side):
    """A thin arm ending in an open pincer."""
    bx, by = base
    tx, ty = tip
    limb(d, [base, ((bx + tx) / 2 + 4 * side, (by + ty) / 2 + 6), tip], 5, SHELL)
    d.polygon([pt(tx - 5, ty + 2), pt(tx + 5, ty + 2), pt(tx + 7 * side, ty - 14)], fill=SHELL)
    d.polygon([pt(tx - 4, ty + 1), pt(tx + 4, ty + 1), pt(tx - 5 * side, ty - 10)], fill=SHELL_DARK)


def main():
    out = Path(sys.argv[1] if len(sys.argv) > 1 else "build/icon/icon.jpg")
    img = gradient((36, 132, 168), (6, 30, 70))
    d = ImageDraw.Draw(img)

    # Light rays from the surface.
    for x0, x1 in ((30, 70), (110, 140), (190, 230)):
        d.polygon([pt(x0, 0), pt(x1, 0), pt(x1 - 30, 200), pt(x0 - 50, 200)], fill=(44, 140, 176))

    # Bubbles.
    for cx, cy, r in ((44, 52, 7), (56, 30, 4), (38, 16, 3), (212, 70, 6), (222, 44, 3)):
        d.ellipse([s(cx - r), s(cy - r), s(cx + r), s(cy + r)], outline=(200, 236, 250), width=s(2))

    # Rock and sand.
    d.ellipse([s(-40), s(170), s(296), s(300)], fill=(26, 52, 80))
    d.ellipse([s(40), s(176), s(216), s(230)], fill=(36, 66, 96))

    # Legs: four long, jointed pairs, spread like a spider crab's.
    cx, cy = 128, 132
    legs = [
        ((-16, 6), (-60, -14), (-96, 30)),
        ((-18, 14), (-70, 14), (-104, 66)),
        ((-14, 22), (-58, 44), (-80, 88)),
        ((-8, 28), (-34, 62), (-44, 96)),
    ]
    for (ax, ay), (kx, ky), (fx, fy) in legs:
        for side in (-1, 1):
            limb(d, [(cx - side * ax, cy + ay), (cx - side * kx, cy + ky), (cx - side * fx, cy + fy)],
                 5, SHELL_DARK)

    # Claws, reaching up and out.
    claw(d, (cx - 14, cy - 10), (cx - 52, cy - 64), -1)
    claw(d, (cx + 14, cy - 10), (cx + 52, cy - 64), 1)

    # Carapace: a rounded, pear-shaped shell, wider behind, with spiny bumps.
    d.ellipse([pt(cx - 34, cy - 30), pt(cx + 34, cy + 40)], fill=SHELL)
    d.polygon([pt(cx - 22, cy - 22), pt(cx + 22, cy - 22), pt(cx, cy - 46)], fill=SHELL)
    for bx, by, r in ((-16, 0, 4), (16, 0, 4), (0, 12, 5), (-20, 18, 3), (20, 18, 3), (0, -10, 3),
                      (-10, 26, 3), (10, 26, 3)):
        d.ellipse([pt(cx + bx - r, cy + by - r), pt(cx + bx + r, cy + by + r)], fill=SHELL_LIGHT)
    # Rostrum horns and eyes.
    d.polygon([pt(cx - 6, cy - 42), pt(cx - 2, cy - 42), pt(cx - 10, cy - 58)], fill=SHELL_DARK)
    d.polygon([pt(cx + 2, cy - 42), pt(cx + 6, cy - 42), pt(cx + 10, cy - 58)], fill=SHELL_DARK)
    for ex in (-12, 12):
        d.ellipse([pt(cx + ex - 5, cy - 34), pt(cx + ex + 5, cy - 24)], fill=(250, 246, 236))
        d.ellipse([pt(cx + ex - 2, cy - 31), pt(cx + ex + 3, cy - 26)], fill=(20, 20, 30))

    # Label band.
    d.rectangle([0, s(206), W, W], fill=(6, 28, 62))
    try:
        font = ImageFont.load_default(size=s(30))
    except TypeError:  # Pillow < 10.1: no scalable default font
        font = ImageFont.load_default()
    text = "SwitchWaker"
    box = d.textbbox((0, 0), text, font=font)
    tw, th = box[2] - box[0], box[3] - box[1]
    d.text(((W - tw) / 2 - box[0], s(231) - th / 2 - box[1]), text, font=font, fill=(240, 248, 255))

    icon = img.resize((SIZE, SIZE), Image.LANCZOS)
    out.parent.mkdir(parents=True, exist_ok=True)
    icon.save(out, "JPEG", quality=92, optimize=True, subsampling=0)
    print(f"wrote {out} ({out.stat().st_size} bytes, {SIZE}x{SIZE})")


if __name__ == "__main__":
    main()
