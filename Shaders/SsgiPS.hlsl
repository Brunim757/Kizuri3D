cbuffer SsrCB : register(b0)
{
    row_major float4x4 ViewProj;
    float4 CamPos;
    float4 Params;
    float4 Screen;
};
Texture2D PositionTX : register(t0);
Texture2D NormalRoughTX : register(t1);
Texture2D AlbedoTX : register(t2);
Texture2D LightTX : register(t3);
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
float4 main(PSIn pin) : SV_Target
{
    float3 wpos = PositionTX.Sample(LinearSampler, pin.uv).xyz;
    if (dot(wpos, wpos) < 1e-8) {
        return float4(0.0, 0.0, 0.0, 0.0);
    }
    float4 nr = NormalRoughTX.Sample(LinearSampler, pin.uv);
    float3 N = normalize(nr.xyz);
    float3 alb = AlbedoTX.Sample(LinearSampler, pin.uv).rgb;
    float3 up = abs(N.y) > 0.99 ? float3(1.0, 0.0, 0.0) : float3(0.0, 1.0, 0.0);
    float3 T = normalize(cross(up, N));
    float3 B = cross(N, T);
    float radius = Params.y;
    float3 acc = float3(0.0, 0.0, 0.0);
    float wsum = 0.0;
    for (int k = 0; k < 8; ++k) {
        float3 rd = normalize(T * Poisson8[k].x + B * Poisson8[k].y + N * 0.7);
        float3 tap = wpos + rd * radius;
        float4 sp = mul(float4(tap, 1.0), ViewProj);
        if (sp.w <= 1e-5) {
            continue;
        }
        float2 suv = sp.xy / sp.w * float2(0.5, -0.5) + 0.5;
        if (suv.x < 0.0 || suv.x > 1.0 || suv.y < 0.0 || suv.y > 1.0) {
            continue;
        }
        float3 scene = PositionTX.SampleLevel(LinearSampler, suv, 0).xyz;
        if (dot(scene, scene) < 1e-8) {
            continue;
        }
        float tapD = length(tap - CamPos.xyz);
        float sceneD = length(scene - CamPos.xyz);
        if (sceneD + 0.1 < tapD) {
            continue;
        }
        if (tapD - sceneD > 0.5) {
            continue;
        }
        float3 sceneN = normalize(NormalRoughTX.SampleLevel(LinearSampler, suv, 0).xyz);
        if (dot(sceneN, -rd) <= 0.0) {
            continue;
        }
        float3 li = LightTX.SampleLevel(LinearSampler, suv, 0).rgb;
        float wgt = saturate(dot(rd, N));
        acc += li * wgt;
        wsum += wgt;
    }
    if (wsum < 1e-4) {
        return float4(0.0, 0.0, 0.0, 0.0);
    }
    float3 indirect = alb * acc / wsum * Params.w;
    return float4(indirect, 1.0);
}
