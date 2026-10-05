#define A_GPU 1
#define A_HLSL 1
#include "fsr/ffx_a.h"
#define FSR_RCAS_F 1
#include "fsr/ffx_fsr1.h"
Texture2D SrcTX : register(t0);
cbuffer FsrCB : register(b0)
{
    uint4 FsrCon0;
    uint4 FsrCon1;
    uint4 FsrCon2;
    uint4 FsrCon3;
};
AF4 FsrRcasLoadF(ASU2 p)
{
    return SrcTX.Load(ASU3(p, 0));
}
void FsrRcasInputF(inout AF1 r, inout AF1 g, inout AF1 b)
{
}
struct PSIn
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};
float4 main(PSIn pin) : SV_Target
{
    AF1 r;
    AF1 g;
    AF1 b;
    FsrRcasF(r, g, b, AU2(pin.pos.xy), FsrCon0);
    return float4(r, g, b, 1.0);
}
