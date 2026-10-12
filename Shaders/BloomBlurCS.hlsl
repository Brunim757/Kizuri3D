Texture2D SrcTX : register(t0);
RWTexture2D<float4> DstUAV : register(u0);
SamplerState SrcSampler : register(s0);
cbuffer BloomCB : register(b0)
{
    float4 BloomParams;
};
[numthreads(16, 16, 1)]
void main(uint3 tid : SV_DispatchThreadID)
{
    float2 dims;
    SrcTX.GetDimensions(dims.x, dims.y);
    if ((float) tid.x >= dims.x || (float) tid.y >= dims.y) {
        return;
    }
    float2 uv = (float2(tid.xy) + 0.5) / dims;
    float2 dir = BloomParams.xy;
    float3 c = SrcTX.SampleLevel(SrcSampler, uv, 0).rgb * 0.227027;
    float2 o1 = dir * 1.384615;
    float2 o2 = dir * 3.230769;
    c += SrcTX.SampleLevel(SrcSampler, uv + o1, 0).rgb * 0.316216;
    c += SrcTX.SampleLevel(SrcSampler, uv - o1, 0).rgb * 0.316216;
    c += SrcTX.SampleLevel(SrcSampler, uv + o2, 0).rgb * 0.070270;
    c += SrcTX.SampleLevel(SrcSampler, uv - o2, 0).rgb * 0.070270;
    DstUAV[tid.xy] = float4(c, 1.0);
}
