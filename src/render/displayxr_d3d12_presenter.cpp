#include "starfox/render/displayxr_d3d12_presenter.hpp"
#include "starfox/render/displayxr_d3d12_binding.hpp"
#include "starfox/render/displayxr_vulkan_binding.hpp"
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "starfox/render/sdl_vulkan_bridge.h"
#include "starfox/render/sdl_d3d12_bridge.h"
#include "starfox/vr/openxr_swapchains.hpp"
#include <SDL3/SDL.h>
#include <windows.h>
#include <initguid.h>
#include <d3d12.h>
#define XR_USE_GRAPHICS_API_D3D12
#define XR_USE_GRAPHICS_API_VULKAN
#include <openxr/openxr_platform.h>
#include <exception>
#include <stdexcept>
#include <vector>

namespace starfox::render {
namespace {
template<class Function> Function function(const DisplayXrRuntime& runtime,const char* name) {
    PFN_xrVoidFunction pointer{};
    if(XR_FAILED(runtime.get_instance_proc()(runtime.instance(),name,&pointer)) || !pointer)
        throw std::runtime_error(std::string("Missing runtime entry point: ")+name);
    return reinterpret_cast<Function>(pointer);
}
void require(bool result,const char* message) {if(!result) throw std::runtime_error(message);}
void require(bool result,const std::string& message) {if(!result) throw std::runtime_error(message);}
}
struct DisplayXrD3D12Presenter::State {
    SDL_GPUDevice* device{};
    ID3D12Device* native{};
    const StarfoxSdlD3D12XrBridgeV1* bridge{};
    const StarfoxSdlVulkanBridgeV2* vk_native{};
    const StarfoxSdlVulkanXrBridgeV1* vk_bridge{};
    XrGraphicsBindingVulkan2KHR vk_binding{XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR};
    VkFence health{};
    PFN_vkGetFenceStatus vk_fence_status{};
    PFN_vkDestroyFence vk_destroy_fence{};
    DisplayXrD3D12Binding binding;
    DisplayXrSession session;
    std::unique_ptr<vr::OpenXrSwapchains> swapchains;
    std::array<std::vector<XrSwapchainImageD3D12KHR>,2> images;
    std::array<std::vector<XrSwapchainImageVulkan2KHR>,2> vk_images;
    std::array<std::array<std::uint32_t,2>,2> extents{};
    SDL_GPUTextureFormat format{};
    SDL_GPUFence* fence{},*scene_fence{};
    std::vector<SDL_GPUFence*> superseded_fences;
    bool active{},cancelling{},uncertain_submission{};
    std::uint64_t presented_frames{};
    State() {superseded_fences.reserve(4);} // source, finished scene, failure retirement
    ~State() {if(health) vk_destroy_fence(vk_native->device,health,nullptr);}
    bool device_lost() const {
        return vk_native?vk_fence_status(vk_native->device,health)==VK_ERROR_DEVICE_LOST
            :FAILED(native->GetDeviceRemovedReason());
    }
    bool device_healthy() const {
        return vk_native?vk_fence_status(vk_native->device,health)==VK_SUCCESS
            :SUCCEEDED(native->GetDeviceRemovedReason());
    }
    template<class Operation> bool serialized(Operation&& operation) {
        // Never allow a C++ exception to unwind through the backend's C mutex
        // callback: unlock first, then rethrow at the outer C++ boundary.
        struct Call {Operation& operation;std::exception_ptr failure;};
        Call call{operation};
        const auto invoke=[](void* user) {
            auto& call=*static_cast<Call*>(user);
            try {return call.operation();} catch(...) {call.failure=std::current_exception();return false;}
        };
        // Backend-specific callbacks share one exception/lifetime discipline.
        struct Dispatch {decltype(invoke)& invoke;void* call;};
        Dispatch dispatch{invoke,&call};
        const bool result=vk_bridge?vk_bridge->with_queue(device,[](void* user,VkQueue) {
            auto& d=*static_cast<Dispatch*>(user);return d.invoke(d.call);
        },&dispatch):bridge->with_queue(device,[](void* user,void*) {
            auto& d=*static_cast<Dispatch*>(user);return d.invoke(d.call);
        },&dispatch);
        if(call.failure) std::rethrow_exception(call.failure);
        return result;
    }
    bool gpu_complete(bool wait) {
        if(uncertain_submission) {
            if(device_lost()) uncertain_submission=false;
            else if(wait) {
                if(!SDL_WaitForGPUIdle(device)) return false;
                uncertain_submission=false;
            } else {
                // An ordered empty submission fences any work accepted before
                // a failed submit returned no fence. Do not block the UI on
                // SDL_WaitForGPUIdle; retain everything if this also fails.
                auto* command=SDL_AcquireGPUCommandBuffer(device);
                if(!command) return false;
                auto* ordered=SDL_SubmitGPUCommandBufferAndAcquireFence(command);
                if(!ordered) return false;
                if(fence) superseded_fences.push_back(fence);
                fence=ordered;uncertain_submission=false;
            }
        }
        const auto retire=[&](SDL_GPUFence*& pending) {
            if(!pending) return true;
            const bool complete=wait?SDL_WaitForGPUFences(device,true,&pending,1):SDL_QueryGPUFence(device,pending);
            if(!complete && !device_lost()) return false;
            SDL_ReleaseGPUFence(device,pending);pending=nullptr;return true;
        };
        // SDL recycles the fence's native handle. A later ordered signal
        // protects images, but does NOT make an earlier pending fence reusable.
        for(auto& pending:superseded_fences) if(!retire(pending)) return false;
        superseded_fences.clear();
        for(auto** pending:{&scene_fence,&fence}) {
            if(!*pending) continue;
            if(!retire(*pending)) return false;
        }
        return true;
    }
    bool shutdown(bool wait=true) noexcept {
        try {
            if(!gpu_complete(wait)) return false;
            if(bridge || vk_bridge) {
                return serialized([&] {
                    if(swapchains) {
                        // Destroying a swapchain releases its acquired images;
                        // no GPU work remains. End an open frame with no layer
                        // first, never with a half-populated projection.
                        if(active) session.end_frame();
                        active=false;swapchains.reset();
                    }
                    session.close();return true;
                });
            }
            return true;
        } catch(...) {return false;}
    }
};
DisplayXrD3D12Presenter::DisplayXrD3D12Presenter()=default;
DisplayXrD3D12Presenter::~DisplayXrD3D12Presenter() {
    if(!close()) {
        // Exceptional driver failure: retain the live resource bundle rather
        // than release runtime images while GPU completion is unconfirmed.
        // Normal owners must explicitly retry close before destroying runtime
        // and SDL dependencies. Do not treat this as successful teardown.
        state_.release();
    }
}
bool DisplayXrD3D12Presenter::close() noexcept {
    if(!state_) return true;
    if(!state_->shutdown()) return false;
    state_.reset();return true;
}
bool DisplayXrD3D12Presenter::try_close() noexcept {
    if(!state_) return true;
    state_->cancelling=true; // any later frame poll must never submit a layer
    if(!state_->shutdown(false)) {status_="Leia cleanup pending: GPU completion not confirmed";return false;}
    state_.reset();return true;
}
void DisplayXrD3D12Presenter::retain_scene_submission(SDL_GPUFence* fence) noexcept {
    // The game owner may submit a source pass followed by native rays and a
    // composition pass on this same ordered queue. The later fence covers all
    // preceding work, including a failed trace's retirement fence. This must
    // protect resources even when native image acquisition times out.
    if(!state_) return;
    if(state_->scene_fence) state_->superseded_fences.push_back(state_->scene_fence);
    state_->scene_fence=fence;
    if(!fence) state_->uncertain_submission=true;
}
bool DisplayXrD3D12Presenter::initialize(const DisplayXrRuntime& runtime,SDL_GPUDevice* device,void* window,
    DisplayXrVulkanBinding* vulkan_creation) {
    if(!close()) {status_="Leia cleanup pending: GPU completion not confirmed";return false;}
    try {
        require(runtime.detected() && device && window,"Confirmed Leia runtime, SDL GPU device and panel window required");
        const bool vulkan=runtime.backend()==DisplayXrBackend::vulkan;
        require(std::string_view(SDL_GetGPUDeviceDriver(device))==(vulkan?"vulkan":"direct3d12"),
            "Leia presenter cannot use a different SDL backend from its runtime");
        auto state=std::make_unique<State>();state->device=device;
        const auto props=SDL_GetGPUDeviceProperties(device);
        if(vulkan) {
            require(vulkan_creation,"Leia Vulkan2 creation must precede SDL device allocation");
            require(vulkan_creation->attach(device),vulkan_creation->status());
            const auto* graphics=static_cast<const XrGraphicsBindingVulkan2KHR*>(vulkan_creation->binding());
            require(graphics,"Leia Vulkan2 binding unavailable");state->vk_binding=*graphics;
            state->vk_native=static_cast<const StarfoxSdlVulkanBridgeV2*>(SDL_GetPointerProperty(props,STARFOX_SDL_VULKAN_BRIDGE,nullptr));
            state->vk_bridge=static_cast<const StarfoxSdlVulkanXrBridgeV1*>(SDL_GetPointerProperty(props,STARFOX_SDL_VULKAN_XR_BRIDGE,nullptr));
            require(state->vk_native && state->vk_bridge && state->vk_bridge->version==1 && state->vk_bridge->queue
                && state->vk_bridge->with_queue && state->vk_bridge->copy_to_color,"SDL Vulkan native XR bridge unavailable");
            auto get=state->vk_native->get_device_proc;auto native=state->vk_native->device;
            const auto create=reinterpret_cast<PFN_vkCreateFence>(get(native,"vkCreateFence"));
            state->vk_fence_status=reinterpret_cast<PFN_vkGetFenceStatus>(get(native,"vkGetFenceStatus"));
            state->vk_destroy_fence=reinterpret_cast<PFN_vkDestroyFence>(get(native,"vkDestroyFence"));
            require(create && state->vk_fence_status && state->vk_destroy_fence,"Vulkan device-health interface unavailable");
            VkFenceCreateInfo info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};info.flags=VK_FENCE_CREATE_SIGNALED_BIT;
            const auto created=create(native,&info,nullptr,&state->health);
            if(created!=VK_SUCCESS) state->health=VK_NULL_HANDLE; // failure output is undefined
            require(created==VK_SUCCESS && state->health,"Vulkan device-health fence allocation failed");
        } else {
            state->native=static_cast<ID3D12Device*>(SDL_GetPointerProperty(props,STARFOX_SDL_D3D12_DEVICE,nullptr));
            state->bridge=static_cast<const StarfoxSdlD3D12XrBridgeV1*>(SDL_GetPointerProperty(props,STARFOX_SDL_D3D12_XR_BRIDGE,nullptr));
            require(state->native && state->bridge && state->bridge->version==1 && state->bridge->queue
                && state->bridge->with_queue && state->bridge->copy_to_color,"SDL native XR bridge unavailable");
            require(state->binding.initialize(runtime,state->native,state->bridge->queue(device)),state->binding.status());
        }
        vr::SwapchainApi api{
            function<PFN_xrEnumerateSwapchainFormats>(runtime,"xrEnumerateSwapchainFormats"),
            function<PFN_xrCreateSwapchain>(runtime,"xrCreateSwapchain"),
            function<PFN_xrDestroySwapchain>(runtime,"xrDestroySwapchain"),
            function<PFN_xrEnumerateSwapchainImages>(runtime,"xrEnumerateSwapchainImages"),
            function<PFN_xrAcquireSwapchainImage>(runtime,"xrAcquireSwapchainImage"),
            function<PFN_xrWaitSwapchainImage>(runtime,"xrWaitSwapchainImage"),
            function<PFN_xrReleaseSwapchainImage>(runtime,"xrReleaseSwapchainImage")};
        // Transfer access is explicit, preserving existing headset defaults.
        constexpr std::array<std::int64_t,4> d3d_formats{DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
            DXGI_FORMAT_R8G8B8A8_UNORM,DXGI_FORMAT_B8G8R8A8_UNORM};
        constexpr std::array<std::int64_t,4> vk_formats{VK_FORMAT_R8G8B8A8_SRGB,VK_FORMAT_B8G8R8A8_SRGB,
            VK_FORMAT_R8G8B8A8_UNORM,VK_FORMAT_B8G8R8A8_UNORM};
        const auto& preferred=vulkan?vk_formats:d3d_formats;
        state_=std::move(state);
        require(state_->serialized([&] {
            require(state_->session.initialize(runtime,vulkan?static_cast<const void*>(&state_->vk_binding):state_->binding.binding(),window),state_->session.status());
            state_->swapchains=std::make_unique<vr::OpenXrSwapchains>(api);
            require(state_->swapchains->initialize(state_->session.handle(),runtime.views(),preferred,
                XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT),state_->swapchains->status());
            return true;
        }),"Leia graphics initialization was not executed");
        const auto selected=state_->swapchains->format();
        state_->format=selected==preferred[0]?SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB
            :selected==preferred[1]?SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB
            :selected==preferred[2]?SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM:SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
        for(unsigned eye=0;eye<2;++eye) {
            const auto& view=runtime.views()[eye];
            state_->extents[eye]={view.recommendedImageRectWidth,view.recommendedImageRectHeight};
            if(vulkan) {
                auto& images=state_->vk_images[eye];images.resize(state_->swapchains->image_count(eye));
                for(auto& image:images) image={XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR};
                std::uint32_t count{};
                require(state_->serialized([&] {
                    return state_->swapchains->enumerate_images(eye,images.size(),&count,
                        reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data()));
                }),state_->swapchains->status());
                require(count==images.size(),"Leia Vulkan swapchain image count changed");
                // Vulkan has no portable query for an image's creation device,
                // format or extent. The runtime's validated same-device session
                // and negotiated swapchain are authoritative; never guess or
                // import a foreign image, blit it, or change its queue family.
                for(const auto& image:images) require(image.type==XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR
                    && !image.next && image.image,"Invalid Leia Vulkan color image");
                continue;
            }
            auto& images=state_->images[eye];images.resize(state_->swapchains->image_count(eye));
            for(auto& image:images) image={XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR};
            std::uint32_t count{};
            require(state_->serialized([&] {
                return state_->swapchains->enumerate_images(eye,images.size(),&count,
                    reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data()));
            }),state_->swapchains->status());
            require(count==images.size(),"Leia swapchain image count changed");
            for(const auto& image:images) {
                require(image.type==XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR && !image.next && image.texture,
                    "Invalid Leia native color image");
                ID3D12Device* owner{};
                require(SUCCEEDED(image.texture->GetDevice(IID_ID3D12Device,reinterpret_cast<void**>(&owner))),
                    "Cannot identify Leia image's graphics device");
                const bool same=owner==state_->native;owner->Release();
                require(same,"Leia image was not created on the SDL device");
                D3D12_RESOURCE_DESC desc{};
#if defined(__MINGW32__)
                image.texture->GetDesc(&desc);
#else
                desc=image.texture->GetDesc();
#endif
                require(desc.Dimension==D3D12_RESOURCE_DIMENSION_TEXTURE2D && desc.Width==view.recommendedImageRectWidth
                    && desc.Height==view.recommendedImageRectHeight && desc.DepthOrArraySize==1 && desc.MipLevels==1
                    && desc.SampleDesc.Count==1 && desc.Format==selected && (desc.Flags&D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET),
                    "Leia image does not match the negotiated color swapchain");
            }
        }
        status_="Leia runtime eye swapchains ready; awaiting session READY";return true;
    } catch(const std::exception& error) {
        const std::string failure=error.what();const bool cleaned=close();
        status_=failure+(cleaned?"":"; GPU cleanup pending");return false;
    }
}
bool DisplayXrD3D12Presenter::running() const noexcept {return state_ && state_->session.running();}
bool DisplayXrD3D12Presenter::exit_requested() const noexcept {return state_ && state_->session.exit_requested();}
bool DisplayXrD3D12Presenter::frame_pending() const noexcept {return state_ && state_->active;}
std::uint64_t DisplayXrD3D12Presenter::presented_frames() const noexcept {return state_?state_->presented_frames:0;}
std::array<std::uint32_t,2> DisplayXrD3D12Presenter::eye_extent(unsigned eye) const noexcept {
    return state_ && eye<2?state_->extents[eye]:std::array<std::uint32_t,2>{};
}
int DisplayXrD3D12Presenter::color_format() const noexcept {return state_?state_->format:0;}
bool DisplayXrD3D12Presenter::poll_events() {
    if(!state_) {status_="Leia presenter not initialized";return false;}
    try {
        require(!state_->active,"Finish the pending Leia frame before polling events");
        require(state_->serialized([&] {return state_->session.poll_events();}),state_->session.status());
        if(exit_requested()) status_="Leia panel/session disconnected";
        return true;
    } catch(const std::exception& error) {status_=error.what();return false;}
}
std::optional<DisplayXrD3D12Frame> DisplayXrD3D12Presenter::begin_frame(const DisplayXrRigSettings& settings) {
    if(!state_) {status_="Leia presenter not initialized";return {};}
    if(state_->active) {status_="Finish the pending Leia frame before beginning another";return {};}
    try {
        std::optional<DisplayXrFrame> frame;
        require(state_->serialized([&] {frame=state_->session.begin_frame(settings);return true;}),"Leia frame callback failed");
        if(!frame) {status_=state_->session.status();return {};}
        state_->active=true;state_->cancelling=false;
        DisplayXrD3D12Frame result{*frame,frame->cameras};
        for(auto& camera:result.cameras) for(unsigned column=0;column<4;++column) camera.projection[column*4+1]*=-1;
        if(!frame->xr.should_render) {
            require(state_->serialized([&] {return state_->session.end_frame();}),state_->session.status());
            state_->active=false;status_="Leia invisible/invalid frame submitted without layers";
        } else require(state_->swapchains->start_frame(frame->xr,state_->session.space()),state_->swapchains->status());
        return result;
    } catch(const std::exception& error) {const std::string failure=error.what();cancel_frame();status_=failure;return {};}
}
DisplayXrSubmission DisplayXrD3D12Presenter::submit_frame(SDL_GPUTexture* left,SDL_GPUTexture* right,XrDuration timeout) {
    if(!state_ || !state_->active) {status_="No active Leia eye frame";return DisplayXrSubmission::failed;}
    if(state_->cancelling) return cancel_frame(timeout);
    if(state_->fence) {status_="Leia copy already submitted: finish it before replacing eye textures";return DisplayXrSubmission::failed;}
    SDL_GPUCommandBuffer* command{};
    try {
        require(left && right && left!=right,"Two distinct calibrated Leia eye textures required");
        require(timeout>=0 && timeout<=100000000,"Leia image wait must be between 0 and 100ms");
        vr::ImageWait wait=vr::ImageWait::ready;
        require(state_->serialized([&] {
            for(unsigned eye=0;eye<2;++eye) {
                wait=state_->swapchains->acquire_eye(eye,timeout);
                if(wait!=vr::ImageWait::ready) break;
            }
            return true;
        }),"Leia eye acquisition callback failed");
        if(wait==vr::ImageWait::waiting) {status_="Waiting for runtime eye images";return DisplayXrSubmission::waiting;}
        require(wait==vr::ImageWait::ready,state_->swapchains->status());
        command=SDL_AcquireGPUCommandBuffer(state_->device);require(command,SDL_GetError());
        const std::array<SDL_GPUTexture*,2> sources{left,right};
        for(unsigned eye=0;eye<2;++eye) {
            const auto index=state_->swapchains->image_index(eye);
            require(index && *index<(state_->vk_bridge?state_->vk_images[eye].size():state_->images[eye].size()),"Leia image index outside retained images");
            const auto size=state_->extents[eye];
            const bool copied=state_->vk_bridge?state_->vk_bridge->copy_to_color(command,sources[eye],state_->vk_images[eye][*index].image,
                static_cast<VkFormat>(state_->swapchains->format()),size[0],size[1])
                :state_->bridge->copy_to_color(command,sources[eye],state_->images[eye][*index].texture);
            if(!copied)
                throw std::runtime_error(SDL_GetError());
        }
        state_->uncertain_submission=true;
        state_->fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);command=nullptr;
        require(state_->fence,SDL_GetError());
        state_->uncertain_submission=false;
        status_="Leia eye copy submitted; GPU completion pending";
        return finish_frame();
    } catch(const std::exception& error) {
        if(command) SDL_CancelGPUCommandBuffer(command);
        const std::string failure=error.what();
        // A failed submission with no fence is not proof that queued work is
        // absent. Confirm device idle/removal before releasing native images.
        // Recovery polls an ordered completion fence rather than stalling
        // the event loop. cancel_frame retains the images while it is pending.
        cancel_frame();status_=failure;return DisplayXrSubmission::failed;
    }
}
DisplayXrSubmission DisplayXrD3D12Presenter::finish_frame() {
    if(!state_ || !state_->active) {status_="No active Leia frame to finish";return DisplayXrSubmission::failed;}
    if(state_->cancelling) return cancel_frame();
    if(!state_->fence) {status_="Leia eye copy has not been submitted";return DisplayXrSubmission::failed;}
    if(!state_->gpu_complete(false)) return DisplayXrSubmission::waiting;
    try {
        require(state_->device_healthy(),"Leia graphics device lost or health query failed");
        const bool submitted=state_->serialized([&] {
            for(unsigned eye=0;eye<2;++eye)
                require(state_->swapchains->release_eye(eye),state_->swapchains->status());
            const auto* projection=state_->swapchains->projection();require(projection,"Incomplete Leia stereo projection");
            const std::array<const XrCompositionLayerBaseHeader*,1> layers{
                reinterpret_cast<const XrCompositionLayerBaseHeader*>(projection)};
            const bool result=state_->session.end_frame(layers);state_->active=false;
            return result;
        });
        require(submitted,state_->session.status());++state_->presented_frames;
        status_="Leia calibrated stereo projection submitted";
        return DisplayXrSubmission::submitted;
    } catch(const std::exception& error) {
        const std::string failure=error.what();cancel_frame();status_=failure;return DisplayXrSubmission::failed;
    }
}
DisplayXrSubmission DisplayXrD3D12Presenter::continue_frame(SDL_GPUTexture* left,SDL_GPUTexture* right) {
    if(!state_ || !state_->active) {status_="No active Leia frame to continue";return DisplayXrSubmission::failed;}
    return state_->fence?finish_frame():submit_frame(left,right);
}
DisplayXrSubmission DisplayXrD3D12Presenter::cancel_frame(XrDuration timeout) {
    if(!state_ || !state_->active) return DisplayXrSubmission::submitted;
    if(timeout<0 || timeout>100000000) {status_="Leia cancellation wait must be between 0 and 100ms";return DisplayXrSubmission::failed;}
    state_->cancelling=true;
    if(!state_->gpu_complete(false)) return DisplayXrSubmission::waiting;
    try {
        vr::ImageWait wait{};
        require(state_->serialized([&] {wait=state_->swapchains->cancel_frame(timeout);return true;}),"Leia cancellation callback failed");
        if(wait==vr::ImageWait::waiting) return DisplayXrSubmission::waiting;
        require(wait==vr::ImageWait::ready,state_->swapchains->status());
        const bool result=state_->serialized([&] {return state_->session.end_frame();});
        state_->active=false;state_->cancelling=false;require(result,state_->session.status());
        status_="Leia frame cancelled without a visible layer";return DisplayXrSubmission::submitted;
    } catch(const std::exception& error) {status_=error.what();return DisplayXrSubmission::failed;}
}
}
