#!/usr/bin/env python3
"""Compose docs/screenshots/piece-themes.png: every piece theme, both sides, on Atlas-coloured squares.

One row per theme (Settings order), White king..pawn then Black king..pawn, read straight from
assets/images/pieces/<id>/. Run after regenerating any theme:  python3 scripts/gen_theme_sheet.py [--out PATH]
Requires Pillow >= 10.1 (ImageFont.load_default(size=...)). Deterministic for a given set of piece PNGs.
"""
import argparse
import os

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
THEMES = ["pixel", "medieval", "bauhaus", "neon", "origami", "ink"]  # Settings order (include/Render/PieceThemes.h)
ORDER = ["king", "queen", "rook", "bishop", "knight", "pawn"]
LIGHT, DARK, PAPER, TEXT = (240, 230, 214), (201, 165, 122), (246, 240, 230), (60, 45, 40)
S, LABEL_W, MARGIN = 96, 130, 14


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(ROOT, "docs", "screenshots", "piece-themes.png"))
    out = ap.parse_args().out
    im = Image.new("RGB", (LABEL_W + 12 * S + 2 * MARGIN, len(THEMES) * S + 2 * MARGIN), PAPER)
    d = ImageDraw.Draw(im)
    font = ImageFont.load_default(size=22)
    for r, theme in enumerate(THEMES):
        y = MARGIN + r * S
        d.text((MARGIN, y + S // 2), theme.capitalize(), fill=TEXT, font=font, anchor="lm")
        for c in range(12):
            side, piece = ("white" if c < 6 else "black"), ORDER[c % 6]
            x = MARGIN + LABEL_W + c * S
            d.rectangle([x, y, x + S - 1, y + S - 1], fill=LIGHT if (r + c) % 2 == 0 else DARK)
            p = Image.open(os.path.join(ROOT, "assets", "images", "pieces", theme, f"{side}_{piece}.png")).convert("RGBA")
            p = p.resize((S, S), Image.NEAREST if theme == "pixel" else Image.LANCZOS)  # pixel art stays crisp
            im.paste(p, (x, y), p)
    im.save(out, optimize=True)
    print(out, im.size)


if __name__ == "__main__":
    main()
