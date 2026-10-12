#!/usr/bin/env bash
# Re-capture the in-game frames that make_artwork.py turns into the PS5
# sce_sys artwork (icon0 / pic0 / pic1).
#
#   platform/ps5/sce_sys/capture_frames.sh RELEASE_DIR [OUTPUT_DIR]
#
# RELEASE_DIR is a Star Fox Enhanced Linux x64 release folder that already
# holds the assets built from your own cartridge (starfox_pc,
# Starfox-Assets.BIN, ...).  It is never modified: the whole folder is copied
# to a temporary directory and the copy is run, because the runtime keeps its
# settings, saves and lock file beside the executable.  OUTPUT_DIR defaults
# to the captures/ folder next to this script.
#
# The runtime's test/capture mode is used (STARFOX_TEST_FRAMES etc.), so the
# window stays hidden, nothing is saved to the copied settings, and each run is
# deterministic: the same release produces the same frames.  Captures are the
# native 16:9 render at the maximum 4x internal scale: 1600x896.
#
# Frames used by make_artwork.py (committed captures were taken with the
# Linux x64 release starfox_pc sha256 b7f2d154...c43c6, built 2026-09-14):
#   corneria_start_f001694.png  pic0   Corneria stage start: the team in
#                                      formation over the city after launch
#   intro_carrier_f002076.png   pic1   attract intro: the attack carrier over
#                                      planet Corneria
#   title_screen_f000108.png    icon0  title screen (logo, team, Arwing)
# Requires bash, python3 and Pillow (for BMP -> PNG).
set -euo pipefail

release=${1:?usage: capture_frames.sh RELEASE_DIR [OUTPUT_DIR]}
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
out=${2:-$here/captures}
release=$(cd "$release" && pwd)
test -x "$release/starfox_pc" || { echo "no starfox_pc in $release" >&2; exit 2; }
mkdir -p "$out"
out=$(cd "$out" && pwd)

# Only the variables below may steer the runtime.
for var in ${!STARFOX_@}; do unset "$var"; done
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cp -r "$release" "$work/release"

# Shared settings: hidden window, no audio, no HUD/FPS counter, 16:9 at 4x,
# heavy anti-aliasing and lighting, no bloom (no added glow), god mode so a
# collision can never end the run.
common=(
    SDL_AUDIODRIVER=dummy
    STARFOX_TEST_HIDDEN=1 STARFOX_TEST_SKIP_PREROLL=1 STARFOX_TEST_UNPACED=1
    STARFOX_TEST_VSYNC=0 STARFOX_TEST_MSU1=0 STARFOX_TEST_SHOW_FPS=0
    STARFOX_TEST_EXPERIENCE=ORIGINAL STARFOX_TEST_DISPLAY_MODE=16_9
    STARFOX_TEST_RENDER_SCALE=4 STARFOX_TEST_HIDE_CONFIGURABLE_HUD=1
    STARFOX_TEST_GOD_MODE=1 STARFOX_TEST_ANTI_ALIASING=3
    STARFOX_TEST_RTX_LIGHTING=1 STARFOX_TEST_SOFTWARE_SHADOWS=1
    STARFOX_TEST_BLOOM=0 STARFOX_TEST_BLOOM_2D=0
)

# shot NAME MAP FRAME [extra VAR=value ...]
# Runs MAP from a fresh start and saves presented frame FRAME (the runtime's
# STARFOX_CAPTURE_DIR numbering) as OUTPUT_DIR/NAME_fFRAME.png.
shot() {
    local name=$1 map=$2 frame=$3; shift 3
    local dir="$work/$name"
    mkdir -p "$dir"
    (cd "$work/release" && env "${common[@]}" STARFOX_TEST_FRAMES=$((frame + 1)) \
        STARFOX_CAPTURE_DIR="$dir" STARFOX_CAPTURE_START="$frame" \
        STARFOX_CAPTURE_INTERVAL=1000000 "$@" ./starfox_pc "$map" \
        >"$dir/stdout.txt" 2>"$dir/stderr.txt")
    local bmp
    bmp=$(printf '%s/%06d.bmp' "$dir" "$frame")
    test -s "$bmp" || { echo "capture failed: $name (see $dir/stderr.txt)" >&2; exit 1; }
    local png
    png=$(printf '%s/%s_f%06d.png' "$out" "$name" "$frame")
    python3 - "$bmp" "$png" <<'PY'
import sys
from PIL import Image
Image.open(sys.argv[1]).convert("RGB").save(sys.argv[2], optimize=True)
PY
    echo "captured $png"
}

# pic0: Corneria from a cold start (carrier launch, then the stage opening
# where the team forms up over the city); no input.
shot corneria_start LEVEL1_1 1694

# pic1: attract-mode intro, the attack carrier above planet Corneria.
shot intro_carrier INTROMAP 2076

# icon0: title screen.
shot title_screen TITLEMAP 108
