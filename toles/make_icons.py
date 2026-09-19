#!/usr/bin/env python3
# =====================================================================
#  FILE: toles/make_icons.py
#  PROJECT: NEON CITY ULTRA v4
#  DESCRIPTION: Generate ONE icon (ICON0.PNG) only
# =====================================================================

from PIL import Image, ImageDraw
import os
import math

NEON_CYAN   = (0, 212, 255)
NEON_ORANGE = (255, 107, 53)
DARK_BG     = (10, 15, 26)


def draw_icon0(path):
    W, H = 512, 288
    img = Image.new('RGB', (W, H), DARK_BG)
    d = ImageDraw.Draw(img)

    # sky
    horizon = int(H * 0.62)
    for y in range(horizon):
        t = y / horizon
        r = int(6 + t*40)
        g = int(15 + t*90)
        b = int(35 + t*140)
        d.line([(0, y), (W, y)], fill=(r, g, b))

    # perspective grid
    cx = W // 2
    for i in range(-25, 26):
        d.line([(cx, horizon), (cx + i*40, H)], fill=(0, 60, 100))
    for i in range(12):
        y = horizon + int(i*i*1.4) + 5
        if y >= H: break
        d.line([(0, y), (W, y)], fill=(0, 60, 100))

    # neon horizon
    d.line([(0, horizon), (W, horizon)], fill=NEON_CYAN, width=3)

    # road
    d.polygon([(W//2-30, horizon), (W//2+30, horizon), (W-60, H), (60, H)],
              fill=(28, 30, 38))

    # yellow dashed line
    for i in range(5):
        y0 = horizon + 15 + i*22
        y1 = y0 + 14
        w = 3 + i
        d.polygon([(W//2-w, y0), (W//2+w, y0), (W//2+w, y1), (W//2-w, y1)],
                  fill=(255, 204, 0))

    # car
    cy = H - 62
    cx = W//2
    d.ellipse([cx-60, cy+18, cx+60, cy+32], fill=(0,0,0))
    d.rectangle([cx-62, cy-8, cx+62, cy+20], fill=(232, 184, 36))
    d.rectangle([cx-42, cy-32, cx+42, cy-8], fill=(232, 184, 36))
    d.rectangle([cx-38, cy-28, cx-8, cy-12], fill=(48, 64, 80))
    d.rectangle([cx+8, cy-28, cx+38, cy-12], fill=(48, 64, 80))
    d.rectangle([cx-62, cy-2, cx+62, cy+2], fill=(255, 255, 255))
    d.ellipse([cx-50, cy+12, cx-34, cy+28], fill=(0,0,0))
    d.ellipse([cx+34, cy+12, cx+50, cy+28], fill=(0,0,0))
    d.rectangle([cx-58, cy+4, cx-48, cy+8], fill=(255, 250, 200))
    d.rectangle([cx+48, cy+4, cx+58, cy+8], fill=(255, 80, 60))

    # title glow
    for dx in range(-3, 4):
        for dy in range(-3, 4):
            if dx*dx + dy*dy > 9: continue
            d.text((W//2 - 90 + dx, 22 + dy), "NEON CITY", fill=(0, 60, 110))
    d.text((W//2 - 90, 22), "NEON CITY", fill=NEON_CYAN)
    d.text((W//2 - 68, 42), "U L T R A", fill=NEON_ORANGE)

    img.save(path, "PNG")
    print(f"ICON0.PNG: {os.path.getsize(path)} bytes")


if __name__ == '__main__':
    out_dir = os.environ.get('OUT_DIR', '.')
    os.makedirs(out_dir, exist_ok=True)
    draw_icon0(os.path.join(out_dir, 'ICON0.PNG'))
    print("Done!")