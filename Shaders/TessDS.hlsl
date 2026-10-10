cbuffer GeoCB : register(b0)
{
    row_major float4x4 World;
    row_major float4x4 View;
    row_major float4x4 Proj;
};
cbuffer TessCB : register(b1)
{
    float4 TessInfo;
};
Texture2D HeightTex : register(t0);
SamplerState TessSamp : register(s0);
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
struct DSOut
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
struct HSTess
{
    float edge[3] : SV_TessFactor;
    float inside : SV_InsideTessFactor;
};
[domain("tri")]
DSOut main(HSTess input, float3 bary : SV_DomainLocation, const OutputPatch<VSOut, 3> patch)
{
    DSOut o;
    float3 wpos = patch[0].wpos * bary.x + patch[1].wpos * bary.y + patch[2].wpos * bary.z;
    float3 wnrm = normalize(patch[0].wnrm * bary.x + patch[1].wnrm * bary.y + patch[2].wnrm * bary.z);
    float2 uv = patch[0].uv * bary.x + patch[1].uv * bary.y + patch[2].uv * bary.z;
    float h = HeightTex.SampleLevel(TessSamp, uv, 0).r;
    float hasH = TessInfo.z;
    h = lerp(0.5, h, hasH);
    wpos += wnrm * ((h - 0.5) * TessInfo.y);
    o.wpos = wpos;
    o.wnrm = wnrm;
    float4 wp = float4(wpos, 1.0);
    float4 vp = mul(wp, View);
    o.pos = mul(vp, Proj);
    o.uv = uv;
    o.wtan = patch[0].wtan * bary.x + patch[1].wtan * bary.y + patch[2].wtan * bary.z;
    o.vts = patch[0].vts * bary.x + patch[1].vts * bary.y + patch[2].vts * bary.z;
    o.luv = patch[0].luv * bary.x + patch[1].luv * bary.y + patch[2].luv * bary.z;
    o.uv2 = patch[0].uv2 * bary.x + patch[1].uv2 * bary.y + patch[2].uv2 * bary.z;
    return o;
}
