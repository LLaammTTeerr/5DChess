#!/usr/bin/env python3
"""Generate the "Origami" piece theme (assets/images/pieces/origami).

Original folded-paper pieces drawn entirely by the code below.  Every piece is a low-poly solid made of flat
triangles and quads (one "paper" facet each).  A facet carries a value v in 0..1 (0 = facing the light, 1 = turned
away from it); the light comes from the top-left, so left/upper facets are light and right/lower facets are dark.
v is snapped to quarter steps and mapped onto three paper tones per side:

  White: ivory   #FFFFFF / #EDE7DC / #CFC6B6, a dark-umber edge stroke (so White reads on light squares and in greyscale) and darker-ivory fold lines.
  Black: indigo  #3A4A8C / #2A3670 / #1A2250, near-black indigo outline and lighter periwinkle fold lines.

Every facet edge is stroked with a crisp fold line, a seeded (deterministic) paper-grain noise is blended over the
piece, and a soft drop shadow sits under the plinth.  Everything is drawn at 4x and reduced with LANCZOS to
256x256 RGBA; pixels outside the piece carry the outline colour in RGB so premultiplied or greyscale conversion never
leaves a halo.  Output is deterministic; re-running gives byte-identical PNGs.

Silhouettes: King = tall cone with a folded cross on a crown block; Queen = fan-fold (accordion) crown with tip
beads; Rook = boxy folded tower with three merlons and notches; Bishop = folded mitre with a crease slit and bead;
Knight = classic origami horse head with folded ears, jaw and mane; Pawn = folded cone with a faceted ball.

Usage: python3 scripts/gen_origami_theme.py [--out DIR] [--sheet PATH]
(--sheet also writes board.png and board_dark.png next to the sheet.)
"""
import argparse
import math
import os
import random

from PIL import Image, ImageChops, ImageDraw, ImageFilter

K = 4
SIZE = 256
BIG = SIZE * K
CX = 128.0
FOLD_W = 0.9           # fold line width

SIDES = {
    "white": {
        "tones": ["#FFFFFF", "#EDE7DC", "#CFC6B6"],
        "outline": "#2E281F",
        "outline_w": 3.4,
        "fold": "#A39A89",
        "deep": "#9A917F",
        "grain": 5,
    },
    "black": {
        "tones": ["#3A4A8C", "#2A3670", "#1A2250"],
        "outline": "#5666B8",
        "outline_w": 2.6,
        "fold": "#5C6DB4",
        "deep": "#0E1334",
        "grain": 4,
    },
}


def hexrgb(h):
    h = h.lstrip("#")
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))


def lerp(a, b, t):
    return tuple(a[i] + (b[i] - a[i]) * t for i in range(3))


def tone(side, v):
    v = min(1.0, max(0.0, v))
    v = round(v * 4) / 4
    t = [hexrgb(c) for c in SIDES[side]["tones"]]
    if v <= 0.5:
        return lerp(t[0], t[1], v / 0.5)
    return lerp(t[1], t[2], (v - 0.5) / 0.5)


# ---------------------------------------------------------------- facet builders
# A piece is a list of (points, v) facets painted in order; v == "deep" paints the cut/slit colour.
def tri(a, b, c, v):
    return [([a, b, c], v)]


def quad(a, b, c, d, v, split=0.1, flip=False):
    """Quad split into two triangles of slightly different value (a visible diagonal fold)."""
    if flip:
        return [([a, b, c], v - split), ([a, c, d], v + split)]
    return [([a, b, d], v - split), ([b, c, d], v + split)]


def rev(profile, fracs=(-1, -0.4, 0.2, 1), vcols=(0.0, 0.35, 0.85), cx=CX):
    """Faceted solid of revolution.  profile = [(y, half_width)] top to bottom."""
    out = []
    for r in range(len(profile) - 1):
        (y0, w0), (y1, w1) = profile[r], profile[r + 1]
        for c in range(len(fracs) - 1):
            a = (cx + fracs[c] * w0, y0)
            b = (cx + fracs[c + 1] * w0, y0)
            cc = (cx + fracs[c + 1] * w1, y1)
            d = (cx + fracs[c] * w1, y1)
            out += quad(a, b, cc, d, vcols[c] + (0.06 if r % 2 else 0.0), flip=(r + c) % 2 == 0)
    return out


def lit_value(mx, my, bias=0.0):
    d = math.hypot(mx, my) or 1.0
    return 0.5 - 0.5 * ((mx * -0.8 + my * -0.6) / d) + bias


def fan(ring, centre, bias=0.0, closed=True):
    """Triangle fan around `centre`; each triangle is shaded by the direction it faces."""
    out = []
    pairs = zip(ring, ring[1:] + ([ring[0]] if closed else []))
    for a, b in pairs:
        out.append(([centre, a, b], lit_value((a[0] + b[0]) / 2 - centre[0], (a[1] + b[1]) / 2 - centre[1], bias)))
    return out


def ball(cx, cy, r, n=8):
    ring = [(cx + r * math.cos(2 * math.pi * k / n - math.pi / 2), cy + r * math.sin(2 * math.pi * k / n - math.pi / 2))
            for k in range(n)]
    return fan(ring, (cx - 0.22 * r, cy - 0.22 * r))


def plinth():
    return rev([(200, 38), (206, 44), (214, 49), (224, 52)], vcols=(0.0, 0.4, 0.9))


COL = (0.0, 0.35, 0.85)


# ---------------------------------------------------------------- pieces
def pawn():
    f = plinth()
    f += rev([(200, 38), (160, 25), (126, 14)], vcols=COL)
    f += rev([(130, 14), (124, 25), (117, 25), (113, 13)], vcols=COL)
    f += ball(CX, 94, 23, 9)
    return f


def rook():
    f = plinth()
    f += rev([(200, 38), (150, 29), (106, 29)], vcols=COL)
    f += rev([(106, 29), (96, 40), (88, 40)], fracs=(-1, -0.3, 0.4, 1), vcols=(0.05, 0.4, 0.9))
    f += [([(CX - 40, 88), (CX + 40, 88), (CX + 40, 62), (CX - 40, 62)], "deep")]
    for xa, xb in ((-40, -22), (-9, 9), (22, 40)):
        x0, x1, xm = CX + xa, CX + xb, CX + (xa + xb) / 2
        f += quad((x0, 62), (xm, 62), (xm, 88), (x0, 88), 0.1, 0.06)
        f += quad((xm, 62), (x1, 62), (x1, 88), (xm, 88), 0.75, 0.06)
        f += [([(x0, 62), (x0 + 5, 54), (x1 + 5, 54), (x1, 62)], 0.0)]
        if xb < 40:
            f += [([(x1, 62), (x1 + 5, 54), (x1 + 5, 80), (x1, 88)], 0.9)]
    return f


def bishop():
    f = plinth()
    f += rev([(200, 38), (170, 26), (146, 22)], vcols=COL)
    f += rev([(146, 22), (140, 33), (132, 33)], vcols=(0.0, 0.4, 0.9))
    right = [(CX + 16, 66), (CX + 27, 90), (CX + 29, 114), (CX + 24, 134)]
    ring = [(CX, 50)] + right + [(CX, 138)] + [(2 * CX - x, y) for x, y in reversed(right)]
    f += fan(ring, (CX - 8, 96))
    f += [([(CX - 2, 84), (CX + 4, 82), (CX + 22, 112), (CX + 15, 116)], "deep")]
    f += ball(CX, 44, 7, 7)
    return f


def queen():
    f = plinth()
    f += rev([(200, 38), (150, 25), (104, 22)], vcols=COL)
    f += rev([(104, 22), (95, 33), (88, 33)], vcols=(0.0, 0.4, 0.9))
    xs = [-34 + 8.5 * k for k in range(9)]
    tops = [52, 70, 44, 68, 38, 68, 44, 70, 52]
    for k in range(8):
        a, b = (CX + xs[k], tops[k]), (CX + xs[k + 1], tops[k + 1])
        d, c = (CX + xs[k + 1], 88), (CX + xs[k], 88)
        f += quad(a, b, c, d, 0.08 if k % 2 == 0 else 0.82, 0.07, flip=(k % 2 == 0))
    for k in (0, 2, 4, 6, 8):
        f += ball(CX + xs[k], tops[k] - 5, 5.5, 6)
    return f


def king():
    f = plinth()
    f += rev([(200, 38), (150, 26), (98, 23)], vcols=COL)
    f += rev([(98, 23), (88, 35), (80, 35)], vcols=(0.0, 0.4, 0.9))
    f += rev([(80, 30), (64, 28), (60, 28)], fracs=(-1, -0.3, 0.4, 1), vcols=(0.0, 0.4, 0.9))
    f += rev([(52, 21), (44, 21)], fracs=(-1, -0.33, 0.33, 1), vcols=(0.1, 0.5, 0.9))
    f += rev([(60, 6), (24, 6)], fracs=(-1, 0, 1), vcols=(0.0, 0.85))
    f += tri((CX - 6, 24), (CX, 18), (CX + 6, 24), 0.0)
    f += tri((CX - 21, 44), (CX - 27, 48), (CX - 21, 52), 0.1)
    f += tri((CX + 21, 44), (CX + 27, 48), (CX + 21, 52), 0.9)
    return f


def knight():
    P = {
        "base_l": (96, 204), "chest": (92, 170), "throat": (86, 144), "chin": (38, 140), "nose": (26, 128),
        "nose_t": (32, 114), "face": (66, 84), "poll": (102, 56), "ear": (112, 32), "ear_b": (124, 54),
        "m1": (140, 68), "m2": (156, 96), "m3": (168, 132), "m4": (176, 168), "base_r": (172, 204),
        "cheek": (80, 104), "neck": (124, 124), "mid": (146, 152),
        "r1": (134, 80), "r2": (146, 104), "r3": (155, 134), "r4": (162, 166),
    }
    q = lambda *names: [P[n] for n in names]
    f = plinth()
    f += fan(q("nose", "nose_t", "face", "poll", "ear_b", "neck", "throat", "chin"), P["cheek"])
    f += fan(q("ear_b", "m1", "m2", "m3", "m4", "base_r", "base_l", "chest", "throat", "neck"), P["mid"], bias=0.05)
    f += tri(P["poll"], P["ear"], P["ear_b"], 0.35)
    for k, (a, b, c, d) in enumerate((("ear_b", "m1", "r1", "ear_b"), ("m1", "m2", "r2", "r1"),
                                       ("m2", "m3", "r3", "r2"), ("m3", "m4", "r4", "r3"))):
        if k == 0:
            f += tri(P["ear_b"], P["m1"], P["r1"], 0.3)
        else:
            f += quad(P[a], P[b], P[c], P[d], 0.25 if k % 2 else 0.85, 0.08)
    f += tri(P["face"], P["poll"], P["cheek"], 0.15)
    f += tri(P["nose"], P["chin"], P["cheek"], 0.7)
    f += [([(82, 90), (92, 88), (88, 97)], "deep")]
    f += [([(30, 121), (36, 119), (34, 126)], "deep")]
    return f


PIECES = {"king": king, "queen": queen, "rook": rook, "bishop": bishop, "knight": knight, "pawn": pawn}


# ---------------------------------------------------------------- rendering
XS = 1.22              # horizontal stretch of all geometry about the centre line


def S(pts):
    return [((CX + (x - CX) * XS) * K, y * K) for x, y in pts]


def render(name, side):
    cfg = SIDES[side]
    facets = PIECES[name]()
    rng = random.Random(f"origami-{name}-{side}")
    outline, fold, deep = hexrgb(cfg["outline"]), hexrgb(cfg["fold"]), hexrgb(cfg["deep"])
    canvas = Image.new("RGB", (BIG, BIG), outline)
    mask = Image.new("L", (BIG, BIG), 0)
    d, md = ImageDraw.Draw(canvas), ImageDraw.Draw(mask)
    for pts, v in facets:
        col = deep if v == "deep" else tuple(int(round(c)) for c in tone(side, v + rng.uniform(-0.03, 0.03)))
        d.polygon(S(pts), fill=col)
        md.polygon(S(pts), fill=255)
    for pts, v in facets:
        if v != "deep":
            d.line(S(pts + [pts[0]]), fill=fold, width=max(1, int(FOLD_W * K)))
    # paper grain: seeded noise blended inside the piece only
    noise = Image.frombytes("L", (SIZE, SIZE), bytes(rng.randrange(256) for _ in range(SIZE * SIZE)))
    noise = noise.resize((BIG, BIG), Image.BILINEAR)
    amp = cfg["grain"]
    grain = noise.point(lambda p: 128 + int((p - 128) * amp / 128))
    shifted = ImageChops.add(canvas, Image.merge("RGB", (grain,) * 3), scale=1, offset=-128)
    canvas = Image.composite(shifted, canvas, mask)
    # outline = dilated silhouette painted under the facets
    alpha = mask.filter(ImageFilter.MaxFilter(int(cfg["outline_w"] * K) * 2 + 1))
    # soft shadow under the plinth
    sh = Image.new("L", (BIG, BIG), 0)
    ImageDraw.Draw(sh).ellipse([c * K for c in (62, 216, 194, 238)], fill=110)
    sh = sh.filter(ImageFilter.GaussianBlur(5 * K))
    full = ImageChops.lighter(alpha, sh)
    rgb = canvas.resize((SIZE, SIZE), Image.LANCZOS)
    a = full.resize((SIZE, SIZE), Image.LANCZOS)
    return Image.merge("RGBA", (*rgb.split(), a))


# ---------------------------------------------------------------- contact sheet
def gray(im):
    r, g, b, a = im.split()
    l = Image.merge("RGB", (r, g, b)).convert("L")
    return Image.merge("RGBA", (l, l, l, a))


def make_sheet(path, sprites):
    order = list(PIECES)
    rows = [(160, "#F0E6D6", False, False), (160, "#B58863", False, False), (160, "#1C1E45", False, False),
            (160, "#808080", True, False), (40, "#F0E6D6", False, True), (32, "#B58863", False, True),
            (16, "#F0E6D6", False, True), (16, "#1C1E45", False, True)]
    W = 12 * 164 + 8
    H = sum(r[0] + 8 for r in rows) + 8
    sheet = Image.new("RGBA", (W, H), "#444444")
    y = 8
    for size, bg, g, red in rows:
        x = 8
        for side in ("white", "black"):
            for n in order:
                tile = Image.new("RGBA", (size, size), bg)
                spr = sprites[(n, side)]
                if g:
                    spr = gray(spr)
                if size != SIZE:
                    spr = spr.reduce(SIZE // size) if red else spr.resize((size, size), Image.LANCZOS)
                tile.alpha_composite(spr)
                sheet.alpha_composite(tile, (x, y))
                x += size + 4
        y += size + 8
    sheet.convert("RGB").save(path)


def make_board(path, sprites, dark=False, sq=48):
    light, dk = ("#3A3F66", "#1C1E45") if dark else ("#F0E6D6", "#C9A57A")
    im = Image.new("RGBA", (8 * sq, 8 * sq))
    d = ImageDraw.Draw(im)
    back = ["rook", "knight", "bishop", "queen", "king", "bishop", "knight", "rook"]
    for r in range(8):
        for c in range(8):
            d.rectangle([c * sq, r * sq, (c + 1) * sq - 1, (r + 1) * sq - 1], fill=light if (r + c) % 2 == 0 else dk)
    for c in range(8):
        for r, n, s in ((0, back[c], "black"), (1, "pawn", "black"), (6, "pawn", "white"), (7, back[c], "white")):
            im.alpha_composite(sprites[(n, s)].resize((sq, sq), Image.LANCZOS), (c * sq, r * sq))
    im.convert("RGB").save(path)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "assets", "images", "pieces", "origami"))
    ap.add_argument("--sheet", default=None, help="write a preview contact sheet here (not committed)")
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)
    sprites = {}
    for side in SIDES:
        for n in PIECES:
            im = render(n, side)
            im.save(os.path.join(args.out, f"{side}_{n}.png"), optimize=True)
            sprites[(n, side)] = im
    if args.sheet:
        base = os.path.dirname(args.sheet) or "."
        os.makedirs(base, exist_ok=True)
        make_sheet(args.sheet, sprites)
        make_board(os.path.join(base, "board.png"), sprites)
        make_board(os.path.join(base, "board_dark.png"), sprites, dark=True)


if __name__ == "__main__":
    main()
