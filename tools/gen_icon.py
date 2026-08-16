#!/usr/bin/env python3
"""Gera ícones Android do Cannon Duel (PNG legado, não adaptativo).

Desenha um canhão estilizado sobre fundo circular com gradiente e exporta
nas cinco densidades padrão do Android (mdpi … xxxhdpi).

Dependências:
    pip install -r tools/requirements.txt

Uso:
    python3 tools/gen_icon.py
    python3 tools/gen_icon.py --output-dir android/res
"""

from __future__ import annotations

import argparse
import math
import sys
from pathlib import Path

try:
    from PIL import Image, ImageDraw
except ImportError:
    print("Erro: Pillow não instalado. Rode: pip install -r tools/requirements.txt", file=sys.stderr)
    sys.exit(1)

SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_OUT = SCRIPT_DIR.parent / "android" / "res"
MASTER_SIZE = 512  # desenha grande e reduz depois para nitidez

DENSITIES: dict[str, int] = {
    "mipmap-mdpi": 48,
    "mipmap-hdpi": 72,
    "mipmap-xhdpi": 96,
    "mipmap-xxhdpi": 144,
    "mipmap-xxxhdpi": 192,
}


def make_icon(size: int) -> Image.Image:
    """Renderiza um ícone quadrado com canhão estilizado."""
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    # Fundo circular com gradiente (paleta do céu diurno do jogo).
    for y in range(size):
        t = y / size
        r = int(255 * (1 - t) + 230 * t)
        g = int(200 * (1 - t) + 150 * t)
        b = int(120 * (1 - t) + 70 * t)
        draw.line([(0, y), (size, y)], fill=(r, g, b, 255))

    mask = Image.new("L", (size, size), 0)
    ImageDraw.Draw(mask).ellipse([0, 0, size, size], fill=255)
    img.putalpha(mask)

    cx, cy = size * 0.46, size * 0.58
    r = size * 0.30
    shadow_col = (0, 0, 0, 70)
    body_col = (55, 110, 200, 255)
    dark_col = (30, 60, 120, 255)
    outline_w = max(2, int(size * 0.012))

    draw.ellipse([cx - r * 1.05, cy + r * 0.55, cx + r * 1.05, cy + r * 0.85], fill=shadow_col)
    draw.rounded_rectangle(
        [cx - r * 1.1, cy + r * 0.25, cx + r * 1.1, cy + r * 0.65],
        radius=r * 0.2,
        fill=dark_col,
    )
    body_rect = [cx - r * 0.95, cy - r * 0.55, cx + r * 0.95, cy + r * 0.35]
    draw.rounded_rectangle(body_rect, radius=r * 0.3, fill=body_col)
    draw.rounded_rectangle(body_rect, radius=r * 0.3, outline=(15, 30, 60, 255), width=outline_w)

    turret_rect = [cx - r * 0.55, cy - r * 1.05, cx + r * 0.55, cy - r * 0.05]
    draw.ellipse(turret_rect, fill=body_col)
    draw.ellipse(turret_rect, outline=(15, 30, 60, 255), width=outline_w)

    angle = math.radians(35)
    barrel_len = r * 1.5
    bx0, by0 = cx, cy - r * 0.55
    bx1 = bx0 + math.cos(angle) * barrel_len
    by1 = by0 - math.sin(angle) * barrel_len
    draw.line([bx0, by0, bx1, by1], fill=(25, 25, 25, 255), width=max(3, int(size * 0.09)))
    draw.line([bx0, by0, bx1, by1], fill=(80, 80, 80, 255), width=max(2, int(size * 0.05)))
    tip = size * 0.02
    draw.ellipse([bx1 - tip, by1 - tip, bx1 + tip, by1 + tip], fill=(15, 15, 15, 255))

    highlight = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    ImageDraw.Draw(highlight).ellipse(
        [cx - r * 0.35, cy - r * 0.95, cx - r * 0.05, cy - r * 0.55],
        fill=(255, 255, 255, 70),
    )
    img = Image.alpha_composite(img, highlight)

    ImageDraw.Draw(img).ellipse(
        [1, 1, size - 2, size - 2],
        outline=(30, 20, 10, 200),
        width=max(2, int(size * 0.015)),
    )
    return img


def generate_icons(output_dir: Path) -> list[Path]:
    """Gera ic_launcher.png para cada densidade Android."""
    master = make_icon(MASTER_SIZE)
    written: list[Path] = []

    for folder, px in DENSITIES.items():
        out_path = output_dir / folder / "ic_launcher.png"
        out_path.parent.mkdir(parents=True, exist_ok=True)
        master.resize((px, px), Image.LANCZOS).save(out_path)
        written.append(out_path)
        print(f"  {out_path.relative_to(output_dir.parent.parent)} ({px}×{px})")

    return written


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Gera ícones Android do Cannon Duel.")
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=DEFAULT_OUT,
        help=f"Diretório base android/res (padrão: {DEFAULT_OUT})",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    output_dir = args.output_dir.resolve()
    print(f"Gerando ícones em {output_dir} …")
    generate_icons(output_dir)
    print("Concluído.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
