cbuffer GeoCB : register(b0)
{
    row_major float4x4 World;
    row_major float4x4 View;
    row_major float4x4 Proj;
};
struct VSIn
{
    float3 pos : POSITION;
};
struct VSOut
{
    float4 pos : SV_Position;
    float3 wpos : TEXCOORD0;
};
VSOut main(VSIn vin)
{
    VSOut vout;
    float4 wp = mul(float4(vin.pos, 1.0), World);
    vout.wpos = wp.xyz;
    float4 vp = mul(wp, View);
    vout.pos = mul(vp, Proj);
    return vout;
}
