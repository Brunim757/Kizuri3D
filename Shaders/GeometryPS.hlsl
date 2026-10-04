cbuffer MatCB : register(b0)
{
    float4 Albedo;
    float4 Params;
};
Texture2D AlbedoTex : register(t0);
Texture2D NormalTex : register(t1);
Texture2D OrmTex : register(t2);
SamplerState GeoSamp : register(s0);
struct PSIn
{
    float4 pos : SV_Position;
    float3 wnrm : NORMAL0;
    float3 wpos : TEXCOORD0;
    float2 uv : TEXCOORD1;
    float4 wtan : TEXCOORD2;
};
struct PSOut
{
    float4 albedo : SV_Target0;
    float4 nrmRough : SV_Target1;
    float4 metallic : SV_Target2;
    float4 wpos : SV_Target3;
};
PSOut main(PSIn pin)
{
    PSOut pout;
    uint flags = (uint) Params.z;
    float3 alb = Albedo.rgb;
    if ((flags & 1u) != 0u) {
        alb *= AlbedoTex.Sample(GeoSamp, pin.uv).rgb;
    }
    float3 n = normalize(pin.wnrm);
    float rough = Params.x;
    float metal = Params.y;
    float occ = 1.0;
    if ((flags & 2u) != 0u) {
        float3 t = normalize(pin.wtan.xyz);
        float3 b = cross(n, t) * pin.wtan.w;
        float2 nxy = NormalTex.Sample(GeoSamp, pin.uv).rg * 2.0 - 1.0;
        float nz = sqrt(saturate(1.0 - dot(nxy, nxy)));
        n = normalize(t * nxy.x + b * nxy.y + n * nz);
    }
    if ((flags & 4u) != 0u) {
        float3 orm = OrmTex.Sample(GeoSamp, pin.uv).rgb;
        occ = orm.r;
        rough = orm.g;
        metal = orm.b;
    }
    pout.albedo = float4(alb * occ, 1.0);
    pout.nrmRough = float4(n, rough);
    pout.metallic = float4(metal, 0.0, 0.0, 1.0);
    pout.wpos = float4(pin.wpos, 1.0);
    return pout;
}
