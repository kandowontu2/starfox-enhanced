#pragma once
#include "starfox/render/shadow_scene.hpp"
#include "starfox/render/framebuffer.hpp"
#include <optional>

namespace starfox::render {
struct VolumetricMedium {
    double extinction{.00006}; // Inverse scene units, not opacity per frame.
    shadows::Vec3 albedo{.9,.95,1};
    // Linear radiance, not display-white RGB. Unit sunlight with a forward
    // phase lobe clips the LDR compositor even before surface contribution.
    shadows::Vec3 ambient{.12,.15,.2};
    shadows::Vec3 sunlight{.18,.17,.15};
    double anisotropy{.35};
    double maximum_distance{8192};
    unsigned samples{32};
};
struct VolumetricIntegral {
    double transmittance{1};
    shadows::Vec3 scattering{}; // Linear RGB, premultiplied by integrated opacity.
};
inline VolumetricMedium volumetric_fog_medium(unsigned quality) {
    VolumetricMedium result;
    quality=std::min(quality,3U);
    constexpr double extinction[]{0,.00003,.00006,.0001};
    constexpr unsigned samples[]{1,8,16,32};
    result.extinction=extinction[quality];result.samples=samples[quality];
    return result;
}
struct VolumetricProjection {double focal_x{},focal_y{},center_x{},center_y{};};
struct VolumetricPixel {
    double view_depth{}; // Axial camera Z; zero denotes sky, not a near surface.
    bool eligible{}; // Explicit world ownership: false protects HUD/portraits.
};
struct VolumetricGround {shadows::Vec3 point,normal;};
// Reference depth acquisition from the same camera-space scene used for
// occlusion. The nearest model or ground intersection terminates the volume.
inline bool build_volumetric_guides(const shadows::Scene& scene,
    VolumetricProjection projection,unsigned width,unsigned height,
    std::span<const std::uint8_t> world_coverage,double maximum_distance,
    std::optional<VolumetricGround> ground,std::vector<VolumetricPixel>& output,bool background_only=false) {
    using namespace shadows;
    const auto finite=[](Vec3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);};
    if(!width||!height||world_coverage.size()!=std::size_t(width)*height
        || !std::isfinite(maximum_distance)||maximum_distance<=0
        || !std::isfinite(projection.focal_x)||projection.focal_x<=0
        || !std::isfinite(projection.focal_y)||projection.focal_y<=0
        || !std::isfinite(projection.center_x)||!std::isfinite(projection.center_y)) return false;
    if(ground && (!finite(ground->point)||!finite(ground->normal)
        || !std::isfinite(dot(ground->normal,ground->normal))||dot(ground->normal,ground->normal)<1e-20)) return false;
    std::vector<VolumetricPixel> result(world_coverage.size());
    for(unsigned y=0;y<height;++y) for(unsigned x=0;x<width;++x) {
        const auto i=std::size_t(y)*width+x;
        if(!world_coverage[i]) continue;
        const Vec3 ray{(x+.5-projection.center_x)/projection.focal_x,
            (y+.5-projection.center_y)/projection.focal_y,1};
        const double length=std::sqrt(dot(ray,ray));
        if(!std::isfinite(length)) return false;
        // Trace the axial ray directly. Normalizing and then dividing its hit
        // back to Z adds rounding at shared triangle edges unnecessarily.
        // Underlays continue to ground/sky behind foreground geometry. The
        // original scene is still supplied to light-visibility integration.
        const auto hit=background_only?std::optional<double>{}:scene.nearest({},ray,0,maximum_distance/length);
        double z=hit?*hit:0;
        if(ground) {
            const double denominator=dot(ray,ground->normal);
            if(std::abs(denominator)>1e-12) {
                const double plane_z=dot(ground->point,ground->normal)/denominator;
                if(std::isfinite(plane_z)&&plane_z>0&&plane_z*length<=maximum_distance
                    && (!hit||plane_z<z)) z=plane_z;
            }
        }
        result[i]={z,true};
    }
    output=std::move(result);return true;
}
// Homogeneous single scattering along a finite view segment. Every sample
// tests visibility toward the directional light against actual scene geometry.
// The caller supplies the first surface distance, or a bounded sky distance.
// There is no temporal randomness or dependence on presentation frame rate.
inline std::optional<VolumetricIntegral> integrate_volumetric_fog(
    const VolumetricMedium& medium,shadows::Vec3 origin,shadows::Vec3 view,
    double surface_distance,shadows::Vec3 toward_light,const shadows::Scene& scene,
    const shadows::Scene::DirectionQuery* light_query=nullptr) {
    using namespace shadows;
    const auto finite=[](Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);};
    const auto positive=[&](Vec3 v){return finite(v)&&v.x>=0&&v.y>=0&&v.z>=0;};
    if(!finite(origin)||!finite(view)||!finite(toward_light)
        || !positive(medium.albedo)||medium.albedo.x>1||medium.albedo.y>1||medium.albedo.z>1
        || !positive(medium.ambient)||!positive(medium.sunlight)
        || !std::isfinite(medium.extinction)||medium.extinction<0
        || !std::isfinite(medium.anisotropy)||std::abs(medium.anisotropy)>.9
        || !std::isfinite(medium.maximum_distance)||medium.maximum_distance<=0
        || !std::isfinite(surface_distance)||surface_distance<0
        || medium.samples<1||medium.samples>128) return std::nullopt;
    const double view_length=std::sqrt(dot(view,view)),light_length=std::sqrt(dot(toward_light,toward_light));
    if(!std::isfinite(view_length)||view_length<1e-12||!std::isfinite(light_length)||light_length<1e-12) return std::nullopt;
    view=view*(1/view_length);toward_light=toward_light*(1/light_length);
    VolumetricIntegral result;
    const double distance=std::min(surface_distance,medium.maximum_distance);
    if(distance==0||medium.extinction==0) return result;
    const double step=distance/medium.samples;
    const double attenuation=std::exp(-medium.extinction*step);
    const double opacity=-std::expm1(-medium.extinction*step);
    const double g=medium.anisotropy,cosine=std::clamp(dot(view,toward_light),-1.,1.);
    // Henyey-Greenstein phase relative to isotropic scattering (g=0 => 1).
    const double phase=(1-g*g)/std::pow(1+g*g-2*g*cosine,1.5);
    const bool direct=medium.sunlight.x!=0||medium.sunlight.y!=0||medium.sunlight.z!=0;
    if(!direct || scene.triangle_count()==0) {
        // Exact homogeneous solution when visibility cannot vary. Avoid all
        // sample traversal for ambient-only fog and empty scenes.
        result.transmittance=std::exp(-medium.extinction*distance);
        const double weight=-std::expm1(-medium.extinction*distance);
        const auto light=medium.ambient+medium.sunlight*phase;
        result.scattering={light.x*medium.albedo.x*weight,light.y*medium.albedo.y*weight,light.z*medium.albedo.z*weight};
        return result;
    }
    for(unsigned i=0;i<medium.samples;++i) {
        const auto point=origin+view*((i+.5)*step);
        const double visible=scene.occluded(point,toward_light,.01,65536.,nullptr,light_query)?0.:1.;
        const auto light=medium.ambient+medium.sunlight*(visible*phase);
        const double weight=result.transmittance*opacity;
        result.scattering=result.scattering+Vec3{light.x*medium.albedo.x,light.y*medium.albedo.y,light.z*medium.albedo.z}*weight;
        result.transmittance*=attenuation;
    }
    // Extinction is independent of visibility and sample count. Use its
    // closed form rather than exposing repeated-multiply rounding to callers.
    result.transmittance=std::exp(-medium.extinction*distance);
    return result;
}
// Linear RGBA reference compositor. Axial depth must be converted to ray
// distance off-axis; using Z directly would thin fog toward screen corners.
// Invalid input leaves output untouched; input/output aliasing is supported.
inline bool render_volumetric_fog(const VolumetricMedium& medium,
    VolumetricProjection projection,unsigned width,unsigned height,
    std::span<const std::array<float,4>> source,std::span<const VolumetricPixel> guides,
    const shadows::Scene& scene,shadows::Vec3 toward_light,
    std::vector<std::array<float,4>>& output) {
    using namespace shadows;
    const auto count=std::size_t(width)*height;
    if(!width||!height||source.size()!=count||guides.size()!=count
        || !std::isfinite(projection.focal_x)||projection.focal_x<=0
        || !std::isfinite(projection.focal_y)||projection.focal_y<=0
        || !std::isfinite(projection.center_x)||!std::isfinite(projection.center_y)) return false;
    if(!integrate_volumetric_fog(medium,{},{0,0,1},0,toward_light,scene)) return false;
    const auto light_query=scene.prepare_direction(toward_light*(1/std::sqrt(dot(toward_light,toward_light))));
    std::vector<std::array<float,4>> result(source.begin(),source.end());
    for(unsigned y=0;y<height;++y) for(unsigned x=0;x<width;++x) {
        const auto i=std::size_t(y)*width+x;
        if(!guides[i].eligible) continue;
        const double z=guides[i].view_depth;
        if(!std::isfinite(z)||z<0) return false;
        Vec3 ray{(x+.5-projection.center_x)/projection.focal_x,
            (y+.5-projection.center_y)/projection.focal_y,1};
        const double length=std::sqrt(dot(ray,ray));
        const double distance=z==0?medium.maximum_distance:std::min(medium.maximum_distance,z*length);
        const auto fog=integrate_volumetric_fog(medium,{},ray,distance,toward_light,scene,&light_query);
        if(!fog) return false;
        const double scatter[]{fog->scattering.x,fog->scattering.y,fog->scattering.z};
        for(unsigned c=0;c<3;++c) {
            if(!std::isfinite(source[i][c])) return false;
            result[i][c]=float(source[i][c]*fog->transmittance+scatter[c]);
        }
        // Alpha and protected pixels retain their exact original values.
    }
    output=std::move(result);return true;
}
inline bool apply_volumetric_fog(const VolumetricMedium& medium,VolumetricProjection projection,
    const Framebuffer& frame,const shadows::Scene& scene,shadows::Vec3 light,
    std::optional<VolumetricGround> ground,std::vector<std::uint8_t>& rgba,
    std::span<const std::uint8_t> protected_pixels={}) {
    if(!frame.layer_tags_enabled()||rgba.size()!=frame.pixels().size()*4
        ||(!protected_pixels.empty()&&protected_pixels.size()!=frame.pixels().size())) return false;
    std::vector<std::uint8_t> coverage(frame.pixels().size());
    std::vector<std::array<float,4>> linear(coverage.size()),fogged;
    const auto decode=[](float v){return v<=.04045f?v/12.92f:std::pow((v+.055f)/1.055f,2.4f);};
    for(std::size_t i=0;i<coverage.size();++i) {
        coverage[i]=frame.layer_tags()[i]!=std::uint8_t(PixelLayer::two_d)&&rgba[i*4+3]!=0
            &&(protected_pixels.empty()||!protected_pixels[i]);
        for(unsigned c=0;c<3;++c) linear[i][c]=decode(rgba[i*4+c]/255.f);
        linear[i][3]=rgba[i*4+3]/255.f;
    }
    std::vector<VolumetricPixel> guides;
    if(!build_volumetric_guides(scene,projection,frame.stored_width(),frame.stored_height(),coverage,medium.maximum_distance,ground,guides)
        || !render_volumetric_fog(medium,projection,frame.stored_width(),frame.stored_height(),linear,guides,scene,light,fogged)) return false;
    for(std::size_t i=0;i<coverage.size();++i) if(coverage[i]) for(unsigned c=0;c<3;++c) {
        const float v=std::clamp(fogged[i][c],0.f,1.f);
        const float encoded=v<=.0031308f?v*12.92f:1.055f*std::pow(v,1.f/2.4f)-.055f;
        rgba[i*4+c]=std::uint8_t(std::lround(std::clamp(encoded,0.f,1.f)*255));
    }
    return true;
}
}
