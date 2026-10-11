#pragma clang diagnostic ignored "-Wmissing-prototypes"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

template<typename T>
[[clang::optnone]] T spvFMul(T l, T r)
{
    return fma(l, r, T(0));
}

template<typename T, int Cols, int Rows>
[[clang::optnone]] vec<T, Cols> spvFMulVectorMatrix(vec<T, Rows> v, matrix<T, Cols, Rows> m)
{
    vec<T, Cols> res = vec<T, Cols>(0);
    for (uint i = Rows; i > 0; --i)
    {
        vec<T, Cols> tmp(0);
        for (uint j = 0; j < Cols; ++j)
        {
            tmp[j] = m[j][i - 1];
        }
        res = fma(tmp, vec<T, Cols>(v[i - 1]), res);
    }
    return res;
}

template<typename T, int Cols, int Rows>
[[clang::optnone]] vec<T, Rows> spvFMulMatrixVector(matrix<T, Cols, Rows> m, vec<T, Cols> v)
{
    vec<T, Rows> res = vec<T, Rows>(0);
    for (uint i = Cols; i > 0; --i)
    {
        res = fma(m[i - 1], vec<T, Rows>(v[i - 1]), res);
    }
    return res;
}

template<typename T, int LCols, int LRows, int RCols, int RRows>
[[clang::optnone]] matrix<T, RCols, LRows> spvFMulMatrixMatrix(matrix<T, LCols, LRows> l, matrix<T, RCols, RRows> r)
{
    static_assert(LCols == RRows, "column-row configuration mismatch");
    matrix<T, RCols, LRows> res;
    for (uint i = 0; i < RCols; i++)
    {
        vec<T, LRows> tmp(0);
        for (uint j = 0; j < LCols; j++)
        {
            tmp = fma(vec<T, LRows>(r[i][j]), l[j], tmp);
        }
        res[i] = tmp;
    }
    return res;
}

template<typename T>
[[clang::optnone]] T spvFAdd(T l, T r)
{
    return fma(T(1), l, r);
}

template<typename T>
[[clang::optnone]] T spvFSub(T l, T r)
{
    return fma(T(-1), r, l);
}

struct type_Settings
{
    uint width;
    uint height;
    uint padding0;
    uint padding1;
    float4 projection;
    float4 planePoint;
    float4 planeNormal;
    float4 worldRow0;
    float4 worldRow1;
    float4 worldRow2;
    float4 depthTime;
};

struct Fullscreen
{
    float4 position;
};

struct Guides
{
    float4 ownership;
    float4 surface;
};

struct water_guides_fragment_main_out
{
    float4 out_var_SV_Target0 [[color(0)]];
    float4 out_var_SV_Target1 [[color(1)]];
};

fragment water_guides_fragment_main_out water_guides_fragment_main(constant type_Settings& Settings [[buffer(0)]], texture2d<float> ownership [[texture(0)]], texture2d<float> surfaces [[texture(1)]], float4 gl_FragCoord [[position]])
{
    bool _843 = false;
    water_guides_fragment_main_out out = {};
    __attribute__((unused)) float4 _559 = gl_FragCoord;
    _843 = false;
    float4 _1434;
    float4 _1435;
    do
    {
        uint2 _844 = uint2(gl_FragCoord.xy);
        bool _845 = true;
        bool _938;
        if (_844.x < Settings.width)
        {
            bool _937 = _844.y >= Settings.height;
            _845 = _937;
            _938 = _937;
        }
        else
        {
            _938 = true;
        }
        if (_938)
        {
            discard_fragment();
        }
        float4 _563 = ownership.read(uint2(int3(int(_844.x), int(_844.y), 0).xy), 0);
        float4 _562 = surfaces.read(uint2(int3(int(_844.x), int(_844.y), 0).xy), 0);
        uint _962 = uint(rint(spvFMul(_563.z, 255.0)));
        __attribute__((unused)) uint _846 = _962;
        bool _847 = false;
        bool _967;
        if (_962 != 1u)
        {
            bool _966 = _962 != 2u;
            _847 = _966;
            _967 = _966;
        }
        else
        {
            _967 = false;
        }
        bool _848 = true;
        bool _974;
        if (!_967)
        {
            bool _973 = _563.x <= 0.0;
            _848 = _973;
            _974 = _973;
        }
        else
        {
            _974 = true;
        }
        float4 _564;
        float4 _565;
        if (_974)
        {
            _843 = true;
            _565 = _563;
            _564 = _562;
            _1434 = _563;
            _1435 = _562;
            break;
        }
        float _17 = spvFAdd(float(_844.x), 0.5);
        __attribute__((unused)) float _850 = _17;
        float _18 = -Settings.projection.z;
        __attribute__((unused)) float _851 = _18;
        float _70 = spvFAdd(_17, _18);
        __attribute__((unused)) float _840 = _70;
        float _71 = spvFSub(_70, _17);
        __attribute__((unused)) float _841 = _71;
        float2 _985 = float2(_70, spvFAdd(spvFSub(_17, spvFSub(_70, _71)), spvFSub(_18, _71)));
        __attribute__((unused)) float2 _842 = _985;
        float2 _852 = _985;
        float2 _853 = float2(Settings.projection.x, 0.0);
        float _76 = _852.x / _853.x;
        __attribute__((unused)) float _831 = _76;
        float2 _833 = _852;
        float2 _834 = float2(_76, 0.0);
        float2 _835 = _853;
        __attribute__((unused)) float _826 = _834.x;
        __attribute__((unused)) float _827 = _835.x;
        float _86 = spvFMul(4097.0, _834.x);
        __attribute__((unused)) float _817 = _86;
        float _87 = spvFMul(4097.0, _835.x);
        __attribute__((unused)) float _818 = _87;
        float _89 = spvFSub(_86, spvFSub(_86, _834.x));
        __attribute__((unused)) float _819 = _89;
        float _91 = spvFSub(_87, spvFSub(_87, _835.x));
        __attribute__((unused)) float _820 = _91;
        float _92 = spvFSub(_834.x, _89);
        __attribute__((unused)) float _821 = _92;
        float _93 = spvFSub(_835.x, _91);
        __attribute__((unused)) float _822 = _93;
        float _94 = spvFMul(_834.x, _835.x);
        __attribute__((unused)) float _823 = _94;
        float2 _1000 = float2(_94, spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_89, _91), _94), spvFMul(_89, _93)), spvFMul(_92, _91)), spvFMul(_92, _93)));
        __attribute__((unused)) float2 _824 = _1000;
        float2 _825 = _1000;
        __attribute__((unused)) float _828 = _825.x;
        float _85 = spvFAdd(spvFAdd(spvFAdd(_825.y, spvFMul(_834.x, _835.y)), spvFMul(_834.y, _835.x)), spvFMul(_834.y, _835.y));
        __attribute__((unused)) float _829 = _85;
        float _103 = spvFAdd(_825.x, _85);
        __attribute__((unused)) float _814 = _103;
        float _104 = spvFSub(_103, _825.x);
        __attribute__((unused)) float _815 = _104;
        float2 _1013 = float2(_103, spvFAdd(spvFSub(_825.x, spvFSub(_103, _104)), spvFSub(_85, _104)));
        __attribute__((unused)) float2 _816 = _1013;
        __attribute__((unused)) float2 _830 = _1013;
        float2 _836 = -_1013;
        __attribute__((unused)) float _809 = _833.x;
        __attribute__((unused)) float _810 = _836.x;
        float _111 = spvFAdd(_833.x, _836.x);
        __attribute__((unused)) float _805 = _111;
        float _112 = spvFSub(_111, _833.x);
        __attribute__((unused)) float _806 = _112;
        float2 _1018 = float2(_111, spvFAdd(spvFSub(_833.x, spvFSub(_111, _112)), spvFSub(_836.x, _112)));
        __attribute__((unused)) float2 _807 = _1018;
        float2 _808 = _1018;
        __attribute__((unused)) float _811 = _808.x;
        float _110 = spvFAdd(spvFAdd(_808.y, _833.y), _836.y);
        __attribute__((unused)) float _812 = _110;
        float _117 = spvFAdd(_808.x, _110);
        __attribute__((unused)) float _802 = _117;
        float _118 = spvFSub(_117, _808.x);
        __attribute__((unused)) float _803 = _118;
        float2 _1027 = float2(_117, spvFAdd(spvFSub(_808.x, spvFSub(_117, _118)), spvFSub(_110, _118)));
        __attribute__((unused)) float2 _804 = _1027;
        __attribute__((unused)) float2 _813 = _1027;
        float2 _832 = _1027;
        __attribute__((unused)) float _837 = _76;
        float _79 = spvFAdd(_832.x, _832.y) / _853.x;
        __attribute__((unused)) float _838 = _79;
        float _123 = spvFAdd(_76, _79);
        __attribute__((unused)) float _799 = _123;
        float _124 = spvFSub(_123, _76);
        __attribute__((unused)) float _800 = _124;
        float2 _1033 = float2(_123, spvFAdd(spvFSub(_76, spvFSub(_123, _124)), spvFSub(_79, _124)));
        __attribute__((unused)) float2 _801 = _1033;
        __attribute__((unused)) float2 _839 = _1033;
        float2 _849 = _1033;
        float _19 = spvFAdd(float(_844.y), 0.5);
        __attribute__((unused)) float _855 = _19;
        float _20 = -Settings.projection.w;
        __attribute__((unused)) float _856 = _20;
        float _129 = spvFAdd(_19, _20);
        __attribute__((unused)) float _796 = _129;
        float _130 = spvFSub(_129, _19);
        __attribute__((unused)) float _797 = _130;
        float2 _1039 = float2(_129, spvFAdd(spvFSub(_19, spvFSub(_129, _130)), spvFSub(_20, _130)));
        __attribute__((unused)) float2 _798 = _1039;
        float2 _857 = _1039;
        float2 _858 = float2(Settings.projection.y, 0.0);
        float _135 = _857.x / _858.x;
        __attribute__((unused)) float _787 = _135;
        float2 _789 = _857;
        float2 _790 = float2(_135, 0.0);
        float2 _791 = _858;
        __attribute__((unused)) float _782 = _790.x;
        __attribute__((unused)) float _783 = _791.x;
        float _145 = spvFMul(4097.0, _790.x);
        __attribute__((unused)) float _773 = _145;
        float _146 = spvFMul(4097.0, _791.x);
        __attribute__((unused)) float _774 = _146;
        float _148 = spvFSub(_145, spvFSub(_145, _790.x));
        __attribute__((unused)) float _775 = _148;
        float _150 = spvFSub(_146, spvFSub(_146, _791.x));
        __attribute__((unused)) float _776 = _150;
        float _151 = spvFSub(_790.x, _148);
        __attribute__((unused)) float _777 = _151;
        float _152 = spvFSub(_791.x, _150);
        __attribute__((unused)) float _778 = _152;
        float _153 = spvFMul(_790.x, _791.x);
        __attribute__((unused)) float _779 = _153;
        float2 _1054 = float2(_153, spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_148, _150), _153), spvFMul(_148, _152)), spvFMul(_151, _150)), spvFMul(_151, _152)));
        __attribute__((unused)) float2 _780 = _1054;
        float2 _781 = _1054;
        __attribute__((unused)) float _784 = _781.x;
        float _144 = spvFAdd(spvFAdd(spvFAdd(_781.y, spvFMul(_790.x, _791.y)), spvFMul(_790.y, _791.x)), spvFMul(_790.y, _791.y));
        __attribute__((unused)) float _785 = _144;
        float _162 = spvFAdd(_781.x, _144);
        __attribute__((unused)) float _770 = _162;
        float _163 = spvFSub(_162, _781.x);
        __attribute__((unused)) float _771 = _163;
        float2 _1067 = float2(_162, spvFAdd(spvFSub(_781.x, spvFSub(_162, _163)), spvFSub(_144, _163)));
        __attribute__((unused)) float2 _772 = _1067;
        __attribute__((unused)) float2 _786 = _1067;
        float2 _792 = -_1067;
        __attribute__((unused)) float _765 = _789.x;
        __attribute__((unused)) float _766 = _792.x;
        float _170 = spvFAdd(_789.x, _792.x);
        __attribute__((unused)) float _761 = _170;
        float _171 = spvFSub(_170, _789.x);
        __attribute__((unused)) float _762 = _171;
        float2 _1072 = float2(_170, spvFAdd(spvFSub(_789.x, spvFSub(_170, _171)), spvFSub(_792.x, _171)));
        __attribute__((unused)) float2 _763 = _1072;
        float2 _764 = _1072;
        __attribute__((unused)) float _767 = _764.x;
        float _169 = spvFAdd(spvFAdd(_764.y, _789.y), _792.y);
        __attribute__((unused)) float _768 = _169;
        float _176 = spvFAdd(_764.x, _169);
        __attribute__((unused)) float _758 = _176;
        float _177 = spvFSub(_176, _764.x);
        __attribute__((unused)) float _759 = _177;
        float2 _1081 = float2(_176, spvFAdd(spvFSub(_764.x, spvFSub(_176, _177)), spvFSub(_169, _177)));
        __attribute__((unused)) float2 _760 = _1081;
        __attribute__((unused)) float2 _769 = _1081;
        float2 _788 = _1081;
        __attribute__((unused)) float _793 = _135;
        float _138 = spvFAdd(_788.x, _788.y) / _858.x;
        __attribute__((unused)) float _794 = _138;
        float _182 = spvFAdd(_135, _138);
        __attribute__((unused)) float _755 = _182;
        float _183 = spvFSub(_182, _135);
        __attribute__((unused)) float _756 = _183;
        float2 _1087 = float2(_182, spvFAdd(spvFSub(_135, spvFSub(_182, _183)), spvFSub(_138, _183)));
        __attribute__((unused)) float2 _757 = _1087;
        __attribute__((unused)) float2 _795 = _1087;
        float2 _854 = _1087;
        float3 _1096 = float3(spvFAdd(_849.x, _849.y), spvFAdd(_854.x, _854.y), 1.0);
        __attribute__((unused)) float3 _859 = _1096;
        float _1097 = length(_1096);
        __attribute__((unused)) float _860 = _1097;
        float3 _23 = _1096 / float3(_1097);
        __attribute__((unused)) float3 _861 = _23;
        float2 _863 = _849;
        float2 _864 = float2(Settings.planeNormal.x, 0.0);
        __attribute__((unused)) float _750 = _863.x;
        __attribute__((unused)) float _751 = _864.x;
        float _194 = spvFMul(4097.0, _863.x);
        __attribute__((unused)) float _741 = _194;
        float _195 = spvFMul(4097.0, _864.x);
        __attribute__((unused)) float _742 = _195;
        float _197 = spvFSub(_194, spvFSub(_194, _863.x));
        __attribute__((unused)) float _743 = _197;
        float _199 = spvFSub(_195, spvFSub(_195, _864.x));
        __attribute__((unused)) float _744 = _199;
        float _200 = spvFSub(_863.x, _197);
        __attribute__((unused)) float _745 = _200;
        float _201 = spvFSub(_864.x, _199);
        __attribute__((unused)) float _746 = _201;
        float _202 = spvFMul(_863.x, _864.x);
        __attribute__((unused)) float _747 = _202;
        float2 _1108 = float2(_202, spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_197, _199), _202), spvFMul(_197, _201)), spvFMul(_200, _199)), spvFMul(_200, _201)));
        __attribute__((unused)) float2 _748 = _1108;
        float2 _749 = _1108;
        __attribute__((unused)) float _752 = _749.x;
        float _193 = spvFAdd(spvFAdd(spvFAdd(_749.y, spvFMul(_863.x, _864.y)), spvFMul(_863.y, _864.x)), spvFMul(_863.y, _864.y));
        __attribute__((unused)) float _753 = _193;
        float _211 = spvFAdd(_749.x, _193);
        __attribute__((unused)) float _738 = _211;
        float _212 = spvFSub(_211, _749.x);
        __attribute__((unused)) float _739 = _212;
        float2 _1121 = float2(_211, spvFAdd(spvFSub(_749.x, spvFSub(_211, _212)), spvFSub(_193, _212)));
        __attribute__((unused)) float2 _740 = _1121;
        __attribute__((unused)) float2 _754 = _1121;
        float2 _865 = _1121;
        float2 _866 = _854;
        float2 _867 = float2(Settings.planeNormal.y, 0.0);
        __attribute__((unused)) float _733 = _866.x;
        __attribute__((unused)) float _734 = _867.x;
        float _223 = spvFMul(4097.0, _866.x);
        __attribute__((unused)) float _724 = _223;
        float _224 = spvFMul(4097.0, _867.x);
        __attribute__((unused)) float _725 = _224;
        float _226 = spvFSub(_223, spvFSub(_223, _866.x));
        __attribute__((unused)) float _726 = _226;
        float _228 = spvFSub(_224, spvFSub(_224, _867.x));
        __attribute__((unused)) float _727 = _228;
        float _229 = spvFSub(_866.x, _226);
        __attribute__((unused)) float _728 = _229;
        float _230 = spvFSub(_867.x, _228);
        __attribute__((unused)) float _729 = _230;
        float _231 = spvFMul(_866.x, _867.x);
        __attribute__((unused)) float _730 = _231;
        float2 _1130 = float2(_231, spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_226, _228), _231), spvFMul(_226, _230)), spvFMul(_229, _228)), spvFMul(_229, _230)));
        __attribute__((unused)) float2 _731 = _1130;
        float2 _732 = _1130;
        __attribute__((unused)) float _735 = _732.x;
        float _222 = spvFAdd(spvFAdd(spvFAdd(_732.y, spvFMul(_866.x, _867.y)), spvFMul(_866.y, _867.x)), spvFMul(_866.y, _867.y));
        __attribute__((unused)) float _736 = _222;
        float _240 = spvFAdd(_732.x, _222);
        __attribute__((unused)) float _721 = _240;
        float _241 = spvFSub(_240, _732.x);
        __attribute__((unused)) float _722 = _241;
        float2 _1143 = float2(_240, spvFAdd(spvFSub(_732.x, spvFSub(_240, _241)), spvFSub(_222, _241)));
        __attribute__((unused)) float2 _723 = _1143;
        __attribute__((unused)) float2 _737 = _1143;
        float2 _868 = _1143;
        __attribute__((unused)) float _716 = _865.x;
        __attribute__((unused)) float _717 = _868.x;
        float _248 = spvFAdd(_865.x, _868.x);
        __attribute__((unused)) float _712 = _248;
        float _249 = spvFSub(_248, _865.x);
        __attribute__((unused)) float _713 = _249;
        float2 _1148 = float2(_248, spvFAdd(spvFSub(_865.x, spvFSub(_248, _249)), spvFSub(_868.x, _249)));
        __attribute__((unused)) float2 _714 = _1148;
        float2 _715 = _1148;
        __attribute__((unused)) float _718 = _715.x;
        float _247 = spvFAdd(spvFAdd(_715.y, _865.y), _868.y);
        __attribute__((unused)) float _719 = _247;
        float _254 = spvFAdd(_715.x, _247);
        __attribute__((unused)) float _709 = _254;
        float _255 = spvFSub(_254, _715.x);
        __attribute__((unused)) float _710 = _255;
        float2 _1157 = float2(_254, spvFAdd(spvFSub(_715.x, spvFSub(_254, _255)), spvFSub(_247, _255)));
        __attribute__((unused)) float2 _711 = _1157;
        __attribute__((unused)) float2 _720 = _1157;
        float2 _869 = _1157;
        float2 _870 = float2(Settings.planeNormal.z, 0.0);
        __attribute__((unused)) float _704 = _869.x;
        __attribute__((unused)) float _705 = _870.x;
        float _262 = spvFAdd(_869.x, _870.x);
        __attribute__((unused)) float _700 = _262;
        float _263 = spvFSub(_262, _869.x);
        __attribute__((unused)) float _701 = _263;
        float2 _1165 = float2(_262, spvFAdd(spvFSub(_869.x, spvFSub(_262, _263)), spvFSub(_870.x, _263)));
        __attribute__((unused)) float2 _702 = _1165;
        float2 _703 = _1165;
        __attribute__((unused)) float _706 = _703.x;
        float _261 = spvFAdd(spvFAdd(_703.y, _869.y), _870.y);
        __attribute__((unused)) float _707 = _261;
        float _268 = spvFAdd(_703.x, _261);
        __attribute__((unused)) float _697 = _268;
        float _269 = spvFSub(_268, _703.x);
        __attribute__((unused)) float _698 = _269;
        float2 _1174 = float2(_268, spvFAdd(spvFSub(_703.x, spvFSub(_268, _269)), spvFSub(_261, _269)));
        __attribute__((unused)) float2 _699 = _1174;
        __attribute__((unused)) float2 _708 = _1174;
        float2 _862 = _1174;
        float _25 = spvFAdd(_862.x, _862.y) / _1097;
        __attribute__((unused)) float _871 = _25;
        float _1179 = abs(_25);
        if (_1179 <= 9.9999999392252902907785028219223e-09)
        {
            _843 = true;
            _565 = _563;
            _564 = _562;
            _1434 = _563;
            _1435 = _562;
            break;
        }
        __attribute__((unused)) float _873 = Settings.planePoint.x;
        __attribute__((unused)) float _874 = Settings.planeNormal.x;
        float _274 = spvFMul(4097.0, Settings.planePoint.x);
        __attribute__((unused)) float _689 = _274;
        float _275 = spvFMul(4097.0, Settings.planeNormal.x);
        __attribute__((unused)) float _690 = _275;
        float _277 = spvFSub(_274, spvFSub(_274, Settings.planePoint.x));
        __attribute__((unused)) float _691 = _277;
        float _279 = spvFSub(_275, spvFSub(_275, Settings.planeNormal.x));
        __attribute__((unused)) float _692 = _279;
        float _280 = spvFSub(Settings.planePoint.x, _277);
        __attribute__((unused)) float _693 = _280;
        float _281 = spvFSub(Settings.planeNormal.x, _279);
        __attribute__((unused)) float _694 = _281;
        float _282 = spvFMul(Settings.planePoint.x, Settings.planeNormal.x);
        __attribute__((unused)) float _695 = _282;
        float2 _1189 = float2(_282, spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_277, _279), _282), spvFMul(_277, _281)), spvFMul(_280, _279)), spvFMul(_280, _281)));
        __attribute__((unused)) float2 _696 = _1189;
        float2 _875 = _1189;
        __attribute__((unused)) float _876 = Settings.planePoint.y;
        __attribute__((unused)) float _877 = Settings.planeNormal.y;
        float _291 = spvFMul(4097.0, Settings.planePoint.y);
        __attribute__((unused)) float _681 = _291;
        float _292 = spvFMul(4097.0, Settings.planeNormal.y);
        __attribute__((unused)) float _682 = _292;
        float _294 = spvFSub(_291, spvFSub(_291, Settings.planePoint.y));
        __attribute__((unused)) float _683 = _294;
        float _296 = spvFSub(_292, spvFSub(_292, Settings.planeNormal.y));
        __attribute__((unused)) float _684 = _296;
        float _297 = spvFSub(Settings.planePoint.y, _294);
        __attribute__((unused)) float _685 = _297;
        float _298 = spvFSub(Settings.planeNormal.y, _296);
        __attribute__((unused)) float _686 = _298;
        float _299 = spvFMul(Settings.planePoint.y, Settings.planeNormal.y);
        __attribute__((unused)) float _687 = _299;
        float2 _1192 = float2(_299, spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_294, _296), _299), spvFMul(_294, _298)), spvFMul(_297, _296)), spvFMul(_297, _298)));
        __attribute__((unused)) float2 _688 = _1192;
        float2 _878 = _1192;
        __attribute__((unused)) float _676 = _875.x;
        __attribute__((unused)) float _677 = _878.x;
        float _310 = spvFAdd(_875.x, _878.x);
        __attribute__((unused)) float _672 = _310;
        float _311 = spvFSub(_310, _875.x);
        __attribute__((unused)) float _673 = _311;
        float2 _1197 = float2(_310, spvFAdd(spvFSub(_875.x, spvFSub(_310, _311)), spvFSub(_878.x, _311)));
        __attribute__((unused)) float2 _674 = _1197;
        float2 _675 = _1197;
        __attribute__((unused)) float _678 = _675.x;
        float _309 = spvFAdd(spvFAdd(_675.y, _875.y), _878.y);
        __attribute__((unused)) float _679 = _309;
        float _316 = spvFAdd(_675.x, _309);
        __attribute__((unused)) float _669 = _316;
        float _317 = spvFSub(_316, _675.x);
        __attribute__((unused)) float _670 = _317;
        float2 _1206 = float2(_316, spvFAdd(spvFSub(_675.x, spvFSub(_316, _317)), spvFSub(_309, _317)));
        __attribute__((unused)) float2 _671 = _1206;
        __attribute__((unused)) float2 _680 = _1206;
        float2 _879 = _1206;
        __attribute__((unused)) float _880 = Settings.planePoint.z;
        __attribute__((unused)) float _881 = Settings.planeNormal.z;
        float _322 = spvFMul(4097.0, Settings.planePoint.z);
        __attribute__((unused)) float _661 = _322;
        float _323 = spvFMul(4097.0, Settings.planeNormal.z);
        __attribute__((unused)) float _662 = _323;
        float _325 = spvFSub(_322, spvFSub(_322, Settings.planePoint.z));
        __attribute__((unused)) float _663 = _325;
        float _327 = spvFSub(_323, spvFSub(_323, Settings.planeNormal.z));
        __attribute__((unused)) float _664 = _327;
        float _328 = spvFSub(Settings.planePoint.z, _325);
        __attribute__((unused)) float _665 = _328;
        float _329 = spvFSub(Settings.planeNormal.z, _327);
        __attribute__((unused)) float _666 = _329;
        float _330 = spvFMul(Settings.planePoint.z, Settings.planeNormal.z);
        __attribute__((unused)) float _667 = _330;
        float2 _1209 = float2(_330, spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_325, _327), _330), spvFMul(_325, _329)), spvFMul(_328, _327)), spvFMul(_328, _329)));
        __attribute__((unused)) float2 _668 = _1209;
        float2 _882 = _1209;
        __attribute__((unused)) float _656 = _879.x;
        __attribute__((unused)) float _657 = _882.x;
        float _341 = spvFAdd(_879.x, _882.x);
        __attribute__((unused)) float _652 = _341;
        float _342 = spvFSub(_341, _879.x);
        __attribute__((unused)) float _653 = _342;
        float2 _1214 = float2(_341, spvFAdd(spvFSub(_879.x, spvFSub(_341, _342)), spvFSub(_882.x, _342)));
        __attribute__((unused)) float2 _654 = _1214;
        float2 _655 = _1214;
        __attribute__((unused)) float _658 = _655.x;
        float _340 = spvFAdd(spvFAdd(_655.y, _879.y), _882.y);
        __attribute__((unused)) float _659 = _340;
        float _347 = spvFAdd(_655.x, _340);
        __attribute__((unused)) float _649 = _347;
        float _348 = spvFSub(_347, _655.x);
        __attribute__((unused)) float _650 = _348;
        float2 _1223 = float2(_347, spvFAdd(spvFSub(_655.x, spvFSub(_347, _348)), spvFSub(_340, _348)));
        __attribute__((unused)) float2 _651 = _1223;
        __attribute__((unused)) float2 _660 = _1223;
        __attribute__((unused)) float2 _872 = _1223;
        float2 _884 = _1223;
        float2 _885 = _862;
        float _353 = _884.x / _885.x;
        __attribute__((unused)) float _640 = _353;
        float2 _642 = _884;
        float2 _643 = float2(_353, 0.0);
        float2 _644 = _885;
        __attribute__((unused)) float _635 = _643.x;
        __attribute__((unused)) float _636 = _644.x;
        float _363 = spvFMul(4097.0, _643.x);
        __attribute__((unused)) float _626 = _363;
        float _364 = spvFMul(4097.0, _644.x);
        __attribute__((unused)) float _627 = _364;
        float _366 = spvFSub(_363, spvFSub(_363, _643.x));
        __attribute__((unused)) float _628 = _366;
        float _368 = spvFSub(_364, spvFSub(_364, _644.x));
        __attribute__((unused)) float _629 = _368;
        float _369 = spvFSub(_643.x, _366);
        __attribute__((unused)) float _630 = _369;
        float _370 = spvFSub(_644.x, _368);
        __attribute__((unused)) float _631 = _370;
        float _371 = spvFMul(_643.x, _644.x);
        __attribute__((unused)) float _632 = _371;
        float2 _1236 = float2(_371, spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_366, _368), _371), spvFMul(_366, _370)), spvFMul(_369, _368)), spvFMul(_369, _370)));
        __attribute__((unused)) float2 _633 = _1236;
        float2 _634 = _1236;
        __attribute__((unused)) float _637 = _634.x;
        float _362 = spvFAdd(spvFAdd(spvFAdd(_634.y, spvFMul(_643.x, _644.y)), spvFMul(_643.y, _644.x)), spvFMul(_643.y, _644.y));
        __attribute__((unused)) float _638 = _362;
        float _380 = spvFAdd(_634.x, _362);
        __attribute__((unused)) float _623 = _380;
        float _381 = spvFSub(_380, _634.x);
        __attribute__((unused)) float _624 = _381;
        float2 _1249 = float2(_380, spvFAdd(spvFSub(_634.x, spvFSub(_380, _381)), spvFSub(_362, _381)));
        __attribute__((unused)) float2 _625 = _1249;
        __attribute__((unused)) float2 _639 = _1249;
        float2 _645 = -_1249;
        __attribute__((unused)) float _618 = _642.x;
        __attribute__((unused)) float _619 = _645.x;
        float _388 = spvFAdd(_642.x, _645.x);
        __attribute__((unused)) float _614 = _388;
        float _389 = spvFSub(_388, _642.x);
        __attribute__((unused)) float _615 = _389;
        float2 _1254 = float2(_388, spvFAdd(spvFSub(_642.x, spvFSub(_388, _389)), spvFSub(_645.x, _389)));
        __attribute__((unused)) float2 _616 = _1254;
        float2 _617 = _1254;
        __attribute__((unused)) float _620 = _617.x;
        float _387 = spvFAdd(spvFAdd(_617.y, _642.y), _645.y);
        __attribute__((unused)) float _621 = _387;
        float _394 = spvFAdd(_617.x, _387);
        __attribute__((unused)) float _611 = _394;
        float _395 = spvFSub(_394, _617.x);
        __attribute__((unused)) float _612 = _395;
        float2 _1263 = float2(_394, spvFAdd(spvFSub(_617.x, spvFSub(_394, _395)), spvFSub(_387, _395)));
        __attribute__((unused)) float2 _613 = _1263;
        __attribute__((unused)) float2 _622 = _1263;
        float2 _641 = _1263;
        __attribute__((unused)) float _646 = _353;
        float _356 = spvFAdd(_641.x, _641.y) / _885.x;
        __attribute__((unused)) float _647 = _356;
        float _400 = spvFAdd(_353, _356);
        __attribute__((unused)) float _608 = _400;
        float _401 = spvFSub(_400, _353);
        __attribute__((unused)) float _609 = _401;
        float2 _1269 = float2(_400, spvFAdd(spvFSub(_353, spvFSub(_400, _401)), spvFSub(_356, _401)));
        __attribute__((unused)) float2 _610 = _1269;
        __attribute__((unused)) float2 _648 = _1269;
        float2 _883 = _1269;
        float _26 = spvFAdd(_883.x, _883.y);
        __attribute__((unused)) float _886 = _26;
        float _27 = spvFMul(_26, _1097);
        __attribute__((unused)) float _887 = _27;
        float _31 = spvFAdd(-Settings.depthTime.x, spvFMul(Settings.depthTime.y, 256.0) / _26);
        __attribute__((unused)) float _888 = _31;
        bool _889 = true;
        bool _1286;
        if (!(isnan(_27) || isinf(_27)))
        {
            bool _1285 = _27 <= 0.0;
            _889 = _1285;
            _1286 = _1285;
        }
        else
        {
            _1286 = true;
        }
        bool _890 = true;
        bool _1291;
        if (!_1286)
        {
            bool _1290 = _26 >= 65536.0;
            _890 = _1290;
            _1291 = _1290;
        }
        else
        {
            _1291 = true;
        }
        bool _891 = true;
        bool _1299;
        if (!_1291)
        {
            bool _1297 = isnan(_31) || isinf(_31);
            _891 = _1297;
            _1299 = _1297;
        }
        else
        {
            _1299 = true;
        }
        bool _892 = true;
        bool _1304;
        if (!_1299)
        {
            bool _1303 = _31 < 0.0;
            _892 = _1303;
            _1304 = _1303;
        }
        else
        {
            _1304 = true;
        }
        bool _893 = true;
        bool _1309;
        if (!_1304)
        {
            bool _1308 = _31 >= 1.0;
            _893 = _1308;
            _1309 = _1308;
        }
        else
        {
            _1309 = true;
        }
        bool _894 = true;
        bool _1328;
        if (!_1309)
        {
            bool _895 = false;
            bool _1321;
            if (_563.x > 0.0039215688593685626983642578125)
            {
                bool _1320 = _562.w > 0.0;
                _895 = _1320;
                _1321 = _1320;
            }
            else
            {
                _1321 = false;
            }
            bool _896 = false;
            bool _1327;
            if (_1321)
            {
                bool _1326 = _26 > _562.w;
                _896 = _1326;
                _1327 = _1326;
            }
            else
            {
                _1327 = false;
            }
            _894 = _1327;
            _1328 = _1327;
        }
        else
        {
            _1328 = true;
        }
        if (_1328)
        {
            _843 = true;
            _565 = _563;
            _564 = _562;
            _1434 = _563;
            _1435 = _562;
            break;
        }
        float2 _898 = _849;
        float2 _899 = _883;
        __attribute__((unused)) float _603 = _898.x;
        __attribute__((unused)) float _604 = _899.x;
        float _412 = spvFMul(4097.0, _898.x);
        __attribute__((unused)) float _594 = _412;
        float _413 = spvFMul(4097.0, _899.x);
        __attribute__((unused)) float _595 = _413;
        float _415 = spvFSub(_412, spvFSub(_412, _898.x));
        __attribute__((unused)) float _596 = _415;
        float _417 = spvFSub(_413, spvFSub(_413, _899.x));
        __attribute__((unused)) float _597 = _417;
        float _418 = spvFSub(_898.x, _415);
        __attribute__((unused)) float _598 = _418;
        float _419 = spvFSub(_899.x, _417);
        __attribute__((unused)) float _599 = _419;
        float _420 = spvFMul(_898.x, _899.x);
        __attribute__((unused)) float _600 = _420;
        float2 _1340 = float2(_420, spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_415, _417), _420), spvFMul(_415, _419)), spvFMul(_418, _417)), spvFMul(_418, _419)));
        __attribute__((unused)) float2 _601 = _1340;
        float2 _602 = _1340;
        __attribute__((unused)) float _605 = _602.x;
        float _411 = spvFAdd(spvFAdd(spvFAdd(_602.y, spvFMul(_898.x, _899.y)), spvFMul(_898.y, _899.x)), spvFMul(_898.y, _899.y));
        __attribute__((unused)) float _606 = _411;
        float _429 = spvFAdd(_602.x, _411);
        __attribute__((unused)) float _591 = _429;
        float _430 = spvFSub(_429, _602.x);
        __attribute__((unused)) float _592 = _430;
        float2 _1353 = float2(_429, spvFAdd(spvFSub(_602.x, spvFSub(_429, _430)), spvFSub(_411, _430)));
        __attribute__((unused)) float2 _593 = _1353;
        __attribute__((unused)) float2 _607 = _1353;
        float2 _897 = _1353;
        float2 _901 = _854;
        float2 _902 = _883;
        __attribute__((unused)) float _586 = _901.x;
        __attribute__((unused)) float _587 = _902.x;
        float _441 = spvFMul(4097.0, _901.x);
        __attribute__((unused)) float _577 = _441;
        float _442 = spvFMul(4097.0, _902.x);
        __attribute__((unused)) float _578 = _442;
        float _444 = spvFSub(_441, spvFSub(_441, _901.x));
        __attribute__((unused)) float _579 = _444;
        float _446 = spvFSub(_442, spvFSub(_442, _902.x));
        __attribute__((unused)) float _580 = _446;
        float _447 = spvFSub(_901.x, _444);
        __attribute__((unused)) float _581 = _447;
        float _448 = spvFSub(_902.x, _446);
        __attribute__((unused)) float _582 = _448;
        float _449 = spvFMul(_901.x, _902.x);
        __attribute__((unused)) float _583 = _449;
        float2 _1360 = float2(_449, spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_444, _446), _449), spvFMul(_444, _448)), spvFMul(_447, _446)), spvFMul(_447, _448)));
        __attribute__((unused)) float2 _584 = _1360;
        float2 _585 = _1360;
        __attribute__((unused)) float _588 = _585.x;
        float _440 = spvFAdd(spvFAdd(spvFAdd(_585.y, spvFMul(_901.x, _902.y)), spvFMul(_901.y, _902.x)), spvFMul(_901.y, _902.y));
        __attribute__((unused)) float _589 = _440;
        float _458 = spvFAdd(_585.x, _440);
        __attribute__((unused)) float _574 = _458;
        float _459 = spvFSub(_458, _585.x);
        __attribute__((unused)) float _575 = _459;
        float2 _1373 = float2(_458, spvFAdd(spvFSub(_585.x, spvFSub(_458, _459)), spvFSub(_440, _459)));
        __attribute__((unused)) float2 _576 = _1373;
        __attribute__((unused)) float2 _590 = _1373;
        float2 _900 = _1373;
        float3 _1382 = float3(spvFAdd(_897.x, _897.y), spvFAdd(_900.x, _900.y), _26);
        __attribute__((unused)) float3 _903 = _1382;
        float3x3 _1392 = float3x3(Settings.worldRow0.xyz, Settings.worldRow1.xyz, Settings.worldRow2.xyz);
        __attribute__((unused)) float3x3 _904 = _1392;
        float3 _905 = spvFAdd(spvFMulMatrixVector(_1392, _1382), float3(Settings.worldRow0.w, Settings.worldRow1.w, Settings.worldRow2.w));
        float _37 = (_27 / precise::max(Settings.projection.x, 1.0)) / precise::max(_1179, 0.039999999105930328369140625);
        __attribute__((unused)) float _906 = _37;
        __attribute__((unused)) float _907 = Settings.depthTime.z;
        __attribute__((unused)) float _909 = _37;
        __attribute__((unused)) float _910 = 0.02199999988079071044921875;
        float _464 = spvFMul(_37, 0.02199999988079071044921875);
        __attribute__((unused)) float _572 = _464;
        float _469 = 1.0 / spvFAdd(1.0, spvFMul(spvFMul(spvFMul(_464, _464), _464), _464));
        __attribute__((unused)) float _573 = _469;
        float _48 = spvFMul(Settings.depthTime.z, 1.2000000476837158203125);
        __attribute__((unused)) float _911 = _37;
        __attribute__((unused)) float _912 = 0.0540000014007091522216796875;
        float _470 = spvFMul(_37, 0.0540000014007091522216796875);
        __attribute__((unused)) float _570 = _470;
        float _475 = 1.0 / spvFAdd(1.0, spvFMul(spvFMul(spvFMul(_470, _470), _470), _470));
        __attribute__((unused)) float _571 = _475;
        float _52 = spvFAdd(spvFMul(spvFMul(0.054999999701976776123046875, precise::cos(spvFSub(spvFAdd(spvFMul(_905.x, 0.017999999225139617919921875), spvFMul(_905.z, 0.010999999940395355224609375)), spvFMul(Settings.depthTime.z, 0.800000011920928955078125)))), _469), spvFMul(spvFMul(0.02500000037252902984619140625, precise::cos(spvFAdd(spvFSub(spvFMul(_905.x, 0.046999998390674591064453125), spvFMul(_905.z, 0.02500000037252902984619140625)), _48))), _475));
        __attribute__((unused)) float _908 = _52;
        __attribute__((unused)) float _914 = _37;
        __attribute__((unused)) float _915 = 0.0240000002086162567138671875;
        float _476 = spvFMul(_37, 0.0240000002086162567138671875);
        __attribute__((unused)) float _568 = _476;
        float _481 = 1.0 / spvFAdd(1.0, spvFMul(spvFMul(spvFMul(_476, _476), _476), _476));
        __attribute__((unused)) float _569 = _481;
        __attribute__((unused)) float _916 = _37;
        __attribute__((unused)) float _917 = 0.0540000014007091522216796875;
        __attribute__((unused)) float _566 = _470;
        __attribute__((unused)) float _567 = _475;
        float _66 = spvFSub(spvFMul(spvFMul(0.04500000178813934326171875, precise::cos(spvFSub(spvFSub(spvFMul(_905.z, 0.02199999988079071044921875), spvFMul(_905.x, 0.0089999996125698089599609375)), spvFMul(Settings.depthTime.z, 0.64999997615814208984375)))), _481), spvFMul(spvFMul(0.0199999995529651641845703125, precise::cos(spvFAdd(spvFSub(spvFMul(_905.x, 0.046999998390674591064453125), spvFMul(_905.z, 0.02500000037252902984619140625)), _48))), _475));
        __attribute__((unused)) float _913 = _66;
        float3 _1419 = fast::normalize(spvFMulVectorMatrix(float3(_52, -1.0, _66), _1392));
        float3 _918 = _1419;
        float3 _1423;
        if (dot(_1419, _23) > 0.0)
        {
            float3 _69 = -_1419;
            _918 = _69;
            _1423 = _69;
        }
        else
        {
            _1423 = _1419;
        }
        _563 = float4(0.0039215688593685626983642578125, 1.0, 0.0039215688593685626983642578125, _563.w);
        _562 = float4(_1423, _26);
        _843 = true;
        _565 = _563;
        _564 = _562;
        _1434 = _563;
        _1435 = _562;
        break;
    } while(false);
    __attribute__((unused)) float4 _561 = _1434;
    __attribute__((unused)) float4 _560 = _1435;
    out.out_var_SV_Target0 = _1434;
    out.out_var_SV_Target1 = _1435;
    return out;
}

