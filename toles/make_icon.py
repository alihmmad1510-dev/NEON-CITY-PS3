#!/usr/bin/env python3
"""Generate PS3 XMB icons for NEON CITY."""
from PIL import Image, ImageDraw
import os
import random

NEON_CYAN    = (0, 212, 255)
NEON_MAGENTA = (255, 51, 170)
NEON_YELLOW  = (255, 204, 0)
NEON_ORANGE  = (255, 107, 53)
DARK_BG      = (10, 15, 26)


def draw_icon0(path):
    """320x176 main XMB icon."""
    W, H = 320, 176
    img = Image.new('RGB', (W, H), DARK_BG)
    d = ImageDraw.Draw(img)

    # gradient sky
    for y in range(H):
        t = y / H
        r = int(10 + t * 20)
        g = int(15 + t * 30)
        b = int(26 + t * 40)
        d.line([(0, y), (W, y)], fill=(r, g, b))

    # perspective grid
    cx = W // 2
    for i in range(-8, 9):
        x_bottom = cx + i * 45
        d.line([(cx, H // 2 + 10), (x_bottom, H)], fill=(0, 60, 100), width=1)
    for y in [H // 2 + 20, H // 2 + 40, H // 2 + 65, H // 2 + 95, H // 2 + 130]:
        d.line([(0, y), (W, y)], fill=(0, 60, 100), width=1)

    # horizon line
    d.line([(0, H // 2 + 10), (W, H // 2 + 10)], fill=NEON_CYAN, width=2)

    # car
    car_x = W // 2 - 55
    car_y = H - 55
    d.rectangle([car_x, car_y + 12, car_x + 110, car_y + 30], fill=NEON_YELLOW)
    d.rectangle([car_x + 20, car_y, car_x + 90, car_y + 12], fill=NEON_YELLOW)
    d.rectangle([car_x + 25, car_y + 3, car_x + 55, car_y + 10], fill=(50, 80, 120))
    d.rectangle([car_x + 60, car_y + 3, car_x + 85, car_y + 10], fill=(50, 80, 120))
    d.rectangle([car_x - 3, car_y + 18, car_x + 3, car_y + 24], fill=(255, 250, 200))
    d.rectangle([car_x + 107, car_y + 18, car_x + 113, car_y + 24], fill=NEON_MAGENTA)
    d.ellipse([car_x + 15, car_y + 26, car_x + 35, car_y + 46], fill=(10, 10, 10))
    d.ellipse([car_x + 20, car_y + 31, car_x + 30, car_y + 41], fill=(128, 136, 144))
    d.ellipse([car_x + 75, car_y + 26, car_x + 95, car_y + 46], fill=(10, 10, 10))
    d.ellipse([car_x + 80, car_y + 31, car_x + 90, car_y + 41], fill=(128, 136, 144))

    # title glow
    for dx in range(-2, 3):
        for dy in range(-2, 3):
            if dx == 0 and dy == 0:
                continue
            d.text((W // 2 - 45 + dx, 25 + dy), "NEON CITY", fill=(0, 80, 120))
    d.text((W // 2 - 45, 25), "NEON CITY", fill=NEON_CYAN)
    d.text((W // 2 - 45 + 1, 26), "NEON CITY", fill=(150, 240, 255))
    d.text((W // 2 - 45, 48), "U L T R A", fill=NEON_ORANGE)

    img.save(path, "PNG")
    print(f"ICON0.PNG created: {path}")


def draw_pic0(path):
    """1920x1080 title background."""
    W, H = 1920, 1080
    img = Image.new('RGB', (W, H), DARK_BG)
    d = ImageDraw.Draw(img)

    for y in range(H // 2):
        t = y / (H // 2)
        r = int(8 + t * 30)
        g = int(18 + t * 90)
        b = int(40 + t * 160)
        d.line([(0, y), (W, y)], fill=(r, g, b))

    d.rectangle([0, H // 2 - 40, W, H // 2], fill=(80, 120, 180))
    d.rectangle([0, H // 2, W, H // 2 + 4], fill=NEON_CYAN)

    for y in range(H // 2 + 4, H):
        t = (y - H // 2) / (H // 2)
        r = int(26 + t * 10)
        g = int(26 + t * 10)
        b = int(34 + t * 15)
        d.line([(0, y), (W, y)], fill=(r, g, b))

    cx = W // 2
    for i in range(-20, 21):
        x_bottom = cx + i * 80
        d.line([(cx, H // 2 + 4), (x_bottom, H)], fill=(30, 50, 80), width=1)
    for i in range(1, 10):
        y = H // 2 + 4 + (i * i * 10)
        if y < H:
            d.line([(0, y), (W, y)], fill=(30, 50, 80), width=1)

    skyline = [
        (100, 300, 120, 400), (240, 250, 90, 450),
        (350, 200, 140, 500), (510, 280, 100, 420),
        (620, 180, 160, 520), (800, 240, 110, 460),
        (930, 200, 130, 500), (1080, 260, 100, 440),
        (1200, 300, 140, 400), (1360, 220, 120, 480),
        (1500, 280, 100, 420), (1620, 320, 130, 380),
        (1770, 260, 90, 440),
    ]
    for (x, y, w, h) in skyline:
        d.rectangle([x, y, x + w, y + h], fill=(40, 42, 60))
        for wy in range(y + 10, y + h - 10, 20):
            for wx in range(x + 8, x + w - 8, 18):
                col = NEON_YELLOW if (wx + wy) % 3 == 0 else (26, 32, 48)
                d.rectangle([wx, wy, wx + 8, wy + 10], fill=col)

    d.text((W // 2 - 200, 250), "NEON CITY", fill=NEON_CYAN)
    d.text((W // 2 - 198, 252), "NEON CITY", fill=(150, 240, 255))
    d.text((W // 2 - 200, 300), "U L T R A", fill=NEON_ORANGE)

    img.save(path, "PNG")
    print(f"PIC0.PNG created: {path}")


def draw_pic1(path):
    """1920x1080 alt background."""
    W, H = 1920, 1080
    img = Image.new('RGB', (W, H), (15, 5, 25))
    d = ImageDraw.Draw(img)

    random.seed(42)
    for _ in range(300):
        x = random.randint(0, W)
        y = random.randint(0, H // 2)
        s = random.choice([1, 1, 1, 2])
        b = random.randint(150, 255)
        d.rectangle([x, y, x + s, y + s], fill=(b, b, b))

    d.rectangle([0, H // 2 - 60, W, H // 2], fill=(60, 20, 80))
    d.rectangle([0, H // 2, W, H // 2 + 3], fill=NEON_MAGENTA)

    for x in range(0, W, 100):
        h = 100 + (x * 7) % 300
        d.rectangle([x, H // 2 - h, x + 80, H // 2], fill=(25, 10, 40))
        for wy in range(H // 2 - h + 10, H // 2 - 10, 22):
            for wx in range(x + 8, x + 72, 16):
                if (wx + wy) % 5 == 0:
                    d.rectangle([wx, wy, wx + 6, wy + 10], fill=NEON_MAGENTA)

    img.save(path, "PNG")
    print(f"PIC1.PNG created: {path}")


if __name__ == '__main__':
    out_dir = os.environ.get('OUT_DIR', '.')
    os.makedirs(out_dir, exist_ok=True)
    draw_icon0(os.path.join(out_dir, 'ICON0.PNG'))
    draw_pic0(os.path.join(out_dir, 'PIC0.PNG'))
    draw_pic1(os.path.join(out_dir, 'PIC1.PNG'))
    print("All icons generated!")