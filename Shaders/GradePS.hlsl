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
float3 AgXFull(float3 color)
{
    float3x3 toRec2020 = float3x3(
        float3(0.6274, 0.0691, 0.0164),
        float3(0.3293, 0.9195, 0.0880),
        float3(0.0433, 0.0113, 0.8956));
    float3x3 inset = float3x3(
        float3(0.856627153315983, 0.137318972929847, 0.11189821299995),
        float3(0.0951212405381588, 0.761241990602591, 0.0767994186031903),
        float3(0.0482516061458583, 0.101439036467562, 0.811302368396859));
    float3x3 outset = float3x3(
        float3(1.1271005818144368, -0.1413297634984383, -0.14132976349843826),
        float3(-0.11060664309660323, 1.157823702216272, -0.11060664309660294),
        float3(-0.016493938717834573, -0.016493938717834257, 1.2519364065950405));
    float3x3 toSrgb = float3x3(
        float3(1.6605, -0.1246, -0.0182),
        float3(-0.5876, 1.1329, -0.1006),
        float3(-0.0728, -0.0083, 1.1187));
    color = mul(color, toRec2020);
    color = mul(color, inset);
    color = max(color, 1e-10);
    color = log2(color);
    color = (color + 12.47393) / (4.026069 + 12.47393);
    color = saturate(color);
    color = AgXApprox(color);
    color = mul(color, outset);
    color = pow(max(color, 0.0), 2.2);
    color = mul(color, toSrgb);
    return saturate(color);
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
        c = AgXFull(c);
    } else {
        c = pow(saturate(c), 1.0 / 2.2);
    }
    return float4(c, 1.0);
}
