"""Checks tools/prepare_ps5_sdl.py against SDL's driver registries."""
import pathlib
import subprocess
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
SCRIPT = ROOT / 'tools/prepare_ps5_sdl.py'

VIDEO = '''#include "SDL_sysvideo.h"
static VideoBootStrap *bootstrap[] = {
#ifdef SDL_VIDEO_DRIVER_DUMMY
    &DUMMY_bootstrap,
#endif
    NULL
};
'''
AUDIO = '''static const AudioBootStrap *const bootstrap[] = {
    &DISKAUDIO_bootstrap,
    NULL
};
'''
FILESYSTEM = 'char *SDL_SYS_GetBasePath(void) { return NULL; }\n'
SDL_CMAKE = 'dep_option(SDL_VIRTUAL_JOYSTICK    "Enable the virtual-joystick driver" ON SDL_HIDAPI OFF)\n'
GPU_VULKAN = '''        if (physicalDeviceExtensions->KHR_driver_properties) {
            isConformant = (physicalDeviceDriverProperties.conformanceVersion.major >= 1);
        } else {
'''


class PrepareSdlTests(unittest.TestCase):
    def tree(self, root, video=VIDEO, gpu=GPU_VULKAN, cmake=SDL_CMAKE):
        files = {'src/video/SDL_video.c': video, 'src/audio/SDL_audio.c': AUDIO,
                 'src/filesystem/unix/SDL_sysfilesystem.c': FILESYSTEM,
                 'src/gpu/vulkan/SDL_gpu_vulkan.c': gpu, 'CMakeLists.txt': cmake}
        for relative, text in files.items():
            path = root / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(text)

    def run_script(self, root):
        return subprocess.run([sys.executable, str(SCRIPT), str(root)], capture_output=True, text=True)

    def test_registers_native_drivers_first_and_is_idempotent(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            self.tree(root)
            self.assertEqual(self.run_script(root).returncode, 0)
            first = {path: path.read_text() for path in root.rglob('*') if path.is_file()}
            self.assertEqual(self.run_script(root).returncode, 0)
            self.assertEqual(first, {path: path.read_text() for path in root.rglob('*') if path.is_file()})
            # Console pads are SDL virtual gamepads; SDL ties that driver to
            # HIDAPI, which the console lacks (first console run: "SDL not
            # built with virtual-joystick support").
            self.assertIn('"SDL_HIDAPI OR STARFOX_PS5"', (root / 'CMakeLists.txt').read_text())

            video = (root / 'src/video/SDL_video.c').read_text()
            self.assertIn('extern VideoBootStrap STARFOXPS5_bootstrap;', video)
            self.assertLess(video.index('&STARFOXPS5_bootstrap'), video.index('&DUMMY_bootstrap'))
            audio = (root / 'src/audio/SDL_audio.c').read_text()
            self.assertLess(audio.index('&STARFOXPS5AUDIO_bootstrap'), audio.index('&DISKAUDIO_bootstrap'))
            filesystem = (root / 'src/filesystem/unix/SDL_sysfilesystem.c').read_text()
            self.assertTrue(filesystem.startswith('#if !defined(STARFOX_PS5)\n'))
            self.assertTrue(filesystem.rstrip().endswith('#endif'))
            # The console's RADV reports conformance 0.0.0.0; SDL must accept
            # it there (first console run: "SDL_HINT_GPU_DRIVER vulkan unsupported!").
            gpu = (root / 'src/gpu/vulkan/SDL_gpu_vulkan.c').read_text()
            console = gpu[gpu.index('#if defined(STARFOX_PS5)'):gpu.index('#else')]
            self.assertIn('driverID == VK_DRIVER_ID_MESA_RADV', console)
            self.assertIn('conformanceVersion.major >= 1);\n#endif', gpu)

    def test_unknown_virtual_joystick_option_fails(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            self.tree(root, cmake='option(SDL_VIRTUAL_JOYSTICK "x" ON)\n')
            result = self.run_script(root)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('Unsupported SDL virtual-joystick option', result.stderr)

    def test_unknown_conformance_check_fails(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            self.tree(root, gpu='isConformant = true;\n')
            result = self.run_script(root)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('Unsupported SDL Vulkan conformance check', result.stderr)

    def test_unknown_registry_fails_instead_of_silently_skipping(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            self.tree(root, video='static VideoBootStrap *drivers[] = { NULL };\n')
            result = self.run_script(root)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('Unsupported SDL driver registry', result.stderr)


class BackendRegistrationTests(unittest.TestCase):
    def test_named_drivers_are_not_preferred(self):
        # SDL_VideoInit skips is_preferred drivers when SDL_HINT_VIDEO_DRIVER
        # names one, and the game always names "ps5" (first console
        # launch: "SDL_Init: <driver> not available").
        video = (ROOT / 'platform/ps5/runtime/sdl_video.c').read_text()
        bootstrap = video[video.index('VideoBootStrap STARFOXPS5_bootstrap'):]
        fields = bootstrap[bootstrap.index('{') + 1:bootstrap.index('}')].split(',')
        self.assertEqual(fields[0].strip(), '"ps5"')
        self.assertEqual(fields[-1].strip(), 'false')
        audio = (ROOT / 'platform/ps5/runtime/sdl_audio.c').read_text()
        bootstrap = audio[audio.index('AudioBootStrap STARFOXPS5AUDIO_bootstrap'):]
        fields = bootstrap[bootstrap.index('{') + 1:bootstrap.index('}')].split(',')
        self.assertEqual(fields[0].strip(), '"ps5"')
        self.assertEqual([field.strip() for field in fields[-2:]], ['false', 'false'])


if __name__ == '__main__':
    unittest.main()
