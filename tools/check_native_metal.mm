// Execute the EXACT emitted runtime Metal source. No SDL/DXR/OpenXR bridge or
// GPU-derived hit/colour oracle. This tests shader semantics, not app ownership
// or whether a particular iPhone has dedicated hardware RT units.
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include "native_reflection_fixture.hpp"
#include "native_indexed_cutout_fixture.hpp"
#include "native_shadow_cutout_fixture.hpp"
#include <cstddef>
#include <cstring>
#include <filesystem>

namespace {
using native_reflection_fixture::require;
struct alignas(16) MetalParameters {
    unsigned width{},height{},quality{},metallic{};
    float focal_x{},focal_y{},center_x{},center_y{};
    float roughness{},has_ground{};unsigned environment{},texel_count{};
    std::array<float,4> point{},normal{},water{},row0{},row1{},row2{};
    std::array<unsigned,4> backdrop{},enhanced_size{};
    std::array<float,4> enhanced_motion{},enhanced_plane{},enhanced_projection{},enhanced_palette{},keep0{},keep1{};
};
static_assert(sizeof(MetalParameters)==272);
struct alignas(16) MetalShadowParameters {
    unsigned width{},height{};float focal_x{},focal_y{};
    float center_x{},center_y{},has_ground{},ground_only{};
    std::array<float,4> point{},normal{};
    std::array<std::array<float,4>,16> lights{};
    std::array<unsigned,4> coverage{};
};
static_assert(sizeof(MetalShadowParameters)==336);
static_assert(offsetof(MetalShadowParameters,lights)==64 && offsetof(MetalShadowParameters,coverage)==320);
MetalShadowParameters convert(const native_reflection_fixture::ShadowParameters& p) {
    MetalShadowParameters m;
    m.width=unsigned(p.extent[0]);m.height=unsigned(p.extent[1]);m.focal_x=p.extent[2];m.focal_y=p.extent[3];
    m.center_x=p.center[0];m.center_y=p.center[1];m.has_ground=p.center[2];m.ground_only=p.center[3];
    m.point=p.point;m.normal=p.normal;m.lights=p.lights;m.coverage=p.coverage;return m;
}
MetalParameters convert(const native_reflection_fixture::Parameters& p) {
    MetalParameters m;
    m.width=p.dimensions[0];m.height=p.dimensions[1];m.quality=p.dimensions[2];m.metallic=p.dimensions[3];
    m.focal_x=p.camera[0];m.focal_y=p.camera[1];m.center_x=p.camera[2];m.center_y=p.camera[3];
    m.roughness=p.settings[0];m.has_ground=p.settings[1];m.texel_count=unsigned(p.settings[2]);m.environment=p.environment[0];
    m.point=p.point;m.point[3]=p.settings[3];m.normal=p.normal;m.water=p.water;m.row0=p.row0;m.row1=p.row1;m.row2=p.row2;
    m.backdrop={p.environment[1],p.environment[2],p.environment[3],0};m.enhanced_size=p.enhanced_size;
    m.enhanced_motion=p.enhanced_motion;m.enhanced_plane=p.enhanced_plane;m.enhanced_projection=p.enhanced_projection;
    m.enhanced_palette=p.enhanced_palette;m.keep0=p.keep0;m.keep1=p.keep1;return m;
}
std::string error_text(NSError* error) {return error?std::string(error.localizedDescription.UTF8String):"No Apple error details";}
void finish(id<MTLCommandBuffer> command) {
    require(command!=nil,"Missing Metal command buffer");[command commit];[command waitUntilCompleted];
    if(command.status!=MTLCommandBufferStatusCompleted) throw std::runtime_error(error_text(command.error));
}
}

int main(int argc,char** argv) { @autoreleasepool { try {
    require(argc==3,"Usage: starfox_native_metal_check exact-reflection.metal exact-shadow.metal");
    if(@available(macOS 14.0,*)) {} else {
        std::cerr<<"SKIP: the native runtime check requires macOS 14 or later; NOT a runtime pass\n";return 2;
    }
    id<MTLDevice> device=nil;
    for(id<MTLDevice> candidate in MTLCopyAllDevices()) if(candidate.supportsRaytracing) {device=candidate;break;}
    if(!device) {std::cerr<<"SKIP: no native Metal ray-intersection device; NOT a runtime pass\n";return 2;}
    std::cout<<"Native Metal shader on "<<device.name.UTF8String<<std::endl;
    NSError* error=nil;
    NSString* source=[NSString stringWithContentsOfFile:[NSString stringWithUTF8String:argv[1]] encoding:NSUTF8StringEncoding error:&error];
    if(!source) throw std::runtime_error(error_text(error));
    // Same runtime source and compile options as MetalHardwareRt, not a
    // replacement scalar shader or a test-only intersection implementation.
    id<MTLLibrary> library=[device newLibraryWithSource:source options:nil error:&error];
    if(!library) throw std::runtime_error(error_text(error));
    id<MTLFunction> kernel=[library newFunctionWithName:@"starfox_hardware_reflection"];
    require(kernel!=nil,"Missing exact runtime reflection kernel");
    id<MTLComputePipelineState> pipeline=[device newComputePipelineStateWithFunction:kernel error:&error];
    if(!pipeline) throw std::runtime_error(error_text(error));
    NSString* shadow_source=[NSString stringWithContentsOfFile:[NSString stringWithUTF8String:argv[2]] encoding:NSUTF8StringEncoding error:&error];
    if(!shadow_source) throw std::runtime_error(error_text(error));
    id<MTLLibrary> shadow_library=[device newLibraryWithSource:shadow_source options:nil error:&error];
    if(!shadow_library) throw std::runtime_error(error_text(error));
    id<MTLFunction> shadow_kernel=[shadow_library newFunctionWithName:@"starfox_hardware_shadow"];
    require(shadow_kernel!=nil,"Missing exact runtime shadow kernel");
    id<MTLComputePipelineState> shadow_pipeline=[device newComputePipelineStateWithFunction:shadow_kernel error:&error];
    if(!shadow_pipeline) throw std::runtime_error(error_text(error));
    auto fixture=native_reflection_fixture::make_fixture();
    id<MTLBuffer> vertices=[device newBufferWithBytes:fixture.vertices.data() length:sizeof(fixture.vertices) options:MTLResourceStorageModeShared];
    id<MTLBuffer> materials=[device newBufferWithBytes:fixture.materials.data() length:sizeof(fixture.materials) options:MTLResourceStorageModeShared];
    // Dedicated CPU-uploaded records and an aligned resident record range.
    // This verifies native shader bindings, NOT SDL ownership/lifetime.
    id<MTLBuffer> resident=[device newBufferWithLength:sizeof(fixture.vertices)+sizeof(fixture.materials) options:MTLResourceStorageModeShared];
    id<MTLBuffer> palette=[device newBufferWithBytes:fixture.palette.data() length:sizeof(fixture.palette) options:MTLResourceStorageModeShared];
    unsigned zero=0;
    id<MTLBuffer> empty=[device newBufferWithBytes:&zero length:4 options:MTLResourceStorageModeShared];
    std::array<unsigned char,16> blank{};
    id<MTLBuffer> texels=[device newBufferWithBytes:blank.data() length:blank.size() options:MTLResourceStorageModeShared];
    constexpr unsigned pixels=native_reflection_fixture::width*native_reflection_fixture::height;
    id<MTLBuffer> output=[device newBufferWithLength:pixels*4 options:MTLResourceStorageModeShared];
    require(vertices && materials && resident && palette && empty && texels && output,"Metal fixture resource allocation");
    id<MTLCommandQueue> queue=[device newCommandQueue];require(queue!=nil,"Metal command queue");
    MTLAccelerationStructureTriangleGeometryDescriptor* triangles=[MTLAccelerationStructureTriangleGeometryDescriptor descriptor];
    triangles.vertexBuffer=vertices;triangles.vertexStride=16;triangles.vertexFormat=MTLAttributeFormatFloat3;triangles.triangleCount=4;triangles.opaque=NO;
    MTLPrimitiveAccelerationStructureDescriptor* description=[MTLPrimitiveAccelerationStructureDescriptor descriptor];description.geometryDescriptors=@[triangles];
    id<MTLAccelerationStructure> acceleration=nil;
    const auto build_scene=[&](const auto& f) {
        std::memcpy(vertices.contents,f.vertices.data(),sizeof(f.vertices));
        const auto sizes=[device accelerationStructureSizesWithDescriptor:description];
        acceleration=[device newAccelerationStructureWithSize:sizes.accelerationStructureSize];
        id<MTLBuffer> scratch=[device newBufferWithLength:sizes.buildScratchBufferSize options:MTLResourceStorageModePrivate];
        require(acceleration && scratch,"Metal acceleration allocation");
        id<MTLCommandBuffer> build=[queue commandBuffer];require(build!=nil,"Metal acceleration command");
        id<MTLAccelerationStructureCommandEncoder> builder=[build accelerationStructureCommandEncoder];require(builder!=nil,"Metal acceleration encoder");
        [builder buildAccelerationStructure:acceleration descriptor:description scratchBuffer:scratch scratchBufferOffset:0];[builder endEncoding];finish(build);
    };
    build_scene(fixture);
    const auto dispatch=[&](const native_reflection_fixture::Parameters& parameters) {
        const auto p=convert(parameters);
        id<MTLCommandBuffer> command=[queue commandBuffer];require(command!=nil,"Metal reflection command");
        id<MTLComputeCommandEncoder> encoder=[command computeCommandEncoder];require(encoder!=nil,"Metal reflection encoder");
        [encoder setComputePipelineState:pipeline];[encoder setAccelerationStructure:acceleration atBufferIndex:0];
        [encoder setBuffer:output offset:0 atIndex:1];[encoder setBytes:&p length:sizeof(p) atIndex:2];
        [encoder setBuffer:vertices offset:0 atIndex:3];[encoder setBuffer:materials offset:0 atIndex:4];[encoder setBuffer:palette offset:0 atIndex:5];
        [encoder setBuffer:texels offset:0 atIndex:6];[encoder setBuffer:empty offset:0 atIndex:7];[encoder setBuffer:empty offset:0 atIndex:8];
        [encoder useResource:acceleration usage:MTLResourceUsageRead];
        [encoder dispatchThreads:MTLSizeMake(pixels,1,1) threadsPerThreadgroup:MTLSizeMake(std::min<NSUInteger>(64,pipeline.maxTotalThreadsPerThreadgroup),1,1)];
        [encoder endEncoding];finish(command);
        require(output.contents!=nullptr,"Metal output mapping");
        std::vector<unsigned> result(pixels);std::memcpy(result.data(),output.contents,pixels*4);return result;
    };
    native_reflection_fixture::run(fixture,dispatch);
    native_reflection_fixture::run_primary(fixture,dispatch);
    native_reflection_fixture::run_indexed_cutouts(fixture,[&](const auto& p,const auto& cutout,const auto& ink) {
        std::memcpy(materials.contents,cutout.materials.data(),sizeof(cutout.materials));
        std::memcpy(palette.contents,cutout.palette.data(),sizeof(cutout.palette));
        std::memcpy(texels.contents,ink.data(),ink.size());
        return dispatch(p);
    });
    for(bool resident_records:{false,true}) {
        native_reflection_fixture::run_shadow_cutouts(fixture,[&](const auto& parameters,const auto& cutout,const auto& ink) {
            std::memcpy(materials.contents,cutout.materials.data(),sizeof(cutout.materials));
            std::memcpy(resident.contents,cutout.vertices.data(),sizeof(cutout.vertices));
            std::memcpy(static_cast<unsigned char*>(resident.contents)+sizeof(cutout.vertices),cutout.materials.data(),sizeof(cutout.materials));
            if(resident_records)std::memset(materials.contents,0,sizeof(cutout.materials)); // Opaque poison must not be read.
            std::memcpy(texels.contents,ink.data(),ink.size());
            const auto p=convert(parameters);
            id<MTLCommandBuffer> command=[queue commandBuffer];require(command!=nil,"Metal shadow command");
            id<MTLComputeCommandEncoder> encoder=[command computeCommandEncoder];require(encoder!=nil,"Metal shadow encoder");
            [encoder setComputePipelineState:shadow_pipeline];[encoder setAccelerationStructure:acceleration atBufferIndex:0];
            [encoder setBuffer:output offset:0 atIndex:1];[encoder setBytes:&p length:sizeof(p) atIndex:2];
            [encoder setBuffer:resident_records?resident:materials offset:resident_records?sizeof(cutout.vertices):0 atIndex:3];
            [encoder setBuffer:texels offset:0 atIndex:4];[encoder useResource:acceleration usage:MTLResourceUsageRead];
            [encoder dispatchThreads:MTLSizeMake(pixels,1,1) threadsPerThreadgroup:MTLSizeMake(std::min<NSUInteger>(64,shadow_pipeline.maxTotalThreadsPerThreadgroup),1,1)];
            [encoder endEncoding];finish(command);
            std::vector<unsigned> result(pixels);std::memcpy(result.data(),output.contents,pixels*4);return result;
        });
        std::cout<<(resident_records?"Metal resident-offset indexed records":"Metal CPU-uploaded indexed records")<<" shader masks passed; NOT SDL owner/lifetime acceptance\n";
    }
    native_reflection_fixture::run_near_cutouts([&](const auto& p,const auto& cutout,const auto& ink) {
        build_scene(cutout);
        std::memcpy(materials.contents,cutout.materials.data(),sizeof(cutout.materials));
        std::memcpy(palette.contents,cutout.palette.data(),sizeof(cutout.palette));
        std::memcpy(texels.contents,ink.data(),ink.size());
        return dispatch(p);
    });
    return 0;
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;} } }
