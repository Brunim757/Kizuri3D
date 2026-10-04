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
};
VSOut main(VSIn vin)
{
    VSOut vout;
    float4 wp = mul(float4(vin.pos, 1.0), World);
    vout.wpos = wp.xyz;
    vout.wnrm = normalize(mul(vin.nrm, (float3x3)World));
    float4 vp = mul(wp, View);
    vout.pos = mul(vp, Proj);
    vout.uv = vin.uv;
    vout.wtan = float4(normalize(mul(vin.tan.xyz, (float3x3)World)), vin.tan.w);
    return vout;
}
