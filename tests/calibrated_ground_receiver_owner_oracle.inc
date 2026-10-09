// Actual game-owner/hardware-ray regression with a mocked display compositor.
// Source geometry, independent foreground labels and live scenery are used;
// no uploaded per-eye replacement image or private renderer guide is read.
template<class Submit>
std::shared_ptr<const CalibratedGameFrame> native_ground_owner_oracle(Submit submit,bool srgb,bool rays,const char* backend) {
    check(rays,"Native ground receiver check requires the hardware ray backend");
    using Images=std::array<std::vector<unsigned char>,2>;
    const bool bgra=mock.backend==DisplayXrBackend::vulkan
        && (gpu_mock.format==VK_FORMAT_B8G8R8A8_SRGB || gpu_mock.format==VK_FORMAT_B8G8R8A8_UNORM);
    const auto render=[&](std::shared_ptr<const CalibratedGameFrame> frame) {
        Images image=submit(std::move(frame));
        if(bgra) for(auto& eye:image) for(unsigned at=0;at<eye.size();at+=4) std::swap(eye[at],eye[at+2]);
        return image;
    };
    native_mock.head_x=5;native_mock.sample_x=native_mock.sample_y=0;
    auto base=std::make_shared<CalibratedGameFrame>();base->settings.srgb=srgb;base->clear={.07F,.23F,.41F,1};
    auto snapshot=std::make_shared<starfox::vr::GameSceneSnapshot>();snapshot->flow=starfox::simulation::GameFlowState::gameplay;
    base->previous=base->current=snapshot;
    starfox::simulation::SnesPpuState ppu;ppu.main_screen=2;ppu.background_mode=2;
    ppu.bg2_character_base=0;ppu.bg2_screen_size=2;ppu.bg2_screen_base=0x1000;ppu.cgram[1]=5|(13<<5)|(27<<10);
    for(unsigned y=0;y<8;++y) ppu.vram[32+y*2]=255;
    for(unsigned tile=0;tile<2048;++tile) ppu.vram[0x2000+tile*2]=1;
    auto floor=starfox::vr::landscape_sphere_packet(ppu,{},112,srgb,false,232);
    starfox::vr::place_landscape_ground(floor,-.75F,true);
    // Constant authored blue sky and a separate ground ramp. The ray cube must
    // capture that sky, not the ground's pre-reflection brown base colour.
    base->draws.push_back({floor,CalibratedGameLayer::world,starfox::vr::SceneBlend::opaque,false});
    const auto quad=[&](float x,float y,float z,float r,bool ink) {
        starfox::vr::DrawPacket packet;packet.preserve_native_colour=ink;
        constexpr int corners[6][2]{{-1,-1},{1,-1},{1,1},{-1,-1},{1,1},{-1,1}};
        for(auto p:corners) {
            starfox::vr::SceneVertex v{};v.position[0]=x+p[0]*r;v.position[1]=y+p[1]*r;v.position[2]=z;
            v.color[0]=ink?1:.81F;v.color[1]=ink?1:.11F;v.color[2]=ink?0:.06F;v.color[3]=1;
            std::copy(std::begin(v.color),std::end(v.color),v.odd_color);packet.geometry.vertices.push_back(v);
        }
        base->draws.push_back({packet,ink?CalibratedGameLayer::native_ui:CalibratedGameLayer::model,
            starfox::vr::SceneBlend::opaque,!ink,0,!ink,ink});
    };
    quad(0,-.30F,-2,.20F,false);quad(.10F,-.14F,-.7F,.035F,true);
    const auto selected=[&](unsigned material,unsigned quality) {
        auto source=std::make_shared<CalibratedGameFrame>(*base);source->settings.ray_tracing=3;source->settings.reflections=quality;
        source->settings.enhanced_ground=true;source->settings.ground_material=material;
        CalibratedGroundGradient surface{{.24F,.19F,.12F},{.12F,.09F,.055F},112,111};surface.material=material;
        surface.seconds=1.75F;surface.origin={123,-415};
        check(apply_calibrated_ground(source->draws[0].packet,surface),"Native metal owner payload failed");
        source->draws[0].reflection_environment=true;return source;
    };
    Images mirror{},gold{};std::shared_ptr<CalibratedGameFrame> retained;
    unsigned different=0,ink=0,ground_changed=0;
    const auto check_sky=[&](const Images& actual,unsigned material) {
        // Independent infinite-sky reflection/conductor reference. This left
        // corner cannot hit either finite model or the protected UI quad.
        const auto fov=fixture_fov();const double l=std::tan(fov.angleLeft),r=std::tan(fov.angleRight);
        const double u=std::tan(fov.angleUp),d=std::tan(fov.angleDown);
        const double sky_rgb[]{41./255,107./255,222./255},f0[]{1,.766,.336};
        const auto decode=[](double c){return c<=.04045?c/12.92:std::pow((c+.055)/1.055,2.4);};
        const auto encode=[](double c){return c<=.0031308?c*12.92:1.055*std::pow(c,1/2.4)-.055;};
        for(unsigned e=0;e<2;++e) for(unsigned y=mock.image_height*4/5;y+4<mock.image_height;++y)
            for(unsigned x=4;x<mock.image_width/5;++x) {
                const double dx=std::lerp(l,r,(x+.5)/mock.image_width),dy=-std::lerp(u,d,(y+.5)/mock.image_height);
                const double facing=dy/std::sqrt(dx*dx+dy*dy+1),grazing=std::pow(1-facing,5);
                const unsigned at=(y*mock.image_width+x)*4;
                for(unsigned c=0;c<3;++c) {
                    double value=srgb?decode(sky_rgb[c]):sky_rgb[c];
                    if(material==7) value*=f0[c]+(1-f0[c])*grazing;
                    if(srgb) value=encode(value);
                    const int expected=int(std::lround(std::clamp(value,0.,1.)*255));
                    if(std::abs(int(actual[e][at+c])-expected)>2)
                        std::cerr<<backend<<" material="<<material<<" srgb="<<srgb<<" eye="<<e
                            <<" pixel="<<x<<','<<y<<" channel="<<c<<" actual="<<unsigned(actual[e][at+c])
                            <<" expected="<<expected<<" rgb="<<unsigned(actual[e][at])<<','<<unsigned(actual[e][at+1])
                            <<','<<unsigned(actual[e][at+2])<<'\n';
                    check(std::abs(int(actual[e][at+c])-expected)<=2,"Native metal transport differs from independent linear-light sky/conductor reference");
                }
            }
    };
    for(unsigned material:{6U,7U}) {
        const auto source=selected(material,3);const auto actual=render(source);check_sky(actual,material);
        auto off=std::make_shared<CalibratedGameFrame>(*source);off->settings.ray_tracing=0;
        const auto baseline=render(off);
        check(actual[0]!=actual[1],"Native ground collapsed stereo geometry");
        for(unsigned e=0;e<2;++e) for(unsigned at=0;at<actual[e].size();at+=4) {
            check(actual[e][at+3]==baseline[e][at+3],"Native reflective floor made foreground transparent");
            if(baseline[e][at]==255 && baseline[e][at+1]==255 && baseline[e][at+2]==0) {
                check(std::equal(actual[e].begin()+at,actual[e].begin()+at+4,baseline[e].begin()+at),"Native reflective floor changed protected ink");++ink;
            }
            ground_changed+=actual[e][at+2]!=baseline[e][at+2];
        }
        const auto low=render(selected(material,1));check(low!=actual,"Ground reflection quality had no native effect");
        auto empty=std::make_shared<CalibratedGameFrame>(*source);
        empty->draws.erase(empty->draws.begin()+1); // No model/caster, still live sky, floor and protected UI.
        const auto empty_image=render(empty);check_sky(empty_image,material);
        check(empty_image!=actual,"Empty native world retained an old reflected model");
        auto empty_off=std::make_shared<CalibratedGameFrame>(*empty);empty_off->settings.ray_tracing=0;
        const auto empty_ink=render(empty_off);
        for(unsigned e=0;e<2;++e) for(unsigned at=0;at<empty_ink[e].size();at+=4) {
            check(empty_image[e][at+3]==empty_ink[e][at+3],"Empty native receiver changed alpha");
            if(empty_ink[e][at]==255 && empty_ink[e][at+1]==255 && empty_ink[e][at+2]==0)
                check(std::equal(empty_image[e].begin()+at,empty_image[e].begin()+at+4,empty_ink[e].begin()+at),
                    "Empty native receiver changed protected ink");
        }
        empty->settings.aa_type=4;empty->settings.aa_quality=2;
        const auto empty_taa=render(empty);check_sky(empty_taa,material);
        check(render(empty)==empty_taa,"Empty native metal TAA resampled during retained retry");
        check(render(source)==actual,"Empty/nonempty native transition kept stale ray geometry");
        auto mirrored=std::make_shared<CalibratedGameFrame>(*source);mirrored->settings.material=unsigned(Effect::mirror);
        const auto bounced=render(mirrored);unsigned secondary_changed=0;
        for(unsigned e=0;e<2;++e) for(unsigned y=mock.image_height*9/10;y<mock.image_height;++y)
            for(unsigned x=mock.image_width/3;x<mock.image_width/2;++x) {
                const unsigned at=(y*mock.image_width+x)*4;
                secondary_changed+=actual[e][at]!=bounced[e][at] || actual[e][at+2]!=bounced[e][at+2];
            }
        check(secondary_changed>5,"Mirrored ground returned a mirrored model's pre-material source colour");
        check(render(source)==actual,"Retained native ground clock/scenery moved during retry");
        auto sky=std::make_shared<CalibratedGameFrame>(*source);sky->clear={.41F,.13F,.055F,1};
        sky->draws[0].packet.geometry.texels[17]=0xff0e2169U; // Live source sky palette entry, not an eye image.
        const auto changed=render(sky);check(changed!=actual,"Ground reflected stale original scenery");
        check(render(source)==actual,"Native enhanced/mirror environment did not restore");
        auto bank=std::make_shared<CalibratedGameFrame>(*source);const float c=std::cos(.21F),s=std::sin(.21F);
        bank->draws[0].packet.model={c,s,0,0,-s,c,0,0,0,0,1,0,0,0,0,1};
        check(render(bank)!=actual,"Native reflective ground ignored retained bank");
        for(unsigned type:{3U,4U,6U}) for(unsigned quality:{1U,2U,3U}) {
            auto aa=std::make_shared<CalibratedGameFrame>(*source);aa->settings.aa_type=type;aa->settings.aa_quality=quality;
            const auto resolved=render(aa);
            std::cout<<backend<<" native metal AA material="<<material<<" type="<<type<<" quality="<<quality<<'\n';
            check_sky(resolved,material);
            for(unsigned e=0;e<2;++e) for(unsigned at=0;at<baseline[e].size();at+=4) {
                check(resolved[e][at+3]==baseline[e][at+3],"Native metal AA changed source alpha");
                if(baseline[e][at]==255 && baseline[e][at+1]==255 && baseline[e][at+2]==0)
                    check(std::equal(resolved[e].begin()+at,resolved[e].begin()+at+4,baseline[e].begin()+at),
                        "Native metal AA changed centre protected ink");
            }
            check(render(aa)==resolved,"Held native metal AA resampled scenery or source");
        }
        if(material==6) mirror=actual;else gold=actual;
        retained=source;
    }
    for(unsigned e=0;e<2;++e) for(unsigned at=0;at<mirror[e].size();at+=4)
        different+=mirror[e][at+2]!=gold[e][at+2];
    check(different>100 && ground_changed>100 && ink>10,"Native metal owner fixture missed distinct gold/floor/protected coverage");
    std::cout<<backend<<" native mirror/gold ground: real ray transport, per-eye coverage, bank, quality, live scenery, empty-world transitions, SSAA/TAA/MSAA, retained retry and opaque protected ink passed ("
        <<different<<" distinct conductor pixels). Mocked compositor, not physical Leia acceptance.\n";
    {
        auto water=selected(5,3);
        auto empty=std::make_shared<CalibratedGameFrame>(*water);empty->draws.erase(empty->draws.begin()+1);
        // Exercise the actual retained game owner with RT disabled as well.
        // The scalar colour/footprint oracle lives in the scene component;
        // this checks settings, clock, composition and protected ownership.
        auto ripples=std::make_shared<CalibratedGameFrame>(*water);ripples->settings.ray_tracing=0;
        const auto ripple_image=render(ripples);
        auto ripple_clock=std::make_shared<CalibratedGameFrame>(*ripples);
        ripple_clock->draws[0].packet.geometry.texels[calibrated_ground_offset+11]=std::bit_cast<std::uint32_t>(2.65F);
        const auto later_ripples=render(ripple_clock);
        check(ripple_image!=later_ripples,"RT-OFF retained owner froze native water wave shading");
        unsigned fixed_ink=0;
        for(unsigned e=0;e<2;++e) for(unsigned at=0;at<ripple_image[e].size();at+=4) {
            check(ripple_image[e][at+3]==later_ripples[e][at+3],"RT-OFF water changed source opacity");
            if(ripple_image[e][at]==255 && ripple_image[e][at+1]==255 && ripple_image[e][at+2]==0) {
                check(std::equal(ripple_image[e].begin()+at,ripple_image[e].begin()+at+4,later_ripples[e].begin()+at),
                    "RT-OFF water waves painted protected native ink");++fixed_ink;
            }
        }
        check(fixed_ink>10,"RT-OFF water owner lacked protected foreground coverage");
        check(render(ripples)==ripple_image,"RT-OFF water changed clock during retained owner retry");
        std::cout<<backend<<" native RT-OFF water: GPU wave shading, retained clock/retry and protected opacity passed.\n";
        // Independent double-precision Snell/Beer/Fresnel oracle on an empty
        // world: source palette values and actual fixture pose/FOV only. Do not
        // read private receiver/depth guides or call the production water shader.
        const auto water_reference=[&](const Images& pixels,unsigned quality,unsigned aa_type=0,unsigned aa_quality=0,
            std::array<double,3> scale={1,1,1}) {
            const auto fov=fixture_fov();
            const double left=std::tan(fov.angleLeft),right=std::tan(fov.angleRight);
            const double top=std::tan(fov.angleUp),bottom=std::tan(fov.angleDown);
            const double focal=mock.image_width/(right-left);
            const auto norm=[](std::array<double,3> v) {
                const double len=std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);for(auto& c:v)c/=len;return v;
            };
            const auto dot=[](auto a,auto b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];};
            const auto decode=[](double c){return c<=.04045?c/12.92:std::pow((c+.055)/1.055,2.4);};
            const auto encode=[](double c){return c<=.0031308?c*12.92:1.055*std::pow(c,1/2.4)-.055;};
            const auto light=norm({-scale[0],-scale[1],-scale[2]});
            double authored[]{.18,.14,.0875},sky[]{41./255,107./255,222./255};
            if(srgb) for(unsigned c=0;c<3;++c) {authored[c]=decode(authored[c]);sky[c]=decode(sky[c]);}
            const double luminance=authored[0]*.3+authored[1]*.59+authored[2]*.11;
            const double tint[]{.20,.58,.85},bed[]{.28,.24,.16},absorption[]{.0025,.0008,.00035},sun[]{1,.95,.82};
            struct Tap {double x,y,focal_scale,weight;};std::vector<Tap> taps;
            if(aa_type==3) {
                // Independent regular supersampling grid. The wave footprint
                // belongs to each high-resolution ray, not the final pixel.
                const unsigned factor=aa_quality+1;
                for(unsigned sy=0;sy<factor;++sy) for(unsigned sx=0;sx<factor;++sx)
                    taps.push_back({(sx+.5)/factor-.5,(sy+.5)/factor-.5,double(factor),1./(factor*factor)});
            } else if(aa_type==6) {
                // Standard hardware sample positions, in sixteenths around
                // the pixel centre; no production camera helper is called.
                constexpr int positions[14][2]{{4,4},{-4,-4},{-2,-6},{6,-2},{-6,2},{2,6},
                    {1,-3},{-1,3},{5,1},{-3,-5},{-5,5},{-7,-1},{3,7},{7,-7}};
                const unsigned count=1U<<aa_quality;
                for(unsigned s=0;s<count;++s) taps.push_back({positions[count-2+s][0]/16.,positions[count-2+s][1]/16.,1,1./count});
            } else if(aa_type==4) {
                // Changing AA options resets this fixture's temporal epoch to
                // the first quantized Halton sample: (0,-43/256). Reactive
                // water has no history contribution; presentation unjitters
                // those source samples with a linear-light bilinear resolve.
                constexpr double jitter_x=0,jitter_y=-43./256;
                const double bx=std::floor(jitter_x),by=std::floor(jitter_y);
                const double fx=jitter_x-bx,fy=jitter_y-by;
                for(unsigned ty=0;ty<2;++ty) for(unsigned tx=0;tx<2;++tx)
                    taps.push_back({bx+tx-jitter_x,by+ty-jitter_y,1,(tx?fx:1-fx)*(ty?fy:1-fy)});
            } else taps.push_back({0,0,1,1});
            unsigned compared=0;
            for(unsigned e=0;e<2;++e) for(unsigned y=mock.image_height*4/5;y+4<mock.image_height;++y)
                for(unsigned x=4;x<mock.image_width/5;++x) {
                    std::array<double,3> resolved{};
                    for(const auto tap:taps) {
                    const auto direction=norm({std::lerp(left,right,(x+.5+tap.x)/mock.image_width),
                        -std::lerp(top,bottom,(y+.5+tap.y)/mock.image_height),1.});
                    const double distance=(192*scale[1]+2)/direction[1],bias=std::max(.05,distance*1.e-5);
                    const double wx=123+(5+(e?8:-8)+direction[0]*distance)/scale[0],wz=-415+(-4+direction[2]*distance)/scale[2];
                    const double footprint=distance/(focal*tap.focal_scale)/std::max(direction[1],.04);
                    const auto band=[&](double frequency){return 1/(1+std::pow(footprint*frequency,4));};
                    const double dx=.055*std::cos(wx*.018+wz*.011-1.75*.8)*band(.022)
                        +.025*std::cos(wx*.047-wz*.025+1.75*1.2)*band(.054);
                    const double dz=.045*std::cos(wz*.022-wx*.009-1.75*.65)*band(.024)
                        -.020*std::cos(wx*.047-wz*.025+1.75*1.2)*band(.054);
                    const auto normal=norm({dx/scale[0],-1./scale[1],dz/scale[2]});const double facing=dot(normal,direction);
                    const double k=1-.75*.75*(1-facing*facing);
                    const double transmitted_y=.75*direction[1]-(.75*facing+std::sqrt(k))*normal[1];
                    const double travel=(640*scale[1]+normal[1]*bias)/transmitted_y;
                    const double diffuse=.65+.35*std::max(0.,dot(normal,light));
                    const double fresnel=(.02+.98*std::pow(1-std::clamp(-facing,0.,1.),5))*quality/3;
                    const auto halfway=norm({light[0]-direction[0],light[1]-direction[1],light[2]-direction[2]});
                    const double specular=std::pow(std::max(0.,dot(normal,halfway)),96)
                        *std::max({authored[0],authored[1],authored[2]})*.55;
                    for(unsigned c=0;c<3;++c) {
                        const double transmission=std::exp(-travel*absorption[c]);
                        double value=(bed[c]*transmission+luminance*tint[c]*diffuse*(1-transmission))*(1-fresnel)
                            +sky[c]*fresnel+sun[c]*specular;
                        value=std::clamp(value,0.,1.);if(srgb)value=encode(value);
                        // Ray output is eight-bit encoded storage. Texture
                        // loads decode sRGB before the AA resolve; averaging
                        // encoded bytes instead would hide a gamma regression.
                        value=std::lround(value*255)/255.;if(srgb)value=decode(value);
                        resolved[c]+=value*tap.weight;
                    }
                    }
                    const unsigned at=(y*mock.image_width+x)*4;
                    for(unsigned c=0;c<3;++c) {
                        double value=std::clamp(resolved[c],0.,1.);if(srgb)value=encode(value);
                        const int expected=int(std::lround(value*255));
                        if(std::abs(int(pixels[e][at+c])-expected)>2)
                            std::cerr<<backend<<" native water srgb="<<srgb<<" quality="<<quality<<" AA="<<aa_type<<'/'<<aa_quality<<" eye="<<e
                                <<" pixel="<<x<<','<<y<<" channel="<<c<<" actual="<<unsigned(pixels[e][at+c])<<" expected="<<expected<<'\n';
                        check(std::abs(int(pixels[e][at+c])-expected)<=2,"Native water differs from independent Snell/Beer/Fresnel reference");++compared;
                    }
                }
            check(compared>1000,"Native water reference missed finite liquid coverage");
        };
        std::cout<<backend<<" native water empty receiver"<<std::endl;
        const auto quiet=render(empty);water_reference(quiet,3);
        std::cout<<backend<<" native water empty reference passed"<<std::endl;
        check(!native_ground_cleanup_failure,"Injected native ground owner check failure");
        auto no_reflection=std::make_shared<CalibratedGameFrame>(*empty);no_reflection->settings.reflections=0;
        const auto transmitted=render(no_reflection);water_reference(transmitted,0);
        std::cout<<backend<<" native water reflection OFF reference passed"<<std::endl;
        check(transmitted!=quiet,"Water transmission/reflection OFF used the same transport");
        auto caustics=std::make_shared<CalibratedGameFrame>(*empty);caustics->settings.water_caustics=3;
        check(render(caustics)!=quiet,"Native water caustics did not reach the transmitted bed");
        auto clock=std::make_shared<CalibratedGameFrame>(*empty);
        clock->draws[0].packet.geometry.texels[calibrated_ground_offset+11]=std::bit_cast<std::uint32_t>(2.65F);
        check(render(clock)!=quiet,"Native water was static despite a retained wave-clock update");
        check(render(empty)==quiet,"Native water clock advanced during an in-flight retry");
        auto palette=std::make_shared<CalibratedGameFrame>(*empty);
        for(unsigned c=0;c<6;++c) palette->draws[0].packet.geometry.texels[calibrated_ground_offset+c]=std::bit_cast<std::uint32_t>(.03F);
        check(render(palette)!=quiet,"Native water ignored the live source palette");
        auto scaled=std::make_shared<CalibratedGameFrame>(*empty);
        scaled->draws[0].packet.model={.9F,0,0,0,0,1.2F,0,0,0,0,1.1F,0,0,0,0,1};
        const auto scaled_image=render(scaled);water_reference(scaled_image,3,0,0,{.9,1.2,1.1});
        check(render(scaled)==scaled_image,"Native affine water resampled a retained source transform");
        auto bank=std::make_shared<CalibratedGameFrame>(*empty);
        const float cosine=std::cos(.21F),sine=std::sin(.21F);
        bank->draws[0].packet.model={cosine,sine,0,0,-sine,cosine,0,0,0,0,1,0,0,0,0,1};
        const auto banked=render(bank);check(banked!=quiet,"Native water ignored its retained bank");
        check(render(bank)==banked,"Native banked water changed during a held retry");
        auto response=std::make_shared<CalibratedGameFrame>(*empty);response->settings.camera_response_pose={.009,-.012,.016};
        check(render(response)!=quiet,"Native water ignored the retained camera-response transform");
        check(render(empty)==quiet,"Native water bank/response did not restore the accepted plane");
        // Start style-only MSAA after a size-changing SSAA frame. There is no
        // AO/DOF/blur source-surface attachment: both merged float targets must
        // still survive every sample swap (including the first allocation).
        auto scale_reset=std::make_shared<CalibratedGameFrame>(*empty);
        scale_reset->settings.aa_type=3;scale_reset->settings.aa_quality=1;(void)render(scale_reset);
        for(unsigned quality:{1U,2U,3U}) {
            auto style_samples=std::make_shared<CalibratedGameFrame>(*empty);
            style_samples->settings.aa_type=6;style_samples->settings.aa_quality=quality;
            style_samples->settings.world_effects={unsigned(Effect::sepia),100,0,0};
            const auto image=render(style_samples);
            check(image!=quiet,"Liquid style-only MSAA produced an unchanged image");
            check(render(style_samples)==image,"Liquid style-only MSAA lost merged targets on a retained sample retry");
        }
        check(render(empty)==quiet,"Style-only liquid MSAA contaminated OFF restoration");
        // A dry foreground stays opaque while a distinct green model below the
        // plane must be visible through the refracted (not screen-space) ray.
        quad(-.5F,-1.F,-2.4F,.18F,false);
        for(auto& v:base->draws.back().packet.geometry.vertices) {
            const float colour[]{.08F,.82F,.13F,1};std::copy(std::begin(colour),std::end(colour),v.color);
            std::copy(std::begin(colour),std::end(colour),v.odd_color);
        }
        water=selected(5,3);auto off=std::make_shared<CalibratedGameFrame>(*water);off->settings.ray_tracing=0;
        const auto original=render(off),wet=render(water);
        std::cout<<backend<<" native water dry/submerged source captured"<<std::endl;
        auto water_no_reflection=std::make_shared<CalibratedGameFrame>(*water);water_no_reflection->settings.reflections=0;
        const auto isolated=render(water_no_reflection);
        auto without_submerged=std::make_shared<CalibratedGameFrame>(*water_no_reflection);
        without_submerged->draws.pop_back();const auto bed_only=render(without_submerged);
        unsigned dry=0,submerged=0,native_ink=0,transmitted_geometry=0;
        const auto source_code=[&](double v) {
            if(srgb) v=v<=.0031308?v*12.92:1.055*std::pow(v,1/2.4)-.055;
            return int(std::lround(v*255));
        };
        const auto colour_is=[&](const auto& image,unsigned at,std::array<double,3> colour) {
            for(unsigned c=0;c<3;++c) if(std::abs(int(image[at+c])-source_code(colour[c]))>1) return false;
            return true;
        };
        for(unsigned e=0;e<2;++e) for(unsigned at=0;at<wet[e].size();at+=4) {
            check(wet[e][at+3]==original[e][at+3],"Water made an opaque source model or HUD transparent");
            if(colour_is(original[e],at,{.81,.11,.06})) {
                // Disable model reflections for this exact dry-colour check;
                // otherwise that model legitimately reflects the live water.
                check(std::equal(isolated[e].begin()+at,isolated[e].begin()+at+4,original[e].begin()+at),"Water transmission shaded an above-plane source model");++dry;
            }
            if(original[e][at]==255 && original[e][at+1]==255 && original[e][at+2]==0) {
                check(std::equal(wet[e].begin()+at,wet[e].begin()+at+4,original[e].begin()+at),"Water changed protected UI ink");++native_ink;
            }
            if(colour_is(original[e],at,{.08,.82,.13}))
                submerged+=!std::equal(wet[e].begin()+at,wet[e].begin()+at+3,original[e].begin()+at);
            // Replacing submerged geometry with its bed must remove the
            // transmitted green radiance. Merely hiding the old green raster
            // under an opaque water colour would not pass this comparison.
            transmitted_geometry+=isolated[e][at+1]>unsigned(bed_only[e][at+1])+5;
        }
        std::cout<<backend<<" native water ownership dry="<<dry<<" submerged="<<submerged<<" protected="<<native_ink
            <<" transmitted geometry="<<transmitted_geometry<<std::endl;
        check(dry>10 && submerged>10 && native_ink>10,"Water fixture missed dry/submerged/protected source ownership");
        check(transmitted_geometry>10,"Water hid the submerged raster without tracing its transmitted geometry");
        check(wet[0]!=wet[1],"Native water/transmitted geometry lost independent eyes");
        auto styled_water=std::make_shared<CalibratedGameFrame>(*water_no_reflection);
        // Neighbourhood styles are post-transport. Palette styles can also
        // legitimately change secondary model/environment radiance, so their
        // primary-only reference is tested separately in the guide component.
        styled_water->settings.world_effects={unsigned(Effect::cel_drawn),100,0,0};
        styled_water->settings.model_effects={unsigned(Effect::emboss),100,0,0};
        const auto styled=render(styled_water);
        unsigned liquid_style_checks=0;
        for(unsigned e=0;e<2;++e) {
            Framebuffer labels(mock.image_width,mock.image_height,1);labels.enable_layer_tags(true);
            for(unsigned at=0;at<original[e].size();at+=4) {
                // Known authored dry red quad and yellow HUD label the source
                // independently of the renderer's private ownership texture.
                // Green submerged geometry is now part of the liquid world.
                const bool dry_model=colour_is(original[e],at,{.81,.11,.06});
                const bool hud=original[e][at]==255 && original[e][at+1]==255 && original[e][at+2]==0;
                labels.layer_tags()[at/4]=unsigned(hud?PixelLayer::two_d:dry_model?PixelLayer::three_d:PixelLayer::background);
                liquid_style_checks+=colour_is(original[e],at,{.08,.82,.13});
            }
            auto expected=isolated[e];std::vector<std::uint8_t> scratch;
            apply_effect(Effect::emboss,labels,expected,scratch,100,Effect::cel_drawn,100);
            for(unsigned at=0;at<expected.size();++at) {
                if(std::abs(int(styled[e][at])-int(expected[at]))>unsigned(at%4==3?0:1))
                    std::cerr<<backend<<" liquid style eye="<<e<<" xy="<<(at/4)%mock.image_width<<','<<(at/4)/mock.image_width
                        <<" channel="<<at%4<<" original="<<unsigned(original[e][at])<<" wet="<<unsigned(isolated[e][at])
                        <<" actual="<<unsigned(styled[e][at])<<" expected="<<unsigned(expected[at])<<" label="<<unsigned(labels.layer_tags()[at/4])<<'\n';
                check(std::abs(int(styled[e][at])-int(expected[at]))<=unsigned(at%4==3?0:1),
                    "Retained liquid inherited submerged model style instead of independent world style");
            }
        }
        check(liquid_style_checks>10,"Liquid owner style oracle missed submerged geometry");
        check(render(styled_water)==styled,"Held liquid guide/style pass changed accepted imagery");
        for(unsigned modes:{3U,12U,15U}) for(unsigned type:{0U,6U}) {
            auto focused=std::make_shared<CalibratedGameFrame>(*water_no_reflection);
            focused->settings.depth_enhancements=modes;focused->settings.aa_type=type;focused->settings.aa_quality=type?2:0;
            const auto focused_image=render(focused);
            check(focused_image!=isolated,"Retained liquid AO/DOF combination had no visible effect");
            check(render(focused)==focused_image,"Held liquid depth/MSAA combination changed accepted imagery");
            for(unsigned e=0;e<2;++e) for(unsigned at=0;at<original[e].size();at+=4) {
                check(focused_image[e][at+3]==original[e][at+3],"Retained liquid depth changed opaque model/ground alpha");
                if(original[e][at]==255 && original[e][at+1]==255 && original[e][at+2]==0)
                    check(std::equal(focused_image[e].begin()+at,focused_image[e].begin()+at+4,original[e].begin()+at),
                        "Retained liquid depth changed protected UI");
            }
        }
        check(render(water)==wet,"Liquid guide/depth/style switching contaminated ordinary transport");
        std::cout<<backend<<" native liquid guides: independent dry/submerged world/model style oracle, AO/DOF + MSAA changed-image/protected-alpha/retained retry passed ("
            <<liquid_style_checks<<" submerged style checks); independent AO/DOF numeric oracle is in the component fixture.\n";
        auto mirrored=std::make_shared<CalibratedGameFrame>(*water);mirrored->settings.material=unsigned(Effect::mirror);
        check(render(mirrored)!=wet,"Native liquid did not participate in model mirror transport");
        for(unsigned type:{3U,4U,6U}) for(unsigned quality:{1U,2U,3U}) {
            std::cout<<backend<<" native water AA type="<<type<<" quality="<<quality<<std::endl;
            auto aa=std::make_shared<CalibratedGameFrame>(*empty);aa->settings.aa_type=type;aa->settings.aa_quality=quality;
            const auto resolved=render(aa);water_reference(resolved,3,type,quality);
            check(render(aa)==resolved,"Held native water AA resampled its source or refraction clock");
        }
        check(render(water)==wet,"Water empty/nonempty or material switching retained stale ray state");retained=water;
        // Force a real accepted shutter interval with a changed eye pose. In
        // an analytic scene the result remains the independent water reference;
        // the native producer/underlay path still executes for both eyes and
        // every hardware sample, rather than only testing uploaded fake layers.
        for(unsigned type:{0U,3U,4U,6U}) {
            auto first=std::make_shared<CalibratedGameFrame>(*empty);
            first->settings.motion_blur=2;first->presentation_seconds=30+type;
            first->settings.aa_type=type;first->settings.aa_quality=type?2:0;
            native_mock.head_x=4;(void)render(first);
            auto next=std::make_shared<CalibratedGameFrame>(*first);next->presentation_seconds+=1./120;
            native_mock.head_x=5;
            const auto shuttered=render(next);water_reference(shuttered,3,type,type?2:0);
            check(render(next)==shuttered,"Retained liquid underlay resampled a held shutter frame");
        }
        check(render(water)==wet,"Liquid shutter layers contaminated ordinary water OFF restoration");
        std::cout<<backend<<" native water: independent Snell/Beer/Fresnel colours, below-plane geometry, dry/protected opacity, reflection OFF, waves, caustics, palette, mirror transport, SSAA/TAA/MSAA and retained retry passed.\n";
    }
    return retained;
}
