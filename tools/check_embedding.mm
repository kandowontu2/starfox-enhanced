#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include "embedding_programs.hpp"
#include <cstdio>
#include <cstring>

int main() {
    @autoreleasepool {
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        if (!device) {
            std::fprintf(stderr, "NO_METAL_DEVICE: native object checks remain separate from runtime acceptance\n");
            return 77;
        }
        unsigned count = 0;
        for (const auto& program : embedding_programs) {
            @autoreleasepool {
                // Static, read-only executable storage remains alive for the entire
                // library load. The empty destructor never frees its static bytes.
                dispatch_data_t data = dispatch_data_create(program.bytes, program.size,
                    dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{});
                NSError* error = nil;
                id<MTLLibrary> library = [device newLibraryWithData:data error:&error];
                if (!library) {
                    std::fprintf(stderr, "LIBRARY_FAILED %s: %s\n", program.name,
                        [[error description] UTF8String]);
                    return 1;
                }
                id<MTLFunction> function = [library newFunctionWithName:
                    [NSString stringWithUTF8String:program.entry]];
                if (!function || function.functionType != MTLFunctionTypeKernel) {
                    std::fprintf(stderr, "KERNEL_FAILED %s / %s\n", program.name, program.entry);
                    return 1;
                }
                id<MTLComputePipelineState> pipeline = [device
                    newComputePipelineStateWithFunction:function error:&error];
                if (!pipeline) {
                    std::fprintf(stderr, "PIPELINE_FAILED %s: %s\n", program.name,
                        [[error description] UTF8String]);
                    return 1;
                }
                const unsigned threads = program.x * program.y * program.z;
                if (pipeline.maxTotalThreadsPerThreadgroup < threads) {
                    std::fprintf(stderr, "THREADGROUP_UNSUPPORTED %s: required=%u maximum=%lu\n",
                        program.name, threads, (unsigned long)pipeline.maxTotalThreadsPerThreadgroup);
                    return 1;
                }
                std::printf("PIPELINE_PASS %s / %s threads=%u\n", program.name, program.entry, threads);
                std::fflush(stdout);
                ++count;
            }
        }
        if (count != 25) return 1;
        std::printf("ALL25_NATIVE_METAL_PIPELINES_PASS; not dispatch/numerical/frame-cost acceptance\n");
        return 0;
    }
}
