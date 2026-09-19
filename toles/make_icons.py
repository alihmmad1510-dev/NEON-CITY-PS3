#!/usr/bin/env python3
# =====================================================================
#  FILE: toles/make_icons.py
#  PROJECT: NEON CITY ULTRA v4
#  DESCRIPTION: Convert ICON.PNG + WALLPAPER.PNG (from repo root)
# =====================================================================

from PIL import Image, ImageDraw
import os

OUT_DIR = os.environ.get('OUT_DIR', '.')

ICON_NAMES = [
    'ICON.PNG', 'icon.png', 'Icon.png', 'ICON.png', 'icon.PNG',
    'assets/ICON.PNG', 'assets/icon.png',
]

WALL_NAMES = [
    'WALLPAPER.PNG', 'wallpaper.png', 'Wallpaper.png',
    'WALLPAPER.png', 'wallpaper.PNG',
    'assets/WALLPAPER.PNG', 'assets/wallpaper.png',
]


def find_file(names):
    for n in names:
        if os.path.isfile(n):
            return n
    return None


def convert_image(src, dst, size):
    try:
        img = Image.open(src).convert('RGB')
        tw, th = size
        ir = img.width / img.height
        tr = tw / th
        if ir > tr:
            nw = tw
            nh = int(tw / ir)
        else:
            nh = th
            nw = int(th * ir)
        img = img.resize((nw, nh), Image.LANCZOS)
        canvas = Image.new('RGB', (tw, th), (0, 0, 0))
        canvas.paste(img, ((tw - nw) // 2, (th - nh) // 2))
        canvas.save(dst, 'PNG')
        print(f"  OK: {src} -> {dst} ({tw}x{th})")
        return True
    except Exception as e:
        print(f"  ERR: {src}: {e}")
        return False


def draw_default_icon(path):
    W, H = 512, 288
    img = Image.new('RGB', (W, H), (10, 15, 26))
    d = ImageDraw.Draw(img)
    horizon = int(H * 0.62)
    for y in range(horizon):
        t = y / horizon
        d.line([(0, y), (W, y)], fill=(int(6+t*40), int(15+t*90), int(35+t*140)))
    cx = W // 2
    for i in range(-25, 26):
        d.line([(cx, horizon), (cx + i*40, H)], fill=(0, 60, 100))
    d.line([(0, horizon), (W, horizon)], fill=(0, 212, 255), width=3)
    d.polygon([(W//2-30, horizon), (W//2+30, horizon), (W-60, H), (60, H)],
              fill=(28, 30, 38))
    for i in range(5):
        y0 = horizon + 15 + i*22
        y1 = y0 + 14
        w = 3 + i
        d.polygon([(W//2-w, y0), (W//2+w, y0), (W//2+w, y1), (W//2-w, y1)],
                  fill=(255, 204, 0))
    cy = H - 62
    d.ellipse([cx-60, cy+18, cx+60, cy+32], fill=(0,0,0))
    d.rectangle([cx-62, cy-8, cx+62, cy+20], fill=(232, 184, 36))
    d.rectangle([cx-42, cy-32, cx+42, cy-8], fill=(232, 184, 36))
    d.rectangle([cx-38, cy-28, cx-8, cy-12], fill=(48, 64, 80))
    d.rectangle([cx+8, cy-28, cx+38, cy-12], fill=(48, 64, 80))
    d.rectangle([cx-62, cy-2, cx+62, cy+2], fill=(255, 255, 255))
    d.ellipse([cx-50, cy+12, cx-34, cy+28], fill=(0,0,0))
    d.ellipse([cx+34, cy+12, cx+50, cy+28], fill=(0,0,0))
    for dx in range(-3, 4):
        for dy in range(-3, 4):
            if dx*dx + dy*dy > 9: continue
            d.text((W//2 - 90 + dx, 22 + dy), "NEON CITY", fill=(0, 60, 110))
    d.text((W//2 - 90, 22), "NEON CITY", fill=(0, 212, 255))
    d.text((W//2 - 68, 42), "U L T R A", fill=(255, 107, 53))
    img.save(path, "PNG")
    print(f"  Default icon generated")


if __name__ == '__main__':
    out_dir = os.environ.get('OUT_DIR', '.')
    os.makedirs(out_dir, exist_ok=True)

    icon_src = find_file(ICON_NAMES)
    if icon_src:
        print(f"Using icon: {icon_src}")
        convert_image(icon_src, os.path.join(out_dir, 'ICON0.PNG'), (512, 288))
    else:
        print("No user icon found - using default")
        draw_default_icon(os.path.join(out_dir, 'ICON0.PNG'))

    wall_src = find_file(WALL_NAMES)
    if wall_src:
        print(f"Using wallpaper: {wall_src}")
        convert_image(wall_src, os.path.join(out_dir, 'PIC0.PNG'), (1920, 1080))
        convert_image(wall_src, os.path.join(out_dir, 'PIC1.PNG'), (1920, 1080))
    else:
        print("No wallpaper - using icon as fallback")
        icon_out = os.path.join(out_dir, 'ICON0.PNG')
        if os.path.isfile(icon_out):
            convert_image(icon_out, os.path.join(out_dir, 'PIC0.PNG'), (1920, 1080))
            convert_image(icon_out, os.path.join(out_dir, 'PIC1.PNG'), (1920, 1080))

    print("\n=== Final icons ===")
    for f in ['ICON0.PNG', 'PIC0.PNG', 'PIC1.PNG']:
        p = os.path.join(out_dir, f)
        if os.path.isfile(p):
            print(f"  {f}: {os.path.getsize(p)/1024:.1f} KB")