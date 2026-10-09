cbuffer DebugCB : register(b0)
{
    float4 DebugMode;
    row_major float4x4 DebugPrevVP;
    float4 DebugCamPos;
    float4 DebugProbeMin[4];
    float4 DebugProbeMax[4];
};
Texture2D AlbedoTX : register(t0);
Texture2D NormalRoughTX : register(t1);
Texture2D MetallicTX : register(t2);
Texture2D PositionTX : register(t3);
Texture2D AoTX : register(t4);
Texture2D EmissiveTX : register(t5);
Texture2D LightmapTX : register(t6);
Texture2D ShadowAtlasTX : register(t7);
Texture2D OverdrawTX : register(t8);
Texture2D UvTX : register(t9);
Texture2D LightTX : register(t10);
SamplerState LinearSampler : register(s0);
struct PSIn
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};
float3 Pal(float t)
{
    return 0.5 + 0.5 * cos(6.2831853 * (t + float3(0.0, 0.33, 0.67)));
}
float4 main(PSIn pin) : SV_Target
{
    float mode = DebugMode.x;
    float3 alb = AlbedoTX.Sample(LinearSampler, pin.uv).rgb;
    float4 nr = NormalRoughTX.Sample(LinearSampler, pin.uv);
    float4 metal = MetallicTX.Sample(LinearSampler, pin.uv);
    float3 wpos = PositionTX.Sample(LinearSampler, pin.uv).xyz;
    float hasPos = dot(wpos, wpos) > 1e-8 ? 1.0 : 0.0;
    if (mode < 1.5) {
        return float4(LightTX.Sample(LinearSampler, pin.uv).rgb, 1.0);
    } else if (mode < 2.5) {
        return float4(alb, 1.0);
    } else if (mode < 3.5) {
        float ao = AoTX.Sample(LinearSampler, pin.uv).r;
        return float4(alb * ao, 1.0);
    } else if (mode < 4.5) {
        return float4(normalize(nr.xyz) * 0.5 + 0.5, 1.0);
    } else if (mode < 5.5) {
        return float4(nr.a, nr.a, nr.a, 1.0);
    } else if (mode < 6.5) {
        return float4(metal.r, metal.r, metal.r, 1.0);
    } else if (mode < 7.5) {
        float ao2 = AoTX.Sample(LinearSampler, pin.uv).r;
        return float4(ao2, ao2, ao2, 1.0);
    } else if (mode < 8.5) {
        return float4(EmissiveTX.Sample(LinearSampler, pin.uv).rgb, 1.0);
    } else if (mode < 9.5) {
        float d = length(wpos - DebugCamPos.xyz) * hasPos;
        float n = max(DebugMode.y, 0.01);
        float f = max(DebugMode.z, n + 0.01);
        return float4(saturate((d - n) / (f - n)), saturate((d - n) / (f - n)), saturate((d - n) / (f - n)), 1.0);
    } else if (mode < 10.5) {
        float4 pp = mul(float4(wpos, 1.0), DebugPrevVP);
        float2 puv = float2(-10.0, -10.0);
        float2 cuv = pin.uv;
        if (pp.w > 1e-5) {
            puv = pp.xy / pp.w * float2(0.5, -0.5) + 0.5;
        }
        float2 vel = (cuv - puv) * hasPos;
        float m = length(vel);
        return float4(saturate(abs(vel.x) * 400.0), saturate(abs(vel.y) * 400.0), saturate(m * 400.0), 1.0);
    } else if (mode < 11.5) {
        float sd = ShadowAtlasTX.SampleLevel(LinearSampler, pin.uv, 0).r;
        return float4(sd, sd, sd, 1.0);
    } else if (mode < 12.5) {
        float3 pc = float3(0.0, 0.0, 0.0);
        for (int pi = 0; pi < 4; ++pi) {
            float3 bmin = DebugProbeMin[pi].xyz;
            float3 bmax = DebugProbeMax[pi].xyz;
            if (bmax.x > bmin.x && bmax.y > bmin.y && bmax.z > bmin.z) {
                if (all(wpos >= bmin) && all(wpos <= bmax)) {
                    pc = Pal((float)pi * 0.25);
                    break;
                }
            }
        }
        return float4(pc * hasPos, 1.0);
    } else if (mode < 13.5) {
        return float4(LightmapTX.Sample(LinearSampler, pin.uv).rgb, 1.0);
    } else if (mode < 14.5) {
        float mid = clamp(metal.a * 255.0, 0.0, 255.0) / 255.0;
        return float4(Pal(mid) * (0.25 + 0.75 * hasPos), 1.0);
    } else if (mode < 15.5) {
        float o = OverdrawTX.Sample(LinearSampler, pin.uv).r;
        float3 heat = float3(0.0, 0.0, 0.0);
        heat = lerp(heat, float3(1.0, 0.1, 0.0), saturate(o * 3.0));
        heat = lerp(heat, float3(1.0, 0.9, 0.1), saturate(o * 3.0 - 1.0));
        heat = lerp(heat, float3(1.0, 1.0, 1.0), saturate(o * 3.0 - 2.0));
        return float4(heat, 1.0);
    } else if (mode < 16.5) {
        float2 u = UvTX.Sample(LinearSampler, pin.uv).rg;
        return float4(frac(u), 0.0, 1.0);
    }
    return float4(LightTX.Sample(LinearSampler, pin.uv).rgb, 1.0);
}
