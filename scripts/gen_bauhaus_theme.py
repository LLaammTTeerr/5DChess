#!/usr/bin/env python3
"""Generate the "Bauhaus" piece theme (assets/images/pieces/bauhaus).

Original flat, modernist geometry in the Bauhaus / Swiss-poster tradition.  Every piece is assembled only from
circles, semicircles, quarter-circles, squares, rectangles and triangles; no gradients, no shading.  Primary
accents mark the rank:

  King   red circle head with a cross of two bars on top, trapezoid body
  Queen  yellow ring head with a small blue dot above it, broad triangular body
  Rook   blue square set in a rectangular tower with three square merlons
  Bishop tall triangle head with a red triangle inset and a yellow dot, rectangular stem
  Knight quarter-circle head, rectangular snout, red eye, yellow mane bar, blue neck square
  Pawn   yellow circle head on a small trapezoid

White side: cream bodies (#F4EFE6) with a thick near-black (#1A1A1A) outline and inner strokes.
Black side: near-black bodies (#1E1E1E) with a cream outline / inner strokes plus a hairline dark halo, so the
pieces read on both light and dark squares.  Bodies differ strongly in value, so the sides stay distinguishable in
greyscale (Blueprint view).  Accent colours are shared by both sides.

Everything is drawn at 4x and reduced with LANCZOS to 256x256 RGBA.  Transparent pixels carry the outline RGB so
premultiplied / greyscale conversion leaves no halo.  Output is deterministic.

Usage: python3 scripts/gen_bauhaus_theme.py [--out DIR] [--sheet PATH]
"""
import argparse
import math
import os

from PIL import Image, ImageChops, ImageDraw

K = 4
SIZE = 256
BIG = SIZE * K
OUTLINE_W = 7.0   # outer outline width in 256-units
LINE_W = 3.0      # inner stroke width
HALO_W = 1.6      # dark hairline outside the cream outline of the black side

RED, YELLOW, BLUE = "#D7263D", "#F2B705", "#1B4D9B"
CREAM, INK, SOOT = "#F4EFE6", "#1A1A1A", "#1E1E1E"

SIDES = {
    "white": {"body": CREAM, "line": INK, "halo": None},
    "black": {"body": SOOT, "line": CREAM, "halo": INK},
}
ACC = {"red": RED, "yellow": YELLOW, "blue": BLUE}


def hexrgb(h):
    return tuple(int(h[i:i + 2], 16) for i in (1, 3, 5))


# ------------------------------------------------------------------ shapes (256-unit coordinates)
def poly(*pts):
    return ("poly", pts)


def rect(x0, y0, x1, y1):
    return ("poly", ((x0, y0), (x1, y0), (x1, y1), (x0, y1)))


def circ(cx, cy, r):
    return ("ell", (cx - r, cy - r, cx + r, cy + r))


def pie(cx, cy, r, a0, a1):
    """Circular sector, degrees, 0 = right, 90 = down."""
    return ("pie", (cx - r, cy - r, cx + r, cy + r), a0, a1)


def mask_of(shapes):
    m = Image.new("L", (BIG, BIG), 0)
    d = ImageDraw.Draw(m)
    for s in shapes:
        if s[0] == "poly":
            d.polygon([(x * K, y * K) for x, y in s[1]], fill=255)
        elif s[0] == "ell":
            d.ellipse([v * K for v in s[1]], fill=255)
        elif s[0] == "pie":
            d.pieslice([v * K for v in s[1]], s[2], s[3], fill=255)
    return m


def grow(mask, w):
    """Round dilation of a mask by w (256-units)."""
    r = w * K
    out = mask
    for ring_r in (r, r * 0.5):
        for i in range(24):
            a = 2 * math.pi * i / 24
            out = ImageChops.lighter(
                out, ImageChops.offset(mask, round(ring_r * math.cos(a)), round(ring_r * math.sin(a))))
    return out


# ------------------------------------------------------------------ piece definitions
# Each part: (shapes, colour) where colour is "body" or an accent name.  Painted back to front.
def base():
    return [([rect(54, 214, 202, 232)], "body"), ([rect(70, 204, 186, 216)], "body")]


def king():
    return base() + [
        ([poly((96, 206), (160, 206), (148, 100), (108, 100))], "body"),
        ([rect(88, 90, 168, 106)], "body"),
        ([rect(121, 14, 135, 52)], "body"),
        ([rect(108, 24, 148, 38)], "body"),
        ([circ(128, 68, 24)], "red"),
    ]


def queen():
    return base() + [
        ([poly((128, 62), (176, 206), (80, 206))], "body"),
        ([rect(86, 112, 170, 126)], "body"),
        ([circ(128, 66, 25)], "yellow"),
        ([circ(128, 66, 11)], "body"),
        ([circ(128, 24, 9)], "blue"),
    ]


def rook():
    return base() + [
        ([rect(96, 100, 160, 206)], "body"),
        ([rect(80, 76, 176, 104)], "body"),
        ([rect(80, 44, 104, 76), rect(116, 44, 140, 76), rect(152, 44, 176, 76)], "body"),
        ([rect(108, 130, 148, 170)], "blue"),
    ]


def bishop():
    return base() + [
        ([rect(104, 140, 152, 206)], "body"),
        ([rect(88, 134, 168, 148)], "body"),
        ([poly((128, 26), (172, 134), (84, 134))], "body"),
        ([poly((128, 62), (150, 118), (106, 118))], "red"),
        ([circ(128, 24, 9)], "yellow"),
    ]


def knight():
    return base() + [
        ([rect(96, 140, 168, 206)], "body"),
        ([pie(168, 140, 108, 180, 270)], "body"),
        ([rect(44, 112, 96, 140)], "body"),
        ([rect(150, 70, 168, 140)], "yellow"),
        ([circ(116, 98, 8)], "red"),
        ([rect(112, 162, 152, 192)], "blue"),
    ]


def pawn():
    return [([rect(76, 218, 180, 232)], "body"), ([rect(88, 208, 168, 220)], "body"),
            ([poly((98, 208), (158, 208), (146, 130), (110, 130))], "body"),
            ([rect(94, 120, 162, 134)], "body"),
            ([circ(128, 96, 24)], "yellow")]


PIECES = {"king": king, "queen": queen, "rook": rook, "bishop": bishop, "knight": knight, "pawn": pawn}


# ------------------------------------------------------------------ rendering
def solid(rgb):
    return Image.new("RGBA", (BIG, BIG), rgb + (255,))


def render(kind, side):
    st = SIDES[side]
    parts = PIECES[kind]()
    line = hexrgb(st["line"])
    union = mask_of([s for shapes, _ in parts for s in shapes])
    outer = grow(union, OUTLINE_W)
    edge = hexrgb(st["halo"] or st["line"])
    canvas = Image.new("RGBA", (BIG, BIG), edge + (0,))
    if st["halo"]:
        canvas.paste(solid(edge), mask=grow(outer, HALO_W))
    canvas.paste(solid(line), mask=outer)
    for shapes, col in parts:
        m = mask_of(shapes)
        canvas.paste(solid(line), mask=grow(m, LINE_W / 2))
        canvas.paste(solid(hexrgb(st["body"] if col == "body" else ACC[col])), mask=m)
    img = canvas.resize((SIZE, SIZE), Image.LANCZOS)
    return fit(img, edge)


def fit(img, edge):
    """Centre horizontally; put the bottom of the piece 20 px (~8%) above the bottom edge."""
    bb = img.getchannel("A").point(lambda v: 255 if v > 8 else 0).getbbox()
    dx = round((SIZE - (bb[0] + bb[2])) / 2)
    dy = 0 if False else 256 - 20 - bb[3]
    out = Image.new("RGBA", (SIZE, SIZE), edge + (0,))
    out.paste(img, (dx, dy))
    return out


# ------------------------------------------------------------------ contact sheet
def tile(img, size, bg, grey=False):
    t = Image.new("RGBA", img.size, bg + (255,))
    t.alpha_composite(img)
    t = t.convert("RGB")
    if grey:
        t = t.convert("L").convert("RGB")
    return t.reduce(256 // size) if size < 256 and 256 % size == 0 else t.resize((size, size), Image.BOX)


def make_sheet(imgs, path):
    order = ["king", "queen", "rook", "bishop", "knight", "pawn"]
    light, dark = (240, 230, 214), (181, 136, 99)
    rows = [(160, light, False), (160, dark, False), (160, (28, 30, 69), False), (160, light, True),
            (40, light, False), (32, light, False), (16, light, False), (40, dark, False), (32, dark, False),
            (16, dark, False)]
    out_dir = os.path.dirname(os.path.abspath(path))
    os.makedirs(out_dir, exist_ok=True)
    W = 12 * 164 + 4
    H = sum(r[0] + 4 for r in rows) + 4
    sheet = Image.new("RGB", (W, H), (90, 90, 90))
    y = 4
    for size, bg, grey in rows:
        x = 4
        for side in ("white", "black"):
            for k in order:
                sheet.paste(tile(imgs[(side, k)], size, bg, grey), (x, y))
                x += size + 4
        y += size + 4
    sheet.save(path)
    back = ["rook", "knight", "bishop", "queen", "king", "bishop", "knight", "rook"]
    for name, lt, dk in (("board.png", (240, 230, 214), (201, 165, 122)),
                         ("board_dark.png", (74, 78, 105), (40, 43, 66))):
        sq = 48
        b = Image.new("RGBA", (sq * 8, sq * 8))
        for r in range(8):
            for c in range(8):
                b.paste((lt if (r + c) % 2 == 0 else dk) + (255,), (c * sq, r * sq, c * sq + sq, r * sq + sq))
        for c in range(8):
            for r, side, k in ((0, "black", back[c]), (1, "black", "pawn"), (6, "white", "pawn"),
                               (7, "white", back[c])):
                b.alpha_composite(imgs[(side, k)].reduce(256 // sq) if False else
                                  imgs[(side, k)].resize((sq, sq), Image.LANCZOS), (c * sq, r * sq))
        b.convert("RGB").save(os.path.join(out_dir, name))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "assets", "images", "pieces", "bauhaus"))
    ap.add_argument("--sheet", default=None)
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    imgs = {}
    for side in SIDES:
        for kind in PIECES:
            im = render(kind, side)
            imgs[(side, kind)] = im
            im.save(os.path.join(a.out, f"{side}_{kind}.png"), optimize=True)
    if a.sheet:
        make_sheet(imgs, a.sheet)


if __name__ == "__main__":
    main()
