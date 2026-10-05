#!/usr/bin/env bash
# Run every tests/ui/scripts/*.ui through the real game under Xvfb and compare the screenshots with
# tests/ui/baseline/.
#
#   tests/ui/run.sh            build ui_script if needed, run all scripts, compare (exit 1 on any mismatch)
#   tests/ui/run.sh --update   same, but replace the baselines with this run (intentional UI change)
#
# Environment: BUILD_DIR (default build), UI_OUT (default ui-out; run/ = screenshots, diff/ = red-overlay diffs),
#              MAX_DIFF_PERCENT (0.2), CHANNEL_THRESHOLD (8), UI_NO_XVFB=1 to use the current display.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
UI="$ROOT/tests/ui"
BUILD_DIR="${BUILD_DIR:-$ROOT/build}"
UI_OUT="${UI_OUT:-$ROOT/ui-out}"
UPDATE=0
[[ "${1:-}" == "--update" ]] && UPDATE=1

if [[ ! -f "$BUILD_DIR/CMakeCache.txt" ]] || ! grep -q '^FDCHESS_BUILD_TOOLS:BOOL=ON' "$BUILD_DIR/CMakeCache.txt"; then
  cmake -S "$ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -DFDCHESS_BUILD_TOOLS=ON
fi
cmake --build "$BUILD_DIR" --target ui_script -j"$(nproc)"
BIN="$BUILD_DIR/tools/ui_script"

rm -rf "$UI_OUT/run"
mkdir -p "$UI_OUT/run"
xvfb=(xvfb-run -a -s "-screen 0 1400x800x24")
[[ "${UI_NO_XVFB:-0}" == "1" ]] && xvfb=()

shopt -s nullglob
scripts=("$UI"/scripts/*.ui)
[[ ${#scripts[@]} -gt 0 ]] || { echo "no scripts in $UI/scripts" >&2; exit 2; }
for s in "${scripts[@]}"; do
  echo "== $(basename "$s")"
  # Each script is a fresh process (fresh GL context, fresh game state)
  "${xvfb[@]}" "$BIN" "$s" "$UI_OUT/run" > "$UI_OUT/$(basename "$s").log" 2>&1 || {
    echo "ui_script failed on $s; log:" >&2; tail -20 "$UI_OUT/$(basename "$s").log" >&2; exit 2; }
  tail -1 "$UI_OUT/$(basename "$s").log"
done

if [[ $UPDATE -eq 1 ]]; then
  rm -f "$UI"/baseline/*.png
  cp "$UI_OUT"/run/*.png "$UI/baseline/"
  if command -v optipng >/dev/null; then
    optipng -quiet -o2 "$UI"/baseline/*.png
  else
    python3 - "$UI/baseline" <<'PY'
import sys, glob
from PIL import Image
for p in glob.glob(sys.argv[1] + "/*.png"):
    with Image.open(p) as im:
        im = im.convert("RGB")
    im.save(p, optimize=True)
PY
  fi
  echo "baselines updated in tests/ui/baseline ($(ls "$UI"/baseline/*.png | wc -l) images, $(du -sh "$UI/baseline" | cut -f1))"
  exit 0
fi

python3 "$UI/compare.py" "$UI_OUT/run" --baseline "$UI/baseline" --diff-dir "$UI_OUT/diff" \
  --max-diff-percent "${MAX_DIFF_PERCENT:-0.2}" --channel-threshold "${CHANNEL_THRESHOLD:-8}"
