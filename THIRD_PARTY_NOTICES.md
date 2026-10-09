# Third-party notices

## AMD FidelityFX Super Resolution 1

The FSR1 EASU/RCAS implementation in `third_party/fsr1` is from
[AMD GPUOpen](https://github.com/GPUOpen-Effects/FidelityFX-FSR), pinned to
commit `a21ffb8f6c13233ba336352bdff293894c706575`.
Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved.
Distributed under the MIT license, reproduced in `third_party/fsr1/LICENSE.txt`
and the upstream headers. This component is independent of NVIDIA DLSS.

## Optional Windows x64 NVIDIA DLSS runtime

Windows x64 releases include unmodified production runtime binaries from
[NVIDIA Streamline 2.14.1](https://github.com/NVIDIA-RTX/Streamline/releases/tag/v2.14.1).
These are separately licensed components, not relicensed under the game's
source license. Full NVIDIA RTX SDK terms, Streamline copyright/license and
third-party notices accompany them in `licenses/dlss` and the embedded runtime
cache (or the `dlss` folder in non-embedded builds). DLSS is optional and
defaults to off. ReShade, RenoDX and DLSS5 add-on binaries are not included.

## RetroCPU

Source: <https://github.com/achaulk/retro_cpu>

Pinned revision: `ea9049ab25084334f7cc1907b3a98bf1c2604a03`

Copyright (c) 2019 Albert Chaulk

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

## snes_spc 0.9.0

Source: <https://github.com/blarggs-audio-libraries/snes_spc>

Pinned revision: `ec8ee2bbe30451614c1d02a83f7af1c97d497d45`

Copyright (C) 2004-2007 Shay Green

Licensed under the GNU Lesser General Public License, version 2.1 or (at your
option) any later version. The complete license text is distributed in the
upstream source as `license.txt`.

## SDL 3.4.14

Source: <https://github.com/libsdl-org/SDL/releases/tag/release-3.4.14>

Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the
use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim
   that you wrote the original software. If you use this software in a
   product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.

The desktop build applies plainly marked, pinned-source extensions for native
GPU/display interoperation. Its private opt-in multisample shader-read extension
allows 2D colour-attachment samples to be read on D3D12 and standard-sample
Vulkan devices; ordinary SDL texture validation and storage restrictions remain
in force. The Windows GPU renderer also has a private opt-in joined callback
for its built-in presenter shader compilation; device/window creation,
command recording and presentation retain their owner-thread behavior.
These are project modifications, not upstream SDL features.

### SDL3 UWP/WinRT fork

Source: <https://github.com/SternXD/SDL3-uwp>

Pinned revision: `8fd8db768df139e9bb4c7a7869fb913a0e29ea89`

The Xbox Developer Mode target uses this maintained SDL 3.4.10-compatible
fork because official SDL no longer ships its UWP backend. The fork retains
SDL's zlib licence above and supplies the WinRT D3D11, WASAPI, filesystem, and
gamepad integration used by the UWP package.

### SDL3 Nintendo Switch libnx backend

Source: <https://github.com/neomody77/sdl3-switch>

Pinned revision: `182e511214d7600e4bdab8606d7caf0ef744afd6`

The Nintendo Switch homebrew target applies the pinned backend patch to SDL
3.4.14. The patch is distributed under SDL's zlib licence, is plainly marked
as a modified SDL source, and its upstream repository states that the backend
was generated with AI assistance and verified on real libnx hardware. It is
used only because upstream SDL's official Nintendo backend is NDA-gated and is
not available to public devkitPro builds.

### SDL3 PS5 backend

The PS5 homebrew target registers its own SDL video, audio, filesystem and
gamepad backend from `platform/ps5/runtime` in SDL 3.4.14.
`tools/prepare_ps5_sdl.py` patches the pinned SDL source only to register
those drivers, to accept the console's RADV driver (which reports no CTS
conformance) and to build SDL's virtual-joystick driver without HIDAPI. These
are marked modifications distributed under SDL's zlib licence.

The pad and AudioOut ABI declarations in `platform/ps5/runtime/ps5_abi.h`
come from [ps5-payload-dev/SDL](https://github.com/ps5-payload-dev/SDL)
revision `ee4c47dc0d617b3bc8f35108f9956baf228a1322`, Copyright (C) 2026 John
Törnblom, under SDL's zlib licence above (retained in that file). The
intercepted-input and Create button bits and `sceAudioOutSetVolume` follow
BlackBearReloaded's `ps5_pad.hpp` and `native_audio.hpp` (GPL-3.0-or-later) as
used by [ProsperoEden](https://github.com/blackbearreloaded/ProsperoEden).

### PS5 title artwork

`platform/ps5/sce_sys` holds the PS5 launcher icon and backgrounds. They are
frames of the game itself (its title screen, the Corneria stage start and the
attract intro) captured with this runtime's capture mode, cropped and scaled by
`make_artwork.py`; see `platform/ps5/sce_sys/ARTWORK.md`. Star Fox, its logo
and characters are trademarks and copyrighted works of Nintendo.

## PlayStation 5 native runtime (PS5 builds only)

The PS5 title statically links, and its package carries, components built
from these pinned public repositories by `platform/ps5/bootstrap.sh`:

- [PS5_Vulkan](https://github.com/mihawk-99/PS5_Vulkan) revision
  `3f3ee69607013b345d2baa6d6a37c86745649a08`: the title CRT and C++
  allocation runtime, the RADV link recipe, AGC import stubs, `ps5-native-tool`
  and the clean-room `sce_module/libc.prx`. Copyright (C) 2026 Mihawk-99 and
  BlackBearReloaded, GPL-3.0-or-later.
- [PS5_Mesa](https://github.com/mihawk-99/PS5_Mesa) revision
  `0b2d6d1a61d9bbf89cf8beb88a696144f67c61f8`: Mesa 26.2 with the RADV PS5
  winsys (the GPU's Vulkan driver, its ACO compiler and VideoOut WSI). Mesa is
  MIT-licensed; individual files carry their own permissive notices.
- [PS5_PayloadSDK](https://github.com/mihawk-99/PS5_PayloadSDK) revision
  `95c08f27386fc698f6bbe21dde3030140a41d10b` over the
  [ps5-payload-dev SDK](https://github.com/ps5-payload-dev/sdk) v0.42:
  compiler wrappers, system-module stubs, LLVM libc++/libc++abi/libunwind
  (Apache-2.0 WITH LLVM-exception) and the `libps5platform` platform layer
  (GPL-3.0-or-later).

Because GPL-3.0-or-later components are linked into the PS5 executable, PS5
binaries are distributed under GPL-3.0-or-later terms with corresponding source
available from this repository and the pinned revisions above.

## dr_flac (dr_libs)

Source: <https://github.com/mackron/dr_libs>

Pinned revision: `b55a0d9a30b91ad8901f89ecf05f76a33186c185`

Copyright 2020 David Reid

This project uses the upstream dual-licence choice of the MIT No Attribution
licence:

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

## Star Fox MSU-1 PCM set

Source: <https://github.com/Sunlitspace542/star-fox-msu1-pcm>

Pinned revision: `7a86296d7f62fd26bc2c51a7ed43eda8e4b21588`

The compressed source tracks are packaged separately as the optional
`Starfox-MSU1.PAK` companion when `STARFOX_PACKAGE_MSU1_MUSIC` is enabled.
The set was assembled by
SunlitSpace542 from Aelieth's Church of Kondo set, with changes and additions
for Sunlit's MSU-1 track mapping. The pack README thanks qwertymodo for
MSUPCM++. Original Star Fox compositions remain credited to Hajime Hirasawa.
No standalone licence file is included by the pinned music-pack repository;
this notice records provenance and attribution and does not alter any rights
in the music or the underlying game.

## Misaki Gothic localization font

Copyright (C) 2002–2021 Num Kadoma. Source and license:
<https://littlelimit.net/misaki.htm> and <https://littlelimit.net/font.htm#license>.
Permission permits use, copying and distribution with or without modification,
commercially or noncommercially, without warranty. The original documentation
and license are included under `docs/fonts` in application packages; source BDF
and generation instructions are in `assets/fonts` in this repository.

## xBRZ (optional)

Source: <https://github.com/janisozaur/xbrz>

Pinned revision: `93c54433fa0df37c689c919e8152fb0b9136584a`

Copyright (C) Zenju (zenju AT gmx DOT de)

Licensed under the GNU General Public License, version 3. Used as the optional
`XBRZ` backend of the `2D FILTER` presentation option. It is fetched and
compiled by default; `-DSTARFOX_ENABLE_XBRZ=OFF` omits it in minimal builds.
The GPU adaptation in `src/render/shaders/xbrz_compute.hlsli` and
`src/render/shaders/xbrz_weights.hlsli`, including their generated SPIR-V/Metal
representations in `src/render/shaders/generated`, is also derived from this implementation
and covered by GPLv3. Distributing a binary containing either implementation carries the GPLv3
obligations for the combined work.


## Optional DisplayXR / Leia SR interfaces

The Windows desktop discovery path uses Khronos OpenXR SDK headers, revision
`f2448a8797c85814aa892efc1ab8707900fbcc78`, and adapted Apache-2.0 interface
declarations from DisplayXR runtime `v2.21.11`. Attribution, modifications and
the complete Apache-2.0 license are in `third_party/displayxr` in source and
`licenses/displayxr` in packaged desktop builds. No DisplayXR runtime is linked
into the game. Windows x64 release packaging additionally includes untouched,
optional upstream installers under `optional-runtimes/displayxr`: runtime
2.21.11 (BSL-1.0, source `316bed95f4ad9f68427966723c99dbe14f3731eb`) and
Leia SR plug-in 2.7.6 (Apache-2.0, source
`c8f7e1cb9b4d0dda485991961759998dd400bbdf`). The original publisher signatures,
sizes and SHA-256 checksums are checked before packaging. Upstream notices and
all runtime `LICENSES` texts are preserved in `licenses/displayxr-runtime` and
`licenses/displayxr-leia-plugin`; pinned inputs are recorded in
`tools/package/displayxr-runtime.json`. The game never runs these installers
automatically. They do not replace the separately required vendor SR Platform
service/drivers; see the optional package README for installation side effects.

### Optional direct SR Platform adapter

The additional Windows x64 `starfox_leia_sr.dll` adapter uses the Leia SR SDK
1.34.10 headers/import libraries from bo3b/SR-lib `api_expansion` commit
`0c80cc01f671e1491ea70f8f9c2358ff938a6503`. The pinned archive SHA-256 is
`bc270bb61eef12581169601b57c8585ad821f46cc1cc004edcd30c6355040056`.
SR-lib's MIT copyright/license (Bo3b Johnson, 2026), SDK permission notice
(Leia, Inc., 2025), and third-party terms are preserved in
`third_party/leia-sr/SR-lib-LICENSE.txt` and the adapter's packaged
`licenses/leia-sr` directory. The license copy is from SR-lib master commit
`174a0cf8868ed91838c9765db5128814c7a31c5c`; the SDK's own License.txt is
installed from the pinned SDK source.

The app-owned texture bridge adapts the GPLv3 implementation in agrofubris's
Star Fox Enhanced stereo fork at `129d9176b9d643b3c7f9627b0809d6777be9cebb`.
Its handoff credits oneup03's RT64 3D and bo3b's SR-lib. Vendor runtime/service
DLLs, optional hand-tracking/UltraLeap components, and DisplayXR are not
redistributed by the adapter. The user installs the supported SR Platform
software for their display. SDK C++ calls stay in a separately built MSVC
module with delay-loaded dependencies; the game loads that module optionally
through a C ABI, including in MinGW packages.

## ScaleFX

The portable ScaleFX compute shader is derived from Sp00kyFox's ScaleFX
passes 0–4 in libretro/slang-shaders, revision
b61e1ee4fc9e2119ec933461a0bfad024dd2950a (2017-03-01 shader revision).
Source: https://github.com/libretro/slang-shaders/tree/b61e1ee4fc9e2119ec933461a0bfad024dd2950a/edge-smoothing/scalefx

Copyright (c) 2016 Sp00kyFox - ScaleFX@web.de

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

## OpenXR SDK 1.1.63 (loader)

Source: <https://github.com/KhronosGroup/OpenXR-SDK>

Pinned revision: `f2448a8797c85814aa892efc1ab8707900fbcc78`

Copyright 2015-2026 The Khronos Group Inc.

The VR targets build the OpenXR loader as a static library. The loader sources
are dual-licensed `Apache-2.0 OR MIT`, and the loader's bundled jsoncpp is
public domain or MIT. The per-file licence data in the upstream source is
authoritative. Licence texts: <https://www.apache.org/licenses/LICENSE-2.0> and
<https://opensource.org/licenses/MIT>.

## Vulkan-Headers

Source: <https://github.com/KhronosGroup/Vulkan-Headers>

Pinned revision: `e5323cdea4ed92dfe825397f6047b8604a40423c`

Copyright 2015-2023 The Khronos Group Inc.

The VR targets use these headers under `Apache-2.0 OR MIT`. Licence texts:
<https://www.apache.org/licenses/LICENSE-2.0> and
<https://opensource.org/licenses/MIT>.

## sfvr

A small C99 library written for the Steam Frame ports of several games, vendored
in `third_party/sfvr` at the commit recorded in `third_party/sfvr/VERSION`. It
has no public repository. It is distributed here under this project's GPL-3.0,
as its README says for vendored copies.
