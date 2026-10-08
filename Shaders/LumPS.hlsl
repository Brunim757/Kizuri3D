cbuffer LumCB : register(b0)
{
    int Mode;
    float AdaptBlend;
    float KeyValue;
    float TileU;
    float TileV;
    float3 LumPad;
};
Texture2D SrcTX : register(t0);
Texture2D PrevTX : register(t1);
SamplerState LinearSampler : register(s0);
struct PSIn
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};
float TileAvgLuma(float2 uv, float2 tile)
{
    float acc = 0.0;
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            float2 off = float2((float(x) - 1.5) * 0.5 * tile.x, (float(y) - 1.5) * 0.5 * tile.y);
            acc += dot(SrcTX.SampleLevel(LinearSampler, uv + off, 0).rgb, float3(0.2126, 0.7152, 0.0722));
        }
    }
    return acc / 16.0;
}
float TileAvgLog(float2 uv, float2 tile)
{
    float acc = 0.0;
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            float2 off = float2((float(x) - 1.5) * 0.5 * tile.x, (float(y) - 1.5) * 0.5 * tile.y);
            acc += SrcTX.SampleLevel(LinearSampler, uv + off, 0).r;
        }
    }
    return acc / 16.0;
}
float4 main(PSIn pin) : SV_Target
{
    if (Mode == 0) {
        float l = TileAvgLuma(pin.uv, float2(TileU, TileV));
        return float4(log(max(l, 1e-4)), 0.0, 0.0, 1.0);
    } else if (Mode == 1) {
        float l = TileAvgLog(pin.uv, float2(TileU, TileV));
        return float4(l, 0.0, 0.0, 1.0);
    }
    float l = TileAvgLog(pin.uv, float2(1.0, 1.0));
    float target = KeyValue / max(exp(l), 1e-4);
    float prev = PrevTX.SampleLevel(LinearSampler, float2(0.5, 0.5), 0).r;
    if (prev < 1e-4) {
        prev = target;
    }
    float adapted = lerp(prev, target, saturate(AdaptBlend));
    return float4(adapted, adapted, adapted, 1.0);
}
