#include "../tests/fsr1_scalar_oracle.inc"
void native_fsr_tests(Gpu& gpu,SDL_GPUTextureFormat format) {
    using namespace render;std::array<GpuCalibratedFsr1,2> fsr;
    for(auto& eye:fsr)check(eye.initialize(gpu.device,format),eye.status().c_str());
    unsigned compared=0,protected_bytes=0,cases=0;
    for(unsigned iteration=0;iteration<8;++iteration){
        const Fsr1Extent output=iteration<4?Fsr1Extent{37,23}:Fsr1Extent{53,31};
        const auto input=fsr1_input_extent(output,static_cast<Fsr1Mode>(iteration%4+1));
        for(unsigned eye=0;eye<2;++eye){
            Texture scene(gpu,input.width,input.height,format),ink(gpu,output.width,output.height,format),
                owners(gpu,output.width,output.height,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM),dest(gpu,output.width,output.height,format);
            Bytes rgb(input.width*input.height*4),art(output.width*output.height*4),mask(art.size());
            for(unsigned y=0;y<input.height;++y)for(unsigned x=0;x<input.width;++x){unsigned at=(y*input.width+x)*4;
                rgb[at]=25+(x*13+y*7+eye*47)%180;rgb[at+1]=31+(x*3+y*11+eye*23)%170;
                rgb[at+2]=40+(x*7+y*3+eye*31)%160;rgb[at+3]=255;}
            for(unsigned i=0;i<art.size()/4;++i){unsigned at=i*4;art[at]=i*17;art[at+1]=i*31+eye*19;art[at+2]=i*7+33;
                art[at+3]=i%256;mask[at+2]=i%5;mask[at+3]=255;}
            std::array<std::vector<fsr_scalar::Rgb>,4> scalar;
            for(unsigned branch=0;branch<4;++branch)scalar[branch]=fsr_scalar::upscale(rgb,input.width,input.height,output.width,output.height,branch&1,branch&2,srgb(format));
            gpu.begin();scene.upload(rgb);ink.upload(art);owners.upload(mask);
            check(!fsr[eye].enqueue(gpu.command,scene.texture,input,ink.texture,owners.texture,scene.texture,output),"Native FSR accepted aliased targets");
            check(!fsr[eye].enqueue(gpu.command,scene.texture,{0,input.height},ink.texture,owners.texture,dest.texture,output),"Native FSR accepted zero input");
            check(fsr[eye].enqueue(gpu.command,scene.texture,input,ink.texture,owners.texture,dest.texture,output),fsr[eye].status().c_str());
            // Cancelled dispatch/texture cycling must not poison the retry.
            if(iteration==3){SDL_CancelGPUCommandBuffer(gpu.command);gpu.command=nullptr;gpu.begin();scene.upload(rgb);ink.upload(art);owners.upload(mask);
                check(fsr[eye].enqueue(gpu.command,scene.texture,input,ink.texture,owners.texture,dest.texture,output),fsr[eye].status().c_str());}
            gpu.submit();auto actual=dest.read();
            unsigned failures=0;
            for(unsigned i=0;i<actual.size()/4;++i){unsigned at=i*4;bool world=mask[at+2]==1 || mask[at+2]==2;
                const int color_tolerance=srgb(format)?2:1;
                // sRGB has API decode, shader re-encode, half storage, then
                // API encode/8-bit rounding; allow their two authored steps.
                // At a stencil boundary all RGB must match the SAME scalar
                // branch. Protected RGB and every alpha remain exact.
                bool world_match=false;std::array<int,3> expected_rgb{};
                if(world)for(const auto& branch:scalar){bool branch_match=true;
                    for(unsigned k=0;k<3;++k){float value=branch[i][k];if(srgb(format))value=float(encode(value));
                        expected_rgb[k]=int(std::lround(value*255));branch_match&=std::abs(int(actual[at+k])-expected_rgb[k])<=color_tolerance;}
                    world_match|=branch_match;}
                for(unsigned k=0;k<4;++k){int expected=art[at+k];bool match=false;
                    if(world && k<3){
                        expected=expected_rgb[k];match=world_match;
                        ++compared;
                    }else ++protected_bytes;
                    if(!(world && k<3))match=actual[at+k]==expected;
                    if(!match){
                        if(failures++<12)std::cerr<<"FSR mode="<<iteration%4+1<<" format="<<unsigned(format)<<" eye="<<eye<<" pixel="<<i<<" channel="<<k
                            <<" actual="<<unsigned(actual[at+k])<<" expected="<<expected<<'\n';}
                }
            }check(!failures,"Native FSR differs from independent scalar EASU/RCAS/ink oracle");++cases;
        }
    }
    std::cout<<"Native FSR format "<<unsigned(format)<<": "<<cases<<" two-eye/mode/resize cases; "<<compared
        <<" independent EASU/RCAS channels ("<<(srgb(format)?2:1)<<"-step RGB bound, common integer-stencil branch); "
        <<protected_bytes<<" exact protected/opacity bytes; cancel/alias/extent guards passed"<<std::endl;
}
