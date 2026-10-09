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

    def test_shadow_is_exact_shared_plus_runtime_kernel(self):
        actual = (self.destination / "shadow.metal").read_text(encoding="utf-8")
        self.assertEqual(actual, self.body("Indexed") + self.body("Shadow"))
        self.assertIn("uint4 coverage;", actual)
        self.assertIn("[[buffer(4)]]", actual)
        self.assertNotIn("@STARFOX_", actual)

    def test_reflection_has_one_identical_indexed_helper(self):
        actual = (self.destination / "reflection.metal").read_text(encoding="utf-8")
        suffix = self.body("Indexed") + self.body("Reflection")
        self.assertTrue(actual.endswith(suffix))
        self.assertEqual(actual.count("struct RayMaterial {"), 1)
        self.assertEqual(actual.count("uint indexed_source_index("), 1)
        self.assertTrue("struct LavaSample" in actual, "Missing shared lava surface source")
        self.assertNotIn("@STARFOX_", actual)

    def test_runtime_uses_same_source_order(self):
        shadow = re.search(r'NSString\* source=\[\[NSString stringWithUTF8String:kIndexedShader\]\s*'
                           r'stringByAppendingString:\[NSString stringWithUTF8String:kShadowShader\]\];',
                           self.implementation)
        reflection = re.search(r'NSString\* source=\[\[NSString stringWithUTF8String:starfox_metal_water_shared\]\s*'
                               r'stringByAppendingString:\[NSString stringWithUTF8String:kIndexedShader\]\];\s*'
                               r'source=\[source stringByAppendingString:\[NSString stringWithUTF8String:kReflectionShader\]\];',
                               self.implementation)
        self.assertIsNotNone(shadow)
        self.assertIsNotNone(reflection)


if __name__ == "__main__":
    unittest.main()
