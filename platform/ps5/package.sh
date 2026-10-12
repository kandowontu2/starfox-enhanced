#!/usr/bin/env bash
# Packages the converted title ELF as a PS5 native title folder:
#
#   <out>/<TITLE_ID>/eboot.bin              fake-signed SELF of the title ELF
#   <out>/<TITLE_ID>/sce_module/libc.prx    PS5_Vulkan's clean-room libc module
#   <out>/<TITLE_ID>/sce_sys/param.json     title metadata
#   <out>/<TITLE_ID>/sce_sys/icon0.png ...  artwork from platform/ps5/sce_sys
#
#   package.sh <PS5_Vulkan root> <title elf> <param.json> <out dir>
#
# The steps are PS5_Vulkan tools/build-radv-title.sh's. Game data is not
# packaged: the player copies Starfox-Assets.BIN (and the optional
# Starfox-MSU1.PAK) into the installed title folder, as the other ports keep
# them separate (platform/ps5/README.md).
set -euo pipefail

[[ $# -eq 4 ]] || { echo "usage: ${0##*/} <PS5_Vulkan root> <title elf> <param.json> <out dir>" >&2; exit 2; }
root=$(cd -- "$1" && pwd)
elf=$2
param=$3
out=$4
here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
tool=${PS5_NATIVE_TOOL:-$root/build/runtime-shim/ps5-native-tool}
libc="$root/runtime/libc.prx"
fself_magic=0x1D3D154F

for file in "$tool" "$elf" "$param" "$libc"; do
    [[ -f $file ]] || { echo "missing $file (run platform/ps5/bootstrap.sh)" >&2; exit 2; }
done
(cd "$root/runtime" && sha256sum --check --strict --quiet libc.prx.sha256)

title_id=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["titleId"])' "$param")
app="$out/$title_id"
rm -rf -- "$app"
mkdir -p "$app/sce_sys" "$app/sce_module"
"$tool" self --sign --in "$elf" --out "$app/eboot.bin" --magic "$fself_magic"
cp "$param" "$app/sce_sys/param.json"
# The launcher artwork is required: a title without it shows a blank tile.
for asset in icon0.png pic0.dds pic1.dds; do
    [[ -f $here/sce_sys/$asset ]] || { echo "missing artwork $here/sce_sys/$asset" >&2; exit 2; }
    # The backgrounds are Git LFS files; without git-lfs a checkout holds
    # small text pointers instead.
    if [[ $asset == *.dds && $(head -c 4 "$here/sce_sys/$asset") != "DDS " ]]; then
        echo "$here/sce_sys/$asset is not a DDS file (a Git LFS pointer?): install git-lfs and run git lfs pull" >&2
        exit 2
    fi
    cp "$here/sce_sys/$asset" "$app/sce_sys/$asset"
    cmp --silent "$here/sce_sys/$asset" "$app/sce_sys/$asset"
done
[[ -f $here/sce_sys/snd0.at9 ]] && cp "$here/sce_sys/snd0.at9" "$app/sce_sys/snd0.at9"
cp "$libc" "$app/sce_module/libc.prx"
"$tool" self --inspect --file "$app/sce_module/libc.prx" > /dev/null
"$tool" self --inspect --file "$app/eboot.bin" > /dev/null
printf 'PS5 title: %s (eboot.bin %s bytes)\n' "$app" "$(stat -c %s "$app/eboot.bin")"
