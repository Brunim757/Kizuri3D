struct PSIn
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};
float4 main(PSIn pin) : SV_Target
{
    return float4(frac(pin.uv), 0.0, 1.0);
}
