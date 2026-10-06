#!/usr/bin/env python3
"""Generate the "Medieval" piece theme (assets/images/Theme_4).

Original, heraldic pieces drawn entirely by the code below (no source artwork, no font glyphs): carved ivory with
gold trim for White, dark walnut with brass trim for Black, crimson cloth for both.  Each piece is a stack of
smooth vector parts (polygons, ellipses, rounded rectangles, round-capped strokes) painted back to front:

  * every part is filled with a banded, cylinder-like gradient (light on the left, shade on the right),
  * gets a thin dark inner stroke, a bright rim on its upper-left edge and a dark rim on its lower-right edge,
  * and the union of all parts is wrapped in one thick dark outline, so the piece reads on light, dark and grey
    squares (and greyscale: Blueprint view) at any size down to about 16 px.

Everything is drawn at 4x (1024 px) and reduced with LANCZOS to 256x256 RGBA for clean anti-aliasing.  Pixels outside
the piece are fully transparent but carry the outline colour in RGB, so premultiplied or greyscale conversion never
leaves a light halo.  Requires Pillow >= 10.1 (ImageFont.load_default(size=...); produced with 12.1).  Output is deterministic (no randomness); re-running is idempotent.

Silhouettes: King = crimson cap with crossed arches, orb and cross; Queen = coronet of pearl-tipped points;
Rook = crenellated tower with a portcullis gate; Bishop = mitre with a diagonal slit and a crosier; Knight = armoured
horse head (chanfron, bridle, crimson plume); Pawn = foot soldier in a kettle helm with a heraldic shield.

Usage: python3 scripts/gen_medieval_theme.py [--out DIR] [--sheet PATH]
"""
import argparse
import math
import os

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

K = 4                 # supersampling factor
SIZE = 256            # output size
BIG = SIZE * K
OUTLINE_W = 6.0       # outer outline, in 256-units
STROKE_W = 2.2        # inner stroke of every part
RIM_W = 3.2           # width of the lit / shaded rims

# Five tones per material: highlight, light, mid, shade, deep
MATERIALS = {
    "white": {
        "body": ["#FFF9E6", "#F3E8C9", "#DDCDA1", "#B8A275", "#8C774F"],  # ivory
        "trim": ["#FFEBA6", "#F2CB5C", "#D6A23A", "#A9751F", "#7A5014"],  # gold
        "cloth": ["#E4604F", "#C63D35", "#A22A28", "#7A1D20", "#52111A"],  # crimson
        "dark": ["#3A2418", "#2E1C12", "#24160E", "#1B100A", "#130A06"],
        "outline": "#2B180D",
        "line": "#4A3220",
    },
    "black": {
        "body": ["#B27649", "#8E5632", "#6C432B", "#4D2F1F", "#33201A"],  # walnut
        "trim": ["#F0D08A", "#D9AE5E", "#B98B3C", "#8B6226", "#5E3F17"],  # brass
        "cloth": ["#D8574A", "#B93A34", "#932827", "#6E1B1E", "#490F17"],
        "dark": ["#2A1A12", "#21130C", "#190E08", "#120905", "#0C0603"],
        "outline": "#0C0604",
        "line": "#BE9A58",
    },
}

CX = 128.0


# ---------------------------------------------------------------- geometry helpers
def sym(right):
    """Mirror a right-hand profile (top to bottom) about the centre line into a closed polygon."""
    return list(right) + [(2 * CX - x, y) for x, y in reversed(right)]


def smooth(points, sharp=(), iters=3):
    """Chaikin corner cutting of a closed polygon; vertices whose index is in `sharp` stay sharp."""
    pts = [(p, i in sharp) for i, p in enumerate(points)]
    for _ in range(iters):
        out = []
        n = len(pts)
        for i in range(n):
            (p, ps), (q, qs) = pts[i], pts[(i + 1) % n]
            out.append((p, True) if ps else ((0.75 * p[0] + 0.25 * q[0], 0.75 * p[1] + 0.25 * q[1]), False))
            if not qs:
                out.append(((0.25 * p[0] + 0.75 * q[0], 0.25 * p[1] + 0.75 * q[1]), False))
        pts = out
    return [p for p, _ in pts]


def arc_path(cx, cy, rx, ry, a0, a1, n=24):
    """Points on an ellipse arc, angles in degrees (0 = right, 90 = down, screen coordinates)."""
    return [(cx + rx * math.cos(math.radians(a0 + (a1 - a0) * i / n)),
             cy + ry * math.sin(math.radians(a0 + (a1 - a0) * i / n))) for i in range(n + 1)]


def S(v):
    return v * K


def sc(points):
    return [(S(x), S(y)) for x, y in points]


def hexrgb(h):
    return tuple(int(h[i:i + 2], 16) for i in (1, 3, 5))


def lerp(a, b, t):
    return tuple(a[i] + (b[i] - a[i]) * t for i in range(3))


# ---------------------------------------------------------------- parts
class Part:
    """One painted piece of a piece: shapes (union), a material key, and style switches."""

    def __init__(self, shapes, mat="body", stroke=True, rim=True, shade=True):
        self.shapes, self.mat, self.stroke, self.rim, self.shade = shapes, mat, stroke, rim, shade


def poly(pts):
    return ("poly", pts)


def spoly(pts, sharp=()):
    return ("poly", smooth(pts, sharp))


def ell(x0, y0, x1, y1):
    return ("ell", (x0, y0, x1, y1))


def circ(cx, cy, r):
    return ("ell", (cx - r, cy - r, cx + r, cy + r))


def rr(x0, y0, x1, y1, r=3.0):
    return ("rr", (x0, y0, x1, y1), r)


def stroke_path(pts, w):
    return ("stroke", pts, w)


def mask_of(shapes):
    m = Image.new("L", (BIG, BIG), 0)
    d = ImageDraw.Draw(m)
    for s in shapes:
        if s[0] == "poly":
            d.polygon(sc(s[1]), fill=255)
        elif s[0] == "ell":
            x0, y0, x1, y1 = s[1]
            d.ellipse([S(x0), S(y0), S(x1), S(y1)], fill=255)
        elif s[0] == "rr":
            x0, y0, x1, y1 = s[1]
            d.rounded_rectangle([S(x0), S(y0), S(x1), S(y1)], radius=S(s[2]), fill=255)
        elif s[0] == "stroke":
            pts, w = sc(s[1]), S(s[2])
            d.line(pts, fill=255, width=int(w), joint="curve")
            for x, y in (pts[0], pts[-1]):
                d.ellipse([x - w / 2, y - w / 2, x + w / 2, y + w / 2], fill=255)
    return m


def grow(m, d):
    """Rounded dilation by about d (in 256-units) using blur + threshold."""
    sigma = max(S(d) / 1.55, 0.5)
    return m.filter(ImageFilter.GaussianBlur(sigma)).point(lambda v: 255 if v > 15 else 0)


def shrink(m, d):
    sigma = max(S(d) / 1.55, 0.5)
    return m.filter(ImageFilter.GaussianBlur(sigma)).point(lambda v: 255 if v > 240 else 0)


def shifted(m, dx, dy):
    return ImageChops.offset(m, int(S(dx)), int(S(dy)))


def banded(pal, x0, x1, levels=7):
    """A horizontal strip image BIG x BIG: light at the left of [x0, x1], deep at the right, in `levels` bands."""
    cols = [hexrgb(c) for c in pal]
    strip = Image.new("RGB", (BIG, 1))
    px = strip.load()
    for x in range(BIG):
        t = (x - S(x0)) / max(S(x1) - S(x0), 1)
        t = min(max(t, 0.0), 1.0)
        t = min(int(t * levels), levels - 1) / (levels - 1)       # banded, not smooth
        t = t ** 1.15 * 0.92                                       # keep the deep tone for the very edge
        f = t * (len(cols) - 1)
        i = min(int(f), len(cols) - 2)
        px[x, 0] = tuple(int(round(v)) for v in lerp(cols[i], cols[i + 1], f - i))
    return strip.resize((BIG, BIG), Image.NEAREST)


def paint_part(rgb, part, side):
    mats = MATERIALS[side]
    pal = mats[part.mat]
    m = mask_of(part.shapes)
    bb = m.getbbox()
    if not bb:
        return m
    x0, x1 = bb[0] / K, bb[2] / K
    if part.shade:
        rgb.paste(banded(pal, x0 - 0.12 * (x1 - x0), x1 + 0.05 * (x1 - x0)), mask=m)
    else:
        rgb.paste(hexrgb(pal[2]), mask=m)
    inner = m
    if part.stroke:
        inner = shrink(m, STROKE_W)
        rgb.paste(hexrgb(mats["outline"]), mask=ImageChops.subtract(m, inner))
    if part.rim and part.shade:
        lit = ImageChops.subtract(inner, shifted(inner, RIM_W, RIM_W * 0.8))
        dim = ImageChops.subtract(inner, shifted(inner, -RIM_W, -RIM_W * 0.8))
        rgb.paste(hexrgb(pal[0]), mask=lit)
        rgb.paste(hexrgb(pal[4]), mask=dim)
    return m


def render_parts(parts, side):
    mats = MATERIALS[side]
    rgb = Image.new("RGB", (BIG, BIG), hexrgb(mats["outline"]))
    union = Image.new("L", (BIG, BIG), 0)
    masks = []
    for p in parts:
        masks.append(mask_of(p.shapes))
        union = ImageChops.lighter(union, masks[-1])
    alpha = grow(union, OUTLINE_W)
    for p in parts:
        paint_part(rgb, p, side)
    img = rgb.convert("RGBA")
    img.putalpha(alpha)
    return img.resize((SIZE, SIZE), Image.LANCZOS)


# ---------------------------------------------------------------- pieces
def base(width=64.0):
    """Shared plinth: a wide foot with a gold band and a moulding above it."""
    return [
        Part([rr(CX - width + 10, 202, CX + width - 10, 222, 7)]),
        Part([rr(CX - width, 216, CX + width, 238, 8)]),
        Part([rr(CX - width + 3, 224, CX + width - 3, 230, 2.5)], "trim"),
    ]


def pawn():
    stem = spoly(sym([(146, 112), (151, 146), (164, 190), (176, 214)]), sharp=(0, 3, 4, 7))
    return [
        Part([stem]),
        *base(60),
        Part([rr(94, 108, 162, 124, 6)], "trim"),
        Part([circ(CX, 92, 24)]),
        # kettle helm: dome, brim, crimson knob
        Part([("poly", arc_path(CX, 84, 29, 31, 180, 360, 24))], "trim"),
        Part([ell(82, 78, 174, 100)], "trim"),
        Part([circ(CX, 52, 7)], "cloth"),
        # heraldic shield on the body
        Part([poly([(114, 146), (142, 146), (142, 168), (128, 188), (114, 168)])], "cloth"),
        Part([rr(125, 151, 131, 180, 1.5), rr(118, 158, 138, 163, 1.5)], "trim", stroke=False, rim=False),
    ]


def rook():
    parts = [
        Part([poly([(80, 208), (88, 98), (168, 98), (176, 208)])]),
        *base(66),
        Part([rr(70, 72, 186, 102, 4)]),
        Part([rr(70, 92, 186, 101, 3)], "trim"),
        Part([rr(78, 50, 178, 76, 3)]),
        # three merlons
        Part([rr(78, 26, 102, 56, 2), rr(116, 26, 140, 56, 2), rr(154, 26, 178, 56, 2)]),
        # stone courses
        Part([stroke_path([(86, 128), (170, 128)], 2.2), stroke_path([(84, 170), (172, 170)], 2.2)], "dark", stroke=False, rim=False, shade=False),
        # arrow slit and the gate with its portcullis
        Part([rr(124.5, 106, 131.5, 124, 2.5)], "dark", stroke=False, rim=False, shade=False),
        Part([("poly", arc_path(CX, 156, 20, 24, 180, 360, 20) + [(148, 210), (108, 210)])], "dark", rim=False),
        Part([stroke_path([(118, 144), (118, 208)], 2.6), stroke_path([(128, 136), (128, 208)], 2.6),
              stroke_path([(138, 144), (138, 208)], 2.6), stroke_path([(110, 168), (146, 168)], 2.6),
              stroke_path([(110, 188), (146, 188)], 2.6)], "trim", stroke=False, rim=False, shade=False),
    ]
    return parts


def bishop():
    stem = spoly(sym([(144, 124), (150, 164), (162, 198), (174, 214)]), sharp=(0, 3, 4, 7))
    mitre = spoly(sym([(CX, 24), (141, 38), (154, 66), (163, 100), (164, 126)]), sharp=(0, 4, 5, 9))
    crosier = [(190, 210), (190, 72)] + arc_path(202, 72, 12, 12, 180, 440, 18)
    return [
        Part([stroke_path(crosier, 8)], "trim"),
        Part([stem]),
        *base(60),
        Part([rr(88, 118, 168, 136, 6)], "trim"),
        Part([mitre]),
        Part([rr(95, 106, 161, 124, 4)], "trim"),
        Part([stroke_path([(CX, 40), (CX, 106)], 6)], "cloth", rim=False),
        Part([circ(CX, 22, 8.5)], "trim"),
        # the slit
        Part([poly([(103, 88), (111, 94), (150, 52), (142, 46)])], "dark", stroke=False, rim=False, shade=False),
    ]


def queen():
    stem = spoly(sym([(144, 98), (150, 140), (164, 190), (176, 214)]), sharp=(0, 3, 4, 7))
    tips = [(94, 52), (108, 38), (CX, 24), (148, 38), (162, 52)]
    zig = [(92, 78)]
    for i, t in enumerate(tips):
        zig.append(t)
        if i < len(tips) - 1:
            zig.append(((t[0] + tips[i + 1][0]) / 2, 66))
    zig += [(164, 78)]
    return [
        Part([stem]),
        *base(62),
        Part([rr(88, 112, 168, 128, 6)], "trim"),
        Part([circ(CX, 98, 21)]),
        Part([poly([(98, 80), (158, 80), (152, 54), (128, 46), (104, 54)])], "cloth"),
        Part([poly(zig)], "trim"),
        *[Part([circ(t[0], t[1] - 1, 6.2 if t[1] > 30 else 7.2)], "trim") for t in tips],
        Part([rr(90, 74, 166, 92, 4)], "trim"),
        *[Part([circ(x, 83, 4.2)], "cloth", rim=False) for x in (108, 128, 148)],
    ]


def king():
    stem = spoly(sym([(146, 100), (152, 140), (167, 190), (180, 214)]), sharp=(0, 3, 4, 7))
    dome = ("poly", arc_path(CX, 80, 37, 32, 180, 360, 28))
    arch1 = stroke_path(arc_path(CX, 80, 35, 31, 180, 360, 28), 7)
    arch2 = stroke_path(arc_path(CX, 80, 13, 31, 180, 360, 28), 6)
    return [
        Part([stem]),
        *base(64),
        Part([rr(86, 112, 170, 128, 6)], "trim"),
        Part([circ(CX, 102, 22)]),
        Part([dome], "cloth"),
        Part([arch1], "trim"),
        Part([arch2], "trim"),
        Part([rr(88, 76, 168, 96, 4)], "trim"),
        *[Part([circ(x, 86, 4.4)], "cloth", rim=False) for x in (106, 128, 150)],
        Part([circ(CX, 47, 9.5)], "trim"),
        Part([rr(122, 10, 134, 44, 2.5), rr(111, 18, 145, 29, 2.5)], "trim"),
    ]


def knight():
    head = smooth([
        (36, 142), (40, 122), (56, 98), (80, 74), (100, 54), (114, 46),
        (122, 8), (142, 38), (154, 52), (172, 84), (186, 124), (191, 166), (194, 214),
        (62, 214), (68, 196), (76, 178), (92, 164), (112, 156), (108, 140),
        (88, 144), (66, 156), (50, 162), (38, 154),
    ], sharp=(6, 7, 12, 13, 18), iters=3)
    mane = poly([(140, 36), (162, 36), (153, 56), (182, 60), (171, 80), (196, 92), (186, 112), (204, 128),
                 (193, 146), (208, 168), (197, 186), (210, 206), (196, 216), (140, 130)])
    chanfron = poly([(42, 130), (58, 106), (80, 84), (98, 66), (108, 74), (94, 94), (76, 114), (58, 138)])
    return [
        Part([mane], "cloth"),
        Part([poly(head)]),
        *base(64),
        Part([chanfron], "trim"),
        # neck barding
        Part([stroke_path(arc_path(138, 150, 60, 30, 20, 70, 12), 6)], "trim"),
        Part([stroke_path(arc_path(150, 176, 62, 30, 20, 70, 12), 6)], "trim"),
        # bridle
        Part([stroke_path([(54, 112), (58, 154)], 6), stroke_path([(58, 154), (94, 124), (108, 98)], 4.5)], "trim", rim=False),
        Part([circ(115, 90, 6.5)], "dark", stroke=False, rim=False, shade=False),
        Part([circ(113.5, 88, 2.2)], "trim", stroke=False, rim=False, shade=False),
        Part([ell(38, 138, 46, 147)], "dark", stroke=False, rim=False, shade=False),
    ]


PIECES = {"king": king, "queen": queen, "rook": rook, "bishop": bishop, "knight": knight, "pawn": pawn}
ORDER = ["king", "queen", "rook", "bishop", "knight", "pawn"]


def render(piece, side):
    return render_parts(PIECES[piece](), side)


# ---------------------------------------------------------------- contact sheet
def make_sheet(path, sprites):
    cell = 200
    light, dark, grey = (241, 230, 207), (30, 33, 68), (236, 236, 236)
    font = ImageFont.load_default(size=14)
    pad = 20
    w = pad + 6 * (cell + 6) + pad
    # 4 big rows + greyscale rows + small rows
    small_rows = [32, 16]
    h = pad + 18 + 4 * (cell + 6) + 2 * (cell // 2 + 6) + 120
    sheet = Image.new("RGBA", (w, h), (60, 60, 60, 255))
    d = ImageDraw.Draw(sheet)

    def tile(x, y, size, bg, spr, gray=False):
        d.rectangle([x, y, x + size - 1, y + size - 1], fill=bg + (255,))
        im = spr.resize((size, size), Image.LANCZOS) if size != SIZE else spr
        if gray:
            la = im.convert("LA").convert("RGBA")
            la.putalpha(im.getchannel("A"))
            im = la
        sheet.alpha_composite(im, (x, y))

    y = pad
    for i, p in enumerate(ORDER):
        d.text((pad + i * (cell + 6) + 4, y), p, fill=(255, 255, 255, 255), font=font)
    y += 18
    for side in ("white", "black"):
        for bg in (light, dark):
            for i, p in enumerate(ORDER):
                tile(pad + i * (cell + 6), y, cell, bg, sprites[(side, p)])
            y += cell + 6
    # greyscale (Blueprint) rows on the off-white board
    for side in ("white", "black"):
        for i, p in enumerate(ORDER):
            tile(pad + i * (cell + 6), y, cell // 2, grey, sprites[(side, p)], gray=True)
            tile(pad + i * (cell + 6) + cell // 2 + 2, y, cell // 2, grey, sprites[(side, p)], gray=False)
        y += cell // 2 + 6
    # small sizes: 32 px and 16 px, both sides, on light / dark / grey squares
    for size in small_rows:
        x = pad
        for side in ("white", "black"):
            for bg in (light, dark, grey):
                for p in ORDER:
                    tile(x, y, size, bg, sprites[(side, p)], gray=(bg == grey))
                    x += size + 2
                x += 8
        y += size + 8
    sheet.crop((0, 0, w, y + pad)).save(path)


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(here, "..", "assets", "images", "Theme_4"))
    ap.add_argument("--sheet", default=None, help="write a preview contact sheet here (docs/screenshots/medieval-pieces.png is this sheet)")
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)
    sprites = {(side, piece): render(piece, side) for side in ("white", "black") for piece in ORDER}
    for (side, piece), img in sprites.items():
        img.save(os.path.join(args.out, f"{side}_{piece}.png"), optimize=True)
    print("wrote", len(ORDER) * 2, "pieces to", os.path.normpath(args.out))
    if args.sheet:
        make_sheet(args.sheet, sprites)
        print("sheet:", args.sheet)


if __name__ == "__main__":
    main()
