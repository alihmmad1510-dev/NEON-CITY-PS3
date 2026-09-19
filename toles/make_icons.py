#!/usr/bin/env python3
# FILE: toles/make_icons.py
from PIL import Image, ImageDraw
import os

OUT_DIR = os.environ.get('OUT_DIR', '.')
ICON_NAMES = ['ICON.PNG', 'icon.png', 'Icon.png', 'assets/ICON.PNG', 'assets/icon.png']
WALL_NAMES = ['WALLPAPER.PNG', 'wallpaper.png', 'assets/WALLPAPER.PNG', 'assets/wallpaper.png']

def find_file(names):
    for n in names:
        if os.path.isfile(n): return n
    return None

def convert_image(src, dst, size):
    try:
        img = Image.open(src).convert('RGB')
        tw, th = size
        ir = img.width / img.height
        tr = tw / th
        if ir > tr: nw, nh = tw, int(tw / ir)
        else: nh, nw = th, int(th * ir)
        img = img.resize((nw, nh), Image.LANCZOS)
        canvas = Image.new('RGB', (tw, th), (0,0,0))
        canvas.paste(img, ((tw-nw)//2, (th-nh)//2))
        canvas.save(dst, 'PNG')
        print(f"  OK: {src} -> {dst}")
        return True
    except Exception as e:
        print(f"  ERR: {e}")
        return False

def draw_default_icon(path):
    W, H = 512, 288
    img = Image.new('RGB', (W,H), (10,15,26))
    d = ImageDraw.Draw(img)
    h = int(H*0.62)
    for y in range(h):
        t = y/h
        d.line([(0,y),(W,y)], fill=(int(6+t*40), int(15+t*90), int(35+t*140)))
    cx = W//2
    for i in range(-25,26): d.line([(cx,h),(cx+i*40,H)], fill=(0,60,100))
    d.line([(0,h),(W,h)], fill=(0,212,255), width=3)
    img.save(path, 'PNG')
    print(f"  Default icon")

if __name__ == '__main__':
    os.makedirs(OUT_DIR, exist_ok=True)

    icon_src = find_file(ICON_NAMES)
    if icon_src:
        print(f"Using icon: {icon_src}")
        convert_image(icon_src, os.path.join(OUT_DIR, 'ICON0.PNG'), (512, 288))
    else:
        draw_default_icon(os.path.join(OUT_DIR, 'ICON0.PNG'))

    wall_src = find_file(WALL_NAMES)
    if wall_src:
        print(f"Using wallpaper: {wall_src}")
        convert_image(wall_src, os.path.join(OUT_DIR, 'PIC0.PNG'), (1920, 1080))
        convert_image(wall_src, os.path.join(OUT_DIR, 'PIC1.PNG'), (1920, 1080))
    else:
        icon_out = os.path.join(OUT_DIR, 'ICON0.PNG')
        if os.path.isfile(icon_out):
            convert_image(icon_out, os.path.join(OUT_DIR, 'PIC0.PNG'), (1920, 1080))
            convert_image(icon_out, os.path.join(OUT_DIR, 'PIC1.PNG'), (1920, 1080))