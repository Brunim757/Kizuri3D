struct CullInst
{
    float4 r0;
    float4 r1;
    float4 r2;
    float4 r3;
};
StructuredBuffer<CullInst> g_In : register(t0);
Texture2D HzbTX : register(t1);
RWStructuredBuffer<CullInst> g_Out : register(u0);
RWStructuredBuffer<uint> g_Args : register(u1);
cbuffer CullCB : register(b0)
{
    float4 Planes[6];
    float4 MeshBounds;
    float4 CullMeta;
    row_major float4x4 CullVP;
    float4 CullCamSize;
    float4 CullHzbSize;
};
[numthreads(64, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID)
{
    uint i = tid.x;
    if (i >= (uint) CullMeta.x) {
        return;
    }
    CullInst inst = g_In[i];
    float3 bc = inst.r0.xyz * MeshBounds.x + inst.r1.xyz * MeshBounds.y + inst.r2.xyz * MeshBounds.z + inst.r3.xyz;
    float sx = length(inst.r0.xyz);
    float sy = length(inst.r1.xyz);
    float sz = length(inst.r2.xyz);
    float smax = max(sx, max(sy, sz));
    float br = MeshBounds.w * smax;
    bool vis = true;
    for (int p = 0; p < 6; ++p) {
        if (dot(Planes[p].xyz, bc) + Planes[p].w < -br) {
            vis = false;
            break;
        }
    }
    if (vis && CullCamSize.w > 0.5) {
        float4 sc = mul(float4(bc, 1.0), CullVP);
        if (sc.w > 1e-5) {
            float2 suv = sc.xy / sc.w * float2(0.5, -0.5) + 0.5;
            if (suv.x >= 0.0 && suv.x <= 1.0 && suv.y >= 0.0 && suv.y <= 1.0) {
                int hx = min((int) (suv.x * CullCamSize.w), (int) CullCamSize.w - 1);
                int hy = min((int) (suv.y * CullHzbSize.x), (int) CullHzbSize.x - 1);
                float cellMin = HzbTX.Load(int3(max(hx, 0), max(hy, 0), 0)).r;
                float rayD = length(bc - CullCamSize.xyz);
                if (rayD > cellMin + br + 0.5) {
                    vis = false;
                }
            }
        }
    }
    if (!vis) {
        return;
    }
    uint dst = 0;
    InterlockedAdd(g_Args[1], 1, dst);
    g_Out[dst] = inst;
}
