#define NOMINMAX
#include "starfox/render/d3d11_fsr1.hpp"
#include <d3d11.h>
#include <wrl/client.h>
#include <vector>
#include <iostream>
#include <stdexcept>
using Microsoft::WRL::ComPtr;
void check(HRESULT value){if(FAILED(value))throw std::runtime_error("D3D11 fixture failed");}
int main(){
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    check(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context));
    starfox::render::D3d11Fsr1 fsr;
    for(unsigned width:{96U,128U})for(unsigned mode=1;mode<=4;++mode){
        std::vector<unsigned> pixels(64*48,0xff806040U);
        D3D11_TEXTURE2D_DESC d{};d.Width=64;d.Height=48;d.MipLevels=d.ArraySize=1;
        d.Format=DXGI_FORMAT_R8G8B8A8_UNORM;d.SampleDesc.Count=1;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA init{pixels.data(),64*4,0};
        ComPtr<ID3D11Texture2D> source,output,readback;
        check(device->CreateTexture2D(&d,&init,&source));
        d.Width=width;d.Height=96;check(device->CreateTexture2D(&d,nullptr,&output));
        d.BindFlags=0;d.Usage=D3D11_USAGE_STAGING;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        check(device->CreateTexture2D(&d,nullptr,&readback));
        if(!fsr.apply(device.Get(),source.Get(),output.Get(),starfox::render::Fsr1Mode(mode)))throw std::runtime_error(fsr.status());
        context->CopyResource(readback.Get(),output.Get());D3D11_MAPPED_SUBRESOURCE mapped{};
        check(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&mapped));
        bool valid=true;
        for(unsigned y=0;y<96;++y)for(unsigned x=0;x<width;++x){
            const auto* p=static_cast<unsigned char*>(mapped.pData)+y*mapped.RowPitch+x*4;
            valid&=p[0]>=63&&p[0]<=65&&p[1]>=95&&p[1]<=97&&p[2]>=127&&p[2]<=129&&p[3]==255;
        }
        context->Unmap(readback.Get(),0);if(!valid)throw std::runtime_error("FSR1 lost constant colour or edge coverage");
    }
    fsr.reset();std::cout<<"D3D11 FSR1: all quality modes, resized outputs, constant colour and full edge coverage passed\n";
}
