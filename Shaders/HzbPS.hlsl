cbuffer HzbCB : register(b0)
{
    float4 HzbInfo;
    float4 HzbCamPos;
};
Texture2D SrcTX : register(t0);
SamplerState LinearSampler : register(s0);
struct PSIn
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};
float4 main(PSIn pin) : SV_Target
{
    float d = 60000.0;
    if (HzbInfo.x < 0.5) {
        float3 wpos = SrcTX.Sample(LinearSampler, pin.uv).xyz;
        if (dot(wpos, wpos) > 1e-8) {
            d = length(wpos - HzbCamPos.xyz);
        }
    } else {
        float2 t = HzbInfo.yz;
        float d0 = SrcTX.SampleLevel(LinearSampler, pin.uv + float2(-0.5 * t.x, -0.5 * t.y), 0).r;
        float d1 = SrcTX.SampleLevel(LinearSampler, pin.uv + float2(0.5 * t.x, -0.5 * t.y), 0).r;
        float d2 = SrcTX.SampleLevel(LinearSampler, pin.uv + float2(-0.5 * t.x, 0.5 * t.y), 0).r;
        float d3 = SrcTX.SampleLevel(LinearSampler, pin.uv + float2(0.5 * t.x, 0.5 * t.y), 0).r;
        d = min(min(d0, d1), min(d2, d3));
    }
    return float4(d, d, d, 1.0);
}
