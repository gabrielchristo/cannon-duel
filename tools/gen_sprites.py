#!/usr/bin/env python3
"""Gera sprites placeholder do Cannon Duel (formas geométricas procedurais).

Produz canhões, projétil, partículas, tile de terreno e backgrounds
diurno/noturno. Placeholders originais — sem assets de terceiros.

Dependências:
    pip install -r tools/requirements.txt

Uso:
    python3 tools/gen_sprites.py
    python3 tools/gen_sprites.py --output-dir assets/sprites --no-retro
"""

from __future__ import annotations

import argparse
import math
import random
import sys
from pathlib import Path
from typing import Optional

try:
    from PIL import Image, ImageDraw, ImageFilter
except ImportError:
    print("Erro: Pillow não instalado. Rode: pip install -r tools/requirements.txt", file=sys.stderr)
    sys.exit(1)

SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_OUT = SCRIPT_DIR.parent / "assets" / "sprites"
BG_WIDTH, BG_HEIGHT = 1280, 720


def quantize_retro(img: Image.Image, n_colors: int = 16) -> Image.Image:
    """Reduz a paleta (estilo 16-bit), preservando o canal alfa."""
    if img.mode != "RGBA":
        img = img.convert("RGBA")
    alpha = img.split()[3]
    rgb = img.convert("RGB")
    quant = rgb.quantize(colors=n_colors, method=Image.MEDIANCUT).convert("RGB")
    quant.putalpha(alpha)
    return quant


def save_sprite(
    img: Image.Image,
    output_dir: Path,
    name: str,
    *,
    retro: bool = True,
    n_colors: int = 16,
) -> Path:
    """Salva PNG em output_dir, opcionalmente com quantização retro."""
    if retro:
        img = quantize_retro(img, n_colors)
    path = output_dir / name
    img.save(path)
    print(f"  {name}")
    return path


def make_cannon(
    color_body: tuple[int, int, int, int],
    color_dark: tuple[int, int, int, int],
    *,
    flip: bool = False,
    size: int = 128,
) -> Image.Image:
    """Desenha um tanque estilizado (corpo + torre + cano + esteiras)."""
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    cx, cy = size * 0.42, size * 0.62
    outline = (28, 28, 32, 255)

    # Sombra no chão
    draw.ellipse([cx - 48, cy + 24, cx + 48, cy + 36], fill=(0, 0, 0, 70))

    # Esteiras
    draw.rounded_rectangle([cx - 46, cy + 4, cx + 46, cy + 28], radius=11, fill=color_dark)
    draw.rounded_rectangle([cx - 44, cy + 6, cx + 44, cy + 26], radius=10, fill=(38, 38, 42, 255))
    for i in range(-3, 4):
        wx = cx + i * 13
        draw.rounded_rectangle([wx - 5, cy + 9, wx + 5, cy + 23], radius=3, fill=(52, 52, 58, 255))
        draw.line([wx, cy + 10, wx, cy + 22], fill=(70, 70, 76, 180), width=1)

    # Casco — camadas pra dar volume
    body_rect = [cx - 40, cy - 24, cx + 40, cy + 8]
    draw.rounded_rectangle(body_rect, radius=14, fill=color_dark)
    inner = [cx - 36, cy - 20, cx + 36, cy + 4]
    draw.rounded_rectangle(inner, radius=12, fill=color_body)
    highlight = [cx - 28, cy - 18, cx + 10, cy - 6]
    draw.rounded_rectangle(highlight, radius=8, fill=_tint(color_body, 1.18, 1.18, 1.18, 90))
    draw.rounded_rectangle(body_rect, radius=14, outline=outline, width=2)

    # Torre
    turret_rect = [cx - 24, cy - 44, cx + 24, cy - 6]
    draw.ellipse(turret_rect, fill=color_dark)
    draw.ellipse([cx - 20, cy - 40, cx + 20, cy - 10], fill=color_body)
    draw.ellipse([cx - 12, cy - 36, cx - 2, cy - 24], fill=(255, 255, 255, 55))
    draw.ellipse(turret_rect, outline=outline, width=2)

    # Cano (duas camadas + boca)
    barrel_len = 56
    bx0, by0 = cx, cy - 24
    bx1 = bx0 + (-barrel_len if flip else barrel_len)
    by1 = by0 - 28
    draw.line([bx0, by0, bx1, by1], fill=(32, 32, 36, 255), width=13)
    draw.line([bx0, by0, bx1, by1], fill=(88, 90, 96, 255), width=7)
    draw.line([bx0, by0, bx1, by1], fill=(120, 122, 128, 255), width=3)
    draw.ellipse([bx1 - 6, by1 - 6, bx1 + 6, by1 + 6], fill=(22, 22, 26, 255))
    draw.ellipse([bx1 - 3, by1 - 3, bx1 + 3, by1 + 3], fill=(50, 50, 55, 255))

    return img


def _tint(rgba: tuple[int, int, int, int], rr: float, gg: float, bb: float, alpha: int) -> tuple[int, int, int, int]:
    r, g, b, _a = rgba
    return (
        min(255, int(r * rr)),
        min(255, int(g * gg)),
        min(255, int(b * bb)),
        alpha,
    )


def make_projectile() -> Image.Image:
    img = Image.new("RGBA", (24, 24), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    draw.ellipse([2, 2, 22, 22], fill=(25, 25, 25, 255))
    draw.ellipse([5, 5, 12, 12], fill=(90, 90, 90, 180))
    return img


def make_particle_glow() -> Image.Image:
    img = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    for radius, alpha in [(15, 255), (11, 220), (7, 180), (3, 255)]:
        color = (255, 200 - radius * 4, 40, alpha) if radius > 3 else (255, 255, 220, 255)
        draw.ellipse([16 - radius, 16 - radius, 16 + radius, 16 + radius], fill=color)
    return img.filter(ImageFilter.GaussianBlur(1.2))


def make_terrain_tile(width: int = 64, height: int = 64) -> Image.Image:
    img = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    draw.rectangle([0, 0, width, height], fill=(101, 67, 33, 255))

    rng = random.Random(7)
    for _ in range(140):
        x, y = rng.randint(0, width - 1), rng.randint(0, height - 1)
        shade = rng.randint(-18, 18)
        color = (
            max(0, min(255, 101 + shade)),
            max(0, min(255, 67 + shade)),
            max(0, min(255, 33 + shade)),
            255,
        )
        draw.point((x, y), fill=color)

    draw.rectangle([0, 0, width, 6], fill=(76, 153, 60, 255))
    for _ in range(40):
        x = rng.randint(0, width - 1)
        y = rng.randint(0, 5)
        color = (60 + rng.randint(-10, 20), 140 + rng.randint(-10, 20), 50, 255)
        draw.point((x, y), fill=color)
    return img


def _draw_hills(
    draw: ImageDraw.ImageDraw,
    width: int,
    height: int,
    base_y: float,
    amplitude: float,
    color: tuple[int, ...],
    *,
    seed: int,
    phase: float,
) -> None:
    rng = random.Random(seed)
    points: list[tuple[float, float]] = [(0, height)]
    point_count = 10
    for i in range(point_count + 1):
        x = width * i / point_count
        y = base_y + math.sin(i * phase + 2) * amplitude + rng.randint(-10, 10)
        points.append((x, y))
    points.append((width, height))
    draw.polygon(points, fill=color)


def make_background_day() -> Image.Image:
    img = Image.new("RGBA", (BG_WIDTH, BG_HEIGHT), (0, 0, 0, 255))
    top, bottom = (255, 200, 150), (255, 235, 205)
    for y in range(BG_HEIGHT):
        t = y / BG_HEIGHT
        color = (
            int(top[0] * (1 - t) + bottom[0] * t),
            int(top[1] * (1 - t) + bottom[1] * t),
            int(top[2] * (1 - t) + bottom[2] * t),
            255,
        )
        ImageDraw.Draw(img).line([(0, y), (BG_WIDTH, y)], fill=color)

    draw = ImageDraw.Draw(img, "RGBA")
    draw.ellipse([BG_WIDTH - 220, 60, BG_WIDTH - 100, 180], fill=(255, 221, 130, 255))
    for radius in range(90, 60, -6):
        draw.ellipse(
            [BG_WIDTH - 160 - radius, 120 - radius, BG_WIDTH - 160 + radius, 120 + radius],
            outline=(255, 221, 130, 30),
            width=4,
        )

    _draw_hills(draw, BG_WIDTH, BG_HEIGHT, 430, 34, (222, 196, 168, 200), seed=3, phase=1.3)
    _draw_hills(draw, BG_WIDTH, BG_HEIGHT, 475, 24, (205, 175, 145, 220), seed=3, phase=1.3)

    for cx, cy, scale in [(180, 120, 1.0), (520, 90, 0.7), (900, 150, 1.2), (1050, 70, 0.6)]:
        for dx, dy, radius in [(0, 0, 26), (24, 4, 20), (-22, 6, 18), (10, -10, 16)]:
            rr = radius * scale
            draw.ellipse(
                [cx + dx - rr, cy + dy - rr, cx + dx + rr, cy + dy + rr],
                fill=(255, 255, 255, 160),
            )
    return img


def make_background_night() -> Image.Image:
    img = Image.new("RGBA", (BG_WIDTH, BG_HEIGHT), (0, 0, 0, 255))
    top, bottom = (18, 22, 48), (55, 45, 80)
    for y in range(BG_HEIGHT):
        t = y / BG_HEIGHT
        color = (
            int(top[0] * (1 - t) + bottom[0] * t),
            int(top[1] * (1 - t) + bottom[1] * t),
            int(top[2] * (1 - t) + bottom[2] * t),
            255,
        )
        ImageDraw.Draw(img).line([(0, y), (BG_WIDTH, y)], fill=color)

    draw = ImageDraw.Draw(img, "RGBA")
    rng = random.Random(11)
    for _ in range(220):
        x = rng.randint(0, BG_WIDTH - 1)
        y = rng.randint(0, int(BG_HEIGHT * 0.62))
        s = rng.choice([1, 1, 1, 2, 2, 3])
        draw.ellipse([x - s, y - s, x + s, y + s], fill=(255, 255, 240, rng.randint(120, 255)))
    for _ in range(14):
        x = rng.randint(0, BG_WIDTH - 1)
        y = rng.randint(0, int(BG_HEIGHT * 0.5))
        draw.ellipse([x - 3, y - 3, x + 3, y + 3], fill=(255, 255, 255, 220))

    mx, my, mr = 310, 120, 52
    draw.ellipse([mx - mr, my - mr, mx + mr, my + mr], fill=(240, 235, 210, 255))
    for cx, cy, cr in [(-14, -10, 9), (10, 8, 7), (-4, 18, 5), (18, -16, 6)]:
        draw.ellipse([mx + cx - cr, my + cy - cr, mx + cx + cr, my + cy + cr], fill=(215, 208, 185, 180))
    for radius in range(70, 52, -6):
        draw.ellipse([mx - radius, my - radius, mx + radius, my + radius], outline=(240, 235, 210, 18), width=6)

    _draw_hills(draw, BG_WIDTH, BG_HEIGHT, 450, 30, (30, 26, 48, 255), seed=11, phase=1.7)
    _draw_hills(draw, BG_WIDTH, BG_HEIGHT, 495, 22, (18, 16, 32, 255), seed=11, phase=1.7)
    return img


# cannon_left/right = casco padrão branco. Demais = cores da loja (cannon_{cor}.png).
CANNON_WHITE_BODY = (248, 250, 252, 255)
CANNON_WHITE_DARK = (198, 206, 218, 255)

CANNON_PALETTES: tuple[tuple[tuple[int, int, int, int], tuple[int, int, int, int], bool, str], ...] = (
    (CANNON_WHITE_BODY, CANNON_WHITE_DARK, False, "cannon_left.png"),
    (CANNON_WHITE_BODY, CANNON_WHITE_DARK, True, "cannon_right.png"),
    ((55, 115, 220, 255), (35, 75, 145, 255), False, "cannon_blue.png"),
    ((35, 175, 195, 255), (20, 120, 140, 255), False, "cannon_cyan.png"),
    ((80, 200, 120, 255), (45, 140, 80, 255), False, "cannon_green.png"),
    ((155, 75, 195, 255), (105, 45, 140, 255), False, "cannon_purple.png"),
    ((215, 65, 55, 255), (150, 35, 30, 255), False, "cannon_red.png"),
    ((235, 135, 45, 255), (180, 95, 30, 255), False, "cannon_orange.png"),
    ((220, 180, 60, 255), (170, 130, 25, 255), False, "cannon_gold.png"),
    ((45, 48, 55, 255), (22, 24, 28, 255), False, "cannon_black.png"),
)


def generate_sprites(output_dir: Path, *, retro: bool) -> None:
    """Gera todos os sprites placeholder."""
    output_dir.mkdir(parents=True, exist_ok=True)

    def save(img: Image.Image, name: str, *, use_retro: Optional[bool] = None, n_colors: int = 16) -> None:
        save_sprite(img, output_dir, name, retro=retro if use_retro is None else use_retro, n_colors=n_colors)

    for body, dark, flip, filename in CANNON_PALETTES:
        save(make_cannon(body, dark, flip=flip), filename)

    save(make_projectile(), "projectile.png")
    save(make_particle_glow(), "particle_glow.png", use_retro=False)
    save(make_terrain_tile(), "terrain_tile.png")
    save(make_background_day(), "background.png", n_colors=32)
    save(make_background_night(), "background_night.png", n_colors=32)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Gera sprites placeholder do Cannon Duel.")
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=DEFAULT_OUT,
        help=f"Diretório de saída (padrão: {DEFAULT_OUT})",
    )
    parser.add_argument(
        "--no-retro",
        action="store_true",
        help="Desativa quantização de paleta (16-bit)",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    output_dir = args.output_dir.resolve()
    print(f"Gerando sprites em {output_dir} …")
    generate_sprites(output_dir, retro=not args.no_retro)
    print("Concluído.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
