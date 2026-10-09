cbuffer CullCB : register(b0)
{
    float4 Spheres[16];
    row_major float4x4 CullProj;
    float4 CullParams;
};
RWStructuredBuffer<uint> Grid : register(u0);
float4 TilePlane(float4 col0, float4 col3, float e)
{
    return col0 - e * col3;
}
float4 TilePlaneR(float4 col0, float4 col3, float e)
{
    return e * col3 - col0;
}
[numthreads(16, 9, 1)]
void main(uint3 tid : SV_DispatchThreadID)
{
    float gw = max(CullParams.y, 1.0);
    float gh = max(CullParams.z, 1.0);
    if (tid.x >= (uint)gw || tid.y >= (uint)gh) {
        return;
    }
    float l = (float)tid.x / gw * 2.0 - 1.0;
    float r = (float)(tid.x + 1) / gw * 2.0 - 1.0;
    float t = 1.0 - (float)tid.y / gh * 2.0;
    float b = 1.0 - (float)(tid.y + 1) / gh * 2.0;
    float4 col0 = float4(CullProj[0][0], CullProj[1][0], CullProj[2][0], CullProj[3][0]);
    float4 col1 = float4(CullProj[0][1], CullProj[1][1], CullProj[2][1], CullProj[3][1]);
    float4 col2 = float4(CullProj[0][2], CullProj[1][2], CullProj[2][2], CullProj[3][2]);
    float4 col3 = float4(CullProj[0][3], CullProj[1][3], CullProj[2][3], CullProj[3][3]);
    float4 planes[6];
    planes[0] = TilePlane(col0, col3, l);
    planes[1] = TilePlaneR(col0, col3, r);
    planes[2] = TilePlane(col1, col3, b);
    planes[3] = TilePlaneR(col1, col3, t);
    planes[4] = col2;
    planes[5] = col3 - col2;
    for (int p = 0; p < 6; ++p) {
        float len = length(planes[p].xyz);
        planes[p] /= max(len, 1e-8);
    }
    int count = (int)min(CullParams.x, 16.0);
    uint mask = 0;
    for (int i = 0; i < 16; ++i) {
        if (i >= count) {
            break;
        }
        float3 c = Spheres[i].xyz;
        float rad = max(Spheres[i].w, 0.0);
        bool inside = true;
        for (int p = 0; p < 6; ++p) {
            if (dot(planes[p].xyz, c) + planes[p].w < -rad) {
                inside = false;
                break;
            }
        }
        if (inside) {
            mask |= (1u << (uint)i);
        }
    }
    Grid[tid.y * 16 + tid.x] = mask;
}
