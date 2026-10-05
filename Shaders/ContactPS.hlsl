cbuffer SsrCB : register(b0)
{
    row_major float4x4 ViewProj;
    float4 CamPos;
    float4 Params;
    float4 Screen;
};
Texture2D PositionTX : register(t0);
Texture2D NormalRoughTX : register(t1);
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
        return float4(1.0, 1.0, 1.0, 1.0);
    }
    float3 N = normalize(NormalRoughTX.Sample(LinearSampler, pin.uv).xyz);
    float3 up = abs(N.y) > 0.99 ? float3(1.0, 0.0, 0.0) : float3(0.0, 1.0, 0.0);
    float3 T = normalize(cross(up, N));
    float3 B = cross(N, T);
    float stepLen = Params.y / max(Params.x, 1.0);
    float occ = 0.0;
    float rays = 0.0;
    for (int k = 0; k < 8; ++k) {
        if ((float) k >= Params.z) {
            break;
        }
        rays += 1.0;
        float3 rd = normalize(T * Poisson8[k].x + B * Poisson8[k].y + N * 0.6);
        float3 march = wpos + N * 0.02;
        for (int i = 0; i < 4; ++i) {
            if ((float) i >= Params.x) {
                break;
            }
            march += rd * stepLen;
            float4 sp = mul(float4(march, 1.0), ViewProj);
            if (sp.w <= 1e-5) {
                break;
            }
            float2 suv = sp.xy / sp.w * float2(0.5, -0.5) + 0.5;
            if (suv.x < 0.0 || suv.x > 1.0 || suv.y < 0.0 || suv.y > 1.0) {
                break;
            }
            float3 scene = PositionTX.SampleLevel(LinearSampler, suv, 0).xyz;
            if (dot(scene, scene) < 1e-8) {
                continue;
            }
            float marchD = length(march - CamPos.xyz);
            float sceneD = length(scene - CamPos.xyz);
            if (marchD > sceneD + 0.02) {
                float3 sceneN = normalize(NormalRoughTX.SampleLevel(LinearSampler, suv, 0).xyz);
                if (dot(sceneN, -rd) <= 0.2) {
                    continue;
                }
                occ += 1.0 - (float) i / max(Params.x, 1.0) * 0.5;
                break;
            }
        }
    }
    float contact = 1.0;
    if (rays > 0.5) {
        contact = 1.0 - clamp(occ / rays, 0.0, 1.0) * Params.w;
    }
    return float4(contact, contact, contact, 1.0);
}
