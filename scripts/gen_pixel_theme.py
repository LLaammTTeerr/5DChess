#!/usr/bin/env python3
"""Generate the "Pixel" piece theme (assets/images/Theme_3).

Every piece is an original 16x16 pixel-art "creature": a wide blocky body with
two vertical slit eyes, tiny side nubs and four stubby legs.  The piece type is
told apart by the headgear / silhouette on top.

Sprite legend (one character per pixel):
    .  transparent
    O  outline            B  body
    S  shade              E  eye
    A  accent / headgear  H  highlight

The grids below only contain the *fill*; a 1px outline ('O') is added
automatically around the whole silhouette (set AUTO_OUTLINE = False to draw it
by hand).  Output is deterministic, so re-running is idempotent.

Usage: python3 scripts/gen_pixel_theme.py [--out DIR] [--sheet PATH]
"""
import argparse
import os
import sys

from PIL import Image, ImageDraw, ImageFont

SIZE = 16
SCALE = 8
AUTO_OUTLINE = True

PALETTES = {
    "white": {  # terracotta
        "O": "#5A2A18", "B": "#D97757", "S": "#B85C3D",
        "H": "#F4B89C", "E": "#1F1E1D", "A": "#F2C14E",
    },
    "black": {  # charcoal
        "O": "#121110", "B": "#3D3A36", "S": "#2A2825",
        "H": "#6B6660", "E": "#F0A27E", "A": "#C9A227",
    },
}

# fmt: off
SPRITES = {
    "pawn": [
        "................",
        "................",
        "................",
        "................",
        "................",
        ".......A........",
        ".......B........",
        "....HBBBBBB.....",
        "....BEBBBEB.....",
        "...BBEBBBEBB....",
        "....BBBBBBB.....",
        "....SSSSSSS.....",
        "...O.O...O.O....",
        "...O.O...O.O....",
        "................",
        "................",
    ],
    "king": [
        "................",
        ".......A........",
        "......AAA.......",
        ".......A........",
        "....A..A..A.....",
        "....AAAAAAA.....",
        "...HBBBBBBBBB...",
        "...BBBBBBBBB....",
        "..BBBEBBBEBBB...",
        "...BBEBBBEBB....",
        "...BBBBBBBBB....",
        "...O.O...O.O....",
        "...O.O...O.O....",
        "...O.O...O.O....",
        "................",
        "................",
    ],
    "queen": [
        "................",
        "................",
        "...H.H.H.H.H....",
        "...A.A.A.A.A....",
        "...AAAAAAAAA....",
        "...AAAAAAAAA....",
        "...HBBBBBBBBB...",
        "...BBBBBBBBB....",
        "..BBBEBBBEBBB...",
        "...BBEBBBEBB....",
        "...BBBBBBBBB....",
        "...O.O...O.O....",
        "...O.O...O.O....",
        "...O.O...O.O....",
        "................",
        "................",
    ],
    "rook": [
        "................",
        "................",
        "...HB.BBB.BB....",
        "...BB.BBB.BB....",
        "...BB.BBB.BB....",
        "...O.O...O.O....",
        "...HBBBBBBBBB...",
        "...BBBBBBBBB....",
        "..BBBEBBBEBBB...",
        "...BBEBBBEBB....",
        "...BBBBBBBBB....",
        "...O.O...O.O....",
        "...O.O...O.O....",
        "...O.O...O.O....",
        "................",
        "................",
    ],
    "bishop": [
        "................",
        ".......H........",
        "......AAA.......",
        ".....AAAOA......",
        "....AAAOAAA.....",
        "....AAOAAAA.....",
        "...HBBBBBBBBB...",
        "...BBBBBBBBB....",
        "..BBBEBBBEBBB...",
        "...BBEBBBEBB....",
        "...BBBBBBBBB....",
        "...O.O...O.O....",
        "...O.O...O.O....",
        "...O.O...O.O....",
        "................",
        "................",
    ],
    "knight": [
        "................",
        "................",
        "....B.B.........",
        "....S.S....A....",
        "...BBBBB..AA....",
        "...BBBBBBAAA....",
        "...HBBBBBBBBB...",
        "...BBBBBBBBB....",
        ".BBBBEBBBEBBB...",
        "BBBBBEBBBEBB....",
        "BEBBBBBBBBB.....",
        ".SSSSSSSSSS.....",
        "...O.O...O.O....",
        "...O.O...O.O....",
        "................",
        "................",
    ],
}
# fmt: on

PIECES = ["king", "queen", "rook", "bishop", "knight", "pawn"]
SIDES = ["white", "black"]


def validate():
    for name, rows in SPRITES.items():
        assert len(rows) == SIZE, f"{name}: {len(rows)} rows"
        for i, r in enumerate(rows):
            assert len(r) == SIZE, f"{name} row {i}: {len(r)} cols"
            bad = set(r) - set(".OBSEAH")
            assert not bad, f"{name} row {i}: bad chars {bad}"


def hex_rgba(h):
    h = h.lstrip("#")
    return (int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16), 255)


def grid_with_outline(rows):
    g = [list(r) for r in rows]
    if AUTO_OUTLINE:
        out = [r[:] for r in g]
        for y in range(SIZE):
            for x in range(SIZE):
                if g[y][x] != ".":
                    continue
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    nx, ny = x + dx, y + dy
                    if 0 <= nx < SIZE and 0 <= ny < SIZE and g[ny][nx] not in ".O":
                        out[y][x] = "O"
                        break
        g = out
    return g


def render(piece, side, scale=SCALE):
    pal = {k: hex_rgba(v) for k, v in PALETTES[side].items()}
    img = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    for y, row in enumerate(grid_with_outline(SPRITES[piece])):
        for x, ch in enumerate(row):
            if ch != ".":
                img.putpixel((x, y), pal[ch])
    return img.resize((SIZE * scale, SIZE * scale), Image.NEAREST)


def lum(c):
    def f(v):
        v /= 255
        return v / 12.92 if v <= 0.03928 else ((v + 0.055) / 1.055) ** 2.4
    return 0.2126 * f(c[0]) + 0.7152 * f(c[1]) + 0.0722 * f(c[2])


def contrast(a, b):
    la, lb = sorted((lum(a), lum(b)), reverse=True)
    return (la + 0.05) / (lb + 0.05)


def check_contrast():
    squares = {"light": hex_rgba("#F0E6D6"), "dark": hex_rgba("#B58863")}
    for side in SIDES:
        p = {k: hex_rgba(v) for k, v in PALETTES[side].items()}
        for sq, sc in squares.items():
            print(f"  {side:5} outline/{sq:5}: {contrast(p['O'], sc):5.2f}  body/{sq:5}: {contrast(p['B'], sc):5.2f}")


def make_sheet(path):
    cell, label_h, pad = 128, 22, 12
    w = pad + 6 * (cell + pad)
    h = pad + 2 * (cell + label_h + pad)
    sheet = Image.new("RGBA", (w, h), (40, 40, 40, 255))
    d = ImageDraw.Draw(sheet)
    try:
        font = ImageFont.load_default(size=14)
    except TypeError:
        font = ImageFont.load_default()
    for r, side in enumerate(SIDES):
        for c, piece in enumerate(PIECES):
            x = pad + c * (cell + pad)
            y = pad + r * (cell + label_h + pad)
            for half in range(2):  # light / dark square halves, swap per column
                col = "#F0E6D6" if (c + half) % 2 == 0 else "#B58863"
                d.rectangle([x + half * cell // 2, y, x + (half + 1) * cell // 2 - 1, y + cell - 1], fill=col)
            sheet.alpha_composite(render(piece, side), (x, y))
            d.text((x + 4, y + cell + 3), f"{side} {piece}", fill=(230, 230, 230), font=font)
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    sheet.save(path)


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(here, "..", "assets", "images", "Theme_3"))
    ap.add_argument("--sheet", default=None, help="write a preview contact sheet here (not committed)")
    args = ap.parse_args()
    validate()
    os.makedirs(args.out, exist_ok=True)
    for side in SIDES:
        for piece in PIECES:
            render(piece, side).save(os.path.join(args.out, f"{side}_{piece}.png"), optimize=False)
    print("wrote", len(SIDES) * len(PIECES), "sprites to", os.path.normpath(args.out))
    check_contrast()
    if args.sheet:
        make_sheet(args.sheet)
        print("sheet:", args.sheet)


if __name__ == "__main__":
    sys.exit(main())
