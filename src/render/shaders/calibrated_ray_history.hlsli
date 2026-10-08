// Match the native ray compositor's actual RGB consumers. A hit in an unused
// reflection buffer is not evidence that the presented pixel was reflected.
// Secondary radiance cannot borrow the primary receiver's rigid motion.
bool calibrated_secondary_radiance(uint word,float4 owner,bool modelReflections) {
    uint layer=uint(round(owner.b*255.));
    if(owner.r<=0 || (layer!=1 && layer!=2)) return false;
    uint kind=word>>24;
    if(kind==253U) return true; // nearer liquid replaces receiver RGB
    if(owner.r==1./255.) return kind==254U; // analytic ground replaces RGB
    return kind==255U && modelReflections && owner.g>0; // actual model mix
}
