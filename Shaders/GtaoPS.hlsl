Texture2D PositionTX : register(t0);
Texture2D NormalTX : register(t1);
SamplerState LinearSampler : register(s0);
cbuffer GtaoCB : register(b0)
{
    float4 GtaoParams;
    float4 GtaoCamPos;
    row_major float4x4 GtaoVP;
};
struct PSIn
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};
float3 GtaoSlice(float3 P, float3 N, float3 V, float3 T, float radius, float2 uv, row_major float4x4 vp, out float3 occDir)
{
    occDir = float3(0.0, 0.0, 0.0);
    float bestP = -1.0;
    float bestN = -1.0;
    for (int side = -1; side <= 1; side += 2) {
        for (int st = 1; st <= 4; ++st) {
            float t = radius * (float)st / 4.0;
            float3 Sp = P + T * (float)side * t;
            float4 clip = mul(float4(Sp, 1.0), vp);
            if (clip.w <= 0.0) {
                continue;
            }
            float2 suv = clip.xy / clip.w * float2(0.5, -0.5) + float2(0.5, 0.5);
            if (suv.x < 0.0 || suv.x > 1.0 || suv.y < 0.0 || suv.y > 1.0) {
                continue;
            }
            float3 Sw = PositionTX.SampleLevel(LinearSampler, suv, 0).xyz;
            float3 H = Sw - P;
            float d2 = max(dot(H, H), 1e-8);
            float cosH = dot(H / sqrt(d2), V) - 0.06 * t / sqrt(d2);
            if (side > 0) {
                bestP = max(bestP, cosH);
            } else {
                bestN = max(bestN, cosH);
            }
        }
    }
    float occP = saturate((1.0 - bestP) * 0.5);
    float occN = saturate((1.0 - bestN) * 0.5);
    occDir = T * (occP - occN);
    return float3(occP, occN, 0.0);
}
float4 main(PSIn pin, out float4 bentOut : SV_Target1) : SV_Target0
{
    float radius = max(GtaoParams.x, 1e-4);
    float slices = GtaoParams.z < 2.5 ? 2.0 : 4.0;
    float3 P = PositionTX.Sample(LinearSampler, pin.uv).xyz;
    if (dot(P, P) < 1e-8) {
        bentOut = float4(0.0, 0.0, 1.0, 1.0);
        return float4(1.0, 1.0, 1.0, 1.0);
    }
    float3 N = normalize(NormalTX.Sample(LinearSampler, pin.uv).xyz);
    float3 V = normalize(GtaoCamPos.xyz - P);
    float h0 = frac(sin(dot(pin.uv, float2(12.9898, 78.233))) * 43758.5453);
    float3 up = abs(N.y) > 0.99 ? float3(1.0, 0.0, 0.0) : float3(0.0, 1.0, 0.0);
    float3 T0 = normalize(cross(up, N));
    float3 B0 = cross(N, T0);
    float occ = 0.0;
    float3 bend = float3(0.0, 0.0, 0.0);
    for (int s = 0; s < 4; ++s) {
        if ((float)s >= slices) {
            break;
        }
        float phi = (float)s * 3.14159265 / slices + h0 * 3.14159265;
        float3 T = T0 * cos(phi) + B0 * sin(phi);
        float3 occDir;
        float3 o = GtaoSlice(P, N, V, T, radius, pin.uv, GtaoVP, occDir);
        occ += (o.x + o.y) * 0.5;
        bend += occDir;
    }
    occ /= max(slices, 1.0);
    float ao = saturate(lerp(1.0, occ, clamp(GtaoParams.y, 0.0, 2.0) * 0.5));
    float3 bent = normalize(N * 2.0 - bend * 0.75 + float3(0.0, 0.0, 0.001));
    bentOut = float4(bent * 0.5 + 0.5, 1.0);
    return float4(ao, ao, ao, 1.0);
}
