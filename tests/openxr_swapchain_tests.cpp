#include "starfox/vr/openxr_swapchains.hpp"
#include <iostream>
#include <stdexcept>
using namespace starfox::vr;
namespace {
void require(bool condition) {if(!condition) throw std::runtime_error("Swapchain assertion failed");}
template<class T> T handle(uintptr_t n) {return reinterpret_cast<T>(n);}
unsigned creates{},destroys{},acquires{},waits{},releases{};
bool timeout{},bad_index{},fail_second{},fail_release{},quad_mode{};
XRAPI_ATTR XrResult XRAPI_CALL formats(XrSession,uint32_t capacity,uint32_t* count,int64_t* out) {
    *count=2;if(capacity) {out[0]=42;out[1]=43;}return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL create(XrSession,const XrSwapchainCreateInfo* info,XrSwapchain* out) {
    ++creates;
    require(info->format==43 && info->sampleCount==1 && info->arraySize==1);
    require(quad_mode?(info->width==1024 && info->height==896):(info->width==100+creates && info->height==200+creates));
    if(fail_second && creates==2) return XR_ERROR_RUNTIME_FAILURE;
    *out=handle<XrSwapchain>(creates);return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL destroy(XrSwapchain) {++destroys;return XR_SUCCESS;}
XRAPI_ATTR XrResult XRAPI_CALL images(XrSwapchain,uint32_t,uint32_t* count,XrSwapchainImageBaseHeader*) {
    *count=3;return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL acquire(XrSwapchain,const XrSwapchainImageAcquireInfo*,uint32_t* index) {
    ++acquires;*index=bad_index?3:1;return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL wait(XrSwapchain,const XrSwapchainImageWaitInfo*) {
    ++waits;return timeout?XR_TIMEOUT_EXPIRED:XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL release(XrSwapchain,const XrSwapchainImageReleaseInfo*) {
    ++releases;return fail_release?XR_ERROR_RUNTIME_FAILURE:XR_SUCCESS;
}
}
int main() {
    try {
        SwapchainApi api{formats,create,destroy,images,acquire,wait,release};
        std::array<XrViewConfigurationView,2> config{};
        for(unsigned i=0;i<2;++i) {
            config[i].recommendedImageRectWidth=101+i;
            config[i].recommendedImageRectHeight=201+i;
            config[i].maxImageRectWidth=config[i].maxImageRectHeight=1024;
            config[i].maxSwapchainSampleCount=4;
        }
        const std::array<int64_t,3> preferences{99,43,42};
        OpenXrSwapchains chains(api);
        require(chains.initialize(handle<XrSession>(1),config,preferences));
        require(chains.format()==43 && chains.image_count(0)==3);
        StereoFrame frame{};frame.should_render=true;
        frame.views[0].pose.position.x=-0.032f;frame.views[1].pose.position.x=0.032f;
        frame.views[0].fov.angleLeft=-0.7f;frame.views[1].fov.angleRight=0.8f;
        const auto space=handle<XrSpace>(2);
        require(chains.start_frame(frame,space));
        require(!chains.projection() && !chains.release_eye(0));
        timeout=true;
        require(chains.acquire_eye(0)==ImageWait::waiting);
        require(!chains.image_index(0) && !chains.start_frame(frame,space));
        require(chains.acquire_eye(0)==ImageWait::waiting && acquires==1 && waits==2);
        timeout=false;
        require(chains.acquire_eye(0)==ImageWait::ready && acquires==1);
        require(chains.image_index(0)==1);
        fail_release=true;require(!chains.release_eye(0));
        fail_release=false;require(chains.release_eye(0));
        require(!chains.projection() && chains.acquire_eye(0)==ImageWait::error);
        require(chains.acquire_eye(1)==ImageWait::ready && chains.release_eye(1));
        const auto* layer=chains.projection();
        require(layer && layer->space==space && layer->viewCount==2);
        require(layer->views[0].pose.position.x==-0.032f && layer->views[1].pose.position.x==0.032f);
        require(layer->views[0].fov.angleLeft==-0.7f && layer->views[1].fov.angleRight==0.8f);
        require(layer->views[0].subImage.imageRect.extent.width==101);
        require(layer->views[1].subImage.imageRect.extent.height==202);
        frame.should_render=false;
        require(!chains.start_frame(frame,space) && !chains.projection());
        frame.should_render=true;require(chains.start_frame(frame,space));
        bad_index=true;const auto prior_waits=waits;
        require(chains.acquire_eye(0)==ImageWait::error);
        require(chains.acquire_eye(0)==ImageWait::error && waits==prior_waits);
        require(!chains.image_index(0) && !chains.release_eye(0));
        // Even a rejected runtime index must be returned without exposing it
        // for rendering. Cancellation never publishes a partial stereo layer.
        require(chains.cancel_frame()==ImageWait::ready && !chains.projection());
        bad_index=false;
        require(chains.start_frame(frame,space));
        timeout=true;
        require(chains.acquire_eye(0)==ImageWait::waiting);
        require(chains.cancel_frame()==ImageWait::waiting);
        require(!chains.start_frame(frame,space));
        timeout=false;
        fail_release=true;
        require(chains.cancel_frame()==ImageWait::error);
        require(!chains.start_frame(frame,space));
        fail_release=false;
        require(chains.cancel_frame()==ImageWait::ready);
        require(!chains.projection() && chains.start_frame(frame,space));
        require(chains.acquire_eye(0)==ImageWait::ready);
        require(chains.acquire_eye(1)==ImageWait::ready);
        const auto prior_acquires=acquires;
        require(chains.cancel_frame()==ImageWait::ready && acquires==prior_acquires);
        require(!chains.projection() && chains.start_frame(frame,space));
        chains.close();require(destroys==2);
        chains.close();require(destroys==2);
        creates=destroys=0;fail_second=true;
        require(!chains.initialize(handle<XrSession>(1),config,preferences));
        require(destroys==1 && chains.handle(0)==XR_NULL_HANDLE && chains.handle(1)==XR_NULL_HANDLE);
        quad_mode=true;fail_second=fail_release=bad_index=timeout=false;
        OpenXrQuad quad(api);require(quad.initialize(handle<XrSession>(1),43));
        require(quad.image_count()==3 && !quad.layer(space,{}));
        const auto prior=acquires;timeout=true;
        require(quad.acquire()==ImageWait::waiting && !quad.image_index());
        require(quad.acquire()==ImageWait::waiting && acquires==prior+1);
        require(quad.cancel()==ImageWait::waiting);
        timeout=false;require(quad.acquire()==ImageWait::ready && quad.image_index()==1);
        require(!quad.layer(space,{}));fail_release=true;require(!quad.release());
        fail_release=false;require(quad.release());
        XrPosef pose{};pose.orientation.w=1;pose.position.z=-1.75F;
        const auto* panel=quad.layer(space,pose);
        require(panel && panel->eyeVisibility==XR_EYE_VISIBILITY_BOTH && panel->space==space
            && panel->size.width==1.15F && panel->pose.position.z==-1.75F && panel->subImage.imageRect.extent.height==896);
        bad_index=true;require(quad.acquire()==ImageWait::error && !quad.image_index() && !quad.layer(space,pose));
        require(quad.cancel()==ImageWait::ready && !quad.layer(space,pose));bad_index=false;
        require(quad.acquire()==ImageWait::ready && quad.cancel()==ImageWait::ready && !quad.layer(space,pose));
        std::cout<<"OpenXR swapchain lifecycle tests passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
