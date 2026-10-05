#!/usr/bin/env bash
# Run tests/ui/run.sh inside an ubuntu:24.04 container, i.e. the same environment as the CI `ui` job (same Mesa
# llvmpipe, FreeType-free raylib text path, compiler). Baselines are captured this way so local and CI agree.
#
#   tests/ui/container.sh            compare against the baselines
#   tests/ui/container.sh --update   re-baseline from the container
#
# Needs podman (or set ENGINE=docker). Build files go to build-container/ and results to ui-out/ in the checkout.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ENGINE="${ENGINE:-podman}"
"$ENGINE" run --rm -v "$ROOT:/src:Z" -w /src -e BUILD_DIR=/src/build-container -e UI_OUT=/src/ui-out \
  -e DEBIAN_FRONTEND=noninteractive ubuntu:24.04 bash -c '
set -euo pipefail
apt-get update -qq
apt-get install -y -qq --no-install-recommends build-essential cmake ninja-build ca-certificates git \
  libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev libgl1-mesa-dri \
  libasound2-dev xvfb xauth python3 python3-pil >/dev/null
export CMAKE_GENERATOR=Ninja
tests/ui/run.sh '"$*"'
'
