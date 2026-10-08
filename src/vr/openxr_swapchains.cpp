#include "starfox/vr/openxr_swapchains.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <vector>
namespace starfox::vr {
namespace {
void check(XrResult result,const char* op) {
    if(XR_FAILED(result)) throw std::runtime_error(std::string(op)+": OpenXR result "+std::to_string(result));
}
}
OpenXrSwapchains::~OpenXrSwapchains() {close();}
void OpenXrSwapchains::close() noexcept {
    for(auto& eye:eyes_) {
        if(eye.handle!=XR_NULL_HANDLE) api_.destroy(eye.handle);
        eye={};
    }
    frame_=false;format_=0;layer_={XR_TYPE_COMPOSITION_LAYER_PROJECTION};
}
bool OpenXrSwapchains::initialize(XrSession session,std::span<const XrViewConfigurationView> config,
    std::span<const int64_t> preferred,XrSwapchainUsageFlags additional_usage) {
    close();
    try {
        if(session==XR_NULL_HANDLE || config.size()!=2 || preferred.empty())
            throw std::runtime_error("Missing stereo session/views/formats");
        uint32_t count{};check(api_.formats(session,0,&count,nullptr),"Count swapchain formats");
        if(!count || count>1024) throw std::runtime_error("Invalid swapchain format count");
        std::vector<int64_t> formats(count);
        check(api_.formats(session,count,&count,formats.data()),"Read swapchain formats");
        if(!count || count>formats.size()) throw std::runtime_error("Swapchain format enumeration changed");
        formats.resize(count);
        const auto selected=std::find_if(preferred.begin(),preferred.end(),[&](auto value) {
            return std::find(formats.begin(),formats.end(),value)!=formats.end();
        });
        if(selected==preferred.end()) throw std::runtime_error("No compatible eye colour format");
        format_=*selected;
        for(unsigned i=0;i<2;++i) {
            const auto& view=config[i];
            if(!view.recommendedImageRectWidth || !view.recommendedImageRectHeight
                || view.recommendedImageRectWidth>view.maxImageRectWidth
                || view.recommendedImageRectHeight>view.maxImageRectHeight
                || view.recommendedImageRectWidth>INT32_MAX || view.recommendedImageRectHeight>INT32_MAX
                || !view.maxSwapchainSampleCount)
                throw std::runtime_error("Invalid recommended eye dimensions");
            XrSwapchainCreateInfo create{XR_TYPE_SWAPCHAIN_CREATE_INFO};
            create.usageFlags=XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT|XR_SWAPCHAIN_USAGE_SAMPLED_BIT|additional_usage;
            create.format=format_;create.sampleCount=1;
            create.width=view.recommendedImageRectWidth;create.height=view.recommendedImageRectHeight;
            create.faceCount=create.arraySize=create.mipCount=1;
            check(api_.create(session,&create,&eyes_[i].handle),"Create eye swapchain");
            check(api_.images(eyes_[i].handle,0,&eyes_[i].count,nullptr),"Count eye images");
            if(!eyes_[i].count || eyes_[i].count>64) throw std::runtime_error("Invalid eye image count");
            views_[i]={XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
            views_[i].subImage.swapchain=eyes_[i].handle;
            views_[i].subImage.imageRect.extent={static_cast<int32_t>(create.width),static_cast<int32_t>(create.height)};
        }
        status_="OpenXR eye swapchains ready";return true;
    } catch(const std::exception& e) {status_=e.what();close();return false;}
}
bool OpenXrSwapchains::enumerate_images(unsigned eye,uint32_t capacity,uint32_t* count,XrSwapchainImageBaseHeader* images) {
    if(eye>=2 || eyes_[eye].handle==XR_NULL_HANDLE || !count || (capacity && !images)) {
        status_="Invalid eye image enumeration";return false;
    }
    const auto result=api_.images(eyes_[eye].handle,capacity,count,images);
    if(XR_FAILED(result)) {status_="Enumerate eye images: OpenXR result "+std::to_string(result);return false;}
    return true;
}
bool OpenXrSwapchains::start_frame(const StereoFrame& frame,XrSpace space) {
    for(const auto& eye:eyes_) if(eye.acquired) {status_="Previous eye image is still acquired";return false;}
    frame_=false;
    for(auto& eye:eyes_) eye.released=false;
    if(space==XR_NULL_HANDLE || !frame.should_render || eyes_[0].handle==XR_NULL_HANDLE || eyes_[1].handle==XR_NULL_HANDLE)
        return false;
    for(unsigned i=0;i<2;++i) {views_[i].pose=frame.views[i].pose;views_[i].fov=frame.views[i].fov;}
    layer_.space=space;layer_.viewCount=2;layer_.views=views_.data();frame_=true;return true;
}
ImageWait OpenXrSwapchains::acquire_eye(unsigned index,XrDuration timeout) {
    if(index>=2 || !frame_ || timeout<0) {status_="Invalid eye acquisition";return ImageWait::error;}
    auto& eye=eyes_[index];
    if(eye.released) {status_="Eye already rendered this frame";return ImageWait::error;}
    try {
        if(!eye.acquired) {
            XrSwapchainImageAcquireInfo acquire{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
            check(api_.acquire(eye.handle,&acquire,&eye.index),"Acquire eye image");
            eye.acquired=true;
        }
        // A bad runtime index must remain rejected on retries, not become a
        // writable image merely because acquisition already succeeded.
        if(eye.index>=eye.count) throw std::runtime_error("Eye image index outside enumerated images");
        if(!eye.ready) {
            XrSwapchainImageWaitInfo wait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};wait.timeout=timeout;
            const auto result=api_.wait(eye.handle,&wait);
            if(result==XR_TIMEOUT_EXPIRED) return ImageWait::waiting;
            check(result,"Wait for eye image");eye.ready=true;
        }
        return ImageWait::ready;
    } catch(const std::exception& e) {status_=e.what();return ImageWait::error;}
}
bool OpenXrSwapchains::release_eye(unsigned index) {
    if(index>=2 || !eyes_[index].acquired || !eyes_[index].ready) {status_="Eye image was not ready for release";return false;}
    auto& eye=eyes_[index];XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
    const auto result=api_.release(eye.handle,&release);
    if(XR_FAILED(result)) {status_="Release eye image: OpenXR result "+std::to_string(result);return false;}
    eye.acquired=eye.ready=false;eye.released=true;return true;
}
std::optional<uint32_t> OpenXrSwapchains::image_index(unsigned index) const noexcept {
    return index<2 && eyes_[index].acquired && eyes_[index].ready
        ?std::optional<uint32_t>(eyes_[index].index):std::nullopt;
}
ImageWait OpenXrSwapchains::cancel_frame(XrDuration timeout) {
    frame_=false;
    if(timeout<0) {status_="Invalid cancellation timeout";return ImageWait::error;}
    auto outcome=ImageWait::ready;
    for(unsigned index=0;index<eyes_.size();++index) {
        auto& eye=eyes_[index];
        if(!eye.acquired) continue;
        if(!eye.ready) {
            XrSwapchainImageWaitInfo wait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
            wait.timeout=timeout;
            const auto result=api_.wait(eye.handle,&wait);
            if(result==XR_TIMEOUT_EXPIRED) {
                if(outcome!=ImageWait::error) outcome=ImageWait::waiting;
                continue;
            }
            if(XR_FAILED(result)) {
                status_="Cancel eye wait: OpenXR result "+std::to_string(result);
                outcome=ImageWait::error;continue;
            }
            eye.ready=true;
        }
        if(!release_eye(index)) outcome=ImageWait::error;
    }
    return outcome;
}
const XrCompositionLayerProjection* OpenXrSwapchains::projection() const noexcept {
    return frame_ && eyes_[0].released && eyes_[1].released?&layer_:nullptr;
}
}
