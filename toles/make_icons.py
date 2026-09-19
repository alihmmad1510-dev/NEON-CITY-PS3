#!/usr/bin/env python3
# =====================================================================
#  FILE: toles/make_icons.py
#  PROJECT: NEON CITY ULTRA v4
#  DESCRIPTION: Generate PKG assets targeting ~45MB total
#    - ICON0.PNG, PIC0.PNG, PIC1.PNG (standard PS3)
#    - 8 PREVIEW PNG files (1920x1080)
#    - 7 WALLPAPER BMP files (1920x1080, uncompressed ~6MB each)
# =====================================================================

from PIL import Image, ImageDraw
import os
import random

NEON_CYAN    = (0, 212, 255)
NEON_MAGENTA = (255, 51, 170)
NEON_YELLOW  = (255, 204, 0)
NEON_ORANGE  = (255, 107, 53)
DARK_BG      = (10, 15, 26)


def _sky_gradient(d, W, H, top, mid, bottom, horizon_y):
    for y in range(horizon_y):
        t = y / max(1, horizon_y)
        if t < 0.5:
            t2 = t * 2
            r = int(top[0] + (mid[0]-top[0])*t2)
            g = int(top[1] + (mid[1]-top[1])*t2)
            b = int(top[2] + (mid[2]-top[2])*t2)
        else:
            t2 = (t - 0.5) * 2
            r = int(mid[0] + (bottom[0]-mid[0])*t2)
            g = int(mid[1] + (bottom[1]-mid[1])*t2)
            b = int(mid[2] + (bottom[2]-mid[2])*t2)
        d.line([(0, y), (W, y)], fill=(r, g, b))


def _draw_grid(d, W, H, horizon_y):
    cx = W // 2
    for i in range(-20, 21):
        d.line([(cx, horizon_y), (cx + i*100, H)], fill=(30, 50, 80), width=1)
    y = horizon_y
    step = 6
    for i in range(15):
        y += step
        step = int(step * 1.25)
        if y >= H:
            break
        d.line([(0, y), (W, y)], fill=(30, 50, 80), width=1)


def _draw_city_skyline(d, W, H, horizon_y, color, lit_color):
    x = 0
    while x < W:
        bw = random.randint(60, 140)
        bh = random.randint(80, 240)
        bx = x
        by = horizon_y - bh
        d.rectangle([bx, by, bx+bw, horizon_y], fill=color)
        for wy in range(by+8, horizon_y-8, 18):
            for wx in range(bx+6, bx+bw-6, 16):
                if random.random() < 0.35:
                    d.rectangle([wx, wy, wx+7, wy+9], fill=lit_color)
        x += bw + random.randint(5, 20)


def draw_icon0(path):
    W, H = 512, 288
    img = Image.new('RGB', (W, H), DARK_BG)
    d = ImageDraw.Draw(img)
    _sky_gradient(d, W, H, (8, 20, 45), (60, 100, 160), (150, 190, 220), H*3//5)
    _draw_grid(d, W, H, H*3//5)
    d.polygon([(W//2-40, H*3//5), (W//2+40, H*3//5), (W, H), (0, H)], fill=(34, 36, 44))
    for i in range(6):
        y0 = H*3//5 + 20 + i*40
        y1 = y0 + 20
        w0 = 4 + i
        d.polygon([(W//2-w0, y0), (W//2+w0, y0), (W//2+w0, y1), (W//2-w0, y1)], fill=(255, 204, 0))
    cx, cy = W//2, H - 80
    d.ellipse([cx-70, cy+20, cx+70, cy+40], fill=(0,0,0))
    d.rectangle([cx-75, cy-10, cx+75, cy+25], fill=(232, 184, 36))
    d.rectangle([cx-50, cy-40, cx+50, cy-10], fill=(232, 184, 36))
    d.rectangle([cx-45, cy-36, cx-8, cy-14], fill=(48, 64, 80))
    d.rectangle([cx+8, cy-36, cx+45, cy-14], fill=(48, 64, 80))
    d.rectangle([cx-75, cy-2, cx+75, cy+2], fill=(255,255,255))
    d.ellipse([cx-60, cy+15, cx-40, cy+35], fill=(0,0,0))
    d.ellipse([cx+40, cy+15, cx+60, cy+35], fill=(0,0,0))
    for dx in range(-3, 4):
        for dy in range(-3, 4):
            if dx*dx + dy*dy > 9: continue
            d.text((W//2 - 95 + dx, 30 + dy), "NEON CITY", fill=(0, 60, 100))
    d.text((W//2 - 95, 30), "NEON CITY", fill=NEON_CYAN)
    d.text((W//2 - 95, 55), "U L T R A   v4", fill=NEON_ORANGE)
    img.save(path, "PNG")
    print(f"ICON0.PNG created: {os.path.getsize(path)} bytes")


def draw_pic0(path):
    W, H = 1920, 1080
    img = Image.new('RGB', (W, H), DARK_BG)
    d = ImageDraw.Draw(img)
    horizon = 620
    _sky_gradient(d, W, H, (5, 18, 40), (80, 130, 180), (200, 220, 240), horizon)
    d.ellipse([1400, 180, 1700, 480], fill=(255, 240, 180))
    d.ellipse([1450, 230, 1650, 430], fill=(255, 250, 220))
    for cx, cy, cw in [(300, 200, 180), (700, 150, 150), (1100, 220, 200)]:
        d.ellipse([cx, cy, cx+cw, cy+60], fill=(255,255,255))
        d.ellipse([cx+cw//2, cy-30, cx+cw+cw//2, cy+40], fill=(255,255,255))
    _draw_city_skyline(d, W, horizon, horizon, (50, 60, 80), (255, 238, 136))
    _draw_city_skyline(d, W, horizon, horizon, (30, 38, 55), (255, 200, 100))
    d.rectangle([0, horizon-4, W, horizon+4], fill=NEON_CYAN)
    d.rectangle([0, horizon, W, H], fill=(18, 20, 28))
    _draw_grid(d, W, H, horizon)
    d.polygon([(W//2-180, horizon), (W//2+180, horizon), (W, H), (0, H)], fill=(30, 32, 40))
    for i in range(8):
        y0 = horizon + 40 + i*70
        y1 = y0 + 40
        w = 6 + i*2
        d.polygon([(W//2-w, y0), (W//2+w, y0), (W//2+w, y1), (W//2-w, y1)], fill=(255, 204, 0))
    for dx in range(-4, 5):
        for dy in range(-4, 5):
            if dx*dx + dy*dy > 16: continue
            d.text((W//2 - 220 + dx, 140 + dy), "NEON CITY", fill=(0, 80, 130))
    d.text((W//2 - 220, 140), "NEON CITY", fill=NEON_CYAN)
    d.text((W//2 - 200, 200), "U L T R A   v 4", fill=NEON_ORANGE)
    d.text((W//2 - 100, 260), "3D 360 DRIVE", fill=(255,255,255))
    img.save(path, "PNG")
    print(f"PIC0.PNG created: {os.path.getsize(path)} bytes")


def draw_pic1(path):
    W, H = 1920, 1080
    img = Image.new('RGB', (W, H), (8, 4, 18))
    d = ImageDraw.Draw(img)
    horizon = 620
    for y in range(horizon):
        t = y / horizon
        r = int(8 + t*60)
        g = int(4 + t*20)
        b = int(20 + t*60)
        d.line([(0, y), (W, y)], fill=(r, g, b))
    for _ in range(400):
        x = random.randint(0, W)
        y = random.randint(0, horizon-100)
        s = random.choice([1, 1, 2])
        b = random.randint(150, 255)
        d.rectangle([x, y, x+s, y+s], fill=(b, b, b))
    d.ellipse([1500, 120, 1700, 320], fill=(240, 240, 255))
    d.ellipse([1520, 140, 1680, 300], fill=(220, 220, 240))
    d.rectangle([0, horizon-80, W, horizon], fill=(120, 20, 100))
    d.rectangle([0, horizon-4, W, horizon+4], fill=NEON_MAGENTA)
    _draw_city_skyline(d, W, horizon, horizon, (25, 10, 45), NEON_MAGENTA)
    d.rectangle([0, horizon, W, H], fill=(15, 8, 25))
    _draw_grid(d, W, H, horizon)
    for i in range(6):
        y = horizon + 40 + i*80
        d.rectangle([W//2-30, y, W//2+30, y+3], fill=(255, 51, 170))
    d.text((W//2 - 220, 140), "NEON CITY", fill=NEON_MAGENTA)
    d.text((W//2 - 200, 200), "U L T R A   v 4", fill=NEON_CYAN)
    d.text((W//2 - 100, 260), "NIGHT DRIVE", fill=(255,255,255))
    img.save(path, "PNG")
    print(f"PIC1.PNG created: {os.path.getsize(path)} bytes")


def draw_preview(path, idx):
    W, H = 1920, 1080
    img = Image.new('RGB', (W, H), DARK_BG)
    d = ImageDraw.Draw(img)
    horizon = H * 3 // 5

    palettes = [
        ((5, 18, 40), (80, 130, 180), (200, 220, 240)),
        ((8, 4, 18), (60, 20, 90), (140, 60, 120)),
        ((20, 10, 5), (140, 70, 40), (220, 150, 90)),
        ((5, 20, 15), (30, 90, 70), (140, 200, 160)),
        ((15, 5, 30), (80, 30, 120), (180, 100, 200)),
        ((30, 20, 5), (180, 130, 60), (240, 200, 120)),
        ((10, 25, 40), (40, 100, 140), (200, 220, 240)),
        ((5, 10, 30), (70, 40, 120), (200, 120, 180)),
    ]
    top, mid, bottom = palettes[idx % len(palettes)]

    _sky_gradient(d, W, H, top, mid, bottom, horizon)
    _draw_grid(d, W, H, horizon)
    d.polygon([(W//2-180, horizon), (W//2+180, horizon), (W, H), (0, H)], fill=(28, 30, 38))
    for i in range(15):
        y0 = horizon + 40 + i*60
        y1 = y0 + 35
        w = 6 + i
        d.polygon([(W//2-w, y0), (W//2+w, y0), (W//2+w, y1), (W//2-w, y1)], fill=(255, 204, 0))
    _draw_city_skyline(d, W, horizon, horizon, (30, 38, 55), (255, 238, 136))
    random.seed(idx * 31)
    for _ in range(10):
        bx = random.randint(0, W-300)
        by = horizon - random.randint(100, 300)
        bw = random.randint(180, 320)
        d.rectangle([bx, by, bx+bw, horizon], fill=(60, 65, 90))
        for wy in range(by+10, horizon-10, 24):
            for wx in range(bx+10, bx+bw-10, 22):
                if random.random() < 0.4:
                    d.rectangle([wx, wy, wx+12, wy+16], fill=(255, 238, 136))
    cx, cy = W//2, H - 150
    d.ellipse([cx-160, cy+30, cx+160, cy+70], fill=(0,0,0))
    d.rectangle([cx-160, cy-25, cx+160, cy+45], fill=(232, 184, 36))
    d.rectangle([cx-105, cy-80, cx+105, cy-25], fill=(232, 184, 36))
    d.rectangle([cx-95, cy-72, cx-15, cy-30], fill=(48, 64, 80))
    d.rectangle([cx+15, cy-72, cx+95, cy-30], fill=(48, 64, 80))
    d.rectangle([cx-160, cy-5, cx+160, cy+5], fill=(255,255,255))
    d.ellipse([cx-130, cy+20, cx-80, cy+70], fill=(0,0,0))
    d.ellipse([cx+80, cy+20, cx+130, cy+70], fill=(0,0,0))
    d.text((40, 40), "NEON CITY", fill=NEON_CYAN)
    d.text((40, 90), "U L T R A   v 4", fill=NEON_ORANGE)
    d.text((40, H-100), f"SPEED {80 + idx*7} KM/H", fill=(255, 204, 0))
    d.text((40, H-60), f"SCORE: {idx*125}", fill=(46, 204, 113))
    d.rectangle([W-280, 40, W-40, 130], fill=(0, 0, 0))
    d.rectangle([W-278, 42, W-42, 128], fill=(0, 40, 60))
    d.text((W-250, 60), f"PREVIEW {idx+1}", fill=NEON_CYAN)
    d.text((W-250, 95), "1920 x 1080", fill=(255, 255, 255))
    img.save(path, "PNG")
    print(f"{os.path.basename(path)} created: {os.path.getsize(path)} bytes")


def draw_wallpaper_bmp(path, idx):
    """Generate large BMP wallpaper — uncompressed ~6MB per file."""
    W, H = 1920, 1080
    img = Image.new('RGB', (W, H), DARK_BG)
    d = ImageDraw.Draw(img)

    palettes = [
        ((5, 18, 40), (60, 120, 180), (255, 200, 120), "SUNSET DRIVE"),
        ((8, 4, 18), (60, 20, 100), (220, 40, 140), "NEON NIGHT"),
        ((20, 30, 60), (80, 140, 200), (255, 240, 180), "CITY MORNING"),
        ((10, 5, 30), (80, 30, 130), (255, 100, 200), "MIDNIGHT RUN"),
        ((15, 25, 15), (60, 120, 80), (255, 220, 140), "GREEN DISTRICT"),
        ((30, 15, 5), (180, 100, 50), (255, 180, 100), "GOLDEN HOUR"),
        ((8, 20, 40), (30, 100, 150), (100, 200, 255), "BLUE HOUR"),
    ]
    top, mid, bottom, label = palettes[idx % len(palettes)]

    horizon = 620
    _sky_gradient(d, W, H, top, mid, bottom, horizon)

    # stars for night
    if idx in (1, 3):
        for _ in range(800):
            x = random.randint(0, W)
            y = random.randint(0, horizon)
            b = random.randint(150, 255)
            d.point((x, y), fill=(b, b, b))

    # sun/moon
    if idx in (0, 2, 5):
        d.ellipse([1400, 200, 1750, 550], fill=bottom)
        d.ellipse([1480, 280, 1670, 470], fill=(255, 255, 220))
    else:
        d.ellipse([1500, 150, 1700, 350], fill=(240, 240, 255))

    # clouds
    for _ in range(6):
        cx = random.randint(100, W-300)
        cy = random.randint(100, 400)
        cw = random.randint(150, 350)
        d.ellipse([cx, cy, cx+cw, cy+cw//3], fill=(255, 255, 255))
        d.ellipse([cx+cw//3, cy-cw//6, cx+cw, cy+cw//4], fill=(255, 255, 255))

    # skyline
    _draw_city_skyline(d, W, horizon, horizon, (40, 45, 65), (255, 220, 100))

    # ground
    d.rectangle([0, horizon, W, H], fill=(20, 22, 30))
    _draw_grid(d, W, H, horizon)

    # road
    d.polygon([(W//2-250, horizon), (W//2+250, horizon), (W, H), (0, H)], fill=(30, 32, 40))

    # dashed lines
    for i in range(15):
        y0 = horizon + 30 + i*55
        y1 = y0 + 40
        w = 8 + i*2
        d.polygon([(W//2-w, y0), (W//2+w, y0), (W//2+w, y1), (W//2-w, y1)], fill=(255, 204, 0))

    # foreground cars
    cx, cy = W//2, H - 180
    d.ellipse([cx-200, cy+40, cx+200, cy+90], fill=(0,0,0))
    d.rectangle([cx-200, cy-30, cx+200, cy+55], fill=(232, 184, 36))
    d.rectangle([cx-130, cy-100, cx+130, cy-30], fill=(232, 184, 36))
    d.rectangle([cx-120, cy-92, cx-20, cy-38], fill=(48, 64, 80))
    d.rectangle([cx+20, cy-92, cx+120, cy-38], fill=(48, 64, 80))
    d.rectangle([cx-200, cy-5, cx+200, cy+5], fill=(255,255,255))
    d.ellipse([cx-165, cy+30, cx-105, cy+90], fill=(0,0,0))
    d.ellipse([cx+105, cy+30, cx+165, cy+90], fill=(0,0,0))

    # text label
    for dx in range(-3, 4):
        for dy in range(-3, 4):
            if dx*dx + dy*dy > 9: continue
            d.text((W//2 - 250 + dx, 200 + dy), "NEON CITY", fill=(0, 80, 120))
    d.text((W//2 - 250, 200), "NEON CITY", fill=NEON_CYAN)
    d.text((W//2 - 220, 280), label, fill=NEON_ORANGE)
    d.text((W//2 - 250, 340), "U L T R A   v 4", fill=(255, 255, 255))

    # save as BMP (uncompressed — large file!)
    img.save(path, "BMP")
    sz_mb = os.path.getsize(path) / 1024 / 1024
    print(f"{os.path.basename(path)} created: {sz_mb:.2f} MB")


if __name__ == '__main__':
    out_dir = os.environ.get('OUT_DIR', '.')
    os.makedirs(out_dir, exist_ok=True)

    # standard PS3 assets
    draw_icon0(os.path.join(out_dir, 'ICON0.PNG'))
    draw_pic0(os.path.join(out_dir, 'PIC0.PNG'))
    draw_pic1(os.path.join(out_dir, 'PIC1.PNG'))

    # previews (PNG)
    for i in range(8):
        draw_preview(os.path.join(out_dir, f'PREVIEW{i+1}.PNG'), i)

    # wallpapers (BMP — large files to boost PKG size)
    for i in range(7):
        draw_wallpaper_bmp(os.path.join(out_dir, f'WALLPAPER{i+1}.BMP'), i)

    total = 0
    for fn in os.listdir(out_dir):
        if fn.endswith(('.PNG', '.BMP')):
            total += os.path.getsize(os.path.join(out_dir, fn))
    print(f"=== Total images size: {total/1024/1024:.2f} MB ===")
    print("All images generated!")