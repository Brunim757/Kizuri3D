cbuffer DecalCB : register(b0)
{
    row_major float4x4 InvW;
    float4 SizeFade;
    float4 DecalScreen;
    float4 DecalMat;
    float4 DecalWorldN;
    float4 DecalAtlasRect;
    float4 DecalAtlasMeta;
};
Texture2D DecalTex : register(t0);
Texture2D PositionTX : register(t1);
Texture2D DecalAtlas : register(t2);
SamplerState DecalSamp : register(s0);
SamplerState LinearSampler : register(s1);
struct PSIn
{
    float4 pos : SV_Position;
    float3 wpos : TEXCOORD0;
};
float4 main(PSIn pin) : SV_Target
{
    float2 suv = (pin.pos.xy - DecalScreen.zw) / max(DecalScreen.xy, float2(1.0, 1.0));
    if (suv.x < 0.0 || suv.x > 1.0 || suv.y < 0.0 || suv.y > 1.0) {
        discard;
    }
    float3 Pw = PositionTX.SampleLevel(LinearSampler, suv, 0).xyz;
    if (dot(Pw, Pw) < 1e-8) {
        discard;
    }
    float3 local = mul(float4(Pw, 1.0), InvW).xyz;
    if (abs(local.x) > 0.5 || abs(local.y) > 0.5 || abs(local.z) > 0.5) {
        discard;
    }
    if (local.z < 0.0) {
        discard;
    }
    float2 uv = local.xy + 0.5;
    float4 tex;
    if (DecalAtlasMeta.x > 0.5) {
        float2 auv = DecalAtlasRect.xy + uv * (DecalAtlasRect.zw - DecalAtlasRect.xy);
        tex = DecalAtlas.Sample(DecalSamp, auv);
    } else {
        tex = DecalTex.Sample(DecalSamp, uv);
    }
    float a = tex.a * SizeFade.w;
    if (a <= 0.001) {
        discard;
    }
    return float4(tex.rgb, a);
}
