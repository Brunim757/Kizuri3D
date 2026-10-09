cbuffer GodCB : register(b0)
{
    float4 GodParams;
    float4 GodParams2;
    float4 GodCamPos;
    float4 GodCamFwd;
};
Texture2D SceneTX : register(t0);
Texture2D PositionTX : register(t1);
SamplerState LinearSampler : register(s0);
struct PSIn
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};
float4 main(PSIn pin) : SV_Target
{
    float3 base = SceneTX.Sample(LinearSampler, pin.uv).rgb;
    float2 sunUV = GodParams.xy;
    float density = GodParams.z;
    float weight = GodParams.w;
    float decay = GodParams2.x;
    float exposure = GodParams2.y;
    float2 delta = (pin.uv - sunUV) * density / 32.0;
    if (dot(delta, delta) < 1e-10) {
        return float4(base, 1.0);
    }
    float3 wposC = PositionTX.Sample(LinearSampler, pin.uv).xyz;
    float cdepth = dot(wposC - GodCamPos.xyz, GodCamFwd.xyz);
    float dth = max(abs(cdepth) * 0.03, 0.5);
    float2 uv = pin.uv;
    float illum = 1.0;
    float3 acc = float3(0.0, 0.0, 0.0);
    for (int i = 0; i < 32; ++i) {
        uv -= delta;
        float2 cuv = clamp(uv, 0.0, 1.0);
        float3 tap = SceneTX.SampleLevel(LinearSampler, cuv, 0).rgb;
        float3 wposT = PositionTX.SampleLevel(LinearSampler, cuv, 0).xyz;
        float tdepth = dot(wposT - GodCamPos.xyz, GodCamFwd.xyz);
        float occ = 1.0;
        if (dot(wposT, wposT) > 1e-8 && dot(wposC, wposC) > 1e-8) {
            occ = saturate((cdepth - tdepth) / dth * 0.5 + 0.5);
        }
        acc += tap * illum * occ * weight;
        illum *= decay;
    }
    float3 outc = base + acc / 32.0 * exposure;
    return float4(outc, 1.0);
}
