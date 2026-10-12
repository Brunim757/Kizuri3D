cbuffer MatCB : register(b0)
{
    float4 Albedo;
    float4 Params;
    float4 Extra;
};
cbuffer MatExt : register(b1)
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
Texture2D AlbedoTex : register(t0);
Texture2D NormalTex : register(t1);
Texture2D OrmTex : register(t2);
Texture2D HeightTex : register(t3);
Texture2D LightmapTex : register(t4);
Texture2D EmissiveTex : register(t5);
Texture2D SpecTex : register(t6);
Texture2D SpecColorTex : register(t7);
Texture2D ClearcoatTex : register(t8);
Texture2D ClearcoatRoughTex : register(t9);
Texture2D ClearcoatNormalTex : register(t10);
Texture2D SheenColorTex : register(t11);
Texture2D SheenRoughTex : register(t12);
Texture2D AnisoTex : register(t13);
Texture2D IriTex : register(t14);
Texture2D IriThickTex : register(t15);
Texture2D DetailTex : register(t16);
Texture2D DetailNormalTex : register(t17);
Texture2D VtTable : register(t18);
Texture2D VtTail : register(t19);
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
struct PSOut
{
    float4 albedo : SV_Target0;
    float4 nrmRough : SV_Target1;
    float4 metallic : SV_Target2;
    float4 wpos : SV_Target3;
    float4 lightmap : SV_Target4;
    float4 emissive : SV_Target5;
    float4 f0 : SV_Target6;
    float4 extA : SV_Target7;
};
float2 XformUv(float2 uv, float2 uv2, int slot)
{
    float4 a = XfA[slot];
    float4 b = XfB[slot];
    float2 base = b.y > 0.5 ? uv2 : uv;
    float2 scaled = base * a.zw;
    float c = cos(b.x);
    float s = sin(b.x);
    float2 rotated = float2(scaled.x * c - scaled.y * s, scaled.x * s + scaled.y * c);
    return rotated + a.xy;
}
PSOut main(PSIn pin)
{
    PSOut pout;
    uint flags = (uint) Params.z;
    if (Params.w > 0.0 && Params.w < 1.0) {
        float2 cell = floor(pin.pos.xy);
        float h = frac(sin(dot(cell, float2(12.9898, 78.233))) * 43758.5453);
        if (h > Params.w) {
            discard;
        }
    }
    float2 pov = XformUv(pin.uv, pin.uv2, 0);
    if ((flags & 8u) != 0u) {
        float2 huv = XformUv(pin.uv, pin.uv2, 3);
        float3 vts = normalize(pin.vts);
        float2 marchDir = -vts.xy / max(vts.z, 0.05) * 0.05;
        float bestD = 0.0;
        float prevD = 0.0;
        for (int pi = 0; pi < 8; ++pi) {
            float pd = (float) (pi + 1) / 8.0;
            float ph = HeightTex.SampleLevel(GeoSamp, huv + marchDir * pd, 0).r;
            if (ph < pd) {
                float d0 = prevD;
                float d1 = pd;
                for (int pj = 0; pj < 4; ++pj) {
                    float dm = (d0 + d1) * 0.5;
                    float hm = HeightTex.SampleLevel(GeoSamp, huv + marchDir * dm, 0).r;
                    if (hm < dm) {
                        d1 = dm;
                    } else {
                        d0 = dm;
                    }
                }
                bestD = (d0 + d1) * 0.5;
                break;
            }
            prevD = pd;
        }
        pov = huv + marchDir * bestD;
    }
    float3 alb = Albedo.rgb;
    float alpha = Albedo.a;
    bool triOn = ((flags & 536870912u) != 0u);
    float3 triW = float3(0.0, 0.0, 0.0);
    float triS = 1.0;
    if (triOn) {
        float3 tn0 = normalize(pin.wnrm);
        triW = pow(abs(tn0), 4.0);
        triW /= max(triW.x + triW.y + triW.z, 1e-4);
        triS = max(Extra.y, 1e-4);
    }
    if ((flags & 1u) != 0u) {
        if (triOn) {
            float3 cx = AlbedoTex.Sample(GeoSamp, pin.wpos.zy * triS).rgb;
            float3 cy = AlbedoTex.Sample(GeoSamp, pin.wpos.xz * triS).rgb;
            float3 cz = AlbedoTex.Sample(GeoSamp, pin.wpos.xy * triS).rgb;
            alb *= cx * triW.x + cy * triW.y + cz * triW.z;
        } else if ((flags & 134217728u) != 0u) {
            float2 vtPage = floor(pov * 64.0);
            float2 vtFrac = frac(pov * 64.0);
            float4 ve = VtTable.Load(int3((int) min(vtPage.x, 63.0), (int) min(vtPage.y, 63.0), 0));
            if (ve.b > 0.5 && abs(ve.a * 255.0 - ExtG.w) < 0.5) {
                float2 phys = float2((ve.r * 255.0 * 34.0 + 1.0 + vtFrac.x * 32.0) / 2048.0, (ve.g * 255.0 * 34.0 + 1.0 + vtFrac.y * 32.0) / 1024.0);
                float4 vtT = AlbedoTex.SampleLevel(GeoSamp, phys, 0);
                alb *= vtT.rgb;
                alpha *= vtT.a;
            } else {
                float4 vtT = VtTail.SampleLevel(GeoSamp, pov, 0);
                alb *= vtT.rgb;
                alpha *= vtT.a;
            }
        } else {
            float4 albT = AlbedoTex.Sample(GeoSamp, pov);
            alb *= albT.rgb;
            alpha *= albT.a;
        }
    }
    if (ExtB.x > 0.5 && ExtB.x < 1.5 && alpha < ExtA.w) {
        discard;
    }
    float3 n = normalize(pin.wnrm);
    if (!pin.front && ExtB.y > 0.5) {
        n = -n;
    }
    float rough = Params.x;
    float metal = Params.y;
    float occ = 1.0;
    if ((flags & 2u) != 0u) {
        float2 nuv = XformUv(pin.uv, pin.uv2, 1);
        float3 t = normalize(pin.wtan.xyz);
        float3 b = cross(n, t) * pin.wtan.w;
        float2 nxy = NormalTex.Sample(GeoSamp, nuv).rg * 2.0 - 1.0;
        float nz = sqrt(saturate(1.0 - dot(nxy, nxy)));
        n = normalize(t * nxy.x + b * nxy.y + n * nz);
    }
    if ((flags & 4u) != 0u) {
        float2 ouv = XformUv(pin.uv, pin.uv2, 2);
        float3 orm = OrmTex.Sample(GeoSamp, ouv).rgb;
        if (triOn) {
            float3 ox = OrmTex.Sample(GeoSamp, pin.wpos.zy * triS).rgb;
            float3 oy = OrmTex.Sample(GeoSamp, pin.wpos.xz * triS).rgb;
            float3 oz = OrmTex.Sample(GeoSamp, pin.wpos.xy * triS).rgb;
            orm = ox * triW.x + oy * triW.y + oz * triW.z;
        }
        occ = orm.r;
        rough = orm.g;
        metal = orm.b;
    }
    if ((flags & 1024u) != 0u) {
        float2 cuv = XformUv(pin.uv, pin.uv2, 1);
        float3 t = normalize(pin.wtan.xyz);
        float3 b = cross(n, t) * pin.wtan.w;
        float2 cnxy = ClearcoatNormalTex.Sample(GeoSamp, cuv).rg * 2.0 - 1.0;
        float cnz = sqrt(saturate(1.0 - dot(cnxy, cnxy)));
        n = normalize(t * cnxy.x + b * cnxy.y + n * cnz);
    }
    float3 emi = ExtE.rgb * ExtA.x;
    if ((flags & 32u) != 0u) {
        float2 euv = XformUv(pin.uv, pin.uv2, 4);
        float3 emiFactor = ExtE.rgb;
        if (dot(emiFactor, float3(1.0, 1.0, 1.0)) <= 0.0) {
            emiFactor = float3(1.0, 1.0, 1.0);
        }
        emi = EmissiveTex.Sample(GeoSamp, euv).rgb * emiFactor * ExtA.x;
    }
    float iorSafe = max(ExtA.y, 1.01);
    float f0d = (iorSafe - 1.0) / (iorSafe + 1.0);
    f0d *= f0d;
    float3 F0 = float3(f0d, f0d, f0d);
    if ((flags & 65536u) != 0u) {
        float sstr = ExtA.z;
        if ((flags & 64u) != 0u) {
            sstr *= SpecTex.Sample(GeoSamp, XformUv(pin.uv, pin.uv2, 0)).a;
        }
        float3 scol = ExtF.rgb;
        if ((flags & 128u) != 0u) {
            scol *= SpecColorTex.Sample(GeoSamp, XformUv(pin.uv, pin.uv2, 0)).rgb;
        }
        F0 = scol * sstr;
    }
    float cc = ExtB.z;
    if ((flags & 256u) != 0u) {
        cc *= ClearcoatTex.Sample(GeoSamp, XformUv(pin.uv, pin.uv2, 0)).r;
    }
    float ccr = ExtB.w;
    if ((flags & 512u) != 0u) {
        ccr *= ClearcoatRoughTex.Sample(GeoSamp, XformUv(pin.uv, pin.uv2, 0)).g;
    }
    float3 sheen = ExtG.rgb;
    if ((flags & 2048u) != 0u) {
        sheen *= SheenColorTex.Sample(GeoSamp, XformUv(pin.uv, pin.uv2, 0)).rgb;
    }
    float sheenR = ExtC.x;
    if ((flags & 4096u) != 0u) {
        sheenR *= SheenRoughTex.Sample(GeoSamp, XformUv(pin.uv, pin.uv2, 0)).a;
    }
    float2 anisoDir = float2(1.0, 0.0);
    float anisoStr = ExtD.x;
    if ((flags & 8192u) != 0u) {
        float3 atx = AnisoTex.Sample(GeoSamp, XformUv(pin.uv, pin.uv2, 0)).rgb;
        anisoDir = atx.rg * 2.0 - 1.0;
        anisoStr *= atx.b;
    }
    float ca = cos(ExtD.y);
    float sa = sin(ExtD.y);
    float2 arot = float2(anisoDir.x * ca - anisoDir.y * sa, anisoDir.x * sa + anisoDir.y * ca);
    float3 wT = normalize(pin.wtan.xyz);
    float3 wB = normalize(cross(n, wT) * pin.wtan.w + 1e-6);
    float3 dirW = wT * arot.x + wB * arot.y;
    float dl = length(dirW);
    if (dl < 1e-4 || anisoStr <= 0.0) {
        dirW = float3(1.0, 0.0, 0.0);
        anisoStr = 0.0;
    } else {
        dirW /= dl;
    }
    float3 octN = dirW / (abs(dirW.x) + abs(dirW.y) + abs(dirW.z));
    float sx = octN.x >= 0.0 ? 1.0 : -1.0;
    float sy = octN.y >= 0.0 ? 1.0 : -1.0;
    float2 oct = octN.z >= 0.0 ? octN.xy : (1.0 - abs(float2(octN.y, octN.x))) * float2(sx, sy);
    float2 avec = oct * 0.5 + 0.5;
    float iri = ExtC.y;
    if ((flags & 16384u) != 0u) {
        iri *= IriTex.Sample(GeoSamp, XformUv(pin.uv, pin.uv2, 0)).r;
    }
    float iriThick = ExtC.w;
    if ((flags & 32768u) != 0u) {
        iriThick = lerp(ExtC.z, ExtC.w, IriThickTex.Sample(GeoSamp, XformUv(pin.uv, pin.uv2, 0)).g);
    }
    if ((flags & 1048576u) != 0u) {
        float3 det = DetailTex.Sample(GeoSamp, pin.uv * max(ExtI.x, 0.01)).rgb;
        alb *= det * 2.0;
    }
    if ((flags & 2097152u) != 0u) {
        float2 dnxy = DetailNormalTex.Sample(GeoSamp, pin.uv * max(ExtI.x, 0.01)).rg * 2.0 - 1.0;
        float dnz = sqrt(saturate(1.0 - dot(dnxy, dnxy)));
        n = normalize(n + (wT * dnxy.x + wB * dnxy.y) * 0.5 + n * (dnz - 1.0) * 0.5);
    }
    if ((flags & 4194304u) != 0u) {
        float lw = saturate(ExtI.y);
        if ((flags & 8388608u) != 0u) {
            float hb = 0.5;
            if ((flags & 8u) != 0u) {
                hb = HeightTex.SampleLevel(GeoSamp, XformUv(pin.uv, pin.uv2, 3), 0).r;
            }
            lw *= saturate((hb - ExtJ.w) * 6.0 + 0.5);
        }
        alb = lerp(alb, ExtJ.rgb, lw);
        rough = lerp(rough, ExtI.z, lw);
        metal = lerp(metal, ExtI.w, lw);
    }
    pout.albedo = float4(alb * occ, avec.x * 0.5 + 0.5);
    pout.nrmRough = float4(n, rough);
    pout.metallic = float4(metal, sheenR, iri, ExtE.w / 255.0);
    pout.wpos = float4(pin.wpos, avec.y * 0.5 + 0.5);
    float3 lmIrr = float3(0.0, 0.0, 0.0);
    if ((flags & 16u) != 0u) {
        lmIrr = LightmapTex.Sample(GeoSamp, pin.luv).rgb * Extra.x;
    }
    pout.lightmap = float4(lmIrr, anisoStr);
    pout.emissive = float4(emi, iriThick / 2000.0);
    pout.f0 = float4(F0, cc);
    pout.extA = float4(ccr, sheen);
    return pout;
}
