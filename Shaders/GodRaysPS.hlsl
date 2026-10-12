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
float4 main(PSIn pin, out float4 depthOut : SV_Target1) : SV_Target0
{
    float3 base = SceneTX.Sample(LinearSampler, pin.uv).rgb;
    float3 wposC = PositionTX.Sample(LinearSampler, pin.uv).xyz;
    float cdepth = dot(wposC - GodCamPos.xyz, GodCamFwd.xyz);
    depthOut = float4(cdepth, cdepth, cdepth, 1.0);
    float sunOff = (GodParams.x < -0.05 || GodParams.x > 1.05 || GodParams.y < -0.05 || GodParams.y > 1.05) ? 1.0 : 0.0;
    float2 sunClamped = clamp(GodParams.xy, 0.0, 1.0);
    float2 stx = GodParams2.zw;
    float3 sunCol = SceneTX.SampleLevel(LinearSampler, sunClamped, 0).rgb * 0.25;
    sunCol += SceneTX.SampleLevel(LinearSampler, sunClamped + float2(stx.x, 0.0), 0).rgb * 0.25;
    sunCol += SceneTX.SampleLevel(LinearSampler, sunClamped + float2(0.0, stx.y), 0).rgb * 0.25;
    sunCol += SceneTX.SampleLevel(LinearSampler, sunClamped + stx, 0).rgb * 0.25;
    float sunLum = dot(sunCol, float3(0.2126, 0.7152, 0.0722));
    if (sunOff > 0.5 || sunLum < 0.02) {
        return float4(base, 1.0);
    }
    float2 sunUV = GodParams.xy;
    float density = GodParams.z;
    float weight = GodParams.w;
    float decay = GodParams2.x;
    float exposure = GodParams2.y;
    float2 delta = (pin.uv - sunUV) * density / 32.0;
    if (dot(delta, delta) < 1e-10) {
        return float4(base, 1.0);
    }
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
    float sunDist = length(pin.uv - sunUV);
    float fall = saturate(1.0 - sunDist * 1.1);
    float sunScale = clamp(sunLum * 2.0, 0.0, 2.0);
    float3 outc = base + acc / 32.0 * exposure * 0.25 * fall * sunScale;
    return float4(outc, 1.0);
}
