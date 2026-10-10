"""Exact-source assembly checks only; NOT Metal compilation or GPU acceptance."""
from pathlib import Path
import re
import shutil
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
        self.assertEqual(self.implementation.count("options.fastMathEnabled=NO;"), 3)
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
        self.assertIn("Metal water layers require calibrated source colour", reflection)
        self.assertIn("Unknown Metal reflection material encoding", reflection)
        self.assertIn("sizeof(ReflectionParameters)==416", self.implementation)
        self.assertIn("offsetof(ReflectionParameters,coverage)==272", self.implementation)
        shader = self.body("Reflection")
        self.assertEqual(shader.count("uint4 coverage;"), 1)
        self.assertEqual(shader.count("p.texel_count,p.coverage"), 7)
        self.assertEqual(shader.count("palette_hit("), 8)  # definition plus all seven colour queries
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
        primary_reflection = reflection.split("VisibleHit hit=", 1)[1].split("float distance=", 1)[0]
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
        self.assertIn("if(!ground_hit && p.optical.w!=0u)", shader)
        self.assertIn("float3(1.0f,.766f,.336f)", shader)
        self.assertIn("float3(.955f,.638f,.538f)", shader)
        host = self.implementation.split("bool MetalHardwareRt::render_reflections", 1)[1]
        self.assertLess(host.index("if(specular_models && (!native_rgba || !colour_encoding))"), host.index("impl_->initialize(device)"))
        self.assertIn("roughness>1 || metallic>3 || colour_encoding>2", host)
        self.assertIn("p.optical={colour_encoding,specular_models?1U:0U,0,native_rgba && colour_encoding?1U:0U};", host)

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

    def test_nonblocking_completion_and_ordered_recovery(self):
        code = self.implementation
        complete = code.split("bool complete() noexcept {", 1)[1].split("void await(", 1)[0]
        self.assertIn("recover_fence(slot)", complete)
        self.assertIn("SDL_QueryGPUFence(device,slot.fence)", complete)
        self.assertNotIn("SDL_Wait", complete)
        recover = code.split("bool recover_fence(", 1)[1].split("bool complete()", 1)[0]
        self.assertIn("SDL_SubmitGPUCommandBufferAndAcquireFence(marker)", recover)
        self.assertNotIn("SDL_CancelGPUCommandBuffer", recover)
        self.assertNotIn("SDL_Wait", recover)
        submit = code.split("void submit(", 1)[1].split("bool release(", 1)[0]
        self.assertLess(submit.index("slot.unfenced=true"), submit.index("SDL_SubmitGPUCommandBufferAndAcquireFence(command)"))
        self.assertIn("(void)recover_fence(slot);", submit)
        self.assertNotIn("SDL_CancelGPUCommandBuffer", submit)
        self.assertEqual(code.count("submitted=true;\n            impl_->submit(slot,command);"), 2)

    def test_cleanup_requires_every_slot_before_any_release(self):
        code = self.implementation
        release = code.split("bool release(bool wait) noexcept {", 1)[1].split("void initialize(", 1)[0]
        self.assertLess(release.index("shadow={};reflection={};"), release.index("if(!device)"))
        self.assertLess(release.index("for(auto& slot:inflight) await(slot);"), release.index("clear_completed(slot,true)"))
        self.assertLess(release.index("else if(!complete())"), release.index("SDL_ReleaseGPUBuffer"))
        self.assertIn("shadow_pipeline=nil;reflection_pipeline=nil;metal=nil;", release)
        self.assertIn("output=reflection_buffer=nullptr;output_capacity=reflection_capacity_bytes=0;", release)
        self.assertIn("serial=0;device=nullptr;", release)
        self.assertIn("if(!impl_->release(true)) (void)impl_.release();", code)
        self.assertNotIn("impl_.reset(new Impl)", code)
        self.assertIn("MetalHardwareRt::try_release_device() noexcept {return impl_->release(false);}", code)

    def test_image_capacity_is_not_claimed_as_available_vram(self):
        code = self.implementation
        body = code.split("MetalHardwareRt::working_image_bytes() const noexcept {", 1)[1].split("bool MetalHardwareRt::try_release_device", 1)[0]
        self.assertIn("std::uint64_t(impl_->output_capacity)*4", body)
        self.assertIn("std::uint64_t(impl_->reflection_capacity_bytes)", body)
        self.assertNotIn("scratch", body)
        self.assertNotIn("acceleration", body)

    def test_calibrated_liquid_host_abi_capacity_and_refusal(self):
        code=self.implementation
        host=code.split("bool MetalHardwareRt::render_reflections",1)[1]
        for check in ("offsetof(ReflectionParameters,source_colour)==384", "offsetof(ReflectionParameters,liquid_layers)==400"):
            self.assertIn(check,code)
        for check in ("!ground || water->material!=0 || !colour_encoding",
                      "!std::isfinite(value) || value<0 || value>1", "std::abs(determinant)<1.e-8",
                      "std::abs(determinant)<=volume*1.e-8", "native_water_layers(camera.width,camera.height,water->auxiliary_layers)",
                      "layers?layers->storage_bytes:std::uint32_t(pixels*4U)", "target.length<output_bytes",
                      "impl_->reflection.water_layers=*layers", "water->auxiliary_layers?3U:2U,layers->storage_bytes/4"):
            self.assertIn(check,host)
        self.assertLess(host.index("native_water_layers("),host.index("impl_->initialize(device)"))
        allocation=code.split("void ensure_reflection_output(",1)[1].split("MetalHardwareRt::MetalHardwareRt",1)[0]
        self.assertIn("info.size=bytes;",allocation)
        self.assertNotIn("pixels*4",allocation)

    def test_canonical_liquid_geometry_and_full_transmission(self):
        sys.path.insert(0,str(ROOT / "tools"))
        from metal_liquid_optics import liquid_optics_source
        generated=(ROOT / "include/starfox/render/metal_liquid_optics.inc").read_text(encoding="utf-8")
        self.assertEqual(generated,liquid_optics_source(ROOT))
        actual=(self.destination / "reflection.metal").read_text(encoding="utf-8")
        self.assertIn(generated,actual)
        self.assertEqual(actual.count("LiquidOpticalSample liquid_optical_sample("),1)
        shader=self.body("Reflection")
        water=shader.split("NativeWaterSample native_water_sample(float3 origin",2)[2].split("uint shade_native_water",1)[0]
        for check in ("optical.entering?.75f:1.0f/.75f", "bool total_internal=dot(transmitted,transmitted)<1e-10f",
                      "min(65536.0f,max(result.bias,bottom))", "receiver_world=receiver_view*rotation+offset",
                      "water_caustic_sample(", "path-result.bias", "water_transmitted_channel(receiver.r",
                      "water_transmitted_channel(receiver.g", "water_transmitted_channel(receiver.b",
                      "result.fresnel=total_internal?1.0f", "result.specular=float3(1,.95f,.82f)"):
            self.assertIn(check,water)
        self.assertIn("materials,texels,p.texel_count,true,true,p.coverage",shader)
        self.assertIn("water.radiance*(1.0f-water.fresnel)+water.specular",shader)
        self.assertIn("return reflection_pack_linear(radiance,253u,p);",shader)

    def test_world_layers_do_not_replace_primary_opaque_models(self):
        shader=self.body("Reflection")
        main=shader.split("kernel void starfox_hardware_reflection(",1)[1]
        self.assertLess(main.index("store_liquid_surface(id,float4(0.0f)"),main.index("VisibleHit hit="))
        self.assertLess(main.index("output[p.liquid_layers.x+id]=world_colour"),main.index("SF_VISIBLE_HIT(primary"))
        self.assertIn("if(ground_hit && p.source_colour.w>0.5f)",main)
        self.assertIn("store_liquid_surface(id,world_valid?world_surface:surface,p,output)",main)
        self.assertIn("surface=float4(water.normal,water.hit.z)",shader)
        self.assertIn("if(at+3u>=p.liquid_layers.w)return;",shader)
        self.assertIn("uint4 words=as_type<uint4>(surface)",shader)
        self.assertIn("if(p.optical.x!=0u || abs(denominator)>1.e-10f)",main)
        self.assertLess(main.index("if(ground_hit && p.source_colour.w>0.5f)"),main.index("if(!ground_hit && p.optical.w!=0u)"))
        self.assertNotIn("source_colour.w",main.split("if(!ground_hit && p.optical.w!=0u)",1)[1].split("return;",1)[0])

    def test_calibrated_planar_surface_keeps_authored_base(self):
        shader=self.body("Reflection")
        planar=shader.split("uint shade_native_metal(",1)[1].split("float hash(",1)[0]
        self.assertIn("float3 authored=rgb(p.environment)",planar)
        self.assertIn("p.backdrop.x!=0u && p.cube_info.w==0u",planar)
        self.assertIn("float3(1.0f,.875f,.58f):float3(.8f,.82f,.85f)",planar)
        self.assertLess(planar.index("float3 radiance="),planar.index("if(p.water_settings.y>0.0f)"))
        self.assertIn("return reflection_pack_linear(radiance,254u,p);",planar)

    def test_actual_cmake_generates_and_refreshes_canonical_runtime_sources(self):
        # Exercise the real public embedding fragment, not a reimplemented
        # template recipe. No compiler or GPU is launched by this NONE project.
        cmake,ninja=shutil.which("cmake"),shutil.which("ninja")
        self.assertIsNotNone(cmake,"CMake is required for the embedding integration check")
        self.assertIsNotNone(ninja,"Ninja is required for regeneration checks")
        with tempfile.TemporaryDirectory(prefix="starfox-metal-cmake-") as temporary:
            project=Path(temporary)
            copied=project / "public-source"
            shutil.copytree(ROOT,copied,ignore=shutil.ignore_patterns("__pycache__"))
            build=project / "build"
            (project / "CMakeLists.txt").write_text(
                'cmake_minimum_required(VERSION 3.24)\nproject(MetalEmbedding NONE)\n'
                f'set(STARFOX_METAL_SOURCE_ROOT "{copied.as_posix()}")\n'
                f'include("{(copied / "cmake/NativeMetalMaterialSource.cmake").as_posix()}")\n',encoding="utf-8")
            subprocess.run([cmake,"-S",str(project),"-B",str(build),"-G","Ninja",
                f"-DCMAKE_MAKE_PROGRAM={ninja}",f"-DPython3_EXECUTABLE={sys.executable}"],
                check=True,capture_output=True,text=True)
            generated=build / "generated"
            for name in ("metal_material_colour.inc","metal_liquid_optics.inc"):
                self.assertEqual((generated / name).read_bytes(),(ROOT / "include/starfox/render" / name).read_bytes())
            for name in ("water","native_material"):
                actual=(generated / f"metal_{name}_shared.hpp").read_text(encoding="utf-8")
                self.assertNotIn("@STARFOX_",actual)
            emitted=project / "runtime"
            subprocess.run([sys.executable,str(copied / "tools/check_metal_rt_shaders.py"),
                "--generated-dir",str(generated),"--emit-only",str(emitted)],check=True,capture_output=True)
            for name in ("shadow.metal","reflection.metal","empty_shadow.metal","empty_reflection.metal"):
                self.assertEqual((emitted / name).read_bytes(),(self.destination / name).read_bytes())
            # Change only the temporary copies. Both canonical generators must
            # participate in CMake's ordinary regeneration dependency graph.
            liquid=copied / "src/render/shaders/liquid_optics.hlsli"
            colour=copied / "src/vr/shaders/scene_colour.hlsli"
            liquid.write_text(liquid.read_text(encoding="utf-8")+"\n// Liquid dependency refresh check.\n",encoding="utf-8")
            colour.write_text(colour.read_text(encoding="utf-8")+"\n// Colour dependency refresh check.\n",encoding="utf-8")
            subprocess.run([cmake,"--build",str(build)],check=True,capture_output=True,text=True)
            self.assertIn("// Liquid dependency refresh check.",(generated / "metal_water_shared.hpp").read_text(encoding="utf-8"))
            self.assertIn("// Colour dependency refresh check.",(generated / "metal_native_material_shared.hpp").read_text(encoding="utf-8"))

    def test_empty_variants_retain_entire_normal_optical_source(self):
        for name in ("shadow","reflection"):
            normal=(self.destination / f"{name}.metal").read_text(encoding="utf-8")
            empty=(self.destination / f"empty_{name}.metal").read_text(encoding="utf-8")
            self.assertEqual(empty,"#define STARFOX_EMPTY_NATIVE_SCENE 1\n"+normal)
        code=self.implementation
        empty=code.split("void ensure_empty_pipeline(",1)[1].split("void ensure_output(",1)[0]
        for check in ("stringWithUTF8String:kEmptyScenePrefix", "starfox_metal_water_shared",
                      "starfox_metal_native_material_shared", "kIndexedShader", "reflection?kReflectionShader:kShadowShader"):
            self.assertIn(check,empty)
        shader=self.body("Indexed")
        native_empty=shader.split("#ifdef STARFOX_EMPTY_NATIVE_SCENE",2)[2].split("#else",1)[0]
        self.assertIn("return result;",native_empty)
        self.assertNotIn("intersect(",native_empty)
        self.assertNotIn("intersection_query",native_empty)
        self.assertIn("#define SF_KERNEL_SCENE constant uint& scene [[buffer(0)]]",shader)
        self.assertEqual(code.count("SF_KERNEL_SCENE,"),2)

    def test_empty_header_contract_no_synthetic_triangle_or_as(self):
        code=self.implementation
        contract=code.split("bool empty_native_geometry(",1)[1].split("constexpr char kEmptyScenePrefix",1)[0]
        for check in ("geometry.complete", "geometry.device==device", "geometry.buffer", "geometry.vertex_count==0",
                      "geometry.material_offset>=16", "geometry.material_offset%16U==0", "geometry.material_bytes>=16",
                      "geometry.material_bytes%4U==0", "RayMaterialEncoding::native_rgba",
                      "geometry.materials->triangles.empty()", "geometry.materials->texels.empty()"):
            self.assertIn(check,contract)
        shadow=code.split("bool MetalHardwareRt::render_shadows",1)[1].split("GpuShadowOutput MetalHardwareRt::shadow_output",1)[0]
        reflection=code.split("bool MetalHardwareRt::render_reflections",1)[1]
        self.assertIn("resident_geometry->vertex_count==0 && !empty_resident",shadow)
        self.assertIn("if(no_models)impl_->ensure_empty_pipeline(false)",shadow)
        self.assertIn("if(no_models)impl_->ensure_empty_pipeline(true)",reflection)
        self.assertIn("(geometry.vertex_count<3 && !no_models)",reflection)
        self.assertIn("if(no_models && !resident)vertices=target",shadow)
        self.assertIn("if(no_models)[encoder setBuffer:target offset:0 atIndex:0]",shadow)
        self.assertIn("if(no_models)[encoder setBuffer:slot.palette offset:0 atIndex:0]",reflection)
        self.assertEqual(code.count("if(!no_models)[encoder useResource:slot.acceleration"),2)
        self.assertEqual(code.count("else [encoder setAccelerationStructure:slot.acceleration atBufferIndex:0]"),2)
        self.assertIn("if((!no_models && (!slot.acceleration || !slot.scratch))",reflection)
        self.assertIn("+material_bytes+cube_bytes>vertices.length",reflection)
        self.assertIn("empty_shadow_pipeline=nil;empty_reflection_pipeline=nil",code)

    def test_palette_empty_scene_uses_the_full_empty_shader(self):
        host=self.implementation.split("bool MetalHardwareRt::render_reflections",1)[1]
        for check in ("render::empty_indexed_ray_geometry(geometry,raw)",
                      "empty_indexed || empty_native_geometry(geometry,raw)",
                      "(!geometry.buffer && !empty_indexed)",
                      "if(empty_indexed) vertices=slot.palette",
                      "if(!material_offset && !empty_indexed)",
                      "if((source && !SDL_StarfoxMetalTrackBuffer(command,source))"):
            self.assertIn(check,host)
        # The new host route uses exactly the existing complete optical
        # specialization: no shorter shader, altered water or fake geometry.
        self.assertIn("if(no_models)impl_->ensure_empty_pipeline(true)",host)
        self.assertIn("if(!no_models) {",host)
        self.assertIn("slot.palette=[impl_->metal newBufferWithBytes:palette.data()",host)


if __name__ == "__main__":
    unittest.main()
