#!/usr/bin/env python3
"""Compare a directory of UI screenshots against the committed baselines.

    compare.py RUN_DIR [--baseline DIR] [--diff-dir DIR] [--max-diff-percent 0.2] [--channel-threshold 8]

A pixel "differs" when any RGB channel differs by more than --channel-threshold (0-255). An image fails when
more than --max-diff-percent percent of its pixels differ, when its size changed, or when it exists on only one
side. For every failing image a diff PNG is written to --diff-dir (the new screenshot, dimmed, with the differing
pixels in solid red). Prints a table; exit status 0 = all match, 1 = mismatch, 2 = usage error.

Requires Python 3 and Pillow only.
"""
import argparse
import os
import sys

from PIL import Image, ImageChops

HERE = os.path.dirname(os.path.abspath(__file__))


def load_rgb(path):
    with Image.open(path) as im:
        return im.convert("RGB")


def diff_mask(a, b, threshold):
    """'L' image: 255 where any channel differs by more than `threshold`, else 0."""
    d = ImageChops.difference(a, b)
    r, g, bl = d.split()
    worst = ImageChops.lighter(ImageChops.lighter(r, g), bl)
    return worst.point(lambda v: 255 if v > threshold else 0)


def write_diff_image(new, mask, path):
    dim = Image.blend(new, Image.new("RGB", new.size, (255, 255, 255)), 0.6)
    red = Image.new("RGB", new.size, (255, 0, 0))
    dim.paste(red, mask=mask)
    dim.save(path, optimize=True)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("run_dir", help="directory with the screenshots of this run")
    ap.add_argument("--baseline", default=os.path.join(HERE, "baseline"))
    ap.add_argument("--diff-dir", default="ui-diff", help="where diff images for failing screenshots are written")
    ap.add_argument("--max-diff-percent", type=float, default=0.2,
                    help="max percentage of differing pixels per image (default 0.2)")
    ap.add_argument("--channel-threshold", type=int, default=8,
                    help="a pixel differs when some RGB channel differs by more than this (default 8)")
    args = ap.parse_args()

    if not os.path.isdir(args.run_dir):
        print(f"compare.py: run dir not found: {args.run_dir}", file=sys.stderr)
        return 2
    if not os.path.isdir(args.baseline):
        print(f"compare.py: baseline dir not found: {args.baseline}", file=sys.stderr)
        return 2

    def pngs(d):
        return {f for f in os.listdir(d) if f.lower().endswith(".png")}

    run, base = pngs(args.run_dir), pngs(args.baseline)
    os.makedirs(args.diff_dir, exist_ok=True)
    for f in os.listdir(args.diff_dir):  # stale diffs from an earlier run would be misleading
        if f.lower().endswith(".png"):
            os.remove(os.path.join(args.diff_dir, f))

    rows, failed = [], 0
    for name in sorted(run | base):
        if name not in base:
            rows.append((name, "NEW", "no baseline (run run.sh --update to add it)"))
            failed += 1
            continue
        if name not in run:
            rows.append((name, "MISSING", "baseline has no matching screenshot in this run"))
            failed += 1
            continue
        new, old = load_rgb(os.path.join(args.run_dir, name)), load_rgb(os.path.join(args.baseline, name))
        if new.size != old.size:
            rows.append((name, "SIZE", f"{old.size[0]}x{old.size[1]} -> {new.size[0]}x{new.size[1]}"))
            failed += 1
            continue
        mask = diff_mask(new, old, args.channel_threshold)
        differing = mask.histogram()[255]
        pct = 100.0 * differing / (new.size[0] * new.size[1])
        if pct > args.max_diff_percent:
            write_diff_image(new, mask, os.path.join(args.diff_dir, name))
            rows.append((name, "FAIL", f"{pct:.3f}% differ ({differing} px) > {args.max_diff_percent}%"))
            failed += 1
        else:
            rows.append((name, "ok", f"{pct:.3f}% differ ({differing} px)"))

    width = max((len(r[0]) for r in rows), default=4)
    print(f"{'image'.ljust(width)}  status   detail")
    for name, status, detail in rows:
        print(f"{name.ljust(width)}  {status.ljust(7)}  {detail}")
    print(f"\n{len(rows) - failed}/{len(rows)} match "
          f"(tolerance {args.max_diff_percent}% pixels, channel threshold {args.channel_threshold})")
    if failed:
        print(f"{failed} mismatch(es); diff images in {args.diff_dir}")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
