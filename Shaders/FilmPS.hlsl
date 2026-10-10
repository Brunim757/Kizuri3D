cbuffer PostCB : register(b0)
{
    row_major float4x4 PrevVP;
    float4 CamPos;
    float4 Focus;
    float4 Film;
    float4 Toggles;
    row_major float4x4 CurVP;
    float4 Shimmer;
};
Texture2D FrameTX : register(t0);
Texture2D PositionTX : register(t1);
Texture2D SsgiTX : register(t2);
SamplerState LinearSampler : register(s0);
struct PSIn
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};
static const float2 Poisson8[8] = {
    float2(-0.9428, -0.3997),
    float2(0.9456, -0.7689),
    float2(-0.0942, -0.9294),
    float2(0.3450, 0.2939),
    float2(-0.9159, 0.4577),
    float2(-0.8154, -0.8791),
    float2(-0.3828, 0.2768),
    float2(0.9748, 0.1661)
};
float Hash21(float2 p)
{
    float3 p3 = frac(float3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return frac((p3.x + p3.y) * p3.z);
}
float4 main(PSIn pin) : SV_Target
{
    float2 suv = pin.uv;
    if (Shimmer.x > 0.001) {
        float lum0 = dot(FrameTX.SampleLevel(LinearSampler, pin.uv, 0).rgb, float3(0.2126, 0.7152, 0.0722));
        float m = saturate((lum0 - 0.6) / 0.4);
        float t = Film.x * 0.05;
        float2 wob = float2(sin(pin.uv.y * 180.0 + t * 7.0) + sin(pin.uv.y * 61.0 - t * 5.0), cos(pin.uv.x * 150.0 - t * 6.0));
        float2 px0 = float2(1.0 / max(Focus.z, 1.0), 1.0 / max(Focus.w, 1.0));
        suv += wob * px0 * 2.0 * Shimmer.x * m;
    }
    float3 col = FrameTX.Sample(LinearSampler, suv).rgb;
    if (Toggles.w > 0.5) {
        col += SsgiTX.Sample(LinearSampler, pin.uv).rgb;
    }
    float3 wpos = PositionTX.Sample(LinearSampler, pin.uv).xyz;
    float hasPos = dot(wpos, wpos) > 1e-8 ? 1.0 : 0.0;
    float2 px = float2(1.0 / max(Focus.z, 1.0), 1.0 / max(Focus.w, 1.0));
    if (Toggles.x > 0.5 && hasPos > 0.5) {
        float depth = length(wpos - CamPos.xyz);
        float coc = saturate(abs(depth - Focus.x) / max(Focus.y, 1e-3));
        if (coc > 0.01) {
            float3 acc = col;
            for (int i = 0; i < 8; ++i) {
                acc += FrameTX.SampleLevel(LinearSampler, pin.uv + Poisson8[i] * coc * 12.0 * px, 0).rgb;
            }
            col = acc / 9.0;
        }
    }
    if (Toggles.y > 0.5 && hasPos > 0.5) {
        float4 pp = mul(float4(wpos, 1.0), PrevVP);
        float4 cp = mul(float4(wpos, 1.0), CurVP);
        float2 puv = float2(-10.0, -10.0);
        float2 cuv = pin.uv;
        if (pp.w > 1e-5 && cp.w > 1e-5) {
            puv = pp.xy / pp.w * float2(0.5, -0.5) + 0.5;
            cuv = cp.xy / cp.w * float2(0.5, -0.5) + 0.5;
        }
        float2 vel = (cuv - puv) * Toggles.z;
        float vl = length(vel);
        float minVel = (px.x + px.y) * 0.75;
        if (vl > minVel && vl < 0.05) {
            float3 macc = col;
            for (int j = 1; j < 6; ++j) {
                macc += FrameTX.SampleLevel(LinearSampler, pin.uv - vel * (float) j / 5.0, 0).rgb;
            }
            col = macc / 6.0;
        }
    }
    float2 center = pin.uv - 0.5;
    float r2 = dot(center, center);
    if (Film.y > 1e-5) {
        float2 off = center * r2 * Film.y * 0.05;
        float3 cac;
        cac.r = FrameTX.Sample(LinearSampler, pin.uv + off).r;
        cac.g = col.g;
        cac.b = FrameTX.Sample(LinearSampler, pin.uv - off).b;
        col = cac;
    }
    if (Film.z > 1e-5) {
        float vig = 1.0 - Film.z * smoothstep(0.25, 1.0, length(center) * 1.4);
        col *= vig;
    }
    if (Film.w > 1e-5) {
        float g = Hash21(pin.uv * 913.0 + frac(Film.x) * 7.0) - 0.5;
        col += g * Film.w;
    }
    return float4(col, 1.0);
}
