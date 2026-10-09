Texture2D DepthAtlas : register(t0);
SamplerState PointSampler : register(s0);
cbuffer MomentsCB : register(b0)
{
    float4 MomentsInfo;
};
struct PSIn
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};
float4 main(PSIn pin) : SV_Target
{
    float d = DepthAtlas.Sample(PointSampler, pin.uv).r;
    d = clamp(d, 0.0, 1.0);
    if (MomentsInfo.x > 1.5) {
        float c = max(MomentsInfo.y, 0.5);
        float p = exp(c * d);
        float n = exp(-c * d);
        return float4(p, p * p, n, n * n);
    }
    return float4(d, d * d, 0.0, 1.0);
}
