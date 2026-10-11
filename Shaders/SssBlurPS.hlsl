cbuffer SssCB : register(b0)
{
    float4 SssInfo;
};
Texture2D LitTX : register(t0);
Texture2D F0TXs : register(t1);
Texture2D MatTableTX : register(t2);
SamplerState LinearSampler : register(s0);
struct PSIn
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};
float4 main(PSIn pin) : SV_Target
{
    float3 orig = LitTX.Sample(LinearSampler, pin.uv).rgb;
    float4 metalFull = F0TXs.SampleLevel(LinearSampler, pin.uv, 0);
    uint matID = (uint) (metalFull.a * 255.0 + 0.5);
    if (matID > 255u) {
        matID = 255u;
    }
    float4 sssInfo = MatTableTX.Load(int3((int) matID, 1, 0));
    float k = sssInfo.a;
    if (k < 0.004) {
        return float4(orig, 1.0);
    }
    float2 texel = SssInfo.xy;
    float3 acc = orig * 0.2270270270;
    float2 o1 = texel * 1.3846153846;
    float2 o2 = texel * 3.2307692308;
    acc += LitTX.Sample(LinearSampler, pin.uv + o1).rgb * 0.3162162162;
    acc += LitTX.Sample(LinearSampler, pin.uv - o1).rgb * 0.3162162162;
    acc += LitTX.Sample(LinearSampler, pin.uv + o2).rgb * 0.0702702703;
    acc += LitTX.Sample(LinearSampler, pin.uv - o2).rgb * 0.0702702703;
    float3 outc = orig * (1.0 - k) + acc * lerp(float3(1.0, 1.0, 1.0), sssInfo.rgb, 0.75) * k;
    return float4(outc, 1.0);
}
