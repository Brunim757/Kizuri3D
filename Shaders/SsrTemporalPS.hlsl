Texture2D SsrTX : register(t0);
Texture2D SsrHistTX : register(t1);
Texture2D PositionTX : register(t2);
SamplerState LinearSampler : register(s0);
cbuffer SsrTemporalCB : register(b0)
{
    row_major float4x4 PrevVP;
    float4 SsrTParams;
    float4 SsrTCamPos;
};
struct PSIn
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};
struct PSOut
{
    float4 acc : SV_Target0;
    float4 hist : SV_Target1;
};
PSOut main(PSIn pin)
{
    PSOut o;
    float4 cur = SsrTX.Sample(LinearSampler, pin.uv);
    float blend = clamp(SsrTParams.x, 0.0, 0.95);
    float3 P = PositionTX.Sample(LinearSampler, pin.uv).xyz;
    float curD = length(P - SsrTCamPos.xyz);
    float3 acc = cur.rgb;
    if (dot(P, P) > 1e-8) {
        float4 prev = mul(float4(P, 1.0), PrevVP);
        if (prev.w > 1e-5) {
            float2 huv = prev.xy / prev.w * float2(0.5, -0.5) + 0.5;
            if (huv.x >= 0.0 && huv.x <= 1.0 && huv.y >= 0.0 && huv.y <= 1.0) {
                float4 hist = SsrHistTX.Sample(LinearSampler, huv);
                float depthGate = 1.0 - saturate(abs(curD - hist.a) / max(curD * 0.1, 0.05));
                float wgt = blend * depthGate * step(0.001, hist.a);
                acc = lerp(cur.rgb, hist.rgb, wgt);
            }
        }
    }
    o.acc = float4(acc, cur.a);
    o.hist = float4(acc, curD);
    return o;
}
