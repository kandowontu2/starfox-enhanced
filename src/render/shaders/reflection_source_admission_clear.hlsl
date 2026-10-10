// Diagnostic mask starts with no admitted lobes on EVERY new candidate.
RWByteAddressBuffer admission:register(u0,space1);
cbuffer AdmissionSettings:register(b0,space2) {
    uint admissionCount,admissionReserved0,admissionReserved1,admissionReserved2;
};
[numthreads(64,1,1)]
void feature_admission_clear_main(uint3 id:SV_DispatchThreadID) {
    const uint pixel=id.x+id.y*(65535U*64U);
    if(!admissionCount || admissionCount>16384U*16384U || admissionReserved0
        || admissionReserved1 || admissionReserved2 || pixel>=admissionCount)return;
    admission.Store(pixel*4,0);
}
