#!/usr/bin/env python3
# =====================================================================
#  FILE: toles/make_textures.py
#  PROJECT: NEON CITY ULTRA v4
#  DESCRIPTION: Generate 20 x 1024x1024 procedural textures (~60MB)
# =====================================================================

import os
import sys
import numpy as np
from PIL import Image

W, H = 1024, 1024
OUT_DIR = os.environ.get('OUT_DIR', '.')


def gen_texture(idx):
    y, x = np.mgrid[0:H, 0:W].astype(np.float32)
    kind = idx % 10

    if kind == 0:
        n = np.random.randn(H, W) * 20
        r = 42 + n; g = 42 + n; b = 48 + n * 0.8
    elif kind == 1:
        bx = (x / 32).astype(int); by = (y / 16).astype(int)
        off = (bx % 2) * 16; xx = (x + off) % 32; yy = y % 16
        mortar = (xx < 2) | (yy < 2)
        r = np.where(mortar, 60, 165 + (bx*11 + by*7) % 40)
        g = np.where(mortar, 60, 85 + (bx*13 + by*3) % 30)
        b = np.where(mortar, 60, 65 + (bx*17 + by*5) % 20)
    elif kind == 2:
        n = np.random.randn(H, W) * 28
        r = 32 + n * 0.5; g = 95 + n; b = 32 + n * 0.5
    elif kind == 3:
        n = np.random.randn(H, W) * 14
        r = 142 + n; g = 142 + n; b = 148 + n
    elif kind == 4:
        waves = np.sin(y/30)*20 + np.sin(x/50)*10
        n = np.random.randn(H, W) * 10
        r = 125 + waves + n; g = 72 + waves*0.5 + n; b = 42 + n
    elif kind == 5:
        grad = (x / W) * 50
        n = np.random.randn(H, W) * 6
        r = 105 + grad + n; g = 110 + grad + n; b = 118 + grad + n
    elif kind == 6:
        n1 = np.sin(x/100)*35; n2 = np.cos(y/80)*35
        base = 185 + n1 + n2
        r = base; g = base*0.98; b = base*1.02
    elif kind == 7:
        tx = (x/64).astype(int); ty = (y/64).astype(int)
        xx = x%64; yy = y%64
        grout = (xx < 2) | (yy < 2)
        r = np.where(grout, 100, 185 + (tx*7 + ty*3) % 40)
        g = np.where(grout, 100, 185 + (tx*11 + ty*5) % 40)
        b = np.where(grout, 100, 195 + (tx*13 + ty*17) % 40)
    elif kind == 8:
        n = np.random.randn(H, W) * 35
        spots = np.sin(x/15) * np.cos(y/12)
        r = 135 + n + spots*45; g = 75 + n*0.5 + spots*22; b = 45 + n*0.3
    else:
        n = np.random.randn(H, W) * 12
        r = 65 + n + np.sin(x/20)*35
        g = 45 + n + np.cos(y/25)*35
        b = 110 + n + np.sin((x+y)/30)*45

    result = np.stack([r, g, b], axis=-1)
    return np.clip(result, 0, 255).astype(np.uint8)


if __name__ == '__main__':
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 20
    os.makedirs(OUT_DIR, exist_ok=True)
    total = 0
    for i in range(n):
        data = gen_texture(i)
        path = os.path.join(OUT_DIR, f'texture{i:02d}.png')
        Image.fromarray(data).save(path, optimize=False)
        sz = os.path.getsize(path)
        total += sz
        print(f'texture{i:02d}.png: {sz/1024/1024:.2f} MB')
    print(f'=== TOTAL: {total/1024/1024:.2f} MB ===')