cbuffer DecalCB : register(b0)
{
    row_major float4x4 InvW;
    float4 SizeFade;
};
Texture2D DecalTex : register(t0);
SamplerState DecalSamp : register(s0);
struct PSIn
{
    float4 pos : SV_Position;
    float3 wpos : TEXCOORD0;
};
float4 main(PSIn pin) : SV_Target
{
    float3 local = mul(float4(pin.wpos, 1.0), InvW).xyz;
    if (abs(local.x) > 0.5 || abs(local.y) > 0.5 || abs(local.z) > 0.5) {
        discard;
    }
    if (local.z < 0.0) {
        discard;
    }
    float2 uv = local.xy + 0.5;
    float4 tex = DecalTex.Sample(DecalSamp, uv);
    float a = tex.a * SizeFade.w;
    if (a <= 0.001) {
        discard;
    }
    return float4(tex.rgb, a);
}
