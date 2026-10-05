Texture2D SsgiTX : register(t0);
Texture2D PositionTX : register(t1);
Texture2D NormalTX : register(t2);
SamplerState LinearSampler : register(s0);
cbuffer BlurCB : register(b0)
{
    float4 BlurParams;
};
struct PSIn
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};
float4 main(PSIn pin) : SV_Target
{
    float2 dir = BlurParams.xy;
    float radius = max(BlurParams.z, 1e-4);
    float3 Pc = PositionTX.Sample(LinearSampler, pin.uv).xyz;
    float3 Nc = normalize(NormalTX.Sample(LinearSampler, pin.uv).xyz);
    float3 acc = SsgiTX.Sample(LinearSampler, pin.uv).rgb * 0.227027;
    float wsum = 0.227027;
    float2 o1 = dir * 1.384615;
    float2 o2 = dir * 3.230769;
    float2 offs[4] = { o1, -o1, o2, -o2 };
    float ws[4] = { 0.316216, 0.316216, 0.070270, 0.070270 };
    for (int k = 0; k < 4; ++k) {
        float2 suv = pin.uv + offs[k];
        float3 gi = SsgiTX.Sample(LinearSampler, suv).rgb;
        float3 Pt = PositionTX.Sample(LinearSampler, suv).xyz;
        float3 Nt = normalize(NormalTX.Sample(LinearSampler, suv).xyz);
        float nw = saturate(dot(Nc, Nt) * 0.5 + 0.5);
        float3 dd = Pt - Pc;
        float dw = exp(-dot(dd, dd) / (radius * radius));
        float w = ws[k] * nw * dw;
        acc += gi * w;
        wsum += w;
    }
    return float4(acc / max(wsum, 1e-4), 1.0);
}
