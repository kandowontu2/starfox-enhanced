"""Guard actual VR lifetime wiring; native injected-driver tests exercise retirement.

These source checks are not physical Vulkan, DXR or headset acceptance.
"""
from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[1]
APPLICATIONS = ('application.cpp', 'steam_frame_application.cpp')
PIPELINE = 'VulkanSpanPipeline ray_pipeline;'
SCENES = 'std::array<std::unique_ptr<VulkanMixedRayScene>,2> ray_scenes;'
FRAMES = 'std::array<std::unique_ptr<VulkanDxrFrame>,2> ray_frames;'


def check_application(source):
    for declaration in (PIPELINE, SCENES, FRAMES, 'VulkanFrameResources frame_resources;'):
        if source.count(declaration) != 1:
            raise ValueError('Missing or duplicate resource owner: ' + declaration)
    if source.index(SCENES) >= source.index(FRAMES):
        raise ValueError('Ray scene destroys before producer retirement')
    if source.index(PIPELINE) >= source.index(SCENES):
        raise ValueError('Ray pipeline destroys before borrowed scenes and producer retirement')
    for name in ('commands', 'circle_pipeline', 'circle_vertices', 'shutter_pipeline', 'shutter_vertices'):
        if source.count(f'auto& {name}=frame_resources.{name};') != 1:
            raise ValueError('Transition resource bypasses the production owner: ' + name)
    replacements = re.findall(r'ray_frames\[eye\]\s*=\s*std::make_unique<VulkanDxrFrame>\(\);', source)
    ordered = re.findall(
        r'ray_frames\[eye\]\.reset\(\);\s*ray_scenes\[eye\]\.reset\(\);\s*'
        r'ray_frames\[eye\]\s*=\s*std::make_unique<VulkanDxrFrame>\(\);', source)
    if len(replacements) != 1 or len(ordered) != len(replacements):
        raise ValueError('Recovery releases ray inputs before retiring the producer')


def check_owner(source):
    declarations = ('VulkanScenePipeline circle_pipeline;', 'VulkanSceneBuffer circle_vertices;',
                    'VulkanScenePipeline shutter_pipeline;', 'VulkanSceneBuffer shutter_vertices;',
                    'VulkanEyeCommands commands;')
    for declaration in declarations:
        if source.count(declaration) != 1:
            raise ValueError('Missing or duplicate production member: ' + declaration)
    if any(source.index(declaration) >= source.index(declarations[-1]) for declaration in declarations[:-1]):
        raise ValueError('Transition resources destroy before eye retirement')


class ResourceOwnershipTests(unittest.TestCase):
    def test_actual_apps(self):
        for name in APPLICATIONS:
            with self.subTest(app=name):
                check_application((ROOT / 'src/vr' / name).read_text())

    def test_actual_owner(self):
        check_owner((ROOT / 'include/starfox/vr/vulkan_frame_resources.hpp').read_text())

    def test_old_ray_declaration_order_is_rejected(self):
        for name in APPLICATIONS:
            source = (ROOT / 'src/vr' / name).read_text()
            source = source.replace(SCENES, 'OLD_ORDER_PLACEHOLDER').replace(FRAMES, SCENES)
            source = source.replace('OLD_ORDER_PLACEHOLDER', FRAMES)
            with self.subTest(app=name), self.assertRaisesRegex(ValueError, 'Ray scene destroys'):
                check_application(source)

    def test_old_recovery_order_is_rejected(self):
        for name in APPLICATIONS:
            source = (ROOT / 'src/vr' / name).read_text()
            source = source.replace('ray_frames[eye].reset();ray_scenes[eye].reset();', 'ray_scenes[eye].reset();')
            with self.subTest(app=name), self.assertRaisesRegex(ValueError, 'Recovery releases'):
                check_application(source)

    def test_old_ray_pipeline_order_is_rejected(self):
        for name in APPLICATIONS:
            source = (ROOT / 'src/vr' / name).read_text()
            source = source.replace(PIPELINE, '')
            source = source.replace(FRAMES, FRAMES + '\n    ' + PIPELINE)
            with self.subTest(app=name), self.assertRaisesRegex(ValueError, 'Ray pipeline destroys'):
                check_application(source)

    def test_bypassed_transition_owner_is_rejected(self):
        source = (ROOT / 'src/vr/application.cpp').read_text()
        for name in ('commands', 'circle_pipeline', 'circle_vertices', 'shutter_pipeline', 'shutter_vertices'):
            unsafe = source.replace(f'auto& {name}=frame_resources.{name};', '')
            with self.subTest(member=name), self.assertRaisesRegex(ValueError, 'bypasses'):
                check_application(unsafe)

    def test_old_transition_member_order_is_rejected(self):
        source = (ROOT / 'include/starfox/vr/vulkan_frame_resources.hpp').read_text()
        source = source.replace('VulkanEyeCommands commands;', '')
        source = source.replace('VulkanScenePipeline circle_pipeline;',
                                'VulkanEyeCommands commands;\n    VulkanScenePipeline circle_pipeline;')
        with self.assertRaisesRegex(ValueError, 'Transition resources destroy'):
            check_owner(source)


if __name__ == '__main__':
    unittest.main()
