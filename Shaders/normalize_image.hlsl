cbuffer CS_CONSTANT_BUFFER : register(b0)
{
    float min;
    float max;
    double unused;
};

struct InOutType
{
    uint RG;
    uint BA;
};

StructuredBuffer<float> BufferIn : register(t0);
RWStructuredBuffer<InOutType> BufferOut : register(u0);

float3 HSVtoRGB(float3 HSV)
{
    float3 RGB = 0;
    float C = HSV.z * HSV.y;
    float H = HSV.x * 6;
    float X = C * (1 - abs(fmod(H, 2) - 1));
    if (HSV.y != 0)
    {
        float I = floor(H);
        if (I == 0) { RGB = float3(C, X, 0); }
        else if (I == 1) { RGB = float3(X, C, 0); }
        else if (I == 2) { RGB = float3(0, C, X); }
        else if (I == 3) { RGB = float3(0, X, C); }
        else if (I == 4) { RGB = float3(X, 0, C); }
        else { RGB = float3(C, 0, X); }
    }
    float M = HSV.z - C;
    return RGB + M;
}

[numthreads(1, 1, 1)]
void CSMain( uint3 DTid : SV_DispatchThreadID )
{
    uint USHRT_MAX = 0xFFFF;

    // Normalize
    float hue = (max - BufferIn[DTid.x]) / (max - min);

    // convert to rgb
    float3 rgb = HSVtoRGB(float3((1 - hue / 2), 1.f, 1.f));
    uint r = rgb.x * USHRT_MAX;
    uint g = rgb.y * USHRT_MAX;
    uint b = rgb.z * USHRT_MAX;
    BufferOut[DTid.x].RG = g << 16 | r;
    BufferOut[DTid.x].BA = USHRT_MAX << 16 | b;
}