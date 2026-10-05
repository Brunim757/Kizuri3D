#define A_GPU 1
#define A_HLSL 1
#include "fsr/ffx_a.h"
#define FSR_EASU_F 1
#include "fsr/ffx_fsr1.h"
Texture2D SrcTX : register(t0);
SamplerState LinearSampler : register(s0);
cbuffer FsrCB : register(b0)
{
    uint4 FsrCon0;
    uint4 FsrCon1;
    uint4 FsrCon2;
    uint4 FsrCon3;
};
AF4 FsrEasuRF(AF2 p)
{
    return SrcTX.GatherRed(LinearSampler, p);
}
AF4 FsrEasuGF(AF2 p)
{
    return SrcTX.GatherGreen(LinearSampler, p);
}
AF4 FsrEasuBF(AF2 p)
{
    return SrcTX.GatherBlue(LinearSampler, p);
}
struct PSIn
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};
float4 main(PSIn pin) : SV_Target
{
    AF3 c;
    FsrEasuF(c, AU2(pin.pos.xy), FsrCon0, FsrCon1, FsrCon2, FsrCon3);
    return float4(c, 1.0);
}
