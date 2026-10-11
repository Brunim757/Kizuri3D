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
Texture2D Hzb0TX : register(t3);
Texture2D Hzb1TX : register(t4);
Texture2D Hzb2TX : register(t5);
Texture2D Hzb3TX : register(t6);
SamplerState LinearSampler : register(s0);
struct PSIn
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};
float4 main(PSIn pin) : SV_Target
{
    float3 wpos = PositionTX.Sample(LinearSampler, pin.uv).xyz;
    if (dot(wpos, wpos) < 1e-8) {
        return float4(0.0, 0.0, 0.0, 0.0);
    }
    float4 nr = NormalRoughTX.Sample(LinearSampler, pin.uv);
    float3 N = normalize(nr.xyz);
    float rough = clamp(nr.w, 0.0, 1.0);
    if (rough > Params.z) {
        return float4(0.0, 0.0, 0.0, 0.0);
    }
    float3 V = normalize(CamPos.xyz - wpos);
    float3 R = reflect(-V, N);
    float stepLen = Params.y / max(Params.x, 1.0);
    float t = stepLen;
    int mip = 2;
    float2 hitUv = float2(-1.0, -1.0);
    float found = 0.0;
    for (int i = 0; i < 32; ++i) {
        float3 march = wpos + R * t;
        float4 sp = mul(float4(march, 1.0), ViewProj);
        if (sp.w <= 1e-5) {
            break;
        }
        float2 suv = sp.xy / sp.w * float2(0.5, -0.5) + 0.5;
        if (suv.x < 0.0 || suv.x > 1.0 || suv.y < 0.0 || suv.y > 1.0) {
            break;
        }
        float cellMin = Hzb0TX.SampleLevel(LinearSampler, suv, 0).r;
        if (mip == 1) {
            cellMin = Hzb1TX.SampleLevel(LinearSampler, suv, 0).r;
        } else if (mip == 2) {
            cellMin = Hzb2TX.SampleLevel(LinearSampler, suv, 0).r;
        } else if (mip >= 3) {
            cellMin = Hzb3TX.SampleLevel(LinearSampler, suv, 0).r;
        }
        float rayD = length(march - CamPos.xyz);
        float bias = 0.05 * (float) (1 << min(mip, 3));
        if (rayD > cellMin + bias) {
            if (mip == 0) {
                hitUv = suv;
                found = 1.0;
                break;
            }
            mip--;
        } else {
            t += stepLen * (float) (1 << min(mip, 3));
            if (mip < 2) {
                mip++;
            }
        }
    }
    if (found < 0.5) {
        return float4(0.0, 0.0, 0.0, 0.0);
    }
    float2 texel = float2(1.0 / max(Screen.x, 1.0), 1.0 / max(Screen.y, 1.0));
    float spread = rough * 6.0;
    float3 acc = AlbedoTX.SampleLevel(LinearSampler, hitUv, 0).rgb;
    acc += AlbedoTX.SampleLevel(LinearSampler, hitUv + float2(texel.x * spread, 0.0), 0).rgb;
    acc += AlbedoTX.SampleLevel(LinearSampler, hitUv - float2(texel.x * spread, 0.0), 0).rgb;
    acc += AlbedoTX.SampleLevel(LinearSampler, hitUv + float2(0.0, texel.y * spread), 0).rgb;
    acc += AlbedoTX.SampleLevel(LinearSampler, hitUv - float2(0.0, texel.y * spread), 0).rgb;
    acc *= 0.2;
    float edge = min(min(hitUv.x, 1.0 - hitUv.x), min(hitUv.y, 1.0 - hitUv.y));
    float conf = saturate(edge * 8.0) * (1.0 - rough) * Params.w;
    return float4(acc, conf);
}
