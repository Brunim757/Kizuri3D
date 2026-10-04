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
};
struct VSOut
{
    float4 pos : SV_Position;
    float3 wnrm : NORMAL0;
    float3 wpos : TEXCOORD0;
    float2 uv : TEXCOORD1;
    float4 wtan : TEXCOORD2;
    float3 vts : TEXCOORD3;
};
VSOut main(VSIn vin)
{
    VSOut vout;
    float4 wp = mul(float4(vin.pos, 1.0), World);
    vout.wpos = wp.xyz;
    float3x3 wrot = (float3x3)World;
    vout.wnrm = normalize(mul(vin.nrm, wrot));
    float4 vp = mul(wp, View);
    vout.pos = mul(vp, Proj);
    vout.uv = vin.uv;
    float3 wt = normalize(mul(vin.tan.xyz, wrot));
    vout.wtan = float4(wt, vin.tan.w);
    float3 wb = cross(vout.wnrm, wt) * vin.tan.w;
    float3 camW = -mul(View[3].xyz, transpose((float3x3)View));
    float3 toCam = camW - wp.xyz;
    vout.vts = float3(dot(toCam, wt), dot(toCam, wb), dot(toCam, vout.wnrm));
    return vout;
}
