// Real SDK, opened before SDL/DXGI. Wrappers record calls only; reconstruction
// is performed by the official adapter/plugins on the actual SDL D3D12 device.
namespace dlss_fixture {
CalibratedDlssApi api;
std::vector<StarfoxDlssFrameV1> evaluations;
unsigned release_count{},fail_call{},last_pair_token{};
unsigned finished_frames{},evaluations_at_finish{};
unsigned finish_attempts{},fail_finish_call{};
// Diagnostic only: overwrite the genuine neural evaluation with an identity
// DLAA copy on its real input/output command list. SDK allocation/lifecycle
// still run. This exposes production raster/ray samples to an independent
// image oracle, not an SDK-quality claim.
bool identity_dlaa{};
decltype(&starfox_dlss_evaluate_v1) real_evaluate{};
decltype(&starfox_dlss_evaluate_v2) real_evaluate_rejection{};
decltype(&starfox_dlss_release_viewport_v1) real_release{};
decltype(&starfox_dlss_finish_frame_v1) real_finish{};
int finish_frame(void* module,char* error,unsigned capacity) {
    if(++finish_attempts==fail_finish_call) {
        if(capacity) std::snprintf(error,capacity,"Injected SDK frame-end failure");
        return 1;
    }
    const int result=real_finish(module,error,capacity);
    if(!result) {++finished_frames;evaluations_at_finish=unsigned(evaluations.size());}
    return result;
}
bool record(const StarfoxDlssFrameV1* frame,char* error,unsigned capacity) {
    if(frame->viewport==100) {
        check(frame->frame_index>last_pair_token,"Recreated native owner reused an SDK common-constants token");
        last_pair_token=frame->frame_index;
    } else check(frame->viewport==101 && frame->frame_index==last_pair_token,"SDK physical eyes used different frame tokens");
    evaluations.push_back(*frame);
    if(fail_call && evaluations.size()==fail_call) {
        if(capacity) std::snprintf(error,capacity,"Injected right-eye SDK evaluation failure");
        return false;
    }
    return true;
}
int evaluate(void* module,const StarfoxDlssFrameV1* frame,char* error,unsigned capacity) {
    return record(frame,error,capacity)?real_evaluate(module,frame,error,capacity):1;
}
int evaluate_rejection(void* module,const StarfoxDlssFrameV2* frame,char* error,unsigned capacity) {
    check(frame && frame->size==sizeof(*frame) && frame->current_color_bias && frame->bias_state==0x40U,
        "Native SDK owner omitted its actual current-colour rejection guide");
    if(!record(&frame->frame,error,capacity)) return 1;
    if(!identity_dlaa) return real_evaluate_rejection(module,frame,error,capacity);
    const int result=real_evaluate_rejection(module,frame,error,capacity);
    if(result) return result;
    const auto& f=frame->frame;
    check(f.width==f.output_width && f.height==f.output_height && f.command && f.color && f.output,
        "Identity SDK diagnostic requires actual equal-size DLAA resources");
    auto* list=static_cast<ID3D12GraphicsCommandList*>(f.command);
    auto* color=static_cast<ID3D12Resource*>(f.color);auto* output=static_cast<ID3D12Resource*>(f.output);
    D3D12_RESOURCE_DESC color_desc{},output_desc{};
#ifdef __MINGW32__
    color->GetDesc(&color_desc);output->GetDesc(&output_desc);
#else
    color_desc=color->GetDesc();output_desc=output->GetDesc();
#endif
    check(color_desc.Format==DXGI_FORMAT_R8G8B8A8_UNORM && output_desc.Format==DXGI_FORMAT_R8G8B8A8_UNORM,
        "Identity SDK diagnostic received nonmatching color resources");
    D3D12_RESOURCE_BARRIER barriers[2]{};
    for(unsigned i=0;i<2;++i) {
        auto& b=barriers[i];b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b.Transition.pResource=i?output:color;b.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        b.Transition.StateBefore=static_cast<D3D12_RESOURCE_STATES>(f.states[i?3:0]);
        b.Transition.StateAfter=i?D3D12_RESOURCE_STATE_COPY_DEST:D3D12_RESOURCE_STATE_COPY_SOURCE;
    }
    list->ResourceBarrier(2,barriers);list->CopyResource(output,color);
    for(auto& b:barriers) std::swap(b.Transition.StateBefore,b.Transition.StateAfter);
    list->ResourceBarrier(2,barriers);return 0;
}
int release(void* module,unsigned viewport,char* error,unsigned capacity) {
    ++release_count;return real_release(module,viewport,error,capacity);
}
struct Sdk {
    HMODULE adapter{};void* module{};decltype(&starfox_dlss_close_v1) close{};
    explicit Sdk(const std::filesystem::path& directory) {
        const auto path=std::filesystem::canonical(directory);
        adapter=LoadLibraryExW((path/L"starfox_dlss_native.dll").c_str(),nullptr,
            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
        check(adapter,"Cannot load explicit native DLSS owner adapter");
        const auto symbol=[&](const char* name){auto* value=GetProcAddress(adapter,name);check(value,name);return value;};
        auto open=reinterpret_cast<decltype(&starfox_dlss_open_v1)>(symbol("starfox_dlss_open_v1"));
        close=reinterpret_cast<decltype(close)>(symbol("starfox_dlss_close_v1"));
        api.bind=reinterpret_cast<decltype(api.bind)>(symbol("starfox_dlss_bind_device_v1"));
        api.configure=reinterpret_cast<decltype(api.configure)>(symbol("starfox_dlss_configure_v1"));
        api.configure_model=reinterpret_cast<decltype(api.configure_model)>(symbol("starfox_dlss_configure_v2"));
        real_evaluate=reinterpret_cast<decltype(real_evaluate)>(symbol("starfox_dlss_evaluate_v1"));
        real_evaluate_rejection=reinterpret_cast<decltype(real_evaluate_rejection)>(symbol("starfox_dlss_evaluate_v2"));
        real_release=reinterpret_cast<decltype(real_release)>(symbol("starfox_dlss_release_viewport_v1"));
        real_finish=reinterpret_cast<decltype(real_finish)>(symbol("starfox_dlss_finish_frame_v1"));
        api.evaluate=evaluate;api.release=release;
        api.finish_frame=finish_frame;
        api.evaluate_rejection=evaluate_rejection;
        char error[512]{};check(open(path.c_str(),&module,error,sizeof(error))==0,error);api.module=module;
    }
    void finish() {
        check(finished_frames && evaluations_at_finish==evaluations.size(),"SDK frame-end did not cover the last evaluation before shutdown");
        char error[512]{};check(close(module,error,sizeof(error))==0,error);
        std::cout<<"Real native SDK lifecycle: "<<evaluations.size()<<" evaluation attempts, "<<release_count
            <<" viewport retirements; "<<finished_frames<<" frame ends in "<<finish_attempts<<" attempts; monotonic token="<<last_pair_token<<" through owner replacement/reconnect.\n";
        module=nullptr;api={};FreeLibrary(adapter);adapter=nullptr;
    }
    ~Sdk() {
        if(module) {char error[512]{};if(close(module,error,sizeof(error))) return;}
        if(adapter) FreeLibrary(adapter);
    }
};
}
