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
        template = template.replace("@STARFOX_METAL_MATERIAL_COLOUR_SOURCE@",
            (ROOT / "include/starfox/render/metal_material_colour.inc").read_text(encoding="utf-8"))
        template = template.replace("@STARFOX_METAL_NATIVE_COLOUR_SOURCE@",
            (ROOT / "include/starfox/render/metal_native_material_colour.inc").read_text(encoding="utf-8"))
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

    def test_native_shadow_bindings_preserved(self):
        shadow = self.implementation.split("bool MetalHardwareRt::render_shadows", 1)[1].split("GpuShadowOutput MetalHardwareRt::shadow_output", 1)[0]
        self.assertIn("p.coverage={vertex_count/3U,resident_geometry->material_bytes,2,native_material_words}", shadow)
        self.assertIn("vertices.length-material_offset", shadow)
        self.assertIn("material_offset%16U", shadow)
        self.assertIn("resident_geometry->material_bytes<record_bytes", shadow)
        self.assertIn("if(!native_rgba) {", shadow)
        self.assertIn("SDL_StarfoxMetalTrackBuffer", shadow)
        self.assertEqual(self.implementation.count("options.fastMathEnabled=NO;"), 2)
        self.assertEqual(self.implementation.count("if(@available(macOS 13.0,iOS 16.0,*))"), 2)
        self.assertEqual(self.implementation.count("triangles.vertexFormat=MTLAttributeFormatFloat3;"), 2)
        self.assertEqual((self.destination / "shadow.metal").read_text(encoding="utf-8").count("uint starfox_native_material_coverage("), 1)

    def test_native_reflection_resident_atlas_and_colour(self):
        reflection = self.implementation.split("bool MetalHardwareRt::render_reflections", 1)[1]
        self.assertIn("p.coverage={geometry.vertex_count/3U,geometry.material_bytes,2,native_material_words}", reflection)
        self.assertIn("vertices.length-geometry.material_offset", reflection)
        self.assertIn("+material_bytes+cube_bytes>vertices.length", reflection)
        self.assertIn("!geometry.materials->triangles.empty() || !geometry.materials->texels.empty()", reflection)
        self.assertIn("if(!native_rgba) {", reflection)
        self.assertIn("setBuffer:native_rgba?vertices:slot.texels", reflection)
        self.assertIn("Calibrated Metal liquid output layers are not implemented", reflection)
        self.assertIn("Unknown Metal reflection material encoding", reflection)
        self.assertIn("sizeof(ReflectionParameters)==384", self.implementation)
        self.assertIn("offsetof(ReflectionParameters,coverage)==272", self.implementation)
        shader = self.body("Reflection")
        self.assertEqual(shader.count("uint4 coverage;"), 1)
        self.assertEqual(shader.count("p.texel_count,p.coverage"), 6)
        self.assertEqual(shader.count("palette_hit("), 7)  # definition plus all six colour queries
        self.assertIn("p.texel_count,true,false,p.coverage);", shader)
        self.assertIn("starfox_native_material_colour(primitive,bary.x,bary.y,pixel.x,pixel.y", shader)
        helper = (ROOT / "include/starfox/render/metal_native_material_colour.inc").read_text(encoding="utf-8")
        self.assertEqual(helper.count("scene_styled_colour("), 1)
        self.assertIn("if(words[at+15u]==2u) return packed;", helper)
        self.assertIn("if(packed==0u) return 0u;", helper)

    def test_primary_camera_planes_do_not_clip_secondary_rays(self):
        shadow = self.body("Shadow")
        reflection = self.body("Reflection")
        for shader in (shadow, reflection):
            self.assertEqual(shader.count("float4 primary_range;"), 1)
            self.assertIn("float near_depth=p.primary_range.z>0.5f?p.primary_range.x:1.0f;", shader)
            self.assertIn("float far_depth=p.primary_range.z>0.5f?p.primary_range.y:65536.0f;", shader)
        primary_reflection = reflection.split("kernel void starfox_hardware_reflection(", 1)[1].split("float distance=", 1)[0]
        self.assertNotIn("normalize(direction)", primary_reflection)
        self.assertNotIn("normalize(direction)", shadow)
        self.assertIn("ray primary(float3(0.0f),direction,near_depth,far_depth);", shadow)
        self.assertIn("ground>near_depth && ground<receiver", shadow)
        self.assertIn("receiver>=far_depth", shadow)
        self.assertIn("ray shadow(point,p.lights[sample].xyz,bias,65536.0f);", shadow)
        self.assertIn("SF_VISIBLE_HIT(primary,float3(0.0f),direction,near_depth,far_depth)", reflection)
        self.assertIn("ground>near_depth && ground<distance", reflection)
        self.assertIn("distance>=far_depth", reflection)
        self.assertIn("SF_VISIBLE_HIT(obstacle,origin,direction,minimum,65536.0f)", reflection)
        self.assertIn("SF_VISIBLE_HIT(submerged,origin,through,bias,min(65536.0f,bed))", reflection)
        self.assertIn("SF_VISIBLE_HIT(overhead,entry,rotation*float3(0,-1,0),bias,65536.0f)", reflection)
        self.assertIn("SF_VISIBLE_HIT(reflected_hit,point+normal*bias,bounce,bias,65536.0f)", reflection)
        self.assertIn("SF_VISIBLE_HIT(bounced,point,cast,max(0.1f,distance*1.e-5f),65536.0f)", reflection)

    def test_primary_camera_host_abi_and_validation(self):
        header = (ROOT / "include/starfox/render/metal_hardware_rt.hpp").read_text(encoding="utf-8")
        self.assertEqual(header.count("std::optional<PrimaryRayRange> primary_range=std::nullopt"), 2)
        self.assertIn("range->valid() && float(range->near_depth)<float(range->far_depth)", self.implementation)
        self.assertIn("float(depth.near_depth),float(depth.far_depth),range?1.f:0.f,0", self.implementation)
        self.assertIn("sizeof(Parameters)==352", self.implementation)
        self.assertIn("offsetof(Parameters,primary_range)==336", self.implementation)
        self.assertIn("offsetof(ReflectionParameters,primary_range)==288", self.implementation)
        for name in ("shadows", "reflections"):
            entry = self.implementation.split(f"bool MetalHardwareRt::render_{name}", 1)[1].split("return true;", 1)[0]
            self.assertIn("std::optional<PrimaryRayRange> primary_range", entry)
            self.assertLess(entry.index("if(!valid_primary_range(primary_range))"), entry.index("impl_->initialize(device)"))
            self.assertEqual(entry.count("p.primary_range=primary_parameters(primary_range);"), 1)

    def test_explicit_colour_transport_and_primary_model_lobes(self):
        shader = self.body("Reflection")
        self.assertIn("if(p.optical.x==2u) return calibrated_decode_srgb(value);", shader)
        self.assertIn("p.optical.x==1u?value:value*value", shader)
        self.assertIn("if(p.optical.x==2u) value=calibrated_encode_srgb(value);", shader)
        self.assertIn("else if(p.optical.x==0u) value=sqrt(value);", shader)
        self.assertIn("result.radiance=reflection_authored_linear(molten,p);", shader)
        self.assertIn("uint samples=p.roughness>0.0f?8u:1u;", shader)
        self.assertIn("bool model_transport=p.optical.y!=0u;", shader)
        self.assertIn("if(!ground_hit && p.optical.y!=0u)", shader)
        self.assertIn("float3(1.0f,.766f,.336f)", shader)
        self.assertIn("float3(.955f,.638f,.538f)", shader)
        host = self.implementation.split("bool MetalHardwareRt::render_reflections", 1)[1]
        self.assertLess(host.index("if(specular_models && (!native_rgba || !colour_encoding))"), host.index("impl_->initialize(device)"))
        self.assertIn("roughness>1 || metallic>3 || colour_encoding>2", host)
        self.assertIn("p.optical={colour_encoding,specular_models?1U:0U,0,0};", host)

    def test_resident_cube_seams_bindings_and_bounds(self):
        shader = self.body("Reflection")
        self.assertIn("cube_coordinates(direction,face,uv);", shader)
        self.assertIn("if(at>=p.coverage.w) return float3(0.0f);", shader)
        self.assertIn("p.cube_info.x+face*size*size+uint(pixel.y)*size+uint(pixel.x)", shader)
        self.assertIn("if(p.cube_info.w!=0u) return reflected_cube(direction,p,panorama);", shader)
        host = self.implementation.split("bool MetalHardwareRt::render_reflections", 1)[1]
        for check in ("cube.relative_offset!=geometry.material_bytes", "cube.face_size<8 || cube.face_size>512",
                      "std::abs(product-double(row==other))>.01", "+material_bytes+cube_bytes>vertices.length",
                      "setBuffer:panorama offset:resident_environment?geometry.material_offset:0 atIndex:7"):
            self.assertIn(check, host)
        self.assertIn("offsetof(ReflectionParameters,optical)==304", self.implementation)
        self.assertIn("offsetof(ReflectionParameters,cube_info)==320", self.implementation)
        self.assertIn("offsetof(ReflectionParameters,cube_row0)==336", self.implementation)
        self.assertIn("offsetof(ReflectionParameters,cube_row2)==368", self.implementation)

    def test_compensated_ground_does_not_change_legacy_or_secondary_range(self):
        shader = self.body("Reflection")
        self.assertIn("float error=fma(a.x,b.x,-high)+a.x*b.y+a.y*b.x;", shader)
        self.assertIn("float ground=p.optical.x!=0u?native_ground_depth(pixel,p)", shader)
        self.assertIn(":dot(p.ground_point.xyz,p.ground_normal.xyz)/denominator;", shader)
        self.assertIn("float ground_t=abs(denominator)>1e-6f?", shader)
        self.assertIn("SF_VISIBLE_HIT(obstacle,origin,direction,minimum,65536.0f)", shader)


if __name__ == "__main__":
    unittest.main()
