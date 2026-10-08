struct PSIn
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};
float RadicalInverseVdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}
float2 Hammersley(uint i, uint n)
{
    return float2(float(i) / float(n), RadicalInverseVdC(i));
}
float3 ImportanceSampleGGX(float2 xi, float3 n, float roughness)
{
    float a = roughness * roughness;
    float phi = 6.28318530718 * xi.x;
    float cosT = sqrt((1.0 - xi.y) / (1.0 + (a * a - 1.0) * xi.y));
    float sinT = sqrt(1.0 - cosT * cosT);
    float3 h = float3(cos(phi) * sinT, sin(phi) * sinT, cosT);
    float3 up = abs(n.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
    float3 tx = normalize(cross(up, n));
    float3 ty = cross(n, tx);
    return normalize(tx * h.x + ty * h.y + n * h.z);
}
float GeometrySchlickGGX(float ndv, float roughness)
{
    float a = roughness;
    float k = (a * a) / 2.0;
    return ndv / (ndv * (1.0 - k) + k);
}
float GeometrySmith(float3 n, float3 v, float3 l, float roughness)
{
    float ndv = max(dot(n, v), 0.0);
    float ndl = max(dot(n, l), 0.0);
    return GeometrySchlickGGX(ndv, roughness) * GeometrySchlickGGX(ndl, roughness);
}
float2 IntegrateBRDF(float ndv, float roughness)
{
    float3 v = float3(sqrt(1.0 - ndv * ndv), 0.0, ndv);
    float3 n = float3(0.0, 0.0, 1.0);
    float a = 0.0;
    float b = 0.0;
    uint count = 64;
    for (uint i = 0; i < count; ++i) {
        float2 xi = Hammersley(i, count);
        float3 h = ImportanceSampleGGX(xi, n, roughness);
        float3 l = normalize(2.0 * dot(v, h) * h - v);
        float ndl = max(l.z, 0.0);
        float ndh = max(h.z, 0.0);
        float vdh = max(dot(v, h), 0.0);
        if (ndl > 0.0) {
            float g = GeometrySmith(n, v, l, roughness);
            float gvis = (g * vdh) / (ndh * ndv + 1e-5);
            float fc = pow(1.0 - vdh, 5.0);
            a += (1.0 - fc) * gvis;
            b += fc * gvis;
        }
    }
    a /= float(count);
    b /= float(count);
    return float2(a, b);
}
float4 main(PSIn pin) : SV_Target
{
    float2 ab = IntegrateBRDF(max(pin.uv.x, 0.001), pin.uv.y);
    return float4(ab.x, ab.y, 0.0, 1.0);
}
