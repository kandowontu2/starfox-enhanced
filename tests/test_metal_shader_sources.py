"""Exact-source assembly checks only; NOT Metal compilation or GPU acceptance."""
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


class MetalSourceAssemblyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory(prefix="starfox-metal-assembly-")
        cls.destination = Path(cls.temporary.name)
        cls.addClassCleanup(cls.temporary.cleanup)
        subprocess.run([sys.executable, str(ROOT / "tools/check_metal_rt_shaders.py"),
                        "--emit-only", str(cls.destination)], check=True, capture_output=True)
        cls.implementation = (ROOT / "src/render/metal_hardware_rt.mm").read_text(encoding="utf-8")

    def body(self, name):
        match = re.search(rf'constexpr char k{name}Shader\[\] = R"METAL\((.*?)\)METAL";',
                          self.implementation, re.S)
        self.assertIsNotNone(match)
        return match[1]

    def native_body(self):
        template = (ROOT / "src/render/shaders/metal_native_material_shared.hpp.in").read_text(encoding="utf-8")
        template = template.replace("@STARFOX_METAL_NATIVE_COVERAGE_SOURCE@",
            (ROOT / "include/starfox/render/metal_native_material_coverage.inc").read_text(encoding="utf-8"))
        match = re.search(r'R"SF_NATIVE\((.*?)\)SF_NATIVE"', template, re.S)
        self.assertIsNotNone(match)
        return match[1]

    def test_shadow_is_exact_shared_plus_runtime_kernel(self):
        actual = (self.destination / "shadow.metal").read_text(encoding="utf-8")
        self.assertEqual(actual, self.native_body() + self.body("Indexed") + self.body("Shadow"))
        self.assertIn("uint4 coverage;", actual)
        self.assertIn("[[buffer(4)]]", actual)
        self.assertNotIn("@STARFOX_", actual)

    def test_reflection_has_one_identical_indexed_helper(self):
        actual = (self.destination / "reflection.metal").read_text(encoding="utf-8")
        suffix = self.native_body() + self.body("Indexed") + self.body("Reflection")
        self.assertTrue(actual.endswith(suffix))
        self.assertEqual(actual.count("struct RayMaterial {"), 1)
        self.assertEqual(actual.count("uint indexed_source_index("), 1)
        self.assertTrue("struct LavaSample" in actual, "Missing shared lava surface source")
        self.assertNotIn("@STARFOX_", actual)

    def test_runtime_uses_same_source_order(self):
        shadow = re.search(r'NSString\* source=\[\[NSString stringWithUTF8String:starfox_metal_native_material_shared\]\s*'
                           r'stringByAppendingString:\[NSString stringWithUTF8String:kIndexedShader\]\];\s*'
                           r'source=\[source stringByAppendingString:\[NSString stringWithUTF8String:kShadowShader\]\];',
                           self.implementation)
        reflection = re.search(r'NSString\* source=\[\[NSString stringWithUTF8String:starfox_metal_water_shared\]\s*'
                               r'stringByAppendingString:\[NSString stringWithUTF8String:starfox_metal_native_material_shared\]\];\s*'
                               r'source=\[source stringByAppendingString:\[NSString stringWithUTF8String:kIndexedShader\]\];\s*'
                               r'source=\[source stringByAppendingString:\[NSString stringWithUTF8String:kReflectionShader\]\];',
                               self.implementation)
        self.assertIsNotNone(shadow)
        self.assertIsNotNone(reflection)

    def test_native_shadow_bindings_and_legacy_reflection_boundary(self):
        shadow = self.implementation.split("bool MetalHardwareRt::render_shadows", 1)[1].split("GpuShadowOutput MetalHardwareRt::shadow_output", 1)[0]
        self.assertIn("p.coverage={vertex_count/3U,resident_geometry->material_bytes,2,native_material_words}", shadow)
        self.assertIn("vertices.length-material_offset", shadow)
        self.assertIn("material_offset%16U", shadow)
        self.assertIn("resident_geometry->material_bytes<record_bytes", shadow)
        self.assertIn("if(!native_rgba) {", shadow)
        self.assertIn("SDL_StarfoxMetalTrackBuffer", shadow)
        self.assertIn("Native calibrated RGBA reflection material ABI not supported", self.implementation)
        self.assertEqual(self.implementation.count("options.fastMathEnabled=NO;"), 2)
        self.assertEqual(self.implementation.count("if(@available(macOS 13.0,iOS 16.0,*))"), 2)
        self.assertEqual(self.implementation.count("triangles.vertexFormat=MTLAttributeFormatFloat3;"), 2)
        self.assertEqual((self.destination / "shadow.metal").read_text(encoding="utf-8").count("uint starfox_native_material_coverage("), 1)


if __name__ == "__main__":
    unittest.main()
