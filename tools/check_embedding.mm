#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include "embedding_programs.hpp"
#include <cstdio>
#include <cstring>

static void report_error(const char* stage, const char* name, NSError* error) {
    if (!error) {
        std::fprintf(stderr, "%s %s: Metal returned nil without NSError\n", stage, name);
        return;
    }
    std::fprintf(stderr, "%s %s: domain=%s code=%ld description=%s\n", stage, name,
        [error.domain UTF8String], (long)error.code, [error.description UTF8String]);
}

int main() {
    @autoreleasepool {
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        if (!device) {
            std::fprintf(stderr, "NO_METAL_DEVICE: native object checks remain separate from runtime acceptance\n");
            return 77;
        }
        const MTLSize maximum = device.maxThreadsPerThreadgroup;
        const NSOperatingSystemVersion os = NSProcessInfo.processInfo.operatingSystemVersion;
        // Generic device capabilities only: no unique registry ID or account data.
        std::printf("DEVICE name=%s mac1=%d mac2=%d apple1=%d apple2=%d common1=%d common2=%d max_threads=%lux%lux%lu os=%ld.%ld.%ld\n",
            [device.name UTF8String], [device supportsFamily:MTLGPUFamilyMac1],
            [device supportsFamily:MTLGPUFamilyMac2], [device supportsFamily:MTLGPUFamilyApple1],
            [device supportsFamily:MTLGPUFamilyApple2], [device supportsFamily:MTLGPUFamilyCommon1],
            [device supportsFamily:MTLGPUFamilyCommon2], (unsigned long)maximum.width,
            (unsigned long)maximum.height, (unsigned long)maximum.depth,
            (long)os.majorVersion, (long)os.minorVersion, (long)os.patchVersion);
        std::fflush(stdout);
        unsigned count = 0, attempted = 0, libraries = 0, kernels = 0;
        for (const auto& program : embedding_programs) {
            @autoreleasepool {
                ++attempted;
                std::printf("PIPELINE_BEGIN %s / %s threads=%ux%ux%u\n",
                    program.name, program.entry, program.x, program.y, program.z);
                std::fflush(stdout);
                // Static, read-only executable storage remains alive for the entire
                // library load. The empty destructor never frees its static bytes.
                dispatch_data_t data = dispatch_data_create(program.bytes, program.size,
                    dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{});
                NSError* error = nil;
                id<MTLLibrary> library = [device newLibraryWithData:data error:&error];
                if (!library) {
                    report_error("LIBRARY_FAILED", program.name, error);
                    continue;
                }
                ++libraries;
                id<MTLFunction> function = [library newFunctionWithName:
                    [NSString stringWithUTF8String:program.entry]];
                if (!function || function.functionType != MTLFunctionTypeKernel) {
                    std::fprintf(stderr, "KERNEL_FAILED %s / %s\n", program.name, program.entry);
                    continue;
                }
                ++kernels;
                // Match SDL_gpu_metal.m's production pipeline creation. Do not
                // reduce the declared workgroup or change shader code/precision.
                MTLComputePipelineDescriptor* descriptor = [MTLComputePipelineDescriptor new];
                descriptor.computeFunction = function;
                error = nil;
                id<MTLComputePipelineState> pipeline = [device
                    newComputePipelineStateWithDescriptor:descriptor options:MTLPipelineOptionNone
                    reflection:nil error:&error];
                if (!pipeline || error) {
                    report_error("PIPELINE_FAILED", program.name, error);
                    continue;
                }
                const unsigned threads = program.x * program.y * program.z;
                if (pipeline.maxTotalThreadsPerThreadgroup < threads) {
                    std::fprintf(stderr, "THREADGROUP_UNSUPPORTED %s: required=%u maximum=%lu\n",
                        program.name, threads, (unsigned long)pipeline.maxTotalThreadsPerThreadgroup);
                    continue;
                }
                std::printf("PIPELINE_PASS %s / %s threads=%u\n", program.name, program.entry, threads);
                std::fflush(stdout);
                ++count;
            }
        }
        std::printf("PIPELINE_SUMMARY attempted=%u libraries=%u kernels=%u passed=%u failed=%u\n",
            attempted, libraries, kernels, count, attempted-count);
        if (attempted != 25 || libraries != 25 || kernels != 25 || count != 25) return 1;
        std::printf("ALL25_NATIVE_METAL_PIPELINES_PASS; not dispatch/numerical/frame-cost acceptance\n");
        return 0;
    }
}
