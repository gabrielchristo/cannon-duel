#!/usr/bin/env python3
"""Gera overlays de skin — acessórios grandes e legíveis sobre o canhão.

Uso:
    python3 tools/gen_skins.py
"""

from __future__ import annotations

import argparse
import math
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


def _plaid_sheet(size: int, cell: int = 8) -> Image.Image:
    """Xadrez cinza/preto do bucket hat do Negão do Zap."""
    sheet = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(sheet)
    dark = (36, 36, 40, 255)
    mid = (110, 110, 116, 255)
    light = (196, 196, 200, 255)
    for y in range(0, size, cell):
        for x in range(0, size, cell):
            gx, gy = x // cell, y // cell
            if gx % 4 == 0 or gy % 4 == 0:
                col = dark
            elif (gx + gy) % 2:
                col = mid
            else:
                col = light
            draw.rectangle([x, y, x + cell - 1, y + cell - 1], fill=col)
    return sheet


def _stamp(img: Image.Image, paint: Image.Image, draw_mask) -> None:
    mask = Image.new("L", img.size, 0)
    draw_mask(ImageDraw.Draw(mask))
    img.paste(paint, (0, 0), mask)


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


def make_skin_mymelody(size: int = 128) -> Image.Image:
    """Capuz rosa + flor amarela na orelha esquerda — leitura My Melody."""
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    pink = (255, 120, 175, 255)
    hot = (235, 70, 130, 255)
    white = (255, 250, 252, 255)
    ink = (40, 22, 32, 255)
    gold = (255, 214, 70, 255)
    blush = (255, 130, 170, 200)

    # Orelhas do capuz
    for side, flower in ((-1, False), (1, True)):
        draw.ellipse(
            [CX + side * 22 - 14, TURRET_TOP - 26, CX + side * 22 + 14, TURRET_TOP + 10],
            fill=pink, outline=ink, width=3,
        )
        draw.ellipse(
            [CX + side * 22 - 7, TURRET_TOP - 16, CX + side * 22 + 7, TURRET_TOP + 2],
            fill=white,
        )
        if flower:
            fx, fy = CX + side * 24, TURRET_TOP - 22
            for ang in range(0, 360, 72):
                rad = math.radians(ang)
                px = fx + math.cos(rad) * 9
                py = fy + math.sin(rad) * 9
                draw.ellipse([px - 5, py - 5, px + 5, py + 5], fill=gold, outline=ink, width=2)
            draw.ellipse([fx - 5, fy - 5, fx + 5, fy + 5], fill=hot, outline=ink, width=2)

    # Capuz sobre a torre
    draw.ellipse([CX - 22, TURRET_TOP - 4, CX + 22, TURRET_TOP + 36], fill=pink, outline=ink, width=3)
    draw.ellipse([CX - 16, TURRET_TOP + 6, CX + 16, TURRET_TOP + 32], fill=white, outline=ink, width=2)
    # Olhos + nariz
    draw.ellipse([CX - 8, TURRET_TOP + 14, CX - 3, TURRET_TOP + 22], fill=ink)
    draw.ellipse([CX + 3, TURRET_TOP + 14, CX + 8, TURRET_TOP + 22], fill=ink)
    draw.ellipse([CX - 3, TURRET_TOP + 21, CX + 3, TURRET_TOP + 27], fill=gold, outline=ink, width=1)
    draw.ellipse([CX - 26, CY - 26, CX - 14, CY - 16], fill=blush)
    draw.ellipse([CX + 14, CY - 26, CX + 26, CY - 16], fill=blush)
    # Babado branco na borda do capuz
    for i in range(-2, 3):
        bx = CX + i * 8
        draw.ellipse([bx - 5, TURRET_TOP + 30, bx + 5, TURRET_TOP + 40], fill=white, outline=ink, width=1)
    return img


def make_skin_cinnamoroll(size: int = 128) -> Image.Image:
    """Orelhas longas + swirl — tudo na torre, não no casco."""
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    white = (252, 252, 255, 255)
    ink = (36, 40, 56, 255)
    sky = (110, 190, 230, 255)
    tan = (210, 150, 80, 255)
    brown = (150, 90, 40, 255)
    blush = (255, 150, 180, 190)

    # Orelhas flopadas saindo do topo da torre
    for side in (-1, 1):
        draw.polygon(
            [
                (CX + side * 6, TURRET_TOP - 4),
                (CX + side * 34, TURRET_TOP + 16),
                (CX + side * 18, TURRET_TOP + 26),
                (CX + side * 2, TURRET_TOP + 10),
            ],
            fill=white, outline=ink,
        )
        draw.polygon(
            [
                (CX + side * 10, TURRET_TOP + 2),
                (CX + side * 26, TURRET_TOP + 14),
                (CX + side * 16, TURRET_TOP + 20),
            ],
            fill=sky,
        )

    draw.ellipse([CX - 18, TURRET_TOP - 10, CX + 18, TURRET_TOP + 22], fill=white, outline=ink, width=3)
    draw.ellipse([CX - 7, TURRET_TOP + 2, CX - 2, TURRET_TOP + 10], fill=sky)
    draw.ellipse([CX + 2, TURRET_TOP + 2, CX + 7, TURRET_TOP + 10], fill=sky)
    draw.ellipse([CX - 2, TURRET_TOP + 10, CX + 2, TURRET_TOP + 13], fill=ink)
    draw.ellipse([CX - 26, TURRET_TOP + 8, CX - 14, TURRET_TOP + 18], fill=blush)
    draw.ellipse([CX + 14, TURRET_TOP + 8, CX + 26, TURRET_TOP + 18], fill=blush)

    for ox in (-16, 0, 16):
        draw.ellipse(
            [CX + ox - 7, TURRET_TOP + 18, CX + ox + 7, TURRET_TOP + 30],
            fill=white, outline=ink, width=2,
        )

    sx, sy = CX + 30, TURRET_TOP + 2
    draw.ellipse([sx - 11, sy - 11, sx + 11, sy + 11], fill=tan, outline=brown, width=3)
    draw.arc([sx - 7, sy - 7, sx + 7, sy + 7], 40, 300, fill=brown, width=3)
    draw.ellipse([sx - 3, sy - 3, sx + 3, sy + 3], fill=brown)
    return img


def make_skin_negaodozap(size: int = 128) -> Image.Image:
    """Bucket hat xadrez + toalha verde-água — foto do meme."""
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    ink = (18, 16, 14, 255)
    towel = (78, 198, 188, 255)
    towel_dark = (42, 150, 142, 255)
    mesh = (36, 110, 62, 255)
    plaid = _plaid_sheet(size)

    brim = [CX - 30, TURRET_TOP - 8, CX + 30, TURRET_TOP + 16]
    crown = [CX - 16, TURRET_TOP - 24, CX + 16, TURRET_TOP + 2]

    # Toalha no "pescoço" da torre, pontas caindo nos dois lados
    draw.polygon(
        [
            (CX - 8, TURRET_TOP + 18),
            (CX - 36, CY - 4),
            (CX - 22, CY + 10),
            (CX - 4, TURRET_TOP + 28),
        ],
        fill=towel, outline=ink,
    )
    draw.polygon(
        [
            (CX + 6, TURRET_TOP + 18),
            (CX + 34, CY + 2),
            (CX + 20, CY + 14),
            (CX + 2, TURRET_TOP + 28),
        ],
        fill=towel_dark, outline=ink,
    )
    draw.rectangle([CX - 22, TURRET_TOP + 16, CX + 22, TURRET_TOP + 26], fill=towel, outline=ink, width=2)
    draw.line([CX - 14, TURRET_TOP + 20, CX + 14, TURRET_TOP + 20], fill=towel_dark, width=2)

    # Rede verde na cintura do casco
    for i in range(-3, 4):
        x0 = CX + i * 8 - 4
        draw.polygon(
            [(x0, CY + 6), (x0 + 4, CY + 2), (x0 + 8, CY + 6), (x0 + 4, CY + 10)],
            fill=mesh, outline=ink,
        )

    # Barra marrom no casco
    shaft = (118, 72, 38, 255)
    tip = (86, 50, 26, 255)
    draw.rounded_rectangle([CX - 5, CY + 4, CX + 5, CY + 30], radius=4, fill=shaft, outline=ink, width=2)
    draw.ellipse([CX - 6, CY + 24, CX + 6, CY + 36], fill=tip, outline=ink, width=2)

    _stamp(img, plaid, lambda md: md.ellipse(brim, fill=255))
    _stamp(img, plaid, lambda md: md.ellipse(crown, fill=255))
    draw.ellipse(brim, outline=ink, width=3)
    draw.ellipse(crown, outline=ink, width=3)
    draw.arc([CX - 12, TURRET_TOP - 20, CX + 12, TURRET_TOP - 6], 200, 340, fill=ink, width=2)
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
    ("skin_mymelody.png", make_skin_mymelody),
    ("skin_cinnamoroll.png", make_skin_cinnamoroll),
    ("skin_negaodozap.png", make_skin_negaodozap),
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
