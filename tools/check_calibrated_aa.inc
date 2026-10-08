// Native graphics AA against independent integer equations and the separate
// upstream compute SMAA implementation. Upload/readback exists only here.
void aa_tests(Gpu& gpu,SDL_GPUTextureFormat format) {
    unsigned long long compared=0;unsigned changed=0;
    render::GpuCalibratedAa aa;check(aa.initialize(gpu.device,format),aa.status().c_str());
    render::GpuSmaa reference_smaa;
    for(const auto extent:{std::array{1U,1U},std::array{17U,13U},std::array{65U,37U},std::array{127U,65U}}) {
        const auto [w,h]=extent;
        Texture source(gpu,w,h,format),mask(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM),target(gpu,w,h,format);
        Texture authored(gpu,w,h,SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);
        Bytes colors(w*h*4),coverage(w*h*4),packed(w*h*4);
        for(unsigned frame=0;frame<3;++frame) {
            for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
                const unsigned at=(y*w+x)*4;const bool eligible=(x%11!=2 && y%13!=3);
                coverage[at+2]=eligible?2:0;coverage[at+3]=eligible?255:0;
                word(packed,at,(eligible?0U:1U)<<8);
                for(unsigned c=0;c<3;++c) colors[at+c]=frame==1?static_cast<unsigned char>(69+c*41)
                    :static_cast<unsigned char>(((x+frame*2)*3+y*5+c*7)%17<8?35+c*19:221-c*17);
                colors[at+3]=static_cast<unsigned char>(41+(x*17+y*11)%215);
                if(!eligible) {colors[at]=255;colors[at+1]=17;colors[at+2]=0;}
            }
            for(unsigned type:{0U,1U,2U,5U}) for(unsigned quality=0;quality<=3;++quality) {
                gpu.begin();source.upload(colors);mask.upload(coverage);authored.upload(colors);
                check(aa.enqueue(gpu.command,source.texture,mask.texture,target.texture,w,h,type,quality),aa.status().c_str());
                gpu.submit();auto actual=target.read();auto expected=colors;
                if(type!=5 && quality) for(unsigned y=1;y+1<h;++y) for(unsigned x=1;x+1<w;++x) {
                    const unsigned at=(y*w+x)*4;if(coverage[at+3]!=255) continue;
                    std::array<unsigned,5> locations{at,at-4,at+4,at-w*4,at+w*4};
                    std::array<int,5> light{};
                    for(unsigned tap=0;tap<5;++tap) {
                        auto& loc=locations[tap];if(coverage[loc+3]!=255) loc=at;
                        light[tap]=(int(colors[loc])*77+int(colors[loc+1])*150+int(colors[loc+2])*29)/256;
                    }
                    const auto [low,high]=std::minmax_element(light.begin(),light.end());
                    const int floor=quality==1?20:quality==2?12:6,divisor=quality==1?6:quality==2?8:12;
                    if(*high-*low<std::max(floor,*high/divisor)) continue;
                    const bool vertical=type==1?std::abs(light[3]+light[4]-2*light[0])<std::abs(light[1]+light[2]-2*light[0])
                        :std::abs(light[1]-light[2])>=std::abs(light[3]-light[4]);
                    const unsigned weight=quality==1?6:quality==2?2:1;
                    for(unsigned c=0;c<3;++c) {
                        unsigned value=colors[at+c]*weight;
                        if(type==2) for(unsigned tap=1;tap<5;++tap) value+=colors[locations[tap]+c];
                        else value+=colors[locations[vertical?3:1]+c]+colors[locations[vertical?4:2]+c];
                        expected[at+c]=static_cast<unsigned char>(value/(weight+(type==2?4:2)));
                    }
                }
                if(type==5 && quality) {
                    gpu.begin();Buffer packed_buffer(gpu,packed);
                    auto* resolved=static_cast<SDL_GPUTexture*>(reference_smaa.enqueue(gpu.device,gpu.command,authored.texture,packed_buffer.buffer,w,h,quality));
                    check(resolved,reference_smaa.status().c_str());
                    gpu.submit();
                    expected=authored.read(resolved); // Borrow, never release the oracle's image.
                }
                for(unsigned at=0;at<actual.size();++at) {
                    const auto pixel=at/4,x=pixel%w,y=pixel/w;
                    if(at%4==3 || coverage[pixel*4+3]==0 || quality==0 || frame==1) {
                        check(actual[at]==colors[at],"Native AA changed protected ink/alpha/OFF/flat input");++compared;
                    } else if(type!=5 || (x>3 && x+4<w && y>3 && y+4<h && x%11>3 && x%11<10 && y%13>4 && y%13<12)) {
                        check(std::abs(int(actual[at])-int(expected[at]))<=1,"Native AA differs from integer/upstream compute reference");++compared;
                    }
                }
                if(quality && actual!=colors) ++changed;
                // Held output and cancelled work cannot introduce jitter or
                // expose stale SMAA intermediates after the next complete pass.
                if(type==5 && quality==3) {
                    gpu.begin();check(aa.enqueue(gpu.command,source.texture,mask.texture,target.texture,w,h,type,quality),aa.status().c_str());
                    SDL_CancelGPUCommandBuffer(gpu.command);gpu.command=nullptr;
                    gpu.begin();check(aa.enqueue(gpu.command,source.texture,mask.texture,target.texture,w,h,type,quality),aa.status().c_str());
                    gpu.submit();
                    check(target.read()==actual,"Cancelled/held native SMAA changed the complete image");
                }
            }
        }
        gpu.begin();
        check(!aa.enqueue(gpu.command,source.texture,mask.texture,target.texture,w,h,4,2),"Native AA silently substituted for TAA");
        check(!aa.enqueue(gpu.command,source.texture,mask.texture,source.texture,w,h,0,1),"Native AA accepted aliased output");
        check(!aa.enqueue(gpu.command,source.texture,mask.texture,target.texture,w,h,0,4),"Native AA accepted malformed quality");
        SDL_CancelGPUCommandBuffer(gpu.command);gpu.command=nullptr;
    }
    check(changed>0,"Native AA produced no visible smoothing");
    std::cout<<"Native spatial AA format "<<unsigned(format)<<": "<<compared<<" integer/upstream/protected samples; "<<changed
        <<" changed cases; FXAA/Sharp/Soft/SMAA qualities, OFF, flat/alpha/UI, resize, held/cancelled work passed.\n";
}
