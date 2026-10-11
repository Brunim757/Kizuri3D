Texture2D AlbedoTX : register(t0);
Texture2D NormalRoughTX : register(t1);
Texture2D MetallicTX : register(t2);
Texture2D PositionTX : register(t3);
SamplerState LinearSampler : register(s0);
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
    float4 ProbeBoxMin[4];
    float4 ProbeBoxMax[4];
    float4 IblInfo;
    float4 ShadowCtl;
    float4 VsmInfo;
};
cbuffer SpotCookieCB : register(b4)
{
    row_major float4x4 SpotCookieVP[16];
};
cbuffer DdgiCB : register(b5)
{
    float4 DdgiMin;
    float4 DdgiMax;
};
cbuffer FogVolCB : register(b6)
{
    float4 FogVolMin[4];
    float4 FogVolMax[4];
    float4 FogVolCol[4];
};
cbuffer AmbientCB : register(b7)
{
    float4 AmbientProbe;
};
Texture2D SSAOTX : register(t6);
Texture2D SsrTX : register(t7);
Texture2D ContactTX : register(t8);
TextureCube EnvTX : register(t9);
Texture2D LightmapTX : register(t10);
TextureCube ProbeCube0 : register(t11);
TextureCube ProbeCube1 : register(t12);
TextureCube ProbeCube2 : register(t13);
TextureCube ProbeCube3 : register(t14);
Texture2D EmissiveTX : register(t15);
Texture2D F0TX : register(t16);
Texture2D ExtATX : register(t17);
Texture2D MatTable : register(t18);
TextureCube IrradianceTX : register(t19);
Texture2D BrdfTX : register(t20);
Texture2D ShadowAtlas : register(t4);
TextureCube PointCube : register(t5);
Texture2D MomentsTX : register(t21);
Texture2D BentTX : register(t22);
Texture2D CookieTX : register(t23);
Texture2DArray CookieArray : register(t27);
Texture2D LinkTX : register(t24);
Texture2D ClusterTX : register(t25);
Texture3D DdgiTX : register(t26);
SamplerState ShadowSampler : register(s1);
static const float2 Poisson16[16] = {
    float2(-0.9428, -0.3997),
    float2(0.9456, -0.7689),
    float2(-0.0942, -0.9294),
    float2(0.3450, 0.2939),
    float2(-0.9159, 0.4577),
    float2(-0.8154, -0.8791),
    float2(-0.3828, 0.2768),
    float2(0.9748, 0.1661),
    float2(0.4435, -0.0843),
    float2(-0.3841, -0.1009),
    float2(0.4458, -0.7300),
    float2(0.3809, 0.8064),
    float2(0.3057, 0.1654),
    float2(-0.1006, 0.7641),
    float2(0.7675, -0.1007),
    float2(-0.0122, 0.1461)
};
struct PSIn
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};
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
struct Lobes
{
    float cc;
    float ccRough;
    float3 sheen;
    float sheenRough;
    float iri;
    float iriNm;
    float iriIor;
    float3 anisoT;
    float anisoStr;
    float disp;
    float iorM;
};
float D_AnisoGGX(float3 H, float3 T, float3 B, float3 N, float ax, float ay)
{
    float th = dot(T, H);
    float bh = dot(B, H);
    float nh = saturate(dot(N, H));
    float a2 = ax * ay;
    float3 v = float3(ay * th, ax * bh, a2 * nh);
    float v2 = dot(v, v);
    float w2 = a2 / max(v2, 1e-7);
    return a2 * w2 * w2 / 3.14159265;
}
float D_Charlie(float cosLh, float rough)
{
    float r = max(rough, 0.05);
    float invR = 1.0 / r;
    float cos2h = cosLh * cosLh;
    float sin2h = max(1.0 - cos2h, 0.0078125);
    return (2.0 + invR) * pow(sin2h, invR * 0.5) / (2.0 * 3.14159265);
}
float3 ThinFilm(float cosT, float iorF, float thickNm)
{
    float r0 = (1.0 - iorF) / (1.0 + iorF);
    r0 *= r0;
    float3 lambda = float3(650.0, 510.0, 475.0);
    float3 delta = (4.0 * 3.14159265 * iorF * thickNm * max(cosT, 1e-3)) / lambda;
    float3 c = cos(delta);
    float3 R = (2.0 * r0 * (1.0 + c)) / (1.0 + r0 * r0 + 2.0 * r0 * c + 1e-5);
    return saturate(R);
}
float3 OctDecode(float2 e)
{
    float3 n = float3(e.x, e.y, 1.0 - abs(e.x) - abs(e.y));
    if (n.z < 0.0) {
        float sx = n.x >= 0.0 ? 1.0 : -1.0;
        float sy = n.y >= 0.0 ? 1.0 : -1.0;
        n.xy = (1.0 - abs(float2(n.y, n.x))) * float2(sx, sy);
    }
    return normalize(n + 1e-7);
}
float3 ShadeOne(float3 N, float3 V, float3 L, float3 lightColor, float3 albedo, float roughness, float metallic, float3 F0, Lobes lb)
{
    float3 H = normalize(L + V);
    float rawLi = dot(N, L);
    float cosLi = saturate(rawLi);
    float cosLo = saturate(dot(N, V));
    float cosLh = saturate(dot(N, H));
    float vh = saturate(dot(V, H));
    float3 Fb = lerp(F0, albedo, metallic);
    float3 F = Fb + (1.0 - Fb) * pow(1.0 - vh, 5.0);
    if (lb.iri > 0.001) {
        float3 film = ThinFilm(cosLh, clamp(lb.iriIor, 1.0, 3.0), clamp(lb.iriNm, 0.0, 2000.0));
        F = lerp(F, film * (Fb + 0.5), clamp(lb.iri, 0.0, 1.0));
    }
    float D = 0.0;
    float G = GaSchlickGGX(cosLi, cosLo, roughness);
    if (lb.anisoStr > 0.001) {
        float3 T = normalize(lb.anisoT - N * dot(N, lb.anisoT) + 1e-6);
        float3 B = normalize(cross(N, T));
        float aspect = sqrt(max(1.0 - 0.9 * clamp(lb.anisoStr, 0.0, 1.0), 1e-4));
        float r2 = max(roughness * roughness, 1e-4);
        float ax = max(r2 / aspect, 1e-4);
        float ay = max(r2 * aspect, 1e-4);
        D = D_AnisoGGX(H, T, B, N, ax, ay);
    } else {
        D = NdfGGX(cosLh, roughness);
    }
    float3 spec = (F * D * G) / max(4.0 * cosLi * cosLo, 1e-5);
    if (lb.disp > 0.001) {
        float spread = clamp(lb.disp, 0.0, 1.0) * 0.03;
        float iorC = clamp(lb.iorM, 1.0, 4.0);
        float3 iorC3 = float3(iorC - spread, iorC, iorC + spread);
        float3 f0c = (iorC3 - 1.0) / (iorC3 + 1.0);
        f0c *= f0c;
        float3 Fbc = lerp(f0c, albedo, metallic);
        float3 Fc = Fbc + (1.0 - Fbc) * pow(1.0 - vh, 5.0);
        spec = (Fc * D * G) / max(4.0 * cosLi * cosLo, 1e-5);
    }
    float3 kd = (1.0 - F) * (1.0 - metallic);
    float3 diff = kd * albedo / 3.14159265;
    float3 sheenAcc = float3(0.0, 0.0, 0.0);
    if (dot(lb.sheen, lb.sheen) > 1e-8) {
        float sheenD = D_Charlie(cosLh, lb.sheenRough);
        float sheenV = 1.0 / max(4.0 * (cosLi + cosLo - cosLi * cosLo), 1e-4);
        sheenAcc = lb.sheen * (sheenD * sheenV * cosLi);
    }
    float ccF = 0.0;
    float3 ccSpec = float3(0.0, 0.0, 0.0);
    if (lb.cc > 0.001) {
        float Fcc = 0.04 + 0.96 * pow(1.0 - cosLo, 5.0);
        float Dcc = NdfGGX(cosLh, clamp(lb.ccRough, 0.03, 1.0));
        float Gcc = GaSchlickGGX(cosLi, cosLo, clamp(lb.ccRough, 0.03, 1.0));
        ccSpec = (Fcc * Dcc * Gcc) / max(4.0 * cosLi * cosLo, 1e-5);
        ccF = lb.cc * Fcc;
    }
    float wrapLi = saturate((rawLi + 0.5) / 1.5);
    wrapLi *= wrapLi;
    float3 Lt = -(L + N * 0.5);
    float trans = pow(saturate(dot(V, -normalize(Lt))), 4.0) * (1.0 - metallic);
    float3 sssCol = albedo * trans;
    float3 base = (diff * wrapLi + spec * cosLi + sssCol) * (1.0 - ccF) + sheenAcc * (1.0 - ccF);
    return base * lightColor + ccSpec * lb.cc * lightColor * cosLi;
}
float3 ShadeSss(float3 N, float3 L, float3 lightColor, float3 albedo, float3 tint, float amt, float wrap)
{
    if (amt < 0.001) {
        return float3(0.0, 0.0, 0.0);
    }
    float w = saturate((dot(N, L) + wrap) / max(1.0 + wrap, 1e-3));
    float prof = w * w * (3.0 - 2.0 * w);
    return albedo * tint * (lightColor * (prof * amt));
}
float BlockerSearch2D(float2 uv, float recvDist, float nearZ, float farZ, float searchUV)
{
    float range = max(farZ - nearZ, 1e-4);
    float sum = 0.0;
    int n = 0;
    for (int k = 0; k < 16; ++k) {
        float d = ShadowAtlas.SampleLevel(ShadowSampler, uv + Poisson16[k] * searchUV, 0).r;
        float dist = nearZ + d * range;
        if (dist < recvDist - range * 0.001) {
            sum += dist;
            n += 1;
        }
    }
    if (n == 0) {
        return -1.0;
    }
    return sum / (float)n;
}
float ApplyAttenuation(float dist, float range)
{
    if (Counts.y < 0.5) {
        return pow(saturate(1.0 - (dist * dist) / max(range * range, 1e-6)), 2.0);
    } else if (Counts.y < 1.5) {
        float phys = 1.0 / (dist * dist + 1.0);
        float win = saturate(1.0 - (dist * dist) / max(range * range, 1e-6));
        return phys * win;
    }
    return saturate(1.0 - dist / max(range, 1e-4));
}
float PCSSFilter2D(float2 uv, float recvZ, float filterUV, float bias)
{
    float lit = 0.0;
    for (int k = 0; k < 16; ++k) {
        float d = ShadowAtlas.SampleLevel(ShadowSampler, uv + Poisson16[k] * filterUV, 0).r;
        lit += (recvZ - bias < d) ? 1.0 : 0.0;
    }
    return lit / 16.0;
}
float SampleTilePCSS(float3 wpos, float3 nrm, row_major float4x4 vp, float tile, float tileK, float nearZ, float farZ, float uvScale, float perspTanHalf, float effSize)
{
    float usInit = uvScale;
    float4 sp0 = mul(float4(wpos, 1.0), vp);
    float recv0 = nearZ;
    if (sp0.w > 1e-6) {
        recv0 = nearZ + (sp0.z / sp0.w) * max(farZ - nearZ, 1e-4);
    }
    if (perspTanHalf > 0.0 && recv0 > 1e-4) {
        usInit = 0.5 * clamp(tileK, 0.05, 1.0) / max(2.0 * perspTanHalf * recv0, 1e-6);
    }
    float nb = ShadowInfo.w / max(usInit, 1e-6) * ShadowCtl.y;
    float3 wposB2 = wpos + nrm * nb;
    float4 sp = mul(float4(wposB2, 1.0), vp);
    if (sp.w <= 0.0) {
        return 1.0;
    }
    sp.xyz /= sp.w;
    if (abs(sp.x) > 1.0 || abs(sp.y) > 1.0 || sp.z < 0.0 || sp.z > 1.0) {
        return 1.0;
    }
    float tx = fmod(tile, 2.0);
    float ty = floor(tile / 2.0);
    float k = clamp(tileK, 0.05, 1.0);
    float u = (sp.x * 0.5 + 0.5) * 0.5 * k + tx * 0.5;
    float v = (0.5 - sp.y * 0.5) * 0.5 * k + ty * 0.5;
    float2 uv = float2(u, v);
    float texel = ShadowInfo.w;
    float range = max(farZ - nearZ, 1e-4);
    float recvDist = nearZ + sp.z * range;
    float bias = ShadowCtl.z > 0.5 ? max(nb / range, texel * 0.25) : 0.0015;
    if (ShadowCtl.x > 0.5) {
        float pr = texel * max(ShadowCtl.w, 0.5);
        float lit = 0.0;
        for (int ay = -1; ay <= 1; ++ay) {
            for (int ax = -1; ax <= 1; ++ax) {
                float d = ShadowAtlas.SampleLevel(ShadowSampler, uv + float2((float)ax, (float)ay) * pr, 0).r;
                lit += (sp.z - bias < d) ? 1.0 : 0.0;
            }
        }
        return lit / 9.0;
    }
    if (effSize <= 0.0001) {
        float d0 = ShadowAtlas.SampleLevel(ShadowSampler, uv, 0).r;
        return (sp.z - bias < d0) ? 1.0 : 0.0;
    }
    float us = uvScale;
    if (perspTanHalf > 0.0 && recvDist > 1e-4) {
        us = 0.5 * k / max(2.0 * perspTanHalf * recvDist, 1e-6);
    }
    float maxR = max(0.25 * k - 4.0 * texel, texel);
    float searchUV = min(effSize * 0.5 * us, maxR);
    float blocker = BlockerSearch2D(uv, recvDist, nearZ, farZ, searchUV);
    if (blocker < 0.0) {
        return 1.0;
    }
    float penumbra = max((recvDist - blocker) * effSize / max(blocker, 1e-4), 0.0);
    float filterUV = min(penumbra * us, maxR);
    if (filterUV <= texel * 0.5) {
        float d1 = ShadowAtlas.SampleLevel(ShadowSampler, uv, 0).r;
        return (sp.z - bias < d1) ? 1.0 : 0.0;
    }
    return PCSSFilter2D(uv, sp.z, filterUV, bias);
}
float ChebyshevUpper(float m1, float m2, float z, float bleed)
{
    float var = max(m2 - m1 * m1, 1e-5);
    float d = max(z - m1, 0.0);
    float p = var / (var + d * d);
    return clamp((p - bleed) / max(1.0 - bleed, 1e-4), 0.0, 1.0);
}
float SampleTileVSM(float3 wpos, float3 nrm, row_major float4x4 vp, float tile, float tileK, float nearZ, float farZ, float uvScale, float perspTanHalf, bool evsm)
{
    if (VsmInfo.w > 0.0 && distance(wpos, CamPos.xyz) > VsmInfo.w) {
        return 1.0;
    }
    if (VsmInfo.x < 0.5) {
        return 1.0;
    }
    float usInit = uvScale;
    float4 sp0 = mul(float4(wpos, 1.0), vp);
    float recv0 = nearZ;
    if (sp0.w > 1e-6) {
        recv0 = nearZ + (sp0.z / sp0.w) * max(farZ - nearZ, 1e-4);
    }
    if (perspTanHalf > 0.0 && recv0 > 1e-4) {
        usInit = 0.5 * clamp(tileK, 0.05, 1.0) / max(2.0 * perspTanHalf * recv0, 1e-6);
    }
    float nb = ShadowInfo.w / max(usInit, 1e-6) * ShadowCtl.y;
    float3 wposB2 = wpos + nrm * nb;
    float4 sp = mul(float4(wposB2, 1.0), vp);
    if (sp.w <= 0.0) {
        return 1.0;
    }
    sp.xyz /= sp.w;
    if (abs(sp.x) > 1.0 || abs(sp.y) > 1.0 || sp.z < 0.0 || sp.z > 1.0) {
        return 1.0;
    }
    float tx = fmod(tile, 2.0);
    float ty = floor(tile / 2.0);
    float k = clamp(tileK, 0.05, 1.0);
    float u = (sp.x * 0.5 + 0.5) * 0.5 * k + tx * 0.5;
    float v = (0.5 - sp.y * 0.5) * 0.5 * k + ty * 0.5;
    float4 m = MomentsTX.SampleLevel(LinearSampler, float2(u, v), 0);
    float bleed = clamp(VsmInfo.y, 0.0, 0.9);
    float zb = sp.z - 0.0003;
    if (!evsm) {
        return (zb <= m.x) ? 1.0 : ChebyshevUpper(m.x, m.y, zb, bleed);
    }
    float c = clamp(VsmInfo.z, 0.5, 5.0);
    float pw = exp(c * zb);
    float nw = exp(-c * zb);
    float sPos = (pw <= m.x) ? 1.0 : ChebyshevUpper(m.x, m.y, pw, bleed);
    float sNeg = (nw <= m.z) ? 1.0 : ChebyshevUpper(m.z, m.w, nw, bleed);
    return min(sPos, sNeg);
}
float SampleTileShadow(float3 wpos, float3 nrm, row_major float4x4 vp, float tile, float tileK, float nearZ, float farZ, float uvScale, float perspTanHalf, float effSize)
{
    float lit = 1.0;
    if (ShadowCtl.x > 1.5) {
        lit = SampleTileVSM(wpos, nrm, vp, tile, tileK, nearZ, farZ, uvScale, perspTanHalf, ShadowCtl.x > 2.5);
    } else {
        lit = SampleTilePCSS(wpos, nrm, vp, tile, tileK, nearZ, farZ, uvScale, perspTanHalf, effSize);
    }
    return lit;
}
float CascadeShadowSingle(float3 wpos, float3 nrm)
{
    float firstLight = 1e9;
    for (int q = 0; q < 4; ++q) {
        if (CascadeLight[q] >= 0.0 && CascadeLight[q] < firstLight) {
            firstLight = CascadeLight[q];
        }
    }
    if (firstLight > 1e8) {
        return 1.0;
    }
    float vd = dot(wpos - CamPos.xyz, CamFwd.xyz);
    for (int c = 0; c < 4; ++c) {
        if (CascadeLight[c] == firstLight && vd <= CascadeSplit[c]) {
            float nb = ShadowInfo.w * 32.0 * ShadowCtl.y;
            float3 wb = wpos + nrm * nb;
            float4 sp = mul(float4(wb, 1.0), CascadeVP[c]);
            if (sp.w <= 0.0) {
                return 1.0;
            }
            sp.xyz /= sp.w;
            if (abs(sp.x) > 1.0 || abs(sp.y) > 1.0 || sp.z < 0.0 || sp.z > 1.0) {
                return 1.0;
            }
            float tx = fmod((float)c, 2.0);
            float ty = floor((float)c / 2.0);
            float k = clamp(CascadeK[c], 0.05, 1.0);
            float u = (sp.x * 0.5 + 0.5) * 0.5 * k + tx * 0.5;
            float v = (0.5 - sp.y * 0.5) * 0.5 * k + ty * 0.5;
            float range = max(CascadeFar[c] - CascadeNear[c], 1e-4);
            float bias = ShadowCtl.z > 0.5 ? max(nb / range, ShadowInfo.w * 0.25) : 0.0025;
            float d = ShadowAtlas.SampleLevel(ShadowSampler, float2(u, v), 0).r;
            return (sp.z - bias < d) ? 1.0 : 0.0;
        }
    }
    return 1.0;
}
float3 ApplyFog(float3 col, float3 rayDir, float tMax, float2 suv)
{
    float dens = FogInfo.y;
    float stepLen = tMax / 8.0;
    float h = frac(sin(dot(suv, float2(12.9898, 78.233))) * 43758.5453);
    float3 toSun = normalize(SkySun.xyz);
    float3 sunCol = SkyColor.rgb * max(SkySun.w, 0.0);
    float cosT = max(dot(rayDir, toSun), 0.0);
    float ph = 0.15 + 0.85 * pow(cosT, 8.0);
    float trans = 1.0;
    float3 insc = float3(0.0, 0.0, 0.0);
    for (int s = 0; s < 8; ++s) {
        float tt = min(h * stepLen + ((float)s + 0.5) * stepLen, tMax);
        float3 p = CamPos.xyz + rayDir * tt;
        float sh = 1.0;
        if (ShadowInfo.y > 0.5) {
            sh = CascadeShadowSingle(p, float3(0.0, 0.0, 0.0));
        }
        float densAt = dens;
        if (FogInfo.z > 1e-6) {
            densAt = dens * exp(-max(p.y - FogInfo.w, 0.0) * FogInfo.z);
        }
        float3 S = (sunCol * ph * sh + FogColor.rgb * 0.15) * densAt;
        float aerial = saturate((p.y - FogInfo.w) * 0.015);
        S = lerp(S, SkyColor.rgb * (0.25 + 0.5 * aerial) * densAt, aerial * 0.6);
        for (int fv = 0; fv < 4; ++fv) {
            if (FogVolMin[fv].w <= 0.0) {
                continue;
            }
            float3 vmin = FogVolMin[fv].xyz;
            float3 vmax = FogVolMax[fv].xyz;
            if (p.x < vmin.x || p.x > vmax.x || p.y < vmin.y || p.y > vmax.y || p.z < vmin.z || p.z > vmax.z) {
                continue;
            }
            float vd = FogVolMin[fv].w;
            S = lerp(S, FogVolCol[fv].rgb * (sunCol * ph * sh + 0.2) * vd * 4.0, 0.5);
            densAt += vd;
        }
        insc += trans * S * stepLen;
        trans *= exp(-densAt * stepLen);
    }
    return col * trans + insc;
}
float LinearizeCube(float ndcZ, float nearZ, float farZ)
{
    float denom = 1.0 - ndcZ * (farZ - nearZ) / max(farZ, 1e-4);
    if (denom < 1e-6) {
        return farZ;
    }
    return nearZ / denom;
}
float SamplePointPCSS(float3 wpos, float3 nrm, float effSize)
{
    float nb = 0.02 * ShadowCtl.y;
    float3 wb = wpos + nrm * nb;
    float3 toFrag = wb - PointInfo.xyz;
    float dist = length(toFrag);
    if (dist > PointInfo.w || dist < 1e-4) {
        return 1.0;
    }
    float3 dir = toFrag / dist;
    float3 upRef = abs(dir.y) > 0.99 ? float3(1.0, 0.0, 0.0) : float3(0.0, 1.0, 0.0);
    float3 t1 = normalize(cross(upRef, dir));
    float3 t2 = cross(dir, t1);
    float nearZ = PointMeta.z;
    float farZ = PointInfo.w;
    float bias = ShadowCtl.z > 0.5 ? max(nb * 2.0, 0.005) : 0.05;
    if (ShadowCtl.x > 0.5) {
        float ang = 0.02 * max(ShadowCtl.w, 0.5);
        float lit = 0.0;
        lit += (dist - bias < LinearizeCube(PointCube.SampleLevel(ShadowSampler, dir, 0).r, nearZ, farZ)) ? 1.0 : 0.0;
        lit += (dist - bias < LinearizeCube(PointCube.SampleLevel(ShadowSampler, normalize(dir + t1 * ang), 0).r, nearZ, farZ)) ? 1.0 : 0.0;
        lit += (dist - bias < LinearizeCube(PointCube.SampleLevel(ShadowSampler, normalize(dir - t1 * ang), 0).r, nearZ, farZ)) ? 1.0 : 0.0;
        lit += (dist - bias < LinearizeCube(PointCube.SampleLevel(ShadowSampler, normalize(dir + t2 * ang), 0).r, nearZ, farZ)) ? 1.0 : 0.0;
        lit += (dist - bias < LinearizeCube(PointCube.SampleLevel(ShadowSampler, normalize(dir - t2 * ang), 0).r, nearZ, farZ)) ? 1.0 : 0.0;
        return lit / 5.0;
    }
    if (effSize <= 0.0001) {
        float z0 = PointCube.SampleLevel(ShadowSampler, dir, 0).r;
        return (dist - bias < LinearizeCube(z0, nearZ, farZ)) ? 1.0 : 0.0;
    }
    float searchAng = min((effSize * 0.5) / max(dist, 1e-4), 0.05);
    float bsum = 0.0;
    int bn = 0;
    for (int k = 0; k < 16; ++k) {
        float3 sd = normalize(dir + (t1 * Poisson16[k].x + t2 * Poisson16[k].y) * searchAng);
        float bz = PointCube.SampleLevel(ShadowSampler, sd, 0).r;
        float blin = LinearizeCube(bz, nearZ, farZ);
        if (blin < dist - bias) {
            bsum += blin;
            bn += 1;
        }
    }
    if (bn == 0) {
        return 1.0;
    }
    float blocker = bsum / (float)bn;
    float penumbra = max((dist - blocker) * effSize / max(blocker, 1e-4), 0.0);
    float filterAng = min(penumbra / max(dist, 1e-4), 0.08);
    float lit = 0.0;
    for (int f = 0; f < 16; ++f) {
        float3 fd = normalize(dir + (t1 * Poisson16[f].x + t2 * Poisson16[f].y) * filterAng);
        float z = PointCube.SampleLevel(ShadowSampler, fd, 0).r;
        float lin = LinearizeCube(z, nearZ, farZ);
        lit += (dist - bias < lin) ? 1.0 : 0.0;
    }
    return lit / 16.0;
}
float ACESFilm(float x)
{
    if (x <= 0.0) {
        return 0.0;
    }
    return (x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14);
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
    float mieG = 0.76;
    float miePh = (1.0 - mieG * mieG) / pow(max(1.0 + mieG * mieG - 2.0 * mieG * cosG, 1e-4), 1.5);
    float rayPh = 0.5 + 0.5 * cosG * cosG;
    float3 scatter = (float3(0.30, 0.45, 0.90) * rayPh * 0.12 + float3(1.0, 0.95, 0.85) * miePh * 0.08) * dayness;
    float sf = (0.15 + 0.85 * dayness) * sunIntensity;
    float3 mdir = -s;
    float cosM = clamp(dot(d, mdir), 0.0, 1.0);
    float moonVis = 0.2 + 0.8 * (1.0 - dayness);
    float moonDisc = smoothstep(0.99955, 0.99985, cosM);
    float moonHalo = pow(cosM, 1200.0) * 0.25;
    float3 moonCol = float3(0.85, 0.90, 1.0) * (moonDisc * 1.5 + moonHalo) * moonVis;
    return sky + sunColor * glow * sf + scatter * sunIntensity + moonCol;
}
float4 main(PSIn pin) : SV_Target
{
    float4 albFull = AlbedoTX.Sample(LinearSampler, pin.uv);
    float3 albedo = albFull.rgb;
    float4 nr = NormalRoughTX.Sample(LinearSampler, pin.uv);
    float3 N = normalize(nr.xyz);
    float roughness = clamp(nr.w, 0.04, 1.0);
    float4 metalFull = MetallicTX.Sample(LinearSampler, pin.uv);
    float metallic = clamp(metalFull.r, 0.0, 1.0);
    float4 posFull = PositionTX.Sample(LinearSampler, pin.uv);
    float3 wpos = posFull.xyz;
    if (dot(wpos, wpos) < 1e-8) {
        float2 ndc2 = float2(pin.uv.x * 2.0 - 1.0, 1.0 - pin.uv.y * 2.0);
        float3 ray = CamFwd.xyz + CamRight.xyz * (ndc2.x * CamRight.w * CamUp.w) + CamUp.xyz * (ndc2.y * CamRight.w);
        ray = normalize(ray);
        float sint = max(SkySun.w, 1e-3);
        float3 sk = SkyGradient(ray, normalize(SkySun.xyz), SkyColor.rgb, sint);
        if (IblInfo.y > 0.5) {
            sk = EnvTX.SampleLevel(LinearSampler, ray, 0).rgb;
        }
        if (FogInfo.x > 0.5) {
            sk = ApplyFog(sk, ray, 150.0, pin.uv);
        }
        return float4(sk, 1.0);
    }
    float3 V = normalize(CamPos.xyz - wpos);
    float3 wposB = wpos;
    float viewDepth = dot(wpos - CamPos.xyz, CamFwd.xyz);
    if (ShadowInfo.z > 0.5 && ShadowInfo.y > 0.5) {
        float firstLight = 1e9;
        for (int q = 0; q < 4; ++q) {
            if (CascadeLight[q] >= 0.0 && CascadeLight[q] < firstLight) {
                firstLight = CascadeLight[q];
            }
        }
        if (firstLight < 1e8) {
            int firstSlot = -1;
            int cidx = -1;
            for (int q = 0; q < 4; ++q) {
                if (CascadeLight[q] == firstLight) {
                    if (firstSlot < 0) {
                        firstSlot = q;
                    }
                    if (viewDepth <= CascadeSplit[q]) {
                        cidx = q;
                        break;
                    }
                }
            }
            if (cidx < 0) {
                for (int q = 3; q >= 0; --q) {
                    if (CascadeLight[q] == firstLight) {
                        cidx = q;
                        break;
                    }
                }
            }
            if (cidx >= 0 && firstSlot >= 0) {
                int cn = cidx - firstSlot;
                float3 dbg = float3(1.0, 1.0, 0.3);
                if (cn == 0) {
                    dbg = float3(1.0, 0.2, 0.2);
                } else if (cn == 1) {
                    dbg = float3(0.2, 1.0, 0.2);
                } else if (cn == 2) {
                    dbg = float3(0.2, 0.4, 1.0);
                }
                return float4(dbg, 1.0);
            }
        }
    }
    float3 Lsun = normalize(-LightDir.xyz);
    float4 emisFull = EmissiveTX.Sample(LinearSampler, pin.uv);
    float3 emis = emisFull.rgb;
    uint matID = (uint) (metalFull.a * 255.0 + 0.5);
    if (matID > 255u) {
        matID = 255u;
    }
    float4 matInfo = MatTable.Load(int3((int) matID, 0, 0));
    float4 sssInfo = MatTable.Load(int3((int) matID, 1, 0));
    float3 sssTint = sssInfo.rgb;
    float sssAmt = sssInfo.a;
    float sssWrap = clamp(AmbientProbe.a * 0.5, 0.0, 2.0);
    float unlitF = matInfo.r;
    float4 f0Full = F0TX.Sample(LinearSampler, pin.uv);
    float3 specF0 = f0Full.rgb;
    float4 extFull = ExtATX.Sample(LinearSampler, pin.uv);
    float4 lmFull = LightmapTX.Sample(LinearSampler, pin.uv);
    Lobes lb;
    lb.cc = f0Full.a;
    lb.ccRough = extFull.r;
    lb.sheen = extFull.gba;
    lb.sheenRough = metalFull.g;
    lb.iri = metalFull.b;
    lb.iriNm = emisFull.a * 2000.0;
    lb.iriIor = 1.0 + matInfo.g * 2.0;
    float2 anOct = float2(albFull.a, posFull.a) * 2.0 - 1.0;
    lb.anisoT = OctDecode(anOct);
    lb.anisoStr = lmFull.a;
    lb.disp = matInfo.b;
    lb.iorM = 1.0 + matInfo.a * 3.0;
    float3 col = float3(0.0, 0.0, 0.0);
    float3 amb = float3(0.0, 0.0, 0.0);
    if (unlitF < 0.5) {
    col = ShadeOne(N, V, Lsun, LightColor.rgb, albedo, roughness, metallic, specF0, lb);
    col += ShadeSss(N, Lsun, LightColor.rgb, albedo, sssTint, sssAmt, sssWrap);
    int count = (int)Counts.x;
    if (count > 16) {
        count = 16;
    }
    for (int i = 0; i < count; ++i) {
        float kind = LightA[i].w;
        if (IblInfo.z > 0.5) {
            int2 ctile = int2(min(pin.uv.x * 16.0, 15.0), min(pin.uv.y * 9.0, 8.0));
            float2 gm = ClusterTX.Load(int3(ctile, 0)).rg * 255.0;
            uint cmask = (uint)(gm.x + 0.5) | ((uint)(gm.y + 0.5) << 8);
            if ((cmask & (1u << (uint)i)) == 0u) {
                continue;
            }
        }
        uint linkM = (uint)(LinkTX.Load(int3((int)matID, 0, 0)).r * 255.0 + 0.5);
        if ((linkM & (uint)LightD[i].z) == 0u) {
            continue;
        }
        if (kind > 2.5) {
            float3 Rn = normalize(LightB[i].xyz);
            float3 rref = abs(Rn.y) > 0.99 ? float3(1.0, 0.0, 0.0) : float3(0.0, 1.0, 0.0);
            float3 rright = normalize(cross(Rn, rref));
            float3 rup = cross(Rn, rright);
            float3 toC = wpos - LightA[i].xyz;
            float2 ph = float2(dot(toC, rright), dot(toC, rup));
            float2 hh = float2(max(LightC[i].a, 0.01), max(LightD[i].x, 0.01));
            float2 cp = clamp(ph, -hh, hh);
            float3 closest = LightA[i].xyz + rright * cp.x + rup * cp.y;
            float3 toL = closest - wpos;
            float rdist = length(toL);
            float3 Lr = toL / max(rdist, 1e-4);
            float rrange = max(LightB[i].w, 1e-3);
            float ratt = ApplyAttenuation(rdist, rrange) * saturate(dot(-Lr, Rn) * 0.5 + 0.5);
            col += ShadeOne(N, V, Lr, LightC[i].rgb, albedo, roughness, metallic, specF0, lb) * ratt;
            col += ShadeSss(N, Lr, LightC[i].rgb, albedo, sssTint, sssAmt, sssWrap) * ratt;
        } else if (kind > 1.5) {
            float3 Ld = normalize(-LightA[i].xyz);
            float dirShadow = 1.0;
            if (ShadowInfo.y > 0.5) {
                [loop]
                for (int c = 0; c < 4; ++c) {
                    if (CascadeLight[c] == (float)i && viewDepth <= CascadeSplit[c]) {
                        dirShadow = SampleTileShadow(wposB, N, CascadeVP[c], (float)c, CascadeK[c], CascadeNear[c], CascadeFar[c], CascadeUV[c], 0.0, CascadeSize[c]);
                        break;
                    }
                }
            }
            col += ShadeOne(N, V, Ld, LightC[i].rgb, albedo, roughness, metallic, specF0, lb) * dirShadow;
            col += ShadeSss(N, Ld, LightC[i].rgb, albedo, sssTint, sssAmt, sssWrap) * dirShadow;
        } else {
            float3 toLight = LightA[i].xyz - wpos;
            float dist = length(toLight);
            float3 L = toLight / max(dist, 1e-4);
            float range = max(LightB[i].w, 1e-3);
            float att = ApplyAttenuation(dist, range);
            float spot = 1.0;
            float lightShadow = 1.0;
            float3 cookie = float3(1.0, 1.0, 1.0);
            if (kind > 0.5) {
                float cosT = dot(L, -LightB[i].xyz);
                spot = smoothstep(LightC[i].a, LightC[i].a + LightD[i].x, cosT);
                if (ShadowInfo.y > 0.5) {
                    [loop]
                    for (int s = 0; s < 4; ++s) {
                        if ((int)(SpotMeta[s].y + 0.5) == i && SpotMeta[s].x >= 0.0) {
                            float cosH = max(LightC[i].a, 0.05);
                            float tanH = sqrt(max(1.0 - cosH * cosH, 1e-6)) / cosH;
                            lightShadow = SampleTileShadow(wposB, N, SpotVP[s], SpotMeta[s].x, SpotMeta[s].w, 0.5, max(LightB[i].w, 1.0), 0.0, tanH, SpotMeta[s].z);
                        }
                    }
                }
                if (Counts.w > 0.5) {
                    float4 scc = mul(float4(wposB, 1.0), SpotCookieVP[i]);
                    if (scc.w > 1e-5) {
                        float2 cuv = scc.xy / scc.w * float2(0.5, -0.5) + 0.5;
                        if (cuv.x >= 0.0 && cuv.x <= 1.0 && cuv.y >= 0.0 && cuv.y <= 1.0) {
                            if (Counts.w > 1.5) {
                                int slice = i < 0 ? 0 : (i > 15 ? 15 : i);
                                cookie = CookieArray.SampleLevel(LinearSampler, float3(cuv, (float) slice), 0).rgb;
                            } else {
                                cookie = CookieTX.SampleLevel(LinearSampler, cuv, 0).rgb;
                            }
                        }
                    }
                }
            } else {
                if (ShadowInfo.y > 0.5 && PointMeta.y > 0.5 && (int)(PointMeta.x + 0.5) == i) {
                    lightShadow = SamplePointPCSS(wposB, N, PointMeta.w);
                }
            }
            col += ShadeOne(N, V, L, LightC[i].rgb, albedo, roughness, metallic, specF0, lb) * att * spot * lightShadow * cookie;
            col += ShadeSss(N, L, LightC[i].rgb, albedo, sssTint, sssAmt, sssWrap) * att * spot * lightShadow * cookie;
        }
    }
    float ao = 1.0;
    if (SsaoInfo.x > 0.5) {
        ao = SSAOTX.Sample(LinearSampler, pin.uv).r;
    }
    ao *= ContactTX.Sample(LinearSampler, pin.uv).r;
    col *= ao;
    float sint2 = max(SkySun.w, 1e-3);
    float3 hemi = SkyGradient(N, normalize(SkySun.xyz), SkyColor.rgb, sint2);
    float3 difEnv = hemi * 0.5;
    if (IblInfo.x > 0.5) {
        float3 Nb = N;
        if (SsaoInfo.z > 0.5) {
            Nb = normalize(BentTX.SampleLevel(LinearSampler, pin.uv, 0).xyz * 2.0 - 1.0);
        }
        difEnv = IrradianceTX.SampleLevel(LinearSampler, Nb, 0).rgb * GradeInfo.z / 3.14159265359;
    }
    amb = albedo * difEnv * ao;
    amb += albedo * AmbientProbe.rgb * ao;
    if (DdgiMin.w > 0.5 && DdgiMax.w > 0.5) {
        float3 dsize = max(DdgiMax.xyz - DdgiMin.xyz, 1e-4);
        float3 duv = (wpos - DdgiMin.xyz) / dsize;
        if (duv.x >= 0.0 && duv.x <= 1.0 && duv.y >= 0.0 && duv.y <= 1.0 && duv.z >= 0.0 && duv.z <= 1.0) {
            float3 gif = DdgiTX.Sample(LinearSampler, duv).rgb;
            amb += albedo * gif * DdgiMin.w * ao;
        }
    }
    float3 R = reflect(-V, N);
    float3 skyEnv = SkyGradient(R, normalize(SkySun.xyz), SkyColor.rgb, sint2);
    float3 cubeEnv = EnvTX.SampleLevel(LinearSampler, R, clamp(roughness, 0.0, 1.0) * 6.0).rgb * GradeInfo.z;
    float3 env = lerp(skyEnv, cubeEnv, clamp(GradeInfo.w, 0.0, 1.0));
    float envInt = GradeInfo.z;
    float3 probeAcc = float3(0.0, 0.0, 0.0);
    float probeWsum = 0.0;
    [loop]
    for (int pi = 0; pi < 4; ++pi) {
        if (ProbeBoxMin[pi].w <= 0.0) {
            continue;
        }
        float3 bmin = ProbeBoxMin[pi].xyz;
        float3 bmax = ProbeBoxMax[pi].xyz;
        if (wpos.x < bmin.x || wpos.x > bmax.x || wpos.y < bmin.y || wpos.y > bmax.y || wpos.z < bmin.z || wpos.z > bmax.z) {
            continue;
        }
        float3 center = (bmin + bmax) * 0.5;
        float3 f = ((R.x > 0.0 ? bmax.x : bmin.x) - wpos.x) / max(abs(R.x), 1e-4);
        float3 g = ((R.y > 0.0 ? bmax.y : bmin.y) - wpos.y) / max(abs(R.y), 1e-4);
        float3 h = ((R.z > 0.0 ? bmax.z : bmin.z) - wpos.z) / max(abs(R.z), 1e-4);
        float tbox = min(min(f.x, g.y), h.z);
        float3 pdir = wpos + R * tbox - center;
        float3 psize = max(bmax - bmin, 1e-4);
        float3 edge = min(wpos - bmin, bmax - wpos) / max(psize * 0.1, 1e-4);
        float fall = saturate(min(min(edge.x, edge.y), edge.z));
        fall *= fall * (3.0 - 2.0 * fall);
        float vol = psize.x * psize.y * psize.z;
        float pw = ProbeBoxMin[pi].w * (0.15 + 0.85 * fall) / (1.0 + vol * 0.0005);
        float3 prefl = float3(0.0, 0.0, 0.0);
        if (pi == 0) {
            prefl = ProbeCube0.SampleLevel(LinearSampler, pdir, clamp(roughness, 0.0, 1.0) * 6.0).rgb;
        } else if (pi == 1) {
            prefl = ProbeCube1.SampleLevel(LinearSampler, pdir, clamp(roughness, 0.0, 1.0) * 6.0).rgb;
        } else if (pi == 2) {
            prefl = ProbeCube2.SampleLevel(LinearSampler, pdir, clamp(roughness, 0.0, 1.0) * 6.0).rgb;
        } else {
            prefl = ProbeCube3.SampleLevel(LinearSampler, pdir, clamp(roughness, 0.0, 1.0) * 6.0).rgb;
        }
        probeAcc += prefl * pw;
        probeWsum += pw;
    }
    if (probeWsum > 1e-6) {
        env = probeAcc / probeWsum * envInt;
    }
    float4 ssr = SsrTX.Sample(LinearSampler, pin.uv);
    float3 specAmb = lerp(env, ssr.rgb, clamp(ssr.a, 0.0, 1.0));
    float cosNV = saturate(dot(N, V));
    float3 F0e = lerp(specF0, albedo, metallic);
    float2 brdf = BrdfTX.Sample(LinearSampler, float2(cosNV, roughness)).rg;
    float ccEnvF = 0.0;
    float3 ccEnv = float3(0.0, 0.0, 0.0);
    if (lb.cc > 0.001) {
        float FccE = 0.04 + 0.96 * pow(1.0 - cosNV, 5.0);
        ccEnvF = lb.cc * FccE;
        ccEnv = EnvTX.SampleLevel(LinearSampler, R, clamp(lb.ccRough, 0.0, 1.0) * 6.0).rgb * GradeInfo.z;
    }
    col += specAmb * (F0e * brdf.x + brdf.y) * ao * (1.0 - ccEnvF);
    col += ccEnv * ccEnvF * ao;
    if (dot(lb.sheen, lb.sheen) > 1e-8) {
        col += lb.sheen * hemi * 0.5 * ao;
    }
    col += albedo * lmFull.rgb;
    } else {
        col = albedo;
    }
    col += emis;
    if (FogInfo.x > 0.5) {
        float3 toFrag = wpos - CamPos.xyz;
        float fragDist = length(toFrag);
        if (fragDist > 1e-3) {
            float3 fogged = ApplyFog(col + amb, toFrag / fragDist, fragDist, pin.uv);
            col = fogged;
            amb = float3(0.0, 0.0, 0.0);
        }
    }
    float3 hdr = col + amb;
    return float4(hdr, 1.0);
}
