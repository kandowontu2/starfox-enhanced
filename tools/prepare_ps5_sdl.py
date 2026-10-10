#!/usr/bin/env python3
"""Register the Star Fox backend in the pinned SDL3 source (idempotently)."""
import pathlib
import sys

root = pathlib.Path(sys.argv[1])
for relative, typename, name in [
    ('src/video/SDL_video.c', 'VideoBootStrap', 'STARFOXPS5'),
    ('src/audio/SDL_audio.c', 'AudioBootStrap', 'STARFOXPS5AUDIO'),
]:
    path = root / relative
    text = path.read_text()
    declaration = f'extern {typename} {name}_bootstrap;'
    if declaration not in text:
        marker = f'static {typename} *bootstrap[] = {{'
        if marker not in text:
            # SDL's video list has const-qualified entries.
            marker = f'static const {typename} *const bootstrap[] = {{'
        if marker not in text:
            raise SystemExit(f'Unsupported SDL driver registry: {path}')
        text = text.replace(marker, declaration + '\n' + marker + f'\n    &{name}_bootstrap,', 1)
        path.write_text(text)
# Supply console paths instead of Unix /proc and XDG paths.
path = root / 'src/filesystem/unix/SDL_sysfilesystem.c'
text = path.read_text()
if 'STARFOX_PS5' not in text:
    path.write_text('#if !defined(STARFOX_PS5)\n' + text + '\n#endif\n')
# SDL GPU skips Vulkan drivers that report no CTS conformance. The PS5's RADV
# (mihawk-99/PS5_Mesa) reports 0.0.0.0 until a CTS release passes on the
# console, yet it is the console's only GPU driver; accept RADV there only.
path = root / 'src/gpu/vulkan/SDL_gpu_vulkan.c'
text = path.read_text()
anchor = '            isConformant = (physicalDeviceDriverProperties.conformanceVersion.major >= 1);\n'
if 'STARFOX_PS5' not in text:
    if text.count(anchor) != 1:
        raise SystemExit(f'Unsupported SDL Vulkan conformance check: {path}')
    path.write_text(text.replace(anchor, '''#if defined(STARFOX_PS5)
            isConformant = physicalDeviceDriverProperties.conformanceVersion.major >= 1 ||
                           physicalDeviceDriverProperties.driverID == VK_DRIVER_ID_MESA_RADV;
#else
''' + anchor + '#endif\n', 1))
# SDL only offers its virtual-joystick driver with HIDAPI, which the console
# lacks (no HID/USB layer for a title); the driver itself does not use HIDAPI,
# and the native pads (platform/ps5/gamepad.c) are virtual gamepads.
path = root / 'CMakeLists.txt'
text = path.read_text()
anchor = 'dep_option(SDL_VIRTUAL_JOYSTICK    "Enable the virtual-joystick driver" ON SDL_HIDAPI OFF)'
patched = 'dep_option(SDL_VIRTUAL_JOYSTICK    "Enable the virtual-joystick driver" ON "SDL_HIDAPI OR STARFOX_PS5" OFF)'
if patched not in text:
    if text.count(anchor) != 1:
        raise SystemExit(f'Unsupported SDL virtual-joystick option: {path}')
    path.write_text(text.replace(anchor, patched, 1))
