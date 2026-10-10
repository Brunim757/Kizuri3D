cbuffer LightCB : register(b0)
{
    float4 CamPos;
    float4 LightDir;
    float4 LightColor;
    float4 Counts;
    float4 CamFwd;
    float4 LightA[16];
    float4 LightB[16];
    float4 LightC[16];
    float4 LightD[16];
    row_major float4x4 CascadeVP[4];
    float4 CascadeSplit;
    float4 CascadeNear;
    float4 CascadeFar;
    float4 CascadeUV;
    float4 CascadeK;
    float4 CascadeSize;
    float4 CascadeLight;
    float4 ShadowInfo;
    row_major float4x4 SpotVP[4];
    float4 SpotMeta[4];
    float4 PointInfo;
    float4 PointMeta;
    float4 GradeInfo;
    float4 CamRight;
    float4 CamUp;
    float4 SkySun;
    float4 SkyColor;
    float4 SsaoInfo;
    row_major float4x4 SsaoVP;
    float4 FogInfo;
    float4 FogColor;
};
cbuffer MatCB : register(b1)
{
    float4 Albedo;
    float4 Params;
    float4 Extra;
};
cbuffer MatExt : register(b2)
{
    float4 ExtA;
    float4 ExtB;
    float4 ExtC;
    float4 ExtD;
    float4 ExtE;
    float4 ExtF;
    float4 ExtG;
    float4 ExtH;
    float4 ExtI;
    float4 ExtJ;
    float4 XfA[5];
    float4 XfB[5];
};
cbuffer ScreenCB : register(b3)
{
    float4 ScreenWH;
    float4 FwdCount;
    float4 FwdIdx[4];
};
Texture2D AlbedoTex : register(t0);
Texture2D NormalTex : register(t1);
Texture2D OrmTex : register(t2);
Texture2D LightmapTex : register(t4);
Texture2D EmissiveTex : register(t5);
Texture2D TransTex : register(t6);
Texture2D ThickTex : register(t7);
Texture2D TransSrc : register(t16);
TextureCube EnvTX : register(t17);
Texture2D DetailFTex : register(t18);
Texture2D DetailFNormalTex : register(t19);
SamplerState GeoSamp : register(s0);
struct PSIn
{
    float4 pos : SV_Position;
    float3 wnrm : NORMAL0;
    float3 wpos : TEXCOORD0;
    float2 uv : TEXCOORD1;
    float4 wtan : TEXCOORD2;
    float3 vts : TEXCOORD3;
    float2 luv : TEXCOORD4;
    float2 uv2 : TEXCOORD5;
    bool front : SV_IsFrontFace;
};
float2 XformUv(float2 uv, float2 uv2, int slot)
{
    float4 a = XfA[slot];
    float4 b = XfB[slot];
    float2 base = b.y > 0.5 ? uv2 : uv;
    float2 scaled = base * a.zw;
    float c = cos(b.x);
    float s = sin(b.x);
    return float2(scaled.x * c - scaled.y * s, scaled.x * s + scaled.y * c) + a.xy;
}
float NdfGGX(float cosLh, float roughness)
{
    float alpha = roughness * roughness;
    float alphaSq = alpha * alpha;
    float denom = (cosLh * cosLh) * (alphaSq - 1.0) + 1.0;
    return alphaSq / (3.14159265 * denom * denom + 1e-6);
}
float GaSchlickG1(float cosTheta, float k)
{
    return cosTheta / (cosTheta * (1.0 - k) + k + 1e-6);
}
float GaSchlickGGX(float cosLi, float cosLo, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return GaSchlickG1(cosLi, k) * GaSchlickG1(cosLo, k);
}
float3 SkyGradient(float3 viewDir, float3 sunDirTo, float3 sunColor, float sunIntensity)
{
    float vl = max(length(viewDir), 1e-6);
    float3 d = viewDir / vl;
    float sl = max(length(sunDirTo), 1e-6);
    float3 s = sunDirTo / sl;
    float dayness = clamp(s.y * 3.0 + 0.3, 0.0, 1.0);
    float upness = clamp(d.y, 0.0, 1.0);
    float3 zen = float3(0.20, 0.38, 0.70);
    float3 hor = float3(0.65, 0.55, 0.45);
    float3 gnd = float3(0.10, 0.09, 0.08);
    float zb = 0.15 + 0.85 * dayness;
    float pw = pow(upness, 0.5);
    float3 sky = hor + (zen - hor) * pw;
    float at = abs(d.y);
    float hb = exp(-at * 6.0);
    sky += (hor - sky) * hb * 0.5;
    if (d.y < 0.0) {
        float gk = clamp(-d.y * 3.0, 0.0, 1.0);
        sky += (gnd - sky) * gk;
    }
    float dim = (0.2 + 0.8 * dayness) * zb;
    sky *= dim;
    float cosG = clamp(dot(d, s), 0.0, 1.0);
    float glow = pow(cosG, 900.0) * 4.0 + pow(cosG, 10.0) * 0.25;
    float sf = (0.15 + 0.85 * dayness) * sunIntensity;
    return sky + sunColor * glow * sf;
}
float3 ShadeFwd(float3 N, float3 V, float3 L, float3 lightColor, float3 albedo, float roughness, float metallic, float3 F0)
{
    float3 H = normalize(L + V);
    float cosLi = saturate(dot(N, L));
    float cosLo = saturate(dot(N, V));
    float cosLh = saturate(dot(N, H));
    float vh = saturate(dot(V, H));
    float3 Fb = lerp(F0, albedo, metallic);
    float3 F = Fb + (1.0 - Fb) * pow(1.0 - vh, 5.0);
    float D = NdfGGX(cosLh, roughness);
    float G = GaSchlickGGX(cosLi, cosLo, roughness);
    float3 spec = (F * D * G) / max(4.0 * cosLi * cosLo, 1e-5);
    float3 kd = (1.0 - F) * (1.0 - metallic);
    float wrapLi = saturate((dot(N, L) + 0.5) / 1.5);
    wrapLi *= wrapLi;
    return (kd * albedo / 3.14159265 * wrapLi + spec * cosLi) * lightColor;
}
float4 main(PSIn pin) : SV_Target
{
    uint flags = (uint) Params.z;
    float3 n = normalize(pin.wnrm);
    if (!pin.front && ExtB.y > 0.5) {
        n = -n;
    }
    float3 alb = Albedo.rgb;
    float alpha = Albedo.a;
    if ((flags & 1u) != 0u) {
        float4 albT = AlbedoTex.Sample(GeoSamp, XformUv(pin.uv, pin.uv2, 0));
        alb *= albT.rgb;
        alpha *= albT.a;
    }
    if (ExtB.x > 0.5 && ExtB.x < 1.5 && alpha < ExtA.w) {
        discard;
    }
    float rough = Params.x;
    float metal = Params.y;
    if ((flags & 2u) != 0u) {
        float2 nuv = XformUv(pin.uv, pin.uv2, 1);
        float3 t = normalize(pin.wtan.xyz);
        float3 b = cross(n, t) * pin.wtan.w;
        float2 nxy = NormalTex.Sample(GeoSamp, nuv).rg * 2.0 - 1.0;
        float nz = sqrt(saturate(1.0 - dot(nxy, nxy)));
        n = normalize(t * nxy.x + b * nxy.y + n * nz);
    }
    if ((flags & 4u) != 0u) {
        float3 orm = OrmTex.Sample(GeoSamp, XformUv(pin.uv, pin.uv2, 2)).rgb;
        rough = orm.g;
        metal = orm.b;
    }
    if ((flags & 1048576u) != 0u) {
        float3 det = DetailFTex.Sample(GeoSamp, pin.uv * max(ExtI.x, 0.01)).rgb;
        alb *= det * 2.0;
    }
    if ((flags & 2097152u) != 0u) {
        float3 t2 = normalize(pin.wtan.xyz);
        float3 b2 = cross(n, t2) * pin.wtan.w;
        float2 dnxy = DetailFNormalTex.Sample(GeoSamp, pin.uv * max(ExtI.x, 0.01)).rg * 2.0 - 1.0;
        float dnz = sqrt(saturate(1.0 - dot(dnxy, dnxy)));
        n = normalize(n + (t2 * dnxy.x + b2 * dnxy.y) * 0.5 + n * (dnz - 1.0) * 0.5);
    }
    if ((flags & 4194304u) != 0u) {
        float lw = saturate(ExtI.y);
        alb = lerp(alb, ExtJ.rgb, lw);
        rough = lerp(rough, ExtI.z, lw);
        metal = lerp(metal, ExtI.w, lw);
    }
    float iorSafe = max(ExtA.y, 1.01);
    float f0d = (iorSafe - 1.0) / (iorSafe + 1.0);
    f0d *= f0d;
    float3 F0 = float3(f0d, f0d, f0d);
    float3 emi = ExtE.rgb * ExtA.x;
    if ((flags & 32u) != 0u) {
        float3 emiFactor = ExtE.rgb;
        if (dot(emiFactor, float3(1.0, 1.0, 1.0)) <= 0.0) {
            emiFactor = float3(1.0, 1.0, 1.0);
        }
        emi = EmissiveTex.Sample(GeoSamp, XformUv(pin.uv, pin.uv2, 4)).rgb * emiFactor * ExtA.x;
    }
    float3 V = normalize(CamPos.xyz - pin.wpos);
    float3 Lsun = normalize(-LightDir.xyz);
    float3 col = ShadeFwd(n, V, Lsun, LightColor.rgb, alb, rough, metal, F0);
    int count = (int) Counts.x;
    if (count > 16) {
        count = 16;
    }
    int fcount = clamp((int)FwdCount.x, 0, 16);
    if (fcount == 0) {
        fcount = count;
    }
    [loop]
    for (int k = 0; k < fcount; ++k) {
        int i = clamp((int)FwdIdx[k >> 2][k & 3], 0, 15);
        if (i >= count) {
            continue;
        }
        float kind = LightA[i].w;
        if (kind > 1.5) {
            float3 Ld = normalize(-LightA[i].xyz);
            col += ShadeFwd(n, V, Ld, LightC[i].rgb, alb, rough, metal, F0);
        } else {
            float3 toLight = LightA[i].xyz - pin.wpos;
            float dist = length(toLight);
            float3 L = toLight / max(dist, 1e-4);
            float range = max(LightB[i].w, 1e-3);
            float att = pow(saturate(1.0 - (dist * dist) / (range * range)), 2.0);
            float spot = 1.0;
            if (kind > 0.5) {
                float cosT = dot(L, -LightB[i].xyz);
                spot = smoothstep(LightC[i].a, LightC[i].a + LightD[i].x, cosT);
            }
            col += ShadeFwd(n, V, L, LightC[i].rgb, alb, rough, metal, F0) * att * spot;
        }
    }
    float sint2 = max(SkySun.w, 1e-3);
    float3 hemi = SkyGradient(n, normalize(SkySun.xyz), SkyColor.rgb, sint2);
    col += alb * hemi * 0.5;
    float3 R = reflect(-V, n);
    float3 cubeEnv = EnvTX.SampleLevel(GeoSamp, R, clamp(rough, 0.0, 1.0) * 6.0).rgb * GradeInfo.z;
    float3 F0e = lerp(F0, alb, metal);
    float3 fres = F0e + (1.0 - F0e) * pow(1.0 - saturate(dot(n, V)), 5.0);
    col += cubeEnv * fres * (1.0 - rough);
    float T = ExtF.w;
    T = clamp(T, 0.0, 1.0);
    float thick = ExtH.w;
    if ((flags & 131072u) != 0u) {
        T *= TransTex.Sample(GeoSamp, XformUv(pin.uv, pin.uv2, 0)).r;
    }
    if ((flags & 262144u) != 0u) {
        thick *= ThickTex.Sample(GeoSamp, XformUv(pin.uv, pin.uv2, 0)).g;
    }
    if (T > 0.0) {
        float eta = 1.0 / iorSafe;
        float3 refr = refract(-V, n, eta);
        float2 suv = pin.pos.xy / ScreenWH.xy;
        float2 off = refr.xy * thick * 0.5;
        float rad = rough * 6.0 / max(ScreenWH.x, 1.0);
        float3 scene = TransSrc.Sample(GeoSamp, suv + off).rgb * 0.4;
        scene += TransSrc.Sample(GeoSamp, suv + off + float2(rad, 0.0)).rgb * 0.15;
        scene += TransSrc.Sample(GeoSamp, suv + off - float2(rad, 0.0)).rgb * 0.15;
        scene += TransSrc.Sample(GeoSamp, suv + off + float2(0.0, rad)).rgb * 0.15;
        scene += TransSrc.Sample(GeoSamp, suv + off - float2(0.0, rad)).rgb * 0.15;
        float3 absorb = float3(1.0, 1.0, 1.0);
        if (ExtD.w > 1e-4) {
            float3 sigma = (float3(1.0, 1.0, 1.0) - ExtH.rgb) * thick / max(ExtD.w, 1e-4);
            absorb = exp(-max(sigma, 0.0));
        }
        float3 transmitted = scene * absorb * T;
        col = col * (1.0 - T) + transmitted;
    }
    col += emi;
    if ((flags & 16u) != 0u) {
        col += alb * LightmapTex.Sample(GeoSamp, pin.luv).rgb * Extra.x;
    }
    if (FogInfo.x > 0.5) {
        float3 toFrag = pin.wpos - CamPos.xyz;
        float fragDist = length(toFrag);
        if (fragDist > 1e-3) {
            float dens = FogInfo.y;
            float trans2 = exp(-dens * fragDist);
            col = col * trans2 + FogColor.rgb * (1.0 - trans2);
        }
    }
    float outA = alpha;
    if (T > 0.0) {
        outA = 1.0;
    }
    return float4(col, outA);
}
