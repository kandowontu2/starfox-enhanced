"""Check the restricted HLSL-to-GLSL colour syntax adapter (not GPU pixels)."""
from pathlib import Path
import re
import unittest
from vulkan_material_colour import (material_colour_source, translate_colour, liquid_optics_source,
                                    liquid_motion_source, translate_liquid_motion)


class MaterialColourTests(unittest.TestCase):
    def test_shared_current_equations(self):
        root = Path(__file__).resolve().parents[1]
        generated = material_colour_source(root)
        self.assertEqual((root / "src/render/shaders/generated/vulkan_material_colour.glsl").read_text(encoding="utf-8"), generated)
        original = (root / "src/render/shaders/calibrated_colour.hlsli").read_text(encoding="utf-8")
        self.assertEqual(re.findall(r"case (\d+):", original), re.findall(r"case (\d+):", generated))
        self.assertIn("rgb*(100-intensity)+value*intensity+50", generated)
        self.assertIn("%384", generated)
        self.assertIn("calibrated_palette_colour(colour,effects,palette_colour)", generated)

    def test_palette_arrays_and_integer_scalars(self):
        sample = "const int3 palette[2]={int3(1,2,3),int3(4,5,6)}; value=light.xxx;"
        self.assertEqual(translate_colour(sample, True),
            "const ivec3 palette[2]=ivec3[2](ivec3(1,2,3),ivec3(4,5,6)); value=ivec3(light);")

    def test_float_scalars_and_reserved_identifier(self):
        self.assertEqual(translate_colour("float3 output=lerp(light.xxx,other,.5); float output_gamma;", False),
            "vec3 palette_output=mix(vec3(light),other,.5); float output_gamma;")

    def test_unknown_syntax_fails_closed(self):
        for source in ("float2 uv;", "value=unknown.xxx;", "const int3 values[2]={{1,2,3},{4,5,6}};"):
            with self.assertRaises(ValueError):
                translate_colour(source, True)

    def test_liquid_matrix_orientation_and_current_equations(self):
        root = Path(__file__).resolve().parents[1]
        generated = liquid_optics_source(root)
        self.assertEqual((root / "src/render/shaders/generated/vulkan_liquid_optics.glsl").read_text(encoding="utf-8"), generated)
        self.assertIn("(hit*rotation)+offset", generated)
        self.assertIn("(direction*rotation)", generated)
        self.assertIn("(rotation*vec3(dx,-1,dz))", generated)
        self.assertNotIn("mul(", generated)
        self.assertIn("max(travel.y,.12f)", generated)

    def test_liquid_motion_uses_authoritative_bounded_solver(self):
        root = Path(__file__).resolve().parents[1]
        generated = liquid_motion_source(root)
        self.assertEqual((root / "src/render/shaders/generated/vulkan_liquid_hit_motion.glsl").read_text(encoding="utf-8"), generated)
        self.assertIn("transpose(mat3(old.rotation0.xyz,old.rotation1.xyz,old.rotation2.xyz))", generated)
        self.assertIn("(currentHit*transpose(mat3(current0.xyz,current1.xyz,current2.xyz)))", generated)
        self.assertIn("iteration<12", generated)
        self.assertIn("backtrack<5", generated)
        self.assertIn("residual<=.002", generated)
        self.assertIn("liquid_optical_sample(hit,direction,distance,footprint,rotation", generated)
        self.assertNotIn("rayQuery", generated)

    def test_liquid_motion_unknown_syntax_fails_closed(self):
        for source in ("float4x4 basis;", "value=mul(unknown,basis);", "any(unknown<0)", "[loop]for(int x=0;x<3;x++) {}"):
            with self.assertRaises(ValueError):
                translate_liquid_motion(source)


if __name__ == "__main__":
    unittest.main()
