Texture2D SrcTX : register(t0);
SamplerState LinearSampler : register(s0);
cbuffer TaaSharpCB : register(b0)
{
    float4 SharpInfo;
};
struct PSIn
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};
float Luma(float3 c)
{
    return dot(c, float3(0.2126, 0.7152, 0.0722));
}
float4 main(PSIn pin) : SV_Target
{
    float strength = clamp(SharpInfo.x, 0.0, 1.0);
    float2 texel = SharpInfo.yz;
    float3 c = SrcTX.SampleLevel(LinearSampler, pin.uv, 0).rgb;
    float3 n = SrcTX.SampleLevel(LinearSampler, pin.uv + float2(0.0, -texel.y), 0).rgb;
    float3 s = SrcTX.SampleLevel(LinearSampler, pin.uv + float2(0.0, texel.y), 0).rgb;
    float3 e = SrcTX.SampleLevel(LinearSampler, pin.uv + float2(texel.x, 0.0), 0).rgb;
    float3 w = SrcTX.SampleLevel(LinearSampler, pin.uv + float2(-texel.x, 0.0), 0).rgb;
    float3 avg = (n + s + e + w) * 0.25;
    float adapt = 1.0 - saturate(abs(Luma(c) - Luma(avg)) * 4.0);
    float3 outc = c + (c - avg) * strength * (0.35 + 0.65 * adapt);
    return float4(outc, 1.0);
}
