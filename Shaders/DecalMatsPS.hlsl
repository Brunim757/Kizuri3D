cbuffer DecalCB : register(b0)
{
    row_major float4x4 InvW;
    float4 SizeFade;
    float4 DecalScreen;
    float4 DecalMat;
    float4 DecalWorldN;
};
Texture2D DecalTex : register(t0);
Texture2D PositionTX : register(t1);
SamplerState DecalSamp : register(s0);
SamplerState LinearSampler : register(s1);
struct PSIn
{
    float4 pos : SV_Position;
    float3 wpos : TEXCOORD0;
};
struct PSOut
{
    float4 nrmRough : SV_Target0;
    float4 metallic : SV_Target1;
    float4 emissive : SV_Target2;
};
PSOut main(PSIn pin)
{
    PSOut o;
    o.nrmRough = float4(0.5, 0.5, 1.0, 1.0);
    o.metallic = float4(0.0, 0.0, 0.0, 1.0);
    o.emissive = float4(0.0, 0.0, 0.0, 1.0);
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
    float4 tex = DecalTex.Sample(DecalSamp, uv);
    float a = tex.a * SizeFade.w;
    if (a <= 0.5) {
        discard;
    }
    float3 dn = normalize(DecalWorldN.xyz);
    o.nrmRough = float4(dn * 0.5 + 0.5, DecalMat.y);
    o.metallic = float4(DecalMat.z, DecalMat.z, DecalMat.z, 1.0);
    o.emissive = float4(tex.rgb * step(7.5, DecalMat.x + 0.5), 1.0);
    return o;
}
