cbuffer TessCB : register(b0)
{
    float4 TessInfo;
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
struct HSTess
{
    float edge[3] : SV_TessFactor;
    float inside : SV_InsideTessFactor;
};
HSTess TessFactors(InputPatch<VSOut, 3> patch)
{
    HSTess o;
    float f = clamp(TessInfo.x, 1.0, 64.0);
    o.edge[0] = f;
    o.edge[1] = f;
    o.edge[2] = f;
    o.inside = f;
    return o;
}
[domain("tri")]
[partitioning("fractional_odd")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
[patchconstantfunc("TessFactors")]
VSOut main(InputPatch<VSOut, 3> patch, uint cpid : SV_OutputControlPointID)
{
    return patch[cpid];
}
