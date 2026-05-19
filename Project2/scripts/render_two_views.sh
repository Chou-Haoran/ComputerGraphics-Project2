#!/usr/bin/env bash
# Render the two required viewpoints of the desktop still-life scene.
# Outputs land in Project2/output/.
#
# Usage: scripts/render_two_views.sh [resolution=800x600] [spp=128]

set -euo pipefail

# Locate the Project2 root regardless of where the script is invoked.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "${SCRIPT_DIR}")"
cd "${PROJECT_ROOT}"

RESOLUTION="${1:-800x600}"
SPP="${2:-128}"
BIN="build/Project2Renderer"
SCENE_DIR="assets/models/desktop_scene"

if [[ ! -x "${BIN}" ]]; then
    echo "${BIN} not found — build first:" >&2
    echo "  cmake -S . -B build && cmake --build build -j8" >&2
    exit 1
fi

mkdir -p output

# TODO: pull camera params from configs/desktop_front.scene /
# configs/desktop_side.scene once SceneLoader grammar lands. Until then
# the loader's fallback handles geometry but ignores camera config, so
# you'll see the same camera for both views — this is the expected
# placeholder behavior.

echo "[1/2] Rendering front view…"
"${BIN}" "${SCENE_DIR}" front "${RESOLUTION}" "${SPP}"

echo "[2/2] Rendering side view…"
"${BIN}" "${SCENE_DIR}" side  "${RESOLUTION}" "${SPP}"

echo "Done. Outputs:"
ls -lh output/front.* output/side.* 2>/dev/null || true
