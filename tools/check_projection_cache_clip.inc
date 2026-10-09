// Bit-exact reference/candidate records, including the raw-camera cases that
// actually exercise projection reuse. Never compare stale unused payload slots.
static void check_projection_cache_clip(SDL_GPUDevice* device,bool radix16=false,bool interior=false) {
    using namespace starfox::render;
    auto* env=SDL_GetEnvironment();
    struct Restore {
        SDL_Environment* env;
        std::array<const char*,6> names{"STARFOX_TEST_CACHED_PROJECTION_CLIP","STARFOX_TEST_FULL_CLIP",
            "STARFOX_TEST_SMALL_CLIP","STARFOX_TEST_IDENTITY_CLIP","STARFOX_TEST_RADIX16_CLIP","STARFOX_TEST_INTERIOR_CLIP"};
        std::array<std::optional<std::string>,6> values;
        explicit Restore(SDL_Environment* e,bool radix16,bool interior):env(e) {
            if(interior)std::swap(names[0],names[5]);
            else if(radix16)std::swap(names[0],names[4]);
            for(unsigned i=0;i<names.size();++i) {
                if(const auto* v=SDL_GetEnvironmentVariable(env,names[i]))values[i]=v;
                require(SDL_UnsetEnvironmentVariable(env,names[i]));
            }
        }
        ~Restore(){for(unsigned i=0;i<names.size();++i) {
            if(values[i])SDL_SetEnvironmentVariable(env,names[i],values[i]->c_str(),true);
            else SDL_UnsetEnvironmentVariable(env,names[i]);
        }}
    } restore{env,radix16,interior};
    struct Owned {
        SDL_GPUDevice* device;
        std::array<SDL_GPUBuffer*,6> buffers{};
        SDL_GPUTransferBuffer *upload{},*download{};
        ~Owned(){SDL_WaitForGPUIdle(device);for(auto* b:buffers)if(b)SDL_ReleaseGPUBuffer(device,b);
            if(upload)SDL_ReleaseGPUTransferBuffer(device,upload);if(download)SDL_ReleaseGPUTransferBuffer(device,download);}
    } owned{device};
    constexpr unsigned packets=257;
    std::vector<ContinuousProjectedPoint> points,tails;
    std::vector<V> corners,polygons;
    std::vector<Uint32> visibility(packets,1);
    std::vector<std::array<float,4>> parameters(packets,{112,96,256,1});
    const auto raw=[](ContinuousProjectedPoint& p,unsigned c,double value) {
        const auto bits=std::bit_cast<std::uint64_t>(value);
        p.camera[c]=std::bit_cast<float>(std::uint32_t(bits));
        p.screen[c]=std::bit_cast<float>(std::uint32_t(bits>>32));
    };
    std::uint32_t seed=0x134857;
    const auto random=[&](){seed=seed*1664525U+1013904223U;return seed;};
    for(unsigned f=0;f<packets;++f) {
        unsigned count=3+f%30;
        if(f==8)count=3;if(f==9)count=1;if(f==10)count=2;
        if(f>=16 && f<=45)count=3+f-16; // Every accepted corner count, wholly interior.
        if(f>=46 && f<=61)count=3;
        polygons.push_back({int(corners.size()),int(count),int(f),f==9?5:f==10?2:int((f%2?8:0)|(f%7==0?1:0))});
        if(f==8)polygons.back()[3]=8; // Actually enter the cached exact/backface path.
        if(f>=16 && f<=45)polygons.back()[3]=8;
        if(f>=46 && f<=61)polygons.back()[3]=8;
        for(unsigned i=0;i<count;++i) {
            double x=int(random()%2001)-1000,y=int(random()%1601)-800,z=1.+double(random()%511)/32.;
            x+=std::ldexp(double(int(random()%2049)-1024),-36);
            y+=std::ldexp(double(int(random()%2049)-1024),-36);
            if(f%17==0 && i==0)z=-1.;if(f==1)z=-2.;
            if(f==3 && i==0){x=y=0;z=-std::ldexp(1.,-900);}
            if(f==8){x=i==0?-1:i==1?0:1;y=i==1?1:-1;z=1;parameters[f]={52,52,8,1};}
            if(f>=16 && f<=45) {
                // Clockwise, sometimes concave; a tiny raw camera residual
                // still survives at raster eighth-pixel boundaries. Interior
                // output cannot assume a triangle, convexity or constant UV.
                const double angle=-2.*std::acos(-1.)*double(i)/count;
                const double radius=(f%2 && i%2)?1.:3.;
                x=radius*std::cos(angle);y=radius*std::sin(angle);z=1.;
                x+=std::ldexp(double(int(i%3)-1),-44);
                parameters[f]={52,52,8,1};
            }
            if(f>=46 && f<=61) {
                const unsigned fixture=f-46,axis=fixture/8;
                const double upper=axis==0?224.:192.;
                const double boundary=fixture%8<4?0.:upper;
                const double tail=std::ldexp(1.,-42);
                const double offset=fixture%4==0?-tail:fixture%4==1?0.:fixture%4==2?tail:-0.;
                // Exact lower/upper boundaries and both low-part sides, in
                // source order. Narrowing alone cannot decide eligibility.
                x=i==0?boundary+offset:(boundary==0?24.:upper-24.);
                y=i==0?48.:i==1?72.:24.;z=1.;parameters[f]={0,0,1,1};
                // Swapping axes reverses winding; keep these packets front.
                if(axis==1) {
                    y=i==0?boundary+offset:(boundary==0?24.:upper-24.);
                    x=i==0?48.:i==1?24.:72.;
                }
            }
            if(f==9)z=64;
            const unsigned index=unsigned(points.size());
            ContinuousProjectedPoint p{},t{};
            p.camera[0]=float(x);p.camera[1]=float(y);p.camera[2]=float(z);p.camera[3]=1;
            // A valid producer screen. Raw tails below carry the authoritative
            // double camera, including a negative depth narrowed to float -0.
            p.screen[0]=112;p.screen[1]=96;p.screen[2]=float(z);p.screen[3]=1;
            t.camera[3]=3;raw(t,0,x);raw(t,1,y);raw(t,2,z);
            if(f==2 && i==0)raw(t,0,std::numeric_limits<double>::quiet_NaN());
            if((f==4 && i==1)||(f==5 && i==0)){t={};t.camera[3]=1;t.screen[0]=112;t.screen[1]=96;}
            if(f==11 && i==0)t.camera[3]=-1;
            points.push_back(p);tails.push_back(t);
            corners.push_back({int(index),f==8?int(i*7):int(std::int16_t(random())),f==8?int(i*11):int(std::int16_t(random())),0});
        }
        if(f%13==0)visibility[f]=0;
        if(f>=16 && f<=61)visibility[f]=1;
        if(f==6)parameters[f][2]=std::numeric_limits<float>::quiet_NaN();
        if(f==7)polygons[f][1]=33;
        if(f==12)polygons[f][0]=-1;
    }
    const std::array<Uint32,6> bytes{Uint32(points.size()*sizeof(points[0])),Uint32(corners.size()*sizeof(corners[0])),
        Uint32(polygons.size()*sizeof(polygons[0])),packets*4,packets*16,Uint32(tails.size()*sizeof(tails[0]))};
    Uint32 total=0;
    for(unsigned i=0;i<bytes.size();++i){SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,bytes[i],0};
        owned.buffers[i]=SDL_CreateGPUBuffer(device,&info);require(owned.buffers[i]);total+=bytes[i];}
    SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,total,0};
    owned.upload=SDL_CreateGPUTransferBuffer(device,&info);require(owned.upload);
    info={SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,packets*129*16,0};
    owned.download=SDL_CreateGPUTransferBuffer(device,&info);require(owned.download);
    auto* data=static_cast<std::uint8_t*>(SDL_MapGPUTransferBuffer(device,owned.upload,false));require(data);
    const void* inputs[]{points.data(),corners.data(),polygons.data(),visibility.data(),parameters.data(),tails.data()};
    Uint32 offset=0;for(unsigned i=0;i<bytes.size();++i){std::memcpy(data+offset,inputs[i],bytes[i]);offset+=bytes[i];}
    SDL_UnmapGPUTransferBuffer(device,owned.upload);
    auto* command=SDL_AcquireGPUCommandBuffer(device);require(command);
    auto* copy=SDL_BeginGPUCopyPass(command);require(copy);offset=0;
    for(unsigned i=0;i<bytes.size();++i){SDL_GPUTransferBufferLocation from{owned.upload,offset};
        SDL_GPUBufferRegion to{owned.buffers[i],0,bytes[i]};SDL_UploadToGPUBuffer(copy,&from,&to,false);offset+=bytes[i];}
    SDL_EndGPUCopyPass(copy);require(SDL_SubmitGPUCommandBuffer(command));
    GpuClip clips[2];std::size_t headers=0,values=0,independent=0,interior_records=0;
    for(const auto dimensions:{std::array<int,2>{224,192},{398,192},{796,448},{157,109}})
        for(unsigned count:{2U,packets,33U,129U,17U,packets}) {
        const NativeClipSettings settings{count,Uint32(points.size()),Uint32(corners.size()),packets,dimensions[0],dimensions[1]};
        std::vector<std::vector<V>> reference;
        // Reference, candidate, and explicit reference override on the reused
        // candidate owner. Same immutable GPU input buffers in every case.
        for(unsigned route=0;route<3;++route) {
            if(route==0)require(SDL_UnsetEnvironmentVariable(env,restore.names[0]));
            else require(SDL_SetEnvironmentVariable(env,restore.names[0],"1",true));
            if(route==2)require(SDL_SetEnvironmentVariable(env,restore.names[1],"1",true));
            else require(SDL_UnsetEnvironmentVariable(env,restore.names[1]));
            auto& clip=clips[route==0?0:1];
            if(route==1 && count==129) {
                auto* cancelled=SDL_AcquireGPUCommandBuffer(device);require(cancelled);
                require(clip.enqueue(device,cancelled,owned.buffers[0],owned.buffers[1],owned.buffers[2],owned.buffers[3],settings,true,
                    owned.buffers[4],count,owned.buffers[5],Uint32(tails.size())));
                require(SDL_CancelGPUCommandBuffer(cancelled));
            }
            command=SDL_AcquireGPUCommandBuffer(device);require(command);
            auto* output=static_cast<SDL_GPUBuffer*>(clip.enqueue(device,command,owned.buffers[0],owned.buffers[1],owned.buffers[2],owned.buffers[3],settings,true,
                owned.buffers[4],count,owned.buffers[5],Uint32(tails.size())));
            if(!output){SDL_CancelGPUCommandBuffer(command);throw std::runtime_error(clip.status());}
            copy=SDL_BeginGPUCopyPass(command);require(copy);
            SDL_GPUBufferRegion from{output,0,count*129*16};SDL_GPUTransferBufferLocation to{owned.download,0};
            SDL_DownloadFromGPUBuffer(copy,&from,&to);SDL_EndGPUCopyPass(copy);
            auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);require(fence);
            require(SDL_WaitForGPUFences(device,true,&fence,1));SDL_ReleaseGPUFence(device,fence);
            const auto* words=static_cast<const V*>(SDL_MapGPUTransferBuffer(device,owned.download,false));require(words);
            for(unsigned f=0;f<count;++f) {
                const auto* record=words+f*129;const unsigned length=unsigned(record[0][0]);
                if(length>128)throw std::runtime_error("Invalid cached clip count");
                if(route==0)reference.emplace_back(record,record+length+1);
                else {
                    if(reference[f].size()!=length+1 || !std::equal(reference[f].begin(),reference[f].end(),record))
                        throw std::runtime_error("Projection-cache header/XYUV bits differ at packet "+std::to_string(f));
                    ++headers;values+=length*4;
                }
                if(f==8) {
                    if(record[0]!=V{3,0,0,0})throw std::runtime_error("Independent interior raw-camera triangle rejected");
                    const float xy[3][2]{{44,44},{52,60},{60,44}};
                    for(unsigned i=0;i<3;++i) {
                        const float expected[]{xy[i][0],xy[i][1],float(i*7),float(i*11)};
                        for(unsigned c=0;c<4;++c)if(record[i+1][c]!=std::bit_cast<std::int32_t>(expected[c]))
                            throw std::runtime_error("Independent exact raw-camera triangle differs");
                    }
                    ++independent;
                }
                if(f>=16 && f<=45) {
                    const auto source_size=polygons[f][1];
                    if(record[0]!=V{source_size,0,0,0})throw std::runtime_error("Independent interior polygon rejected");
                    for(int i=0;i<source_size;++i) {
                        const auto& corner=corners[polygons[f][0]+i];
                        const auto& raw_point=tails[corner[0]];
                        for(unsigned c=0;c<4;++c) {
                            float expected;
                            if(c<2) {
                                const auto bits=std::uint64_t(std::bit_cast<std::uint32_t>(raw_point.camera[c]))
                                    |(std::uint64_t(std::bit_cast<std::uint32_t>(raw_point.screen[c]))<<32);
                                const double exact=52.+8.*std::bit_cast<double>(bits);
                                expected=float(exact);
                                const float tail=float(exact-double(expected));
                                if(tail!=0 && expected!=0 && std::fmod(expected*8.f,1.f)==0)
                                    expected=std::nextafter(expected,tail>0?std::numeric_limits<float>::infinity():-std::numeric_limits<float>::infinity());
                            } else expected=float(corner[c-1]);
                            if(record[i+1][c]!=std::bit_cast<std::int32_t>(expected))
                                throw std::runtime_error("Independent interior XYUV bits differ at packet "+std::to_string(f));
                        }
                    }
                    ++interior_records;
                }
                if(f==3 && record[0][1]!=1)throw std::runtime_error("Negative raw depth rounded to -0 changed classification");
                if(f==4 && record[0][1]!=1)throw std::runtime_error("Mixed raw payload changed classification");
            }
            SDL_UnmapGPUTransferBuffer(device,owned.download);
        }
    }
    std::cout<<(interior?"Interior exact output: ":radix16?"Radix16 exact division: ":"Cached raw projection: ")<<headers<<" bit-exact headers and "<<values<<" consumed XYUV words; "
        <<independent<<" independent exact triangles, "<<interior_records<<" independent interior polygon records; "
        <<"mixed/tiny-negative/invalid/near/line/billboard/32-corner/reuse/cancellation/override passed\n";
}
