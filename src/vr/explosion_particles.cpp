#include "starfox/vr/explosion_particles.hpp"
#include "starfox/simulation/math.hpp"
#include "starfox/timing/fixed_step.hpp"
#include <algorithm>
#include <cmath>
namespace starfox::vr {
namespace {
using V3=std::array<double,3>;
constexpr std::size_t particle_limit=4096;
// Cartridge world: Y down, so up is -Y.
constexpr V3 up{0,-1,0};
struct Random {
    std::uint64_t state;
    std::uint64_t next() noexcept {
        std::uint64_t z=(state+=0x9e3779b97f4a7c15ULL);
        z=(z^(z>>30U))*0xbf58476d1ce4e5b9ULL;z=(z^(z>>27U))*0x94d049bb133111ebULL;
        return z^(z>>31U);
    }
    double uniform() noexcept {return double(next()>>11U)*(1./9007199254740992.);}
    double range(double a,double b) noexcept {return a+(b-a)*uniform();}
    V3 direction() noexcept {
        const double z=range(-1,1),a=range(0,6.283185307179586),r=std::sqrt(std::max(0.,1-z*z));
        return {r*std::cos(a),r*std::sin(a),z};
    }
};
V3 add(V3 a,const V3& b,double s=1) noexcept {for(unsigned i=0;i<3;++i) a[i]+=b[i]*s;return a;}
V3 scaled(V3 a,double s) noexcept {for(auto& c:a) c*=s;return a;}
double wrap(double d) noexcept {
    d=std::fmod(d,65536.);if(d>=32768.) d-=65536.;else if(d<-32768.) d+=65536.;return d;
}
float smooth(float a,float b,float x) noexcept {
    const float t=std::clamp((x-a)/(b-a),0.F,1.F);return t*t*(3-2*t);
}
// Star Fox's explosion inks, dark red through orange and yellow to white
// (sRGB), by heat 0..1.
std::array<float,3> fire_ramp(float heat) noexcept {
    struct Stop {float at;std::array<float,3> colour;};
    constexpr Stop stops[]{{0.F,{.30F,.04F,.02F}},{.3F,{.85F,.20F,.04F}},{.55F,{1.F,.50F,.08F}},
        {.78F,{1.F,.84F,.30F}},{1.F,{1.F,1.F,.88F}}};
    heat=std::clamp(heat,0.F,1.F);
    for(std::size_t i=1;i<std::size(stops);++i) if(heat<=stops[i].at) {
        const float t=(heat-stops[i-1].at)/(stops[i].at-stops[i-1].at);
        std::array<float,3> c{};
        for(unsigned k=0;k<3;++k) c[k]=stops[i-1].colour[k]+(stops[i].colour[k]-stops[i-1].colour[k])*t;
        return c;
    }
    return stops[std::size(stops)-1].colour;
}
// A boost flame: deep blue through cyan to a white core (sRGB), by heat 0..1.
std::array<float,3> boost_ramp(float heat) noexcept {
    struct Stop {float at;std::array<float,3> colour;};
    constexpr Stop stops[]{{0.F,{.02F,.05F,.30F}},{.4F,{.08F,.32F,1.F}},{.72F,{.42F,.80F,1.F}},{1.F,{.94F,.98F,1.F}}};
    heat=std::clamp(heat,0.F,1.F);
    for(std::size_t i=1;i<std::size(stops);++i) if(heat<=stops[i].at) {
        const float t=(heat-stops[i-1].at)/(stops[i].at-stops[i-1].at);
        std::array<float,3> c{};
        for(unsigned k=0;k<3;++k) c[k]=stops[i-1].colour[k]+(stops[i].colour[k]-stops[i-1].colour[k])*t;
        return c;
    }
    return stops[std::size(stops)-1].colour;
}
float linear(float c) noexcept {return c<=.04045F?c/12.92F:std::pow((c+.055F)/1.055F,2.4F);}
}
void ExplosionParticles::reset() noexcept {
    tracks_.clear();particles_.clear();anchored_.clear();
    epoch_.reset();revision_.reset();
}
std::optional<std::array<double,4>> ExplosionParticles::place(const Particle& p,double time) noexcept {
    const double age=time-p.birth;
    if(age<0 || age>=p.life) return std::nullopt;
    // dv/dt = pull - drag v: exact, so every display rate agrees.
    const double k=p.drag;
    const double carried=k>1e-6?(1-std::exp(-k*age))/k:age;
    const double pulled=k>1e-6?(age-carried)/k:age*age/2;
    std::array<double,4> out{};
    for(unsigned i=0;i<3;++i) out[i]=p.origin[i]+p.velocity[i]*carried+p.pull[i]*pulled;
    const double t=age/p.life;
    out[3]=p.size0+(p.size1-p.size0)*(1-(1-t)*(1-t));
    return out;
}
void ExplosionParticles::spawn(std::uint64_t key,Track& track,const SourceExplosionEmitter& emitter,std::uint64_t tick,double birth) {
    Random random{key*0x2545f4914f6cdd1dULL^tick*0x9e3779b97f4a7c15ULL};
    const double h=std::max(emitter.frame.half_size,1.);
    const auto puff=[&](Style style,V3 at,V3 velocity,double size0,double size1,double life,double drag,double rise,float heat,float opacity) {
        Particle p;
        p.origin=at;p.velocity=velocity;p.pull=scaled(up,rise);
        p.birth=birth;p.life=life;p.size0=float(size0);p.size1=float(size1);p.drag=float(drag);
        p.heat=heat;p.opacity=opacity;p.style=style;p.seed=std::uint8_t(random.next());
        particles_.push_back(p);
    };
    // Born when the tick began to show (whatever the display rate), where
    // the emitter was then.
    const V3 moving=track.velocity,at=add(track.world,moving,-(time_-birth));
    const double age=time_-track.first_seen;
    // How it throws off particles follows the object (its shape); a large
    // frame that keeps looping is a standing fire too (BONFIRE).
    auto source=emitter.source;
    if(source==ExplosionSource::explosion && track.looped) source=ExplosionSource::fire;
    switch(source) {
    case ExplosionSource::fire:
        {
            // A standing fire (BONFIRE, FIRE): flames lick upward, a little smoke.
            for(int i=0;i<3;++i) {
                auto d=random.direction();d[1]=std::abs(d[1])*.5;
                puff(Style::fire,add(at,d,h*.35),add(scaled(up,h*random.range(.05,.1)),moving,.5),
                    h*random.range(.3,.4),h*.1,random.range(5,9),.3,h*.01,float(random.range(.8,.95)),.55F);
            }
            if(tick%2U==0U)
                puff(Style::smoke,add(at,up,h*.6),add(scaled(up,h*.04),moving,.3),h*.35,h*.9,random.range(18,26),.15,h*.006,.7F,.35F);
        }
        break;
    case ExplosionSource::explosion:
        if(emitter.frame.kind==render::ExplosionSpriteKind::small && !track.burst) {
            // A small explosion from the start: a smaller burst.
            track.burst=true;
            for(int i=0;i<10;++i) {
                const auto d=random.direction();
                puff(Style::fire,add(at,d,h*.1),add(scaled(d,h*random.range(.1,.25)),moving,.6),
                    h*random.range(.3,.4),h*.55,random.range(5,9),.35,h*.01,float(random.range(.7,.9)),.35F);
            }
            for(int i=0;i<5;++i)
                puff(Style::fire,at,add(scaled(random.direction(),h*random.range(.3,.5)),moving,.6),h*.07,h*.05,random.range(5,8),.12,-h*.02,1.F,1.F);
        }
        if(!track.burst) {
            track.burst=true;
            // The fireball: a burst of hot puffs, then sparks.
            for(int i=0;i<18;++i) {
                const auto d=random.direction();
                puff(Style::fire,add(at,d,h*.15),add(scaled(d,h*random.range(.1,.28)),moving,.6),
                    h*random.range(.3,.45),h*random.range(.6,.8),random.range(7,13),.35,h*.01,float(random.range(.7,.9)),.35F);
            }
            for(int i=0;i<10;++i) {
                const auto d=random.direction();
                puff(Style::fire,at,add(scaled(d,h*random.range(.35,.6)),moving,.6),
                    h*.06,h*.04,random.range(6,10),.12,-h*.02,1.F,1.F);
            }
        }
        if(age>=3 || emitter.frame.frame>=3U) for(int i=0,n=1+int(random.next()%2U);i<n;++i)
            puff(Style::smoke,add(at,random.direction(),h*.4),add(scaled(random.direction(),h*.03),moving,.3),
                h*.35,h*.9,random.range(22,34),.2,h*.008,.6F,.5F);
        break;
    case ExplosionSource::smoke:
        // Its first frames are a flash (a large, then a small explosion
        // frame): a few hot puffs, no fireball.
        if(!track.burst && emitter.frame.kind!=render::ExplosionSpriteKind::smoke) {
            track.burst=true;
            for(int i=0;i<3;++i) {
                const auto d=random.direction();
                puff(Style::fire,add(at,d,h*.1),add(scaled(d,h*.08),moving,.5),h*.35,h*.5,random.range(3,5),.35,0,.85F,.35F);
            }
        }
        // Each smoke sprite sheds a thinner puff that hangs behind it.
        puff(Style::smoke,add(at,random.direction(),h*.3),add(add(scaled(random.direction(),h*.02),moving,.5),up,h*.02),
            h*.4,h*.9,random.range(14,22),.2,h*.004,.15F,.4F);
        break;
    case ExplosionSource::boost:
        // The jet: blue puffs that keep most of the ship's speed, so even a
        // ship boosting away from the camera draws a short streak behind
        // it that cools and shrinks, not puffs falling back to the camera.
        for(int i=0;i<3;++i)
            puff(Style::boost,add(at,random.direction(),h*.25),add(scaled(random.direction(),h*.03),moving,.85),
                h*random.range(.45,.6),h*.1,random.range(2.5,4.5),.05,0,float(random.range(.55,.75)),.5F);
        break;
    case ExplosionSource::fireball:
        for(int i=0;i<3;++i)
            puff(Style::fire,add(at,random.direction(),h*.2),add(scaled(random.direction(),h*.04),moving,.1),
                h*.5,h*.15,random.range(3,6),.3,0,float(random.range(.85,1.)),.6F);
        break;
    }
}
void ExplosionParticles::update(const GameSceneSnapshot& previous,const GameSceneSnapshot& current,double alpha,
    std::span<const SourceExplosionEmitter> emitters) {
    if(!std::isfinite(alpha)) alpha=1;
    alpha=std::clamp(alpha,0.,1.);
    // As interpolate_scene_poses: the emitters' camera-space positions came
    // from this camera.
    if(previous.flow!=current.flow || timing::camera_transform_is_discontinuous(previous.camera,current.camera)) alpha=1;
    const auto camera=timing::interpolate(previous.camera,current.camera,alpha);
    const auto view=simulation::interpolate_rotation_matrix_q15(previous.view_matrix,current.view_matrix,alpha);
    camera_={camera.x,camera.y,camera.z};
    for(unsigned i=0;i<9;++i) view_[i]=view[i]/32768.;
    brightness_=std::clamp(float(current.display_brightness)/15.F,0.F,1.F);
    // Source ticks (20 Hz), interpolated.
    const std::uint64_t ticks=current.revision>previous.revision?current.revision-previous.revision:0;
    const double now=ticks>0 && ticks<=4?double(previous.revision)+alpha*double(ticks):double(current.revision);
    if(epoch_!=current.scene_epoch || (revision_ && (now<time_-.01 || now>time_+120))) {
        tracks_.clear();particles_.clear();
    }
    epoch_=current.scene_epoch;
    time_=now;
    const bool new_tick=!revision_ || *revision_!=current.revision;
    // A tick's particles are born when it starts to show: the interpolation
    // from the previous tick begins.
    const double birth=ticks>0 && ticks<=4?double(previous.revision):time_;
    revision_=current.revision;
    anchored_.clear();
    for(const auto& emitter:emitters) {
        // Source camera space back to the cartridge's world (the view is a
        // rotation: its transpose inverts it).
        const auto& c=emitter.position;
        V3 world;
        for(unsigned j=0;j<3;++j) world[j]=camera_[j]+c[0]*view_[3*j]+c[1]*view_[3*j+1]+c[2]*view_[3*j+2];
        const std::uint64_t key=std::uint64_t(emitter.handle)|(emitter.generation<<16U);
        auto [found,inserted]=tracks_.try_emplace(key);
        auto& track=found->second;
        if(inserted) {track.first_seen=time_;track.world=world;track.frame=emitter.frame.frame;}
        else {
            const double dt=time_-track.last_seen;
            if(dt>.2) for(unsigned j=0;j<3;++j) track.velocity[j]=wrap(world[j]-track.world[j])/dt;
            if(emitter.frame.frame<track.frame) track.looped=true;
        }
        track.world=world;track.last_seen=time_;track.frame=emitter.frame.frame;
        if(new_tick && track.spawned_tick!=current.revision) {
            track.spawned_tick=current.revision;
            spawn(key,track,emitter,current.revision,birth);
        }
        // The anchored cluster: fixed 3D offsets per emitter, a new look per
        // animation frame (the cartridge's own frame rate).
        using Kind=render::ExplosionSpriteKind;
        const double h=std::max(emitter.frame.half_size,1.);
        const float progress=(float(emitter.frame.frame)+.5F)/float(std::max<std::uint8_t>(emitter.frame.frames,1));
        Random random{key*0x94d049bb133111ebULL};
        const auto anchor=[&](Style style,double spread,double radius,float heat,float opacity) {
            Anchored a;
            a.world=add(world,random.direction(),h*spread*random.range(.4,1.));
            a.particle.size0=a.particle.size1=float(radius*random.range(.85,1.15));
            a.particle.style=style;a.particle.heat=heat;a.particle.opacity=opacity;
            a.particle.seed=std::uint8_t(random.next()+emitter.frame.frame*37U);
            anchored_.push_back(a);
        };
        if(emitter.source==ExplosionSource::boost) {
            // A bright core in a blue sheath, flickering with the sprite's two frames.
            const float flicker=emitter.frame.frame?.9F:1.F;
            anchor(Style::boost,0,h*.45*flicker,.78F,.9F);
            for(int i=0;i<3;++i) anchor(Style::boost,.25,h*.6*flicker,.5F,.55F);
            continue;
        }
        switch(emitter.frame.kind) {
        case Kind::large:
            if(track.looped) {for(int i=0;i<4;++i) anchor(Style::fire,.3,h*.45,.8F,.6F);break;}
            // Hot only at first, then the cartridge's pale ball with an orange edge.
            for(int i=0;i<6;++i) anchor(Style::fire,.35*(.5+progress),h*(.45+.2*progress),.85F-.6F*progress,.45F*(1-progress*progress));
            if(progress>.3F) for(int i=0;i<4;++i) anchor(Style::smoke,.4,h*.5,.15F,std::min(.85F,(progress-.3F)*1.6F));
            break;
        case Kind::smoke:
            for(int i=0;i<4;++i) anchor(Style::smoke,.3,h*.5,.1F,.85F*(1-progress*progress*progress));
            break;
        case Kind::fireball:
            for(int i=0;i<3;++i) anchor(Style::fire,.2,h*.55,.95F,.8F);
            break;
        case Kind::small:
            for(int i=0;i<4;++i) anchor(Style::fire,.25,h*.5,.9F-.6F*progress,.5F);
            break;
        }
    }
    std::erase_if(tracks_,[&](const auto& entry){return entry.second.last_seen<time_-3;});
    std::erase_if(particles_,[&](const Particle& p){return time_-p.birth>=p.life || time_<p.birth-.01;});
    if(particles_.size()>particle_limit)
        particles_.erase(particles_.begin(),particles_.begin()+std::ptrdiff_t(particles_.size()-particle_limit));
}
ExplosionParticles::Packets ExplosionParticles::build(bool srgb,const Matrix4& model) const {
    struct Visual {std::array<float,3> centre;float radius,heat,opacity;Style style;std::uint8_t seed;};
    std::vector<Visual> smoke,fire;
    const auto camera_space=[&](const V3& world) {
        V3 d;for(unsigned j=0;j<3;++j) d[j]=wrap(world[j]-camera_[j]);
        std::array<float,3> c{};
        for(unsigned i=0;i<3;++i) c[i]=float(d[0]*view_[i]+d[1]*view_[3+i]+d[2]*view_[6+i]);
        return c;
    };
    const auto push=[&](const std::array<float,3>& c,double radius,float heat,float opacity,Style style,std::uint8_t seed) {
        // Behind the camera or too faint to see.
        if(!(c[2]>radius*.25+8) || opacity<=.004F || !(radius>0)) return;
        (style==Style::smoke?smoke:fire).push_back({c,float(radius),heat,opacity,style,seed});
    };
    for(const auto& p:particles_) {
        const auto at=place(p,time_);
        if(!at) continue;
        const float t=float((time_-p.birth)/p.life);
        // In quickly, out slowly.
        const float fade=smooth(0,.12F,t)*(1-smooth(.55F,1,t));
        const float heat=p.style!=Style::smoke?p.heat*std::pow(1-t,.8F):p.heat;
        push(camera_space({(*at)[0],(*at)[1],(*at)[2]}),(*at)[3],heat,p.opacity*fade,p.style,p.seed);
    }
    for(const auto& a:anchored_)
        push(camera_space(a.world),a.particle.size0,a.particle.heat,a.particle.opacity,a.particle.style,a.particle.seed);
    // Smoke is alpha blended: farthest first.
    std::sort(smoke.begin(),smoke.end(),[](const Visual& a,const Visual& b){return a.centre[2]>b.centre[2];});
    const auto encode=[&](std::array<float,3> c) {
        for(auto& v:c) {v=std::clamp(v*brightness_,0.F,1.F);if(srgb) v=linear(v);}
        return c;
    };
    const auto packet=[&](const std::vector<Visual>& list)->std::optional<DrawPacket> {
        if(list.empty()) return std::nullopt;
        DrawPacket out;out.model=model;
        auto& vertices=out.geometry.vertices;vertices.reserve(list.size()*6);
        constexpr std::array<std::array<float,2>,4> corners{{{-1,-1},{1,-1},{1,1},{-1,1}}};
        for(const auto& v:list) {
            std::array<float,3> base,core;
            if(v.style==Style::fire) {base=encode(fire_ramp(v.heat));core=encode(fire_ramp(std::min(1.F,v.heat+.35F)));}
            else if(v.style==Style::boost) {base=encode(boost_ramp(v.heat));core=encode(boost_ramp(std::min(1.F,v.heat+.35F)));}
            else {
                // The cartridge's smoke: pale grey-green, darker fresh from a fire.
                const float d=v.heat;
                base=encode({.78F-.36F*d,.82F-.40F*d,.74F-.36F*d});
                core=encode({.26F-.1F*d,.30F-.12F*d,.27F-.1F*d});
            }
            for(const unsigned corner:{0U,1U,2U,0U,2U,3U}) {
                SceneVertex vertex{};
                const auto [u,w]=corners[corner];
                for(unsigned k=0;k<3;++k) vertex.position[k]=v.centre[k];
                for(unsigned k=0;k<3;++k) {vertex.color[k]=base[k];vertex.odd_color[k]=core[k];}
                vertex.color[3]=std::clamp(v.opacity,0.F,1.F);vertex.odd_color[3]=1;
                vertex.uv[0]=u;vertex.uv[1]=w;
                vertex.billboard[0]=u*v.radius;vertex.billboard[1]=w*v.radius;
                // scene.hlsl: a round decal (4096), eye-facing (4); texture.z =
                // particle (1) | style << 1 | seed << 8.
                vertex.texture[2]=1U|(unsigned(v.style)<<1U)|(unsigned(v.seed)<<8U);
                vertex.texture[3]=4096U|4U;
                vertices.push_back(vertex);
            }
        }
        return out;
    };
    return {packet(smoke),packet(fire)};
}
}
