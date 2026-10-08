cbuffer GradeCB : register(b0)
{
    float Exposure;
    int ToneMode;
    float2 GradePad;
};
Texture2D HdrTX : register(t0);
SamplerState LinearSampler : register(s0);
struct PSIn
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};
float ACESFilm(float x)
{
    if (x <= 0.0) {
        return 0.0;
    }
    return (x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14);
}
float3 AgXApprox(float3 x)
{
    float3 x2 = x * x;
    float3 x4 = x2 * x2;
    float3 p = 15.5 * x4 * x2 - 40.14 * x4 * x + 31.96 * x4 - 6.868 * x2 * x + 0.4298 * x2 + 0.1191 * x - 0.00232;
    return max(p, 0.0);
}
float4 main(PSIn pin) : SV_Target
{
    float3 c = HdrTX.Sample(LinearSampler, pin.uv).rgb * Exposure;
    if (ToneMode == 0) {
        c = float3(ACESFilm(c.r), ACESFilm(c.g), ACESFilm(c.b));
    } else if (ToneMode == 1) {
        c = c / (1.0 + c);
        c = pow(saturate(c), 1.0 / 2.2);
    } else if (ToneMode == 2) {
        c = AgXApprox(c);
        c = pow(saturate(c), 1.0 / 2.2);
    } else {
        c = pow(saturate(c), 1.0 / 2.2);
    }
    return float4(c, 1.0);
}
