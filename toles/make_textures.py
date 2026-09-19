#!/usr/bin/env python3
# =====================================================================
#  FILE: toles/make_textures.py
#  PROJECT: NEON CITY ULTRA v4
#  DESCRIPTION: Generate 20 realistic 1024x1024 textures (60MB total)
#    These textures are USED by the game as building/car/wall textures
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

    if kind == 0:  # asphalt road
        n = np.random.randn(H, W) * 18
        r = 45 + n
        g = 45 + n
        b = 50 + n * 0.8
    elif kind == 1:  # brick wall
        bx = (x / 32).astype(int)
        by = (y / 16).astype(int)
        off = (bx % 2) * 16
        xx = (x + off) % 32
        yy = y % 16
        mortar = (xx < 2) | (yy < 2)
        r = np.where(mortar, 60, 160 + (bx * 11 + by * 7) % 40)
        g = np.where(mortar, 60, 80 + (bx * 13 + by * 3) % 30)
        b = np.where(mortar, 60, 60 + (bx * 17 + by * 5) % 20)
    elif kind == 2:  # grass
        n = np.random.randn(H, W) * 25
        r = 30 + n * 0.5
        g = 90 + n
        b = 30 + n * 0.5
    elif kind == 3:  # concrete
        n = np.random.randn(H, W) * 12
        r = 140 + n
        g = 140 + n
        b = 145 + n
    elif kind == 4:  # wood
        waves = np.sin(y / 30) * 20 + np.sin(x / 50) * 10
        n = np.random.randn(H, W) * 8
        r = 120 + waves + n
        g = 70 + waves * 0.5 + n
        b = 40 + n
    elif kind == 5:  # metal
        grad = (x / W) * 40
        n = np.random.randn(H, W) * 5
        r = 100 + grad + n
        g = 105 + grad + n
        b = 110 + grad + n
    elif kind == 6:  # marble
        n1 = np.sin(x / 100) * 30
        n2 = np.cos(y / 80) * 30
        base = 180 + n1 + n2
        r = base
        g = base * 0.98
        b = base * 1.02
    elif kind == 7:  # tile
        tx = (x / 64).astype(int)
        ty = (y / 64).astype(int)
        xx = x % 64
        yy = y % 64
        grout = (xx < 2) | (yy < 2)
        r = np.where(grout, 100, 180 + (tx * 7 + ty * 3) % 40)
        g = np.where(grout, 100, 180 + (tx * 11 + ty * 5) % 40)
        b = np.where(grout, 100, 190 + (tx * 13 + ty * 17) % 40)
    elif kind == 8:  # rust
        n = np.random.randn(H, W) * 30
        spots = np.sin(x / 15) * np.cos(y / 12)
        r = 130 + n + spots * 40
        g = 70 + n * 0.5 + spots * 20
        b = 40 + n * 0.3
    else:  # neon glow
        n = np.random.randn(H, W) * 10
        r = 60 + n + np.sin(x / 20) * 30
        g = 40 + n + np.cos(y / 25) * 30
        b = 100 + n + np.sin((x + y) / 30) * 40

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
        print(f'texture{i:02d}.png: {sz / 1024 / 1024:.2f} MB')
    print(f'=== Total: {total / 1024 / 1024:.2f} MB ===')