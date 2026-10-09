cbuffer IrrCB : register(b0)
{
    int FaceIndex;
    float3 IrrPad;
};
TextureCube EnvTX : register(t0);
SamplerState LinearSampler : register(s0);
struct PSIn
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};
float3 FaceDirection(int face, float2 uv)
{
    float2 p = float2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);
    if (face == 0) {
        return normalize(float3(1.0, p.y, -p.x));
    } else if (face == 1) {
        return normalize(float3(-1.0, p.y, p.x));
    } else if (face == 2) {
        return normalize(float3(p.x, 1.0, -p.y));
    } else if (face == 3) {
        return normalize(float3(p.x, -1.0, p.y));
    } else if (face == 4) {
        return normalize(float3(p.x, p.y, 1.0));
    }
    return normalize(float3(-p.x, p.y, -1.0));
}
float4 main(PSIn pin) : SV_Target
{
    float3 n = FaceDirection(FaceIndex, pin.uv);
    float3 up = abs(n.y) < 0.999 ? float3(0.0, 1.0, 0.0) : float3(1.0, 0.0, 0.0);
    float3 t = normalize(cross(up, n));
    float3 b = cross(n, t);
    float3 acc = float3(0.0, 0.0, 0.0);
    float wsum = 0.0;
    int steps = 8;
    for (int a = 0; a < steps; ++a) {
        for (int e = 0; e < steps; ++e) {
            float phi = 6.28318530718 * (float(a) + 0.5) / float(steps);
            float theta = 1.57079632679 * (float(e) + 0.5) / float(steps);
            float3 tap = t * (cos(phi) * sin(theta)) + b * (sin(phi) * sin(theta)) + n * cos(theta);
            float wgt = cos(theta) * sin(theta);
            acc += EnvTX.SampleLevel(LinearSampler, tap, 0).rgb * wgt;
            wsum += wgt;
        }
    }
    float3 irr = wsum > 1e-6 ? acc / wsum * 3.14159265359 : float3(0.0, 0.0, 0.0);
    return float4(irr, 1.0);
}
