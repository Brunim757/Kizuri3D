Texture2D SrcTX : register(t0);
Texture2D BloomTX : register(t1);
Texture2D DirtTX : register(t2);
SamplerState LinearSampler : register(s0);
cbuffer BloomCB : register(b0)
{
    float4 BloomParams;
};
struct PSIn
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};
float4 main(PSIn pin) : SV_Target
{
    float3 c = SrcTX.Sample(LinearSampler, pin.uv).rgb;
    float3 b = BloomTX.Sample(LinearSampler, pin.uv).rgb;
    float3 dirt = DirtTX.Sample(LinearSampler, pin.uv).rgb;
    float lum = dot(b, float3(0.2126, 0.7152, 0.0722));
    return float4(c + b * BloomParams.x + dirt * lum * BloomParams.y, 1.0);
}
