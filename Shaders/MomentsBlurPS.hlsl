Texture2D SrcTX : register(t0);
SamplerState LinearSampler : register(s0);
cbuffer MomentsBlurCB : register(b0)
{
    float4 BlurDir;
};
struct PSIn
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};
float4 main(PSIn pin) : SV_Target
{
    float2 dir = BlurDir.xy;
    float4 c = SrcTX.Sample(LinearSampler, pin.uv) * 0.227027;
    float2 o1 = dir * 1.384615;
    float2 o2 = dir * 3.230769;
    c += SrcTX.Sample(LinearSampler, pin.uv + o1) * 0.316216;
    c += SrcTX.Sample(LinearSampler, pin.uv - o1) * 0.316216;
    c += SrcTX.Sample(LinearSampler, pin.uv + o2) * 0.070270;
    c += SrcTX.Sample(LinearSampler, pin.uv - o2) * 0.070270;
    return c;
}
