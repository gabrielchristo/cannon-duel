#!/usr/bin/env python3
"""Gera sprites de munição (64x64) para a loja."""

from __future__ import annotations

import math
import sys
from pathlib import Path

try:
    from PIL import Image, ImageDraw
except ImportError:
    print("Erro: Pillow não instalado.", file=sys.stderr)
    sys.exit(1)

OUT = Path(__file__).resolve().parent.parent / "assets" / "sprites"
SIZE = 64
CX = CY = SIZE / 2


def save(img: Image.Image, name: str) -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    img.save(OUT / name)
    print(f"  {name}")


def make_ice() -> Image.Image:
    img = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    ink = (20, 40, 60, 255)
    ice = (150, 230, 255, 255)
    hi = (240, 250, 255, 255)
    pts = [(CX, 6), (52, 22), (46, 54), (18, 54), (12, 22)]
    d.polygon(pts, fill=ice, outline=ink)
    d.polygon([(CX, 14), (40, 26), (CX, 36), (24, 26)], fill=hi)
    return img


def make_rasengan() -> Image.Image:
    img = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.ellipse([8, 8, 56, 56], fill=(40, 120, 230, 255), outline=(20, 50, 120, 255), width=3)
    d.ellipse([16, 16, 48, 48], fill=(120, 200, 255, 255))
    d.arc([12, 12, 52, 52], 20, 200, fill=(230, 250, 255, 255), width=3)
    d.arc([18, 18, 46, 46], 200, 40, fill=(80, 160, 255, 255), width=2)
    return img


def make_chidori() -> Image.Image:
    img = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.ellipse([14, 14, 50, 50], fill=(160, 230, 255, 255), outline=(40, 90, 160, 255), width=2)
    d.ellipse([22, 22, 42, 42], fill=(240, 250, 255, 255))
    for a in range(0, 360, 45):
        rad = math.radians(a)
        x1 = CX + math.cos(rad) * 10
        y1 = CY + math.sin(rad) * 10
        x2 = CX + math.cos(rad) * 28
        y2 = CY + math.sin(rad) * 28
        d.line([(x1, y1), (x2, y2)], fill=(80, 180, 255, 255), width=2)
    return img


def make_shuriken() -> Image.Image:
    img = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    steel = (200, 205, 215, 255)
    ink = (30, 32, 38, 255)
    pts = []
    for i in range(8):
        ang = math.radians(i * 45 - 90)
        r = 28 if i % 2 == 0 else 10
        pts.append((CX + math.cos(ang) * r, CY + math.sin(ang) * r))
    d.polygon(pts, fill=steel, outline=ink)
    d.ellipse([CX - 6, CY - 6, CX + 6, CY + 6], fill=ink)
    return img


def make_kuromi() -> Image.Image:
    img = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    pink = (255, 90, 180, 255)
    black = (20, 16, 28, 255)
    pts = []
    for i in range(10):
        ang = math.radians(i * 36 - 90)
        r = 28 if i % 2 == 0 else 12
        pts.append((CX + math.cos(ang) * r, CY + math.sin(ang) * r))
    d.polygon(pts, fill=pink, outline=black)
    d.ellipse([CX - 8, CY - 6, CX + 8, CY + 10], fill=(255, 250, 252, 255), outline=black)
    d.ellipse([CX - 4, CY - 2, CX - 1, CY + 2], fill=black)
    d.ellipse([CX + 1, CY - 2, CX + 4, CY + 2], fill=black)
    return img


def make_pride() -> Image.Image:
    img = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    colors = [
        (228, 28, 36, 255), (250, 140, 20, 255), (250, 220, 30, 255),
        (40, 170, 70, 255), (50, 100, 220, 255), (150, 40, 170, 255),
    ]
    box = [6, 6, 58, 58]
    for i, col in enumerate(colors):
        d.pieslice(box, i * 60, (i + 1) * 60, fill=col)
    d.ellipse(box, outline=(20, 16, 22, 255), width=3)
    return img


def digit(d: ImageDraw.ImageDraw, ch: str, x: float, y: float, col) -> None:
    s = 4
    if ch == "6":
        d.rectangle([x, y, x + 3 * s, y + s], fill=col)
        d.rectangle([x, y, x + s, y + 7 * s], fill=col)
        d.rectangle([x, y + 3 * s, x + 3 * s, y + 4 * s], fill=col)
        d.rectangle([x, y + 6 * s, x + 3 * s, y + 7 * s], fill=col)
        d.rectangle([x + 2 * s, y + 3 * s, x + 3 * s, y + 7 * s], fill=col)
    elif ch == "7":
        d.rectangle([x, y, x + 3 * s, y + s], fill=col)
        d.polygon([(x + 3 * s, y), (x + s, y + 7 * s), (x + 2 * s, y + 7 * s), (x + 3 * s, y + s)], fill=col)


def make_67() -> Image.Image:
    img = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.ellipse([4, 4, 60, 60], fill=(255, 220, 50, 255), outline=(40, 28, 10, 255), width=3)
    digit(d, "6", 12, 16, (30, 22, 10, 255))
    digit(d, "7", 34, 16, (30, 22, 10, 255))
    return img


def make_tomato() -> Image.Image:
    img = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.ellipse([10, 14, 54, 58], fill=(210, 40, 36, 255), outline=(80, 16, 14, 255), width=3)
    d.ellipse([16, 18, 30, 30], fill=(255, 90, 70, 180))
    d.polygon([(CX, 8), (CX - 8, 18), (CX + 8, 18)], fill=(40, 140, 50, 255))
    d.rectangle([CX - 2, 6, CX + 2, 16], fill=(70, 50, 20, 255))
    return img


def make_duck() -> Image.Image:
    img = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    yellow = (255, 220, 60, 255)
    ink = (40, 28, 10, 255)
    d.ellipse([10, 24, 54, 56], fill=yellow, outline=ink, width=2)
    d.ellipse([28, 8, 56, 36], fill=yellow, outline=ink, width=2)
    d.ellipse([44, 16, 50, 22], fill=ink)
    d.polygon([(52, 20), (62, 24), (52, 28)], fill=(255, 140, 40, 255), outline=ink)
    return img


def make_dildo() -> Image.Image:
    img = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    ink = (90, 50, 125, 255)
    shaft = (214, 176, 236, 255)
    shaft_d = (176, 128, 210, 255)
    glans = (205, 145, 215, 255)
    glans_d = (170, 100, 185, 255)
    hi = (246, 232, 255, 230)

    d.ellipse([1, 12, 27, 36], fill=ink)
    d.ellipse([1, 28, 27, 52], fill=ink)
    d.ellipse([3, 14, 25, 34], fill=shaft_d)
    d.ellipse([3, 30, 25, 50], fill=shaft_d)
    d.ellipse([8, 18, 16, 26], fill=hi)
    d.ellipse([8, 34, 16, 42], fill=hi)

    d.rounded_rectangle([14, 21, 46, 43], radius=9, fill=ink)
    d.rounded_rectangle([16, 23, 44, 41], radius=8, fill=shaft)
    d.ellipse([20, 24, 40, 31], fill=hi)
    d.line([(22, 37), (40, 35)], fill=shaft_d, width=2)

    d.ellipse([40, 17, 54, 47], fill=glans_d, outline=ink, width=2)
    d.ellipse([44, 15, 63, 49], fill=glans, outline=ink, width=2)
    d.ellipse([50, 19, 58, 28], fill=hi)
    d.line([(58, 26), (58, 38)], fill=ink, width=2)
    return img


def make_nuclear() -> Image.Image:
    img = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.ellipse([10, 14, 54, 58], fill=(36, 36, 40, 255), outline=(12, 12, 14, 255), width=3)
    d.ellipse([24, 6, 40, 20], fill=(50, 50, 56, 255), outline=(12, 12, 14, 255), width=2)
    yel = (255, 220, 40, 255)
    d.pieslice([20, 22, 44, 46], -30, 30, fill=yel)
    d.pieslice([20, 22, 44, 46], 90, 150, fill=yel)
    d.pieslice([20, 22, 44, 46], 210, 270, fill=yel)
    d.ellipse([28, 30, 36, 38], fill=(36, 36, 40, 255))
    return img


GENERATORS = (
    ("ammo_ice.png", make_ice),
    ("ammo_rasengan.png", make_rasengan),
    ("ammo_chidori.png", make_chidori),
    ("ammo_shuriken.png", make_shuriken),
    ("ammo_kuromi.png", make_kuromi),
    ("ammo_pride.png", make_pride),
    ("ammo_67.png", make_67),
    ("ammo_tomato.png", make_tomato),
    ("ammo_duck.png", make_duck),
    ("ammo_dildo.png", make_dildo),
    ("ammo_nuclear.png", make_nuclear),
)


def main() -> int:
    print(f"Gerando munições em {OUT} …")
    for name, fn in GENERATORS:
        save(fn(), name)
    print("Concluído.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
