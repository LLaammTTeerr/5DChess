#!/usr/bin/env python3
"""Generate the "Neon" piece theme (assets/images/pieces/neon).

Original synthwave neon-sign pieces drawn entirely by the code below.  Every piece is a glass tube bent into the
silhouette outline plus a few inner detail lines (round joins = tube bends).  Each tube is painted as

  * a dark translucent glass fill inside the silhouette (~80% opaque navy) so the piece reads on LIGHT squares,
  * a tight Gaussian glow around the tubes (tight enough not to bleed into neighbouring squares),
  * a saturated tube body and a thin, near-white core line.

White side: cyan / ice tubes (glow #19E3FF) on deep navy glass.  Black side: magenta / hot pink tubes
(glow #FF2BA6) on an almost black glass with dimmer tube and core, so the sides also differ by value alone
(greyscale / Blueprint view).  A small mounting bracket (plate with two bolts) sits under every piece.

Silhouettes: King = crown with a cross; Queen = coronet with ball tips; Rook = crenellated tower with a door;
Bishop = mitre with a slit and a ball; Knight = horse head with mane; Pawn = round head on a flared body.

Everything is drawn at 4x (1024 px) and reduced with LANCZOS to 256x256 RGBA.  Pixels outside the piece are fully
transparent but carry the glass colour in RGB (no halos).  Deterministic; re-running is idempotent.

Usage: python3 scripts/gen_neon_theme.py [--out DIR] [--sheet PATH]
"""
import argparse
import math
import os

from PIL import Image, ImageDraw, ImageFilter

K = 4
SIZE = 256
BIG = SIZE * K
CX = 128.0
TUBE_W = 6.0
CORE_W = 2.4
GLOW_SIGMA = 3.5

SIDES = {
    "white": dict(glass=(18, 24, 72), glass_a=0.80, glow=(25, 227, 255), tube=(40, 210, 245), core=(232, 255, 255),
                  glow_a=0.9),
    "black": dict(glass=(3, 3, 14), glass_a=0.88, glow=(255, 43, 166), tube=(205, 25, 135), core=(255, 190, 235),
                  glow_a=0.6),
}


# ---------------------------------------------------------------- geometry
def sym(right):
    """Mirror a right-hand profile (top to bottom) about the centre line."""
    return list(right) + [(2 * CX - x, y) for x, y in reversed(right)]


def arc(cx, cy, r, a0, a1, n=14):
    return [(cx + r * math.cos(math.radians(a0 + (a1 - a0) * i / n)),
             cy + r * math.sin(math.radians(a0 + (a1 - a0) * i / n))) for i in range(n + 1)]


def smooth(pts, sharp=(), iters=2):
    """Chaikin corner cutting of a closed polygon; indices in `sharp` stay sharp."""
    p = [(q, i in sharp) for i, q in enumerate(pts)]
    for _ in range(iters):
        out = []
        for i in range(len(p)):
            (a, sa), (b, sb) = p[i], p[(i + 1) % len(p)]
            out.append((a, True) if sa else ((.75 * a[0] + .25 * b[0], .75 * a[1] + .25 * b[1]), False))
            if not sb:
                out.append(((.25 * a[0] + .75 * b[0], .25 * a[1] + .75 * b[1]), False))
        p = out
    return [q for q, _ in p]


def sc(pts):
    return [(x * K, y * K) for x, y in pts]


class Piece:
    """fills: polygons filled with glass; lines: (points, closed) tube strokes; dots: bolts."""

    def __init__(self):
        self.fills, self.lines, self.dots = [], [], []

    def shape(self, pts):
        self.fills.append(pts)
        self.lines.append((pts, True))

    def circle(self, cx, cy, r):
        self.shape(arc(cx, cy, r, 0, 360, 24)[:-1])

    def line(self, pts):
        self.lines.append((pts, False))

    def dot(self, cx, cy, r):
        self.dots.append((cx, cy, r))


def bracket(p, half):
    y0, y1 = 212.0, 224.0
    p.shape([(CX - half, y0), (CX + half, y0), (CX + half, y1), (CX - half, y1)])
    for dx in (-half + 8, half - 8):
        p.dot(CX + dx, (y0 + y1) / 2, 1.8)


# ---------------------------------------------------------------- pieces
def pawn():
    p = Piece()
    right = arc(128, 82, 22, -90, 68) + [(135, 108), (150, 112), (150, 121), (137, 125), (148, 166), (160, 207)]
    p.shape(sym(right))
    p.line([(104, 124), (152, 124)])
    bracket(p, 40)
    return p


def rook():
    p = Piece()
    right = [(128, 56), (140, 56), (140, 70), (152, 70), (152, 56), (168, 56), (168, 84), (156, 94),
             (156, 186), (170, 207)]
    p.shape(sym(right))
    p.line([(100, 94), (156, 94)])
    p.line([(118, 190), (118, 160)] + arc(128, 160, 10, 180, 360, 8) + [(138, 190), (118, 190)])
    bracket(p, 48)
    return p


def bishop():
    p = Piece()
    p.circle(128, 42, 7)
    right = [(128, 52), (140, 66), (154, 90), (156, 108), (146, 120), (160, 126), (160, 136), (140, 138),
             (146, 170), (162, 207)]
    p.shape(sym(right))
    p.line([(118, 108), (146, 76)])
    bracket(p, 48)
    return p


def queen():
    p = Piece()
    right = [(128, 58), (141, 100), (155, 66), (163, 108), (180, 80), (170, 124), (158, 128), (158, 138),
             (142, 141), (148, 172), (164, 207)]
    p.shape(sym(right))
    for x, y in [(128, 52), (155, 61), (180, 75), (101, 61), (76, 75)]:
        p.circle(x, y, 6.0)
    p.line([(99, 132), (157, 132)])
    bracket(p, 50)
    return p


def king():
    p = Piece()
    right = [(128, 56)] + arc(128, 80, 24, -90, -10, 6)[1:] + [(160, 96), (174, 84), (178, 118), (160, 126),
                                                               (160, 138), (142, 141), (148, 172), (164, 207)]
    p.shape(sym(right))
    p.line([(128, 22), (128, 52)])
    p.line([(115, 32), (141, 32)])
    p.line([(98, 134), (158, 134)])
    bracket(p, 50)
    return p


def knight():
    p = Piece()
    pts = [(84, 207), (92, 172), (112, 150), (102, 138), (86, 150), (68, 142), (64, 124), (74, 108), (92, 90),
           (100, 64), (108, 44), (122, 56), (134, 62), (156, 80), (172, 118), (176, 164), (174, 207)]
    p.shape(smooth(pts, {0, 16, 10}, 2))
    p.dot(100, 96, 2.4)
    p.dot(74, 128, 1.6)
    p.line([(130, 70), (148, 100), (156, 140)])
    p.line([(120, 76), (132, 110), (138, 150)])
    bracket(p, 52)
    return p


PIECES = {"king": king, "queen": queen, "rook": rook, "bishop": bishop, "knight": knight, "pawn": pawn}


# ---------------------------------------------------------------- rendering
def stroke_mask(lines, dots, width):
    m = Image.new("L", (BIG, BIG), 0)
    d = ImageDraw.Draw(m)
    w = width * K
    for pts, closed in lines:
        q = sc(pts + ([pts[0]] if closed else []))
        d.line(q, fill=255, width=round(w))
        for x, y in q:
            d.ellipse([x - w / 2, y - w / 2, x + w / 2, y + w / 2], fill=255)
    for cx, cy, r in dots:
        d.ellipse([(cx - r) * K, (cy - r) * K, (cx + r) * K, (cy + r) * K], fill=255)
    return m


def layer(color, mask, a=1.0):
    im = Image.new("RGBA", (BIG, BIG), color + (0,))
    im.putalpha(mask.point(lambda v: int(v * a)))
    return im


def render(piece, side):
    s = SIDES[side]
    fill = Image.new("L", (BIG, BIG), 0)
    d = ImageDraw.Draw(fill)
    for pts in piece.fills:
        d.polygon(sc(pts), fill=255)
    tube = stroke_mask(piece.lines, piece.dots, TUBE_W)
    core = stroke_mask(piece.lines, [], CORE_W)
    glow = tube.filter(ImageFilter.GaussianBlur(GLOW_SIGMA * K)).point(lambda v: min(255, int(v * 2.4)))
    im = Image.new("RGBA", (BIG, BIG), s["glass"] + (0,))
    for lay in (layer(s["glass"], fill, s["glass_a"]), layer(s["glow"], glow, s["glow_a"]),
                layer(s["tube"], tube), layer(s["core"], core)):
        im = Image.alpha_composite(im, lay)
    out = im.resize((SIZE, SIZE), Image.LANCZOS)
    r, g, b, a = out.split()
    flat = Image.new("RGB", (SIZE, SIZE), s["glass"])
    rgb = Image.composite(Image.merge("RGB", (r, g, b)), flat, a.point(lambda v: 255 if v > 0 else 0))
    return Image.merge("RGBA", (*rgb.split(), a))


# ---------------------------------------------------------------- sheets
def small(spr, size):
    return spr.resize((size, size), Image.BOX)


def make_sheet(path, sprites):
    names = list(PIECES)
    rows = [((240, 230, 214), 160, False), ((181, 136, 99), 160, False), ((28, 30, 69), 160, False),
            ((181, 136, 99), 160, True), ((240, 230, 214), 40, False), ((181, 136, 99), 32, False),
            ((28, 30, 69), 16, False), ((181, 136, 99), 16, True)]
    W = 12 * 164 + 6
    H = sum(r[1] + 8 for r in rows) + 6
    sheet = Image.new("RGB", (W, H), (60, 60, 60))
    y = 6
    for bg, size, gray in rows:
        x = 6
        for side in ("white", "black"):
            for n in names:
                spr = sprites[(side, n)].resize((size, size), Image.LANCZOS) if size == 160 else small(sprites[(side, n)], size)
                tile = Image.alpha_composite(Image.new("RGBA", (size, size), bg + (255,)), spr).convert("RGB")
                if gray:
                    tile = tile.convert("L").convert("RGB")
                sheet.paste(tile, (x, y))
                x += size + 4
        y += size + 8
    sheet.save(path)


def make_board(path, sprites, cols):
    sq = 48
    img = Image.new("RGBA", (sq * 8, sq * 8))
    order = ["rook", "knight", "bishop", "queen", "king", "bishop", "knight", "rook"]
    dr = ImageDraw.Draw(img)
    for r in range(8):
        for c in range(8):
            dr.rectangle([c * sq, r * sq, c * sq + sq - 1, r * sq + sq - 1], fill=cols[(r + c) % 2] + (255,))
            name = {0: order[c], 1: "pawn", 6: "pawn", 7: order[c]}.get(r)
            if name:
                spr = sprites[("black" if r < 2 else "white", name)].resize((sq, sq), Image.LANCZOS)
                img.alpha_composite(spr, (c * sq, r * sq))
    img.convert("RGB").save(path)


def main():
    ap = argparse.ArgumentParser()
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    ap.add_argument("--out", default=os.path.join(root, "assets", "images", "pieces", "neon"))
    ap.add_argument("--sheet", default=None)
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    sprites = {}
    for side in SIDES:
        for n, f in PIECES.items():
            im = render(f(), side)
            im.save(os.path.join(a.out, f"{side}_{n}.png"), optimize=True)
            sprites[(side, n)] = im
    if a.sheet:
        d = os.path.dirname(a.sheet) or "."
        os.makedirs(d, exist_ok=True)
        make_sheet(a.sheet, sprites)
        make_board(os.path.join(d, "board.png"), sprites, ((240, 230, 214), (201, 165, 122)))
        make_board(os.path.join(d, "board_dark.png"), sprites, ((74, 78, 120), (38, 40, 80)))


if __name__ == "__main__":
    main()
