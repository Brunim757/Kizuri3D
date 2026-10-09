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
};
struct VSOut
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};
VSOut main(VSIn vin)
{
    VSOut o;
    float4 wp = mul(float4(vin.pos, 1.0), World);
    float4 vp = mul(wp, View);
    o.pos = mul(vp, Proj);
    o.uv = vin.uv;
    return o;
}
