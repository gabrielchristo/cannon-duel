#!/usr/bin/env python3
"""Gera overlays de skin — acessórios grandes e legíveis sobre o canhão.

Uso:
    python3 tools/gen_skins.py
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

try:
    from PIL import Image, ImageDraw
except ImportError:
    print("Erro: Pillow não instalado.", file=sys.stderr)
    sys.exit(1)

SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_OUT = SCRIPT_DIR.parent / "assets" / "sprites"

CX, CY = 128 * 0.42, 128 * 0.62
TURRET_TOP = CY - 44


def save_overlay(img: Image.Image, output_dir: Path, name: str) -> None:
    path = output_dir / name
    img.save(path)
    print(f"  {name}")


def outline_ellipse(draw: ImageDraw.ImageDraw, box, fill, outline, width=3) -> None:
    draw.ellipse(box, fill=fill, outline=outline, width=width)


def make_skin_kuromi(size: int = 128) -> Image.Image:
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    pink = (255, 95, 180, 255)
    hot = (255, 40, 130, 255)
    black = (20, 16, 28, 255)
    white = (255, 250, 252, 255)

    bow_y = TURRET_TOP - 2
    # Laços bem grandes
    for side in (-1, 1):
        draw.ellipse(
            [CX + side * 28 - 16, bow_y - 14, CX + side * 28 + 16, bow_y + 16],
            fill=pink, outline=black, width=3,
        )
        draw.ellipse(
            [CX + side * 26 - 8, bow_y - 6, CX + side * 26 + 8, bow_y + 8],
            fill=hot,
        )
    draw.ellipse([CX - 11, bow_y - 10, CX + 11, bow_y + 12], fill=hot, outline=black, width=3)
    # Caveira no nó
    draw.ellipse([CX - 8, bow_y - 7, CX + 8, bow_y + 9], fill=white, outline=black, width=2)
    draw.ellipse([CX - 5, bow_y - 3, CX - 1, bow_y + 2], fill=black)
    draw.ellipse([CX + 1, bow_y - 3, CX + 5, bow_y + 2], fill=black)
    draw.polygon([(CX - 2, bow_y + 3), (CX, bow_y + 8), (CX + 2, bow_y + 3)], fill=hot)

    # Orelhas altas
    for side in (-1, 1):
        tip_x = CX + side * 22
        draw.polygon(
            [(CX + side * 10, TURRET_TOP + 6), (tip_x, TURRET_TOP - 22), (CX + side * 26, TURRET_TOP + 10)],
            fill=black,
        )
        draw.polygon(
            [(CX + side * 13, TURRET_TOP + 5), (tip_x + side * 2, TURRET_TOP - 14), (CX + side * 22, TURRET_TOP + 8)],
            fill=pink,
        )

    blush = (255, 110, 175, 190)
    draw.ellipse([CX - 30, CY - 28, CX - 16, CY - 14], fill=blush)
    draw.ellipse([CX + 16, CY - 28, CX + 30, CY - 14], fill=blush)
    return img


def make_skin_gothic(size: int = 128) -> Image.Image:
    """Arco ogival + vitral e cruz pendente — leitura forte no canhão pequeno."""
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    silver = (226, 220, 236, 255)
    steel = (42, 36, 52, 255)
    crimson = (168, 18, 42, 255)
    glass = (92, 40, 120, 230)
    gold = (210, 170, 70, 240)

    # Arco de catedral no topo da torre
    arch = [CX - 20, TURRET_TOP - 10, CX + 20, TURRET_TOP + 28]
    draw.pieslice(arch, 200, 340, fill=glass, outline=silver)
    draw.arc(arch, 200, 340, fill=silver, width=4)
    draw.line([CX, TURRET_TOP - 6, CX, TURRET_TOP + 22], fill=silver, width=3)
    draw.line([CX - 12, TURRET_TOP + 10, CX + 12, TURRET_TOP + 10], fill=silver, width=3)
    draw.polygon([(CX - 6, TURRET_TOP + 4), (CX, TURRET_TOP - 8), (CX + 6, TURRET_TOP + 4)], fill=crimson)

    # Cruz latina grande na frente da torre — precisa ler no canhão pequeno
    cy = TURRET_TOP + 18
    # haste vertical
    draw.rectangle([CX - 7, cy - 8, CX + 7, cy + 38], fill=steel)
    draw.rectangle([CX - 5, cy - 6, CX + 5, cy + 36], fill=silver)
    draw.rectangle([CX - 2, cy - 4, CX + 2, cy + 34], fill=(250, 246, 255, 255))
    # travessa
    draw.rectangle([CX - 20, cy + 4, CX + 20, cy + 16], fill=steel)
    draw.rectangle([CX - 18, cy + 6, CX + 18, cy + 14], fill=silver)
    draw.rectangle([CX - 16, cy + 8, CX + 16, cy + 12], fill=(250, 246, 255, 255))
    # joia no cruzamento
    draw.ellipse([CX - 8, cy + 3, CX + 8, cy + 19], fill=crimson, outline=gold, width=3)
    draw.ellipse([CX - 4, cy + 7, CX + 4, cy + 15], fill=(255, 70, 90, 255))
    # pontas douradas
    draw.polygon([(CX - 6, cy - 8), (CX, cy - 16), (CX + 6, cy - 8)], fill=gold, outline=steel)
    draw.polygon([(CX - 20, cy + 6), (CX - 28, cy + 10), (CX - 20, cy + 14)], fill=gold, outline=steel)
    draw.polygon([(CX + 20, cy + 6), (CX + 28, cy + 10), (CX + 20, cy + 14)], fill=gold, outline=steel)

    # Espinhos prata no casco
    for side in (-1, 1):
        draw.polygon(
            [(CX + side * 18, CY - 10), (CX + side * 36, CY - 18), (CX + side * 28, CY + 2)],
            fill=silver, outline=steel,
        )
        draw.polygon(
            [(CX + side * 22, CY + 2), (CX + side * 34, CY + 8), (CX + side * 20, CY + 10)],
            fill=crimson,
        )
    return img


def make_skin_samurai(size: int = 128) -> Image.Image:
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    red = (220, 28, 28, 255)
    gold = (236, 188, 48, 255)
    ink = (48, 28, 12, 255)

    # Chifres kuwagata bem abertos
    draw.polygon(
        [(CX - 6, TURRET_TOP + 14), (CX - 30, TURRET_TOP - 18), (CX - 8, TURRET_TOP + 18)],
        fill=gold, outline=ink,
    )
    draw.polygon(
        [(CX + 6, TURRET_TOP + 14), (CX + 30, TURRET_TOP - 18), (CX + 8, TURRET_TOP + 18)],
        fill=gold, outline=ink,
    )
    draw.polygon(
        [(CX - 24, TURRET_TOP - 8), (CX - 36, TURRET_TOP - 20), (CX - 16, TURRET_TOP - 2)],
        fill=red,
    )
    draw.polygon(
        [(CX + 24, TURRET_TOP - 8), (CX + 36, TURRET_TOP - 20), (CX + 16, TURRET_TOP - 2)],
        fill=red,
    )

    draw.ellipse([CX - 14, TURRET_TOP + 4, CX + 14, TURRET_TOP + 30], fill=red, outline=ink, width=2)
    draw.ellipse([CX - 6, TURRET_TOP + 12, CX + 6, TURRET_TOP + 24], fill=gold)

    draw.rectangle([CX - 34, CY - 12, CX + 34, CY - 4], fill=gold, outline=ink, width=2)
    draw.rectangle([CX - 28, CY - 2, CX + 28, CY + 4], fill=red)

    # Katana no flanco direito — lâmina longa, tsuba dourada, tsuka enrolada
    steel = (230, 232, 236, 255)
    wrap = (90, 28, 22, 255)
    tip = (CX + 46, TURRET_TOP - 10)
    guard = (CX + 36, CY + 2)
    pommel = (CX + 28, CY + 26)
    draw.line([tip, guard], fill=ink, width=7)
    draw.line([tip, guard], fill=steel, width=4)
    draw.line(
        [(tip[0] - 1, tip[1] + 4), (guard[0] - 1, guard[1] - 2)],
        fill=(255, 255, 255, 220), width=2,
    )
    draw.polygon(
        [(tip[0] - 3, tip[1] + 2), tip, (tip[0] + 5, tip[1] + 8)],
        fill=steel, outline=ink,
    )
    draw.ellipse(
        [guard[0] - 8, guard[1] - 6, guard[0] + 8, guard[1] + 6],
        fill=gold, outline=ink, width=2,
    )
    draw.line([guard, pommel], fill=ink, width=8)
    draw.line([guard, pommel], fill=wrap, width=5)
    for i in range(4):
        u = (i + 0.5) / 4.0
        hx = guard[0] + (pommel[0] - guard[0]) * u
        hy = guard[1] + (pommel[1] - guard[1]) * u
        draw.line([(hx - 4, hy + 2), (hx + 4, hy - 2)], fill=gold, width=2)
    draw.ellipse(
        [pommel[0] - 4, pommel[1] - 3, pommel[0] + 5, pommel[1] + 4],
        fill=gold, outline=ink, width=2,
    )
    return img


def make_skin_pirate(size: int = 128) -> Image.Image:
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    red = (204, 24, 32, 255)
    dark = (90, 10, 16, 255)
    gold = (244, 196, 40, 255)
    white = (255, 255, 255, 200)
    black = (24, 16, 16, 255)

    # Bandana cobrindo o topo da torre
    draw.pieslice([CX - 26, TURRET_TOP - 8, CX + 22, TURRET_TOP + 32], 190, 355, fill=red)
    draw.arc([CX - 22, TURRET_TOP, CX + 18, TURRET_TOP + 26], 210, 340, fill=white, width=3)

    # Caudas grandes
    draw.polygon(
        [(CX - 22, TURRET_TOP + 10), (CX - 42, TURRET_TOP - 2), (CX - 36, TURRET_TOP + 22)],
        fill=dark,
    )
    draw.polygon(
        [(CX - 24, TURRET_TOP + 16), (CX - 44, TURRET_TOP + 28), (CX - 26, TURRET_TOP + 24)],
        fill=red,
    )

    # Caveira na bandana
    draw.ellipse([CX - 10, TURRET_TOP + 4, CX + 6, TURRET_TOP + 20], fill=white, outline=black, width=2)
    draw.ellipse([CX - 6, TURRET_TOP + 8, CX - 2, TURRET_TOP + 13], fill=black)
    draw.ellipse([CX - 1, TURRET_TOP + 8, CX + 3, TURRET_TOP + 13], fill=black)

    # Brinco grande
    draw.ellipse([CX + 16, TURRET_TOP + 8, CX + 30, TURRET_TOP + 24], outline=gold, width=4)
    draw.ellipse([CX + 20, TURRET_TOP + 20, CX + 26, TURRET_TOP + 26], fill=gold)
    return img


def make_skin_default(size: int = 128) -> Image.Image:
    """Sem overlay — placeholder transparente do item `skin_default`."""
    return Image.new("RGBA", (size, size), (0, 0, 0, 0))


# Todas as skins da loja (`ShopCatalog.h`). Ordem = overlay index 1..N.
SKIN_GENERATORS = (
    ("skin_kuromi.png", make_skin_kuromi),
    ("skin_gothic.png", make_skin_gothic),
    ("skin_samurai.png", make_skin_samurai),
    ("skin_pirate.png", make_skin_pirate),
)


def generate_skins(output_dir: Path, only: str | None = None) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    for filename, builder in SKIN_GENERATORS:
        if only and only not in (filename, filename.removesuffix(".png")):
            continue
        save_overlay(builder(), output_dir, filename)


def main() -> int:
    parser = argparse.ArgumentParser(description="Gera overlays de skin de canhão.")
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUT)
    parser.add_argument("--only", help="Gera só uma skin (ex: kuromi ou skin_kuromi.png)")
    parser.add_argument("--list", action="store_true", help="Lista as skins do jogo")
    args = parser.parse_args()
    if args.list:
        for filename, builder in SKIN_GENERATORS:
            print(f"  {filename}  ({builder.__name__})")
        return 0
    print(f"Gerando skins em {args.output_dir.resolve()} …")
    generate_skins(args.output_dir.resolve(), args.only)
    print("Concluído.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
