#include "starfox/render/d3d11_fsr1.hpp"
#include <algorithm>
#if defined(STARFOX_D3D11_FSR1)
#define NOMINMAX
#include <d3d11.h>
#include <wrl/client.h>
#include <stdexcept>
#include "fsr1_d3d11_shader.hpp"
#endif
namespace starfox::render {
struct D3d11Fsr1::Impl {
    std::string status;
#if defined(STARFOX_D3D11_FSR1)
    template<class T> using Ptr=Microsoft::WRL::ComPtr<T>;
    Ptr<ID3D11Device> device;Ptr<ID3D11DeviceContext> context;
    Ptr<ID3D11ComputeShader> shader;Ptr<ID3D11Buffer> constants;Ptr<ID3D11SamplerState> sampler;
    struct Image {Ptr<ID3D11Texture2D> texture;Ptr<ID3D11ShaderResourceView> srv;Ptr<ID3D11UnorderedAccessView> uav;};
    Image reduced,easu,rcas;Fsr1Extent input{},output{};
    static void check(HRESULT hr){if(FAILED(hr))throw std::runtime_error("D3D11 FSR1 resource/dispatch failed");}
    void initialize(ID3D11Device* d) {
        if(device.Get()==d)return;
        device=d;d->GetImmediateContext(context.ReleaseAndGetAddressOf());
        check(d->CreateComputeShader(fsr1_d3d11_shader,sizeof(fsr1_d3d11_shader),nullptr,shader.ReleaseAndGetAddressOf()));
        D3D11_BUFFER_DESC cb{};cb.ByteWidth=32;cb.Usage=D3D11_USAGE_DEFAULT;cb.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
        check(d->CreateBuffer(&cb,nullptr,constants.ReleaseAndGetAddressOf()));
        D3D11_SAMPLER_DESC s{};s.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        s.AddressU=s.AddressV=s.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;s.MaxLOD=D3D11_FLOAT32_MAX;
        check(d->CreateSamplerState(&s,sampler.ReleaseAndGetAddressOf()));
        input={};output={};reduced={};easu={};rcas={};
    }
    Image image(Fsr1Extent size) {
        Image result;D3D11_TEXTURE2D_DESC t{};t.Width=size.width;t.Height=size.height;t.MipLevels=t.ArraySize=1;
        t.Format=DXGI_FORMAT_R8G8B8A8_UNORM;t.SampleDesc.Count=1;t.Usage=D3D11_USAGE_DEFAULT;
        t.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_UNORDERED_ACCESS;
        check(device->CreateTexture2D(&t,nullptr,&result.texture));
        check(device->CreateShaderResourceView(result.texture.Get(),nullptr,&result.srv));
        check(device->CreateUnorderedAccessView(result.texture.Get(),nullptr,&result.uav));return result;
    }
    void dispatch(ID3D11ShaderResourceView* source,Image& target,Fsr1Extent from,Fsr1Extent to,unsigned stage) {
        struct Params {unsigned sw,sh,ow,oh,stage;float sharpness;unsigned p0,p1;};
        const Params p{from.width,from.height,to.width,to.height,stage,.2f,0,0};
        context->UpdateSubresource(constants.Get(),0,nullptr,&p,0,0);
        context->CSSetShader(shader.Get(),nullptr,0);context->CSSetConstantBuffers(0,1,constants.GetAddressOf());
        context->CSSetSamplers(0,1,sampler.GetAddressOf());context->CSSetShaderResources(0,1,&source);
        context->CSSetUnorderedAccessViews(0,1,target.uav.GetAddressOf(),nullptr);
        context->Dispatch((to.width+7)/8,(to.height+7)/8,1);
        ID3D11ShaderResourceView* noSource=nullptr;ID3D11UnorderedAccessView* noTarget=nullptr;
        context->CSSetShaderResources(0,1,&noSource);context->CSSetUnorderedAccessViews(0,1,&noTarget,nullptr);
        context->CSSetShader(nullptr,nullptr,0);
    }
#endif
};
D3d11Fsr1::D3d11Fsr1():impl_(std::make_unique<Impl>()){}
D3d11Fsr1::~D3d11Fsr1()=default;
void D3d11Fsr1::reset(){impl_=std::make_unique<Impl>();}
const std::string& D3d11Fsr1::status()const{return impl_->status;}
bool D3d11Fsr1::apply(void* raw,void* src,void* dst,Fsr1Mode mode) {
#if defined(STARFOX_D3D11_FSR1)
    if(!raw||!src||!dst||mode==Fsr1Mode::off)return false;
    try {
        auto& p=*impl_;p.initialize(static_cast<ID3D11Device*>(raw));
        auto* source=static_cast<ID3D11Texture2D*>(src);auto* destination=static_cast<ID3D11Texture2D*>(dst);
        D3D11_TEXTURE2D_DESC a{},b{};source->GetDesc(&a);destination->GetDesc(&b);
        if(a.SampleDesc.Count!=1 || b.Format!=DXGI_FORMAT_R8G8B8A8_UNORM)return false;
        const Fsr1Extent out{b.Width,b.Height};auto in=fsr1_input_extent(out,mode);
        // Do not invent additional input detail by pre-enlarging a tiny native
        // framebuffer. When it is already smaller, EASU sees the original.
        in.width=std::min(in.width,a.Width);in.height=std::min(in.height,a.Height);
        if(in!=p.input||out!=p.output){p.reduced=p.image(in);p.easu=p.image(out);p.rcas=p.image(out);p.input=in;p.output=out;}
        Impl::Ptr<ID3D11ShaderResourceView> view;
        Impl::check(p.device->CreateShaderResourceView(source,nullptr,&view));
        auto* easuSource=view.Get();
        if(in.width!=a.Width||in.height!=a.Height){p.dispatch(view.Get(),p.reduced,{a.Width,a.Height},in,2);easuSource=p.reduced.srv.Get();}
        p.dispatch(easuSource,p.easu,in,out,0);p.dispatch(p.easu.srv.Get(),p.rcas,out,out,1);
        p.context->CopyResource(destination,p.rcas.texture.Get());p.status="D3D11 FSR1 EASU + RCAS";return true;
    }catch(const std::exception& e){impl_->status=e.what();return false;}
#else
    (void)raw;(void)src;(void)dst;(void)mode;return false;
#endif
}
}
