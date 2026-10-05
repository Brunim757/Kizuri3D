cbuffer FxaaCB : register(b0)
{
    float4 RcpFrame;
};
Texture2D FrameTX : register(t0);
SamplerState LinearSampler : register(s0);
struct PSIn
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};
float FxaaLuma(float3 rgb)
{
    return dot(rgb, float3(0.299, 0.587, 0.114));
}
float4 main(PSIn pin) : SV_Target
{
    float2 rcp = RcpFrame.xy;
    float3 rgbM = FrameTX.Sample(LinearSampler, pin.uv).rgb;
    float3 rgbN = FrameTX.SampleLevel(LinearSampler, pin.uv + float2(0.0, -1.0) * rcp, 0).rgb;
    float3 rgbS = FrameTX.SampleLevel(LinearSampler, pin.uv + float2(0.0, 1.0) * rcp, 0).rgb;
    float3 rgbE = FrameTX.SampleLevel(LinearSampler, pin.uv + float2(1.0, 0.0) * rcp, 0).rgb;
    float3 rgbW = FrameTX.SampleLevel(LinearSampler, pin.uv + float2(-1.0, 0.0) * rcp, 0).rgb;
    float lumaM = FxaaLuma(rgbM);
    float lumaN = FxaaLuma(rgbN);
    float lumaS = FxaaLuma(rgbS);
    float lumaE = FxaaLuma(rgbE);
    float lumaW = FxaaLuma(rgbW);
    float maxSM = max(lumaS, lumaM);
    float minSM = min(lumaS, lumaM);
    float maxESM = max(lumaE, maxSM);
    float minESM = min(lumaE, minSM);
    float maxWN = max(lumaN, lumaW);
    float minWN = min(lumaN, lumaW);
    float rangeMax = max(maxWN, maxESM);
    float rangeMin = min(minWN, minESM);
    float range = rangeMax - rangeMin;
    if (range < max(0.0625, rangeMax * 0.125)) {
        return float4(rgbM, 1.0);
    }
    float3 rgbNW = FrameTX.SampleLevel(LinearSampler, pin.uv + float2(-1.0, -1.0) * rcp, 0).rgb;
    float3 rgbNE = FrameTX.SampleLevel(LinearSampler, pin.uv + float2(1.0, -1.0) * rcp, 0).rgb;
    float3 rgbSW = FrameTX.SampleLevel(LinearSampler, pin.uv + float2(-1.0, 1.0) * rcp, 0).rgb;
    float3 rgbSE = FrameTX.SampleLevel(LinearSampler, pin.uv + float2(1.0, 1.0) * rcp, 0).rgb;
    float lumaNW = FxaaLuma(rgbNW);
    float lumaNE = FxaaLuma(rgbNE);
    float lumaSW = FxaaLuma(rgbSW);
    float lumaSE = FxaaLuma(rgbSE);
    float edgeH = abs(lumaN + lumaS - 2.0 * lumaM) * 2.0 + abs(lumaNE + lumaSE - 2.0 * lumaE) + abs(lumaNW + lumaSW - 2.0 * lumaW);
    float edgeV = abs(lumaE + lumaW - 2.0 * lumaM) * 2.0 + abs(lumaNE + lumaNW - 2.0 * lumaN) + abs(lumaSE + lumaSW - 2.0 * lumaS);
    bool horzSpan = edgeH >= edgeV;
    float luma1 = horzSpan ? lumaN : lumaE;
    float luma2 = horzSpan ? lumaS : lumaW;
    float grad1 = horzSpan ? abs(lumaN - lumaM) : abs(lumaE - lumaM);
    float grad2 = horzSpan ? abs(lumaS - lumaM) : abs(lumaW - lumaM);
    float lengthSign = grad1 >= grad2 ? -rcp.y : rcp.y;
    if (!horzSpan) {
        lengthSign = grad1 >= grad2 ? -rcp.x : rcp.x;
    }
    float2 dir = horzSpan ? float2(0.0, lengthSign * 0.5) : float2(lengthSign * 0.5, 0.0);
    float2 dirR = horzSpan ? float2(rcp.x, 0.0) : float2(0.0, rcp.y);
    float3 rgbA = 0.5 * (FrameTX.SampleLevel(LinearSampler, pin.uv + dir * (1.0 / 3.0 - 0.5), 0).rgb + FrameTX.SampleLevel(LinearSampler, pin.uv + dir * (2.0 / 3.0 - 0.5), 0).rgb);
    float3 rgbB = rgbA * 0.5 + 0.25 * (FrameTX.SampleLevel(LinearSampler, pin.uv + dir * -0.5, 0).rgb + FrameTX.SampleLevel(LinearSampler, pin.uv + dir * 0.5, 0).rgb);
    float lumaB = FxaaLuma(rgbB);
    float2 spanEnd = dir * 8.0;
    float3 rgbEnd0 = FrameTX.SampleLevel(LinearSampler, pin.uv + spanEnd * (1.0 / 4.0 - 0.5) - dirR * 0.5, 0).rgb;
    float3 rgbEnd1 = FrameTX.SampleLevel(LinearSampler, pin.uv + spanEnd * (1.0 / 4.0 - 0.5) + dirR * 0.5, 0).rgb;
    float3 rgbEnd2 = FrameTX.SampleLevel(LinearSampler, pin.uv + spanEnd * (3.0 / 4.0 - 0.5) - dirR * 0.5, 0).rgb;
    float3 rgbEnd3 = FrameTX.SampleLevel(LinearSampler, pin.uv + spanEnd * (3.0 / 4.0 - 0.5) + dirR * 0.5, 0).rgb;
    float lumaEnd0 = FxaaLuma(rgbEnd0);
    float lumaEnd1 = FxaaLuma(rgbEnd1);
    float lumaEnd2 = FxaaLuma(rgbEnd2);
    float lumaEnd3 = FxaaLuma(rgbEnd3);
    bool doneN = abs(lumaEnd0 - lumaEnd1) >= grad1 + grad2 * 0.25;
    bool doneP = abs(lumaEnd2 - lumaEnd3) >= grad1 + grad2 * 0.25;
    bool doneNP = doneN && doneP;
    float dstN = 1.0 / 4.0;
    float dstP = 3.0 / 4.0;
    if (!doneNP) {
        float2 span2 = spanEnd * 2.0;
        rgbEnd0 = FrameTX.SampleLevel(LinearSampler, pin.uv + span2 * (1.0 / 4.0 - 0.5) - dirR * 0.5, 0).rgb;
        rgbEnd1 = FrameTX.SampleLevel(LinearSampler, pin.uv + span2 * (1.0 / 4.0 - 0.5) + dirR * 0.5, 0).rgb;
        rgbEnd2 = FrameTX.SampleLevel(LinearSampler, pin.uv + span2 * (3.0 / 4.0 - 0.5) - dirR * 0.5, 0).rgb;
        rgbEnd3 = FrameTX.SampleLevel(LinearSampler, pin.uv + span2 * (3.0 / 4.0 - 0.5) + dirR * 0.5, 0).rgb;
        lumaEnd0 = FxaaLuma(rgbEnd0);
        lumaEnd1 = FxaaLuma(rgbEnd1);
        lumaEnd2 = FxaaLuma(rgbEnd2);
        lumaEnd3 = FxaaLuma(rgbEnd3);
        doneN = abs(lumaEnd0 - lumaEnd1) >= grad1 + grad2 * 0.25;
        doneP = abs(lumaEnd2 - lumaEnd3) >= grad1 + grad2 * 0.25;
        dstN = 2.0 / 4.0;
        dstP = 6.0 / 4.0;
    }
    float spanLength = (dstP - dstN) * 8.0;
    float pixelOffset = 0.0;
    if (!doneN || !doneP) {
        pixelOffset = -0.5;
    } else {
        float avgN = 0.5 * (lumaEnd0 + lumaEnd1);
        float avgP = 0.5 * (lumaEnd2 + lumaEnd3);
        bool doneJN = abs(avgN - lumaM) >= grad1 + grad2 * 0.25;
        bool doneJP = abs(avgP - lumaM) >= grad1 + grad2 * 0.25;
        if (!doneJN) {
            dstN = 0.0;
        }
        if (!doneJP) {
            dstP = 8.0;
        }
        if (dstP - dstN < 8.0) {
            pixelOffset = -0.5;
        } else {
            float dist = dstN < dstP ? dstN : dstP;
            pixelOffset = (dist - 4.0) / spanLength - 0.5;
        }
    }
    float3 rgbF = FrameTX.SampleLevel(LinearSampler, pin.uv + dir * pixelOffset, 0).rgb;
    float lumaF = FxaaLuma(rgbF);
    float rangeF = abs(lumaF - lumaM);
    float blendF = 0.0;
    if (rangeF < grad1 + grad2 * 0.25) {
        blendF = 0.5;
    }
    float lumaSS = lumaM * 0.5 + (lumaN + lumaS + lumaE + lumaW) * 0.125;
    float rangeSS = abs(lumaSS - lumaM);
    float blendSS = clamp(rangeSS / max(range, 1e-5), 0.0, 1.0);
    blendSS = blendSS * blendSS * 0.75;
    float blend = max(blendF, blendSS);
    float3 rgbR = lerp(rgbM, rgbF, blend);
    return float4(rgbR, 1.0);
}
