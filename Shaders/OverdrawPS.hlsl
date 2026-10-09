struct PSIn
{
    float4 pos : SV_Position;
};
float4 main(PSIn pin) : SV_Target
{
    return float4(1.0, 1.0, 1.0, 0.08);
}
