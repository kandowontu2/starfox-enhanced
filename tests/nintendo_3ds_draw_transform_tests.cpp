#include "starfox/platform/nintendo_3ds/pica_draw_transform.hpp"
#include <iostream>
#include <stdexcept>

namespace {
using namespace starfox::platform::nintendo_3ds;
unsigned checks{};
void require(bool value,const char* message) {
    ++checks;if(!value)throw std::runtime_error(message);
}
bool same(const PicaMatrix& a,const PicaMatrix& b) {
    return std::memcmp(a.data(),b.data(),sizeof(a))==0;
}
FramePlan plan(bool stereo) {
    return plan_frame(stereo?1.F:0.F,true,ScreenUse::world);
}
void repeated_models() {
    const auto frame=plan(true);
    PicaDrawTransform cache;PicaDraw draw{};draw.space=PicaSpace::world;
    PicaMatrix gpu{};unsigned computed{},uploaded{},draws{};
    for(unsigned eye=0;eye<frame.eye_count;++eye) {
        cache.reset(); // The native backdrop/eye target has its own uniform.
        for(unsigned i=0;i<128;++i) {
            const auto reference=pica_draw_matrix(frame,eye,draw);
            cache.prepare(draw,[&]{++computed;return pica_draw_matrix(frame,eye,draw);},
                [&](const auto& next){++uploaded;gpu=next;});
            require(same(gpu,reference),"Repeated stereo draw lost its exact source transform");
            ++draws;
        }
    }
    require(draws==256 && computed==2 && uploaded==2,
        "Redundant transforms were not removed while retaining both eyes/all draws");
}
void mixed_draws() {
    for(bool stereo:{false,true}) {
        const auto frame=plan(stereo);PicaDrawTransform cache;PicaMatrix gpu{};
        for(unsigned eye=0;eye<frame.eye_count;++eye) {
            cache.reset();
            for(unsigned i=0;i<300;++i) {
                PicaDraw draw{};
                draw.space=std::array{PicaSpace::world,PicaSpace::scenery,PicaSpace::screen}[(i/3)%3];
                draw.model[0][3]=float((i/9)%5);
                draw.model[1][1]=float((i/15)%3+1);
                const auto reference=pica_draw_matrix(frame,eye,draw);
                cache.prepare(draw,[&]{return pica_draw_matrix(frame,eye,draw);},
                    [&](const auto& next){gpu=next;});
                require(same(gpu,reference),"Changed model/space or LCD eye reused a stale transform");
            }
        }
    }
}
void failure_and_reset() {
    const auto frame=plan(false);PicaDrawTransform cache;PicaDraw first{},next{};
    first.space=next.space=PicaSpace::world;next.model[0][3]=25;
    PicaMatrix gpu{};unsigned uploads{};
    const auto submit=[&](const auto& matrix){++uploads;gpu=matrix;};
    cache.prepare(first,[&]{return pica_draw_matrix(frame,0,first);},submit);
    bool upload_failed{};
    try {
        cache.prepare(next,[&]{return pica_draw_matrix(frame,0,next);},
            [&](const auto& matrix){gpu=matrix;throw std::runtime_error("partial upload");});
    } catch(const std::runtime_error&) {upload_failed=true;}
    require(upload_failed,"Upload failure was swallowed");
    require(!cache.valid(),"Failed upload retained a valid transform key");
    require(cache.prepare(first,[&]{return pica_draw_matrix(frame,0,first);},submit)
        && same(gpu,pica_draw_matrix(frame,0,first)),"Old source key did not repair a partial upload");
    bool compute_failed{};
    try {
        cache.prepare(next,[]{throw std::runtime_error("compute failure");return PicaMatrix{};},submit);
    } catch(const std::runtime_error&) {compute_failed=true;}
    require(compute_failed,"Calculation failure was swallowed");
    require(!cache.valid(),"Failed calculation retained a valid transform key");
    cache.prepare(first,[&]{return pica_draw_matrix(frame,0,first);},submit);
    cache.reset();
    require(cache.prepare(first,[&]{return pica_draw_matrix(frame,0,first);},submit),
        "New frame/program/eye scope did not force its first upload");
    next=first;next.model[0][1]=-0.F;
    require(cache.prepare(next,[&]{return pica_draw_matrix(frame,0,next);},submit),
        "Object-representation comparison lost a signed-zero model change");
    require(uploads==5,"Failure/reset sequence emitted an unexpected transform count");
}
void changed_frame_scope() {
    PicaDrawTransform cache;PicaDraw draw{};draw.space=PicaSpace::world;
    PicaMatrix gpu{};unsigned uploads{};
    for(float slider:{0.F,.25F,1.F}) {
        StereoSettings settings{};
        settings.convergence=256.F+256.F*slider;
        const auto frame=plan_frame(slider,true,ScreenUse::world,settings);
        for(unsigned eye=0;eye<frame.eye_count;++eye) {
            cache.reset();
            require(cache.prepare(draw,[&]{return pica_draw_matrix(frame,eye,draw);},
                [&](const auto& matrix){gpu=matrix;++uploads;}),
                "Changed frame/slider/convergence did not upload its transform");
            require(same(gpu,pica_draw_matrix(frame,eye,draw)),
                "Changed frame/slider/convergence retained an old transform");
        }
    }
    require(uploads==5,"Changed plan/eye count differs from the native presenter");
}
}
int main() try {
    repeated_models();mixed_draws();failure_and_reset();changed_frame_scope();
    std::cout<<"3DS draw transforms: "<<checks<<" checks; exact matrices for all draws/eyes, changed spaces/models, reset and failure repair\n";
    return 0;
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
