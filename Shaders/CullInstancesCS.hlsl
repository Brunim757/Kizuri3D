struct CullInst
{
    float4 r0;
    float4 r1;
    float4 r2;
    float4 r3;
};
StructuredBuffer<CullInst> g_In : register(t0);
RWStructuredBuffer<CullInst> g_Out : register(u0);
RWStructuredBuffer<uint> g_Args : register(u1);
cbuffer CullCB : register(b0)
{
    float4 Planes[6];
    float4 MeshBounds;
    float4 CullMeta;
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
    if (!vis) {
        return;
    }
    uint dst = 0;
    InterlockedAdd(g_Args[1], 1, dst);
    g_Out[dst] = inst;
}
