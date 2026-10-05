#!/usr/bin/env bash
# Mirrors the Linux CI legs: install deps, configure, build, test.
# Usage (clean container): podman run --rm -v $PWD:/src:Z -w /src ubuntu:24.04 bash scripts/ci-linux.sh
set -euxo pipefail
BUILD_DIR="${BUILD_DIR:-build-ci}"
export DEBIAN_FRONTEND=noninteractive
SUDO=""; [ "$(id -u)" -ne 0 ] && SUDO="sudo"
$SUDO apt-get update
$SUDO apt-get install -y --no-install-recommends build-essential cmake ca-certificates \
  libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev libasound2-dev
cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" -j"$(nproc)"
ctest --test-dir "$BUILD_DIR" -C Release --output-on-failure
