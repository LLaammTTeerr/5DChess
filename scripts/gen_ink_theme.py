#!/usr/bin/env python3
"""Generate the "Ink" piece theme (assets/images/pieces/ink).

East-Asian ink brush (sumi-e) tokens, with the sides told apart xiangqi-style by colour: vermilion on paper vs chalk on black lacquer.
Every piece is a round paper/wood disc (warm paper with a seeded fibre texture and a thin darker rim, ~84% of the
canvas wide).  On it a Western chess silhouette is painted in a handful of brush strokes, each a path of round
stamps whose width follows a profile (loaded start, tapered tail), with seeded dry-brush streaks erased towards the
tail.  No glyphs or text of any kind: only abstract strokes.

  White : vermilion #B42A18 strokes and rim on bright paper #F3E9D2.
  Black : chalk-white ink #ECE1C8 strokes and rim on a black lacquer disc #2A2724 (a dark disc against White's bright
          paper, so the sides differ strongly in value, i.e. in greyscale / Blueprint).

Silhouettes: King = cross over a domed arch crown; Queen = five-pointed crown with pearls; Rook = three merlons on
a tower; Bishop = mitre with a slit and ball; Knight = horse head in a few bold strokes (neck, muzzle, ear, mane);
Pawn = round head over a bell body.  All sit on a brush-stroke base line.

Everything is drawn at 4x and reduced with LANCZOS; the area outside the disc is transparent but carries the rim
colour in RGB.  Fully deterministic (seeded random, no clock): re-running gives byte-identical PNGs.

Usage: python3 scripts/gen_ink_theme.py [--out DIR] [--sheet PATH] [--board PATH]
"""
import argparse
import math
import os
import random

from PIL import Image, ImageChops, ImageDraw, ImageFilter

K = 4
SIZE = 256
BIG = SIZE * K
DISC_R = 107.5  # 84% of 256 wide

SIDES = {
    "white": {"ink": (0xB4, 0x2A, 0x18), "paper": (0xF3, 0xE9, 0xD2)},
    "black": {"ink": (0xEC, 0xE1, 0xC8), "paper": (0x2A, 0x27, 0x24)},
}
PIECES = ["king", "queen", "rook", "bishop", "knight", "pawn"]
BASE_Y = 196


# ---------------------------------------------------------------- stroke engine
def catmull(pts, per=24):
    if len(pts) == 2:
        return [(pts[0][0] + (pts[1][0] - pts[0][0]) * i / per, pts[0][1] + (pts[1][1] - pts[0][1]) * i / per)
                for i in range(per + 1)]
    p = [pts[0]] + list(pts) + [pts[-1]]
    out = []
    for i in range(1, len(p) - 2):
        p0, p1, p2, p3 = p[i - 1], p[i], p[i + 1], p[i + 2]
        for s in range(per):
            t = s / per
            out.append(tuple(0.5 * (2 * p1[j] + (-p0[j] + p2[j]) * t + (2 * p0[j] - 5 * p1[j] + 4 * p2[j] - p3[j]) * t * t
                                    + (-p0[j] + 3 * p1[j] - 3 * p2[j] + p3[j]) * t ** 3) for j in range(2)))
    out.append(pts[-1])
    return out


def lerp_profile(prof, t):
    for (t0, w0), (t1, w1) in zip(prof, prof[1:]):
        if t <= t1:
            return w0 + (w1 - w0) * (t - t0) / max(1e-9, t1 - t0)
    return prof[-1][1]


class Painter:
    def __init__(self, rng):
        self.rng = rng
        self.ink = Image.new("L", (BIG, BIG), 0)
        self.erase = Image.new("L", (BIG, BIG), 0)
        self.di = ImageDraw.Draw(self.ink)
        self.de = ImageDraw.Draw(self.erase)

    @staticmethod
    def disc(d, x, y, r, v=255):
        d.ellipse([(x - r) * K, (y - r) * K, (x + r) * K, (y + r) * K], fill=v)

    def stroke(self, pts, w, prof=None, head=0.5, tail=0.35, dry=True):
        """Brush stroke through pts: width w * profile(t), loaded start (head) and tapered tail."""
        path = catmull(pts)
        prof = prof or [(0, 1), (1, 1)]
        n = len(path)
        seg = [0.0]
        for a, b in zip(path, path[1:]):
            seg.append(seg[-1] + math.hypot(b[0] - a[0], b[1] - a[1]))
        L = max(seg[-1], 1e-6)
        wob = self.rng.uniform(0, 6.28)
        norms = []
        for i, (x, y) in enumerate(path):
            t = seg[i] / L
            a, b = path[max(i - 1, 0)], path[min(i + 1, n - 1)]
            dx, dy = b[0] - a[0], b[1] - a[1]
            m = math.hypot(dx, dy) or 1
            norms.append((-dy / m, dx / m))
            width = w * lerp_profile(prof, t)
            width *= head + (1 - head) * min(1.0, t / 0.18) ** 0.7
            width *= tail + (1 - tail) * min(1.0, (1 - t) / 0.3) ** 0.8
            width *= 1 + 0.05 * math.sin(wob + t * 9)
            self.disc(self.di, x, y, max(width, 0.8) / 2)
            if i + 1 < n:
                steps = int(max(1, (seg[i + 1] - seg[i]) / 0.5))
                for s in range(1, steps):
                    f = s / steps
                    self.disc(self.di, x + (path[i + 1][0] - x) * f, y + (path[i + 1][1] - y) * f, max(width, 0.8) / 2)
        if dry and w >= 9:
            self.streaks(path, seg, L, w, prof, norms)

    def streaks(self, path, seg, L, w, prof, norms):
        rng = self.rng
        for _ in range(max(2, int(w / 4))):
            off = rng.uniform(-0.42, 0.42)
            t0 = rng.uniform(0.35, 0.8)
            t1 = min(1.0, t0 + rng.uniform(0.18, 0.5))
            sw = rng.uniform(0.5, 1.0)
            for i, (x, y) in enumerate(path):
                t = seg[i] / L
                if t0 <= t <= t1:
                    ww = w * lerp_profile(prof, t)
                    self.disc(self.de, x + norms[i][0] * off * ww, y + norms[i][1] * off * ww, sw / 2, 215)

    def chisel(self, quad, streaks=0):
        """Flat-ended chisel stroke: a quad (clockwise) with brushy edges; optional vertical dry streaks."""
        rng = self.rng
        pts = []
        for i in range(4):
            (x0, y0), (x1, y1) = quad[i], quad[(i + 1) % 4]
            m = math.hypot(x1 - x0, y1 - y0)
            nx, ny = (y1 - y0) / m, -(x1 - x0) / m
            for s in range(8):
                f = s / 8
                j = rng.uniform(-0.6, 0.6)
                pts.append(((x0 + (x1 - x0) * f + nx * j) * K, (y0 + (y1 - y0) * f + ny * j) * K))
        self.di.polygon(pts, fill=255)
        ys = [q[1] for q in quad]
        xs = [q[0] for q in quad]
        for _ in range(streaks):
            x = rng.uniform(min(xs) + 4, max(xs) - 4)
            ya = rng.uniform(min(ys) + (max(ys) - min(ys)) * 0.4, max(ys) - 14)
            yb = min(max(ys) + 1, ya + rng.uniform(12, 30))
            self.de.line([(x * K, ya * K), ((x + rng.uniform(-1, 1)) * K, yb * K)], fill=215, width=int(rng.uniform(0.6, 1.1) * K))

    def dot(self, x, y, r):
        self.disc(self.di, x, y, r)

    def hole(self, x, y, r):
        self.disc(self.de, x, y, r)

    def hole_stroke(self, a, b, w):
        for i in range(41):
            f = i / 40
            self.disc(self.de, a[0] + (b[0] - a[0]) * f, a[1] + (b[1] - a[1]) * f, w / 2)

    def result(self):
        ink = ImageChops.multiply(self.ink, ImageChops.invert(self.erase))
        return ink.filter(ImageFilter.GaussianBlur(0.9))


# ---------------------------------------------------------------- pieces
def base(p):
    p.stroke([(72, BASE_Y), (128, BASE_Y + 1), (184, BASE_Y - 1)], 14, head=0.5, tail=0.3)


def pawn(p):
    p.dot(128, 100, 19)
    p.stroke([(106, 126), (150, 126)], 11, head=0.8, tail=0.6, dry=False)
    p.stroke([(128, 128), (128, 188)], 50, [(0, 0.4), (0.4, 0.62), (1, 1)], head=0.9, tail=0.8)
    base(p)


def rook(p):
    for x in (88, 120, 152):
        p.chisel([(x, 78), (x + 16, 78), (x + 16, 106), (x, 106)])
    p.chisel([(86, 104), (170, 104), (170, 120), (86, 120)], streaks=1)
    p.chisel([(106, 118), (150, 118), (156, 192), (100, 192)], streaks=6)
    base(p)


def bishop(p):
    p.dot(128, 52, 7)
    p.stroke([(128, 64), (128, 126)], 46, [(0, 0.2), (0.35, 0.85), (1, 0.8)], head=0.4, tail=0.8)
    p.hole_stroke((122, 100), (137, 82), 3.2)
    p.stroke([(98, 136), (158, 136)], 11, head=0.8, tail=0.7, dry=False)
    p.stroke([(128, 140), (128, 188)], 44, [(0, 0.5), (1, 1)], head=0.9, tail=0.8)
    base(p)


def queen(p):
    tips = [(86, 98), (107, 88), (128, 84), (149, 88), (170, 98)]
    bases = [(102, 130), (115, 130), (128, 130), (141, 130), (154, 130)]
    for (bx, by), (tx, ty) in zip(bases, tips):
        p.stroke([(bx, by), ((bx + tx) / 2, (by + ty) / 2 + 3), (tx, ty + 4)], 18, [(0, 1.1), (1, 0.6)], head=0.9, tail=0.8, dry=False)
        p.dot(tx, ty, 6.5)
    p.stroke([(84, 134), (172, 134)], 20, head=0.9, tail=0.9, dry=False)
    p.stroke([(128, 152), (128, 190)], 56, [(0, 0.5), (1, 1)], head=0.95, tail=0.85)
    base(p)


def king(p):
    p.stroke([(128, 38), (128, 72)], 11, head=0.8, tail=0.7, dry=False)
    p.stroke([(113, 51), (143, 51)], 10, head=0.8, tail=0.7, dry=False)
    p.stroke([(94, 126), (92, 100), (108, 80), (128, 76), (148, 80), (164, 100), (162, 126)], 17, head=0.9, tail=0.8, dry=False)
    p.stroke([(100, 112), (156, 112)], 26, head=0.9, tail=0.9, dry=False)
    p.stroke([(92, 136), (164, 136)], 16, head=0.8, tail=0.8, dry=False)
    p.stroke([(128, 140), (128, 190)], 48, [(0, 0.6), (1, 1)], head=0.9, tail=0.8)
    base(p)


def knight(p):
    # neck and chest
    p.stroke([(164, 190), (158, 146), (150, 104), (134, 72)], 44, [(0, 1), (0.6, 0.75), (1, 0.45)], head=0.8, tail=0.7)
    p.stroke([(116, 156), (146, 186)], 34, head=0.7, tail=0.7, dry=False)
    # head and muzzle
    p.stroke([(138, 80), (110, 92), (84, 124)], 30, [(0, 1.1), (1, 0.7)], head=0.9, tail=0.8, dry=False)
    p.stroke([(100, 118), (112, 142), (132, 150)], 14, head=0.8, tail=0.6, dry=False)
    # ear and mane
    p.stroke([(134, 76), (144, 48)], 13, [(0, 1), (1, 0.5)], head=0.9, tail=0.4, dry=False)
    p.stroke([(158, 70), (178, 112), (180, 164)], 11, head=0.7, tail=0.2, dry=False)
    p.hole(116, 92, 4)
    p.hole_stroke((80, 126), (90, 132), 3)
    base(p)


DRAW = {"king": king, "queen": queen, "rook": rook, "bishop": bishop, "knight": knight, "pawn": pawn}


# ---------------------------------------------------------------- token
def make_token(side, name):
    cfg = SIDES[side]
    ink, paper = cfg["ink"], cfg["paper"]
    rng = random.Random(f"ink-{side}-{name}")
    img = Image.new("RGB", (BIG, BIG), ink)
    disc = Image.new("RGB", (BIG, BIG), paper)
    dd = ImageDraw.Draw(disc)
    for _ in range(240):
        x, y = rng.uniform(20, 236), rng.uniform(20, 236)
        a = rng.uniform(0, math.pi)
        ln = rng.uniform(3, 12)
        v = rng.choice([-11, -7, 6])
        col = tuple(max(0, min(255, c + v)) for c in paper)
        dd.line([(x * K, y * K), ((x + math.cos(a) * ln) * K, (y + math.sin(a) * ln) * K)], fill=col,
                width=int(rng.uniform(0.6, 1.2) * K))
    for _ in range(90):
        x, y = rng.uniform(20, 236), rng.uniform(20, 236)
        r = rng.uniform(0.4, 0.9)
        col = tuple(max(0, c - 22) for c in paper)
        dd.ellipse([(x - r) * K, (y - r) * K, (x + r) * K, (y + r) * K], fill=col)
    disc = disc.filter(ImageFilter.GaussianBlur(0.9 * K))
    vig = Image.new("L", (BIG, BIG), 0)
    e = DISC_R - 6
    ImageDraw.Draw(vig).ellipse([(128 - e) * K, (128 - e) * K, (128 + e) * K, (128 + e) * K], fill=255)
    vig = vig.filter(ImageFilter.GaussianBlur(7 * K))
    dark = tuple(int(c * 0.9) for c in paper)
    disc = Image.composite(disc, Image.new("RGB", (BIG, BIG), dark), vig)
    mask = Image.new("L", (BIG, BIG), 0)
    ImageDraw.Draw(mask).ellipse([(128 - DISC_R) * K, (128 - DISC_R) * K, (128 + DISC_R) * K, (128 + DISC_R) * K], fill=255)
    inner = Image.new("L", (BIG, BIG), 0)
    ri = DISC_R - 5.5
    ImageDraw.Draw(inner).ellipse([(128 - ri) * K, (128 - ri) * K, (128 + ri) * K, (128 + ri) * K], fill=255)
    img.paste(disc, (0, 0), inner)  # rim stays in ink colour
    ring = Image.new("L", (BIG, BIG), 0)
    rr = DISC_R - 10
    ImageDraw.Draw(ring).ellipse([(128 - rr) * K, (128 - rr) * K, (128 + rr) * K, (128 + rr) * K], outline=70, width=int(0.9 * K))
    img = Image.composite(Image.new("RGB", (BIG, BIG), ink), img, ring)
    p = Painter(rng)
    DRAW[name](p)
    img = Image.composite(Image.new("RGB", (BIG, BIG), ink), img, p.result())
    rgb = img.resize((SIZE, SIZE), Image.LANCZOS)
    out = rgb.convert("RGBA")
    out.putalpha(mask.resize((SIZE, SIZE), Image.LANCZOS))
    return out


# ---------------------------------------------------------------- sheets
def load_all(out):
    return {(s, n): Image.open(os.path.join(out, f"{s}_{n}.png")).convert("RGBA") for s in SIDES for n in PIECES}


def paste_row(canvas, imgs, x, y, size, bg, grey=False):
    for i, im in enumerate(imgs):
        t = im.reduce(256 // size) if size < 160 else im.resize((size, size), Image.LANCZOS)
        tile = Image.new("RGBA", (size, size), bg)
        tile.alpha_composite(t)
        if grey:
            tile = tile.convert("L").convert("RGBA")
        canvas.paste(tile, (x + i * size, y))


def contact_sheet(out, path):
    P = load_all(out)
    W = [P[("white", n)] for n in PIECES]
    B = [P[("black", n)] for n in PIECES]
    light, mid, navy = (0xF0, 0xE6, 0xD6, 255), (0xB5, 0x88, 0x63, 255), (0x1C, 0x1E, 0x45, 255)
    margin = 10
    rows = []
    for bg, grey in ((light, False), (mid, False), (navy, False), (light, True)):
        rows += [(160, bg, grey, W), (160, bg, grey, B)]
    for size in (40, 32, 16):
        rows += [(size, light, False, W + B), (size, mid, False, W + B), (size, light, True, W + B)]
    canvas = Image.new("RGBA", (6 * 160 + 2 * margin, margin * 2 + sum(r[0] + 4 for r in rows)), (80, 80, 80, 255))
    y = margin
    for size, bg, grey, imgs in rows:
        paste_row(canvas, imgs, margin, y, size, bg, grey)
        y += size + 4
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    canvas.convert("RGB").save(path)


def board_image(out, path):
    P = load_all(out)
    sq = 48
    back = ["rook", "knight", "bishop", "queen", "king", "bishop", "knight", "rook"]
    boards = [((0xF0, 0xE6, 0xD6), (0xC9, 0xA5, 0x7A)), ((0x6E, 0x58, 0x45), (0x3A, 0x2C, 0x22))]
    canvas = Image.new("RGB", (8 * sq * 2 + 30, 8 * sq + 20), (60, 60, 60))
    for b, (lc, dc) in enumerate(boards):
        ox = 10 + b * (8 * sq + 10)
        for r in range(8):
            for c in range(8):
                canvas.paste(lc if (r + c) % 2 == 0 else dc, (ox + c * sq, 10 + r * sq, ox + (c + 1) * sq, 10 + (r + 1) * sq))
        for c in range(8):
            for r, side, nm in ((0, "black", back[c]), (1, "black", "pawn"), (6, "white", "pawn"), (7, "white", back[c])):
                t = P[(side, nm)].resize((sq, sq), Image.LANCZOS)
                canvas.paste(t, (ox + c * sq, 10 + r * sq), t)
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    canvas.save(path)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "assets", "images", "pieces", "ink"))
    ap.add_argument("--sheet")
    ap.add_argument("--board")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    for side in SIDES:
        for name in PIECES:
            make_token(side, name).save(os.path.join(a.out, f"{side}_{name}.png"), optimize=True)
    if a.sheet:
        contact_sheet(a.out, a.sheet)
    if a.board:
        board_image(a.out, a.board)


if __name__ == "__main__":
    main()
