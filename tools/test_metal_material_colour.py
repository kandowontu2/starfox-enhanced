"""Full current shared-colour syntax checks only; not Metal execution."""
from pathlib import Path
import re
import unittest

from metal_material_colour import material_colour_source, translate_colour


class MetalColourSourceTests(unittest.TestCase):
    def test_complete_current_shared_sources(self):
        root = Path(__file__).resolve().parents[1]
        actual = (root / "include/starfox/render/metal_material_colour.inc").read_text(encoding="utf-8")
        self.assertEqual(actual, material_colour_source(root))
        calibrated = (root / "src/render/shaders/calibrated_colour.hlsli").read_text(encoding="utf-8")
        self.assertEqual(re.findall(r"case (\d+):", calibrated), re.findall(r"case (\d+):", actual))
        self.assertIn("rgb*(100-intensity)+value*intensity+50", actual)
        self.assertIn("%384", actual)
        self.assertIn("calibrated_palette_colour(colour,effects,palette_colour)", actual)
        self.assertIn("thread float4& output", actual)

    def test_only_known_scalar_swizzles(self):
        self.assertEqual(translate_colour("value=light.xxx+grey.xxx;", True), "value=int3(light)+int3(grey);")
        self.assertEqual(translate_colour("result=light.xxx;", False), "result=float3(light);")

    def test_reference_and_float_syntax(self):
        self.assertEqual(translate_colour("out float4 output; float value=lerp(.5,1.,.25);", False),
                         "thread float4& output; float value=mix(.5f,1.f,.25f);")
        self.assertEqual(translate_colour("float value=1.055*pow(v,1./2.4)-.055;", False),
                         "float value=1.055f*pow(v,1.f/2.4f)-.055f;")

    def test_unknown_syntax_refuses(self):
        for source in ("unknown.xxx;", "out float3 output;", "double value;", "float3x3 matrix;", '#include "unheld.hlsli"'):
            with self.assertRaises(ValueError):
                translate_colour(source, False)


if __name__ == "__main__":
    unittest.main()
