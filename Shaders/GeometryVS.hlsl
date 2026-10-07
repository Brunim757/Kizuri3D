cbuffer GeoCB : register(b0)
{
    row_major float4x4 World;
    row_major float4x4 View;
    row_major float4x4 Proj;
};
struct VSIn
{
    float3 pos : POSITION;
    float3 nrm : NORMAL;
    float2 uv : TEXCOORD0;
    float4 tan : TANGENT;
    float2 uv2 : TEXCOORD1;
};
struct VSOut
{
    float4 pos : SV_Position;
    float3 wnrm : NORMAL0;
    float3 wpos : TEXCOORD0;
    float2 uv : TEXCOORD1;
    float4 wtan : TEXCOORD2;
    float3 vts : TEXCOORD3;
    float2 luv : TEXCOORD4;
    float2 uv2 : TEXCOORD5;
};
VSOut GeoMain(VSIn vin, float4x4 W)
{
    VSOut vout;
    float4 wp = mul(float4(vin.pos, 1.0), W);
    vout.wpos = wp.xyz;
    float3x3 wrot = (float3x3)W;
    vout.wnrm = normalize(mul(vin.nrm, wrot));
    float4 vp = mul(wp, View);
    vout.pos = mul(vp, Proj);
    vout.uv = vin.uv;
    vout.luv = vin.uv2;
    vout.uv2 = vin.uv2;
    float3 wt = normalize(mul(vin.tan.xyz, wrot));
    vout.wtan = float4(wt, vin.tan.w);
    float3 wb = cross(vout.wnrm, wt) * vin.tan.w;
    float3 camW = -mul(View[3].xyz, transpose((float3x3)View));
    float3 toCam = camW - wp.xyz;
    vout.vts = float3(dot(toCam, wt), dot(toCam, wb), dot(toCam, vout.wnrm));
    return vout;
}
VSOut main(VSIn vin)
{
    return GeoMain(vin, World);
}
VSOut mainInst(VSIn vin, float4 i0 : INSTANCE0, float4 i1 : INSTANCE1, float4 i2 : INSTANCE2, float4 i3 : INSTANCE3)
{
    float4x4 iw = float4x4(i0, i1, i2, i3);
    return GeoMain(vin, iw);
}
