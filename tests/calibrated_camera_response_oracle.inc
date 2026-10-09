// Independent Euler/world transform oracle. Does not call the production
// response matrix or model composer; both native GPU fixtures use it.
starfox::vr::Matrix4 response_reference_product(const starfox::vr::Matrix4& a,const starfox::vr::Matrix4& b) {
    starfox::vr::Matrix4 result{};
    for(unsigned c=0;c<4;++c) for(unsigned r=0;r<4;++r) {
        double value=0;for(unsigned k=0;k<4;++k) value+=double(a[k*4+r])*b[c*4+k];
        result[c*4+r]=float(value);
    }
    return result;
}
std::shared_ptr<CalibratedGameFrame> response_reference_frame(const std::shared_ptr<const CalibratedGameFrame>& source) {
    auto result=std::make_shared<CalibratedGameFrame>(*source);
    const auto p=source->settings.camera_response_pose;
    const double sx=std::sin(p[0]),cx=std::cos(p[0]),sy=std::sin(-p[1]),cy=std::cos(-p[1]),sz=std::sin(-p[2]),cz=std::cos(-p[2]);
    const double rx[]{1,0,0,0,cx,-sx,0,sx,cx},ry[]{cy,0,sy,0,1,0,-sy,0,cy},rz[]{cz,-sz,0,sz,cz,0,0,0,1};
    double yx[9]{},zyx[9]{};
    for(unsigned r=0;r<3;++r) for(unsigned c=0;c<3;++c) for(unsigned k=0;k<3;++k) yx[r*3+c]+=ry[r*3+k]*rx[k*3+c];
    for(unsigned r=0;r<3;++r) for(unsigned c=0;c<3;++c) for(unsigned k=0;k<3;++k) zyx[r*3+c]+=rz[r*3+k]*yx[k*3+c];
    starfox::vr::Matrix4 rotation{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    for(unsigned r=0;r<3;++r) for(unsigned c=0;c<3;++c) rotation[c*4+r]=float(zyx[r*3+c]);
    for(auto& draw:result->draws) if(!draw.after_rays && (draw.layer==CalibratedGameLayer::world || draw.layer==CalibratedGameLayer::model))
        draw.packet.model=response_reference_product(rotation,draw.packet.model);
    const double light[]{source->source_light[0],-source->source_light[1],-source->source_light[2]};
    result->source_light={};
    for(unsigned r=0;r<3;++r) for(unsigned c=0;c<3;++c) result->source_light[r]+=rotation[c*4+r]*light[c];
    result->source_light[1]=-result->source_light[1];result->source_light[2]=-result->source_light[2];
    result->settings.camera_response_pose={};return result;
}
