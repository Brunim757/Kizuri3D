cbuffer TaaCB : register(b0)
{
    row_major float4x4 PrevVP;
    float4 CamPos;
    float4 CamFwd;
    float4 TaaMisc;
};
Texture2D CurrentTX : register(t0);
Texture2D HistoryTX : register(t1);
Texture2D PositionTX : register(t2);
SamplerState LinearSampler : register(s0);
struct PSIn
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};
float3 ClipAABB(float3 hist, float3 mn, float3 mx)
{
    float3 c = (mn + mx) * 0.5;
    float3 e = (mx - mn) * 0.5 + 1e-4;
    float3 v = hist - c;
    float3 av = abs(v);
    float m = max(max(av.x / e.x, av.y / e.y), av.z / e.z);
    if (m > 1.0) {
        v /= m;
    }
    return c + v;
}
float4 main(PSIn pin) : SV_Target
{
    float3 wpos = PositionTX.Sample(LinearSampler, pin.uv).xyz;
    float3 curr = CurrentTX.Sample(LinearSampler, pin.uv).rgb;
    float valid = TaaMisc.x;
    float blend = TaaMisc.y;
    if (valid < 0.5) {
        return float4(curr, 1.0);
    }
    float4 pp = mul(float4(wpos, 1.0), PrevVP);
    float2 puv = float2(-10.0, -10.0);
    float pdepth = 1e9;
    if (pp.w > 1e-5) {
        puv = pp.xy / pp.w * float2(0.5, -0.5) + 0.5;
        pdepth = dot(wpos - CamPos.xyz, CamFwd.xyz);
    }
    float isSky = dot(wpos, wpos) < 1e-8 ? 1.0 : 0.0;
    if (isSky > 0.5) {
        puv = pin.uv;
    }
    float3 hist = CurrentTX.Sample(LinearSampler, pin.uv).rgb;
    float hdepth = pdepth;
    if (puv.x >= 0.0 && puv.x <= 1.0 && puv.y >= 0.0 && puv.y <= 1.0) {
        float4 hs = HistoryTX.Sample(LinearSampler, puv);
        hist = hs.rgb;
        hdepth = hs.a;
    }
    float cdepth = dot(wpos - CamPos.xyz, CamFwd.xyz);
    float dth = max(abs(cdepth) * 0.05, 0.1);
    float useHist = 1.0;
    if (isSky < 0.5 && abs(cdepth - hdepth) > dth) {
        useHist = 0.0;
    }
    float3 mn = curr;
    float3 mx = curr;
    float2 px = TaaMisc.zw;
    for (int oy = -1; oy <= 1; ++oy) {
        for (int ox = -1; ox <= 1; ++ox) {
            float3 s = CurrentTX.SampleLevel(LinearSampler, pin.uv + float2(float(ox) * px.x, float(oy) * px.y), 0).rgb;
            mn = min(mn, s);
            mx = max(mx, s);
        }
    }
    float3 clipped = ClipAABB(hist, mn, mx);
    float b = lerp(1.0, blend, useHist);
    float3 outc = lerp(curr, clipped, 1.0 - b);
    return float4(outc, cdepth);
}
