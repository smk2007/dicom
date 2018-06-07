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
    float4 K = float4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    float3 p = abs(frac(HSV.xxx + K.xyz) * 6.0 - K.www);
    return HSV.z * lerp(K.xxx, clamp(p - K.xxx, 0.0, 1.0), HSV.y);
}

[numthreads(1, 1, 1)]
void CSMain( uint3 DTid : SV_DispatchThreadID )
{
    uint USHRT_MAX = 0xFFFF;

    // Normalize
    float normalizedValue = (BufferIn[DTid.x] - min) / (max - min);

    float darkness = normalizedValue;
    //.67 - (hue*.67);
    float hue = (-normalizedValue * .6666 /.6) + .6666;

    if (hue < 0.f)
    {
        hue = 0.f;
    }

    // convert to rgb
    float3 rgb = HSVtoRGB(float3(hue, 1.f, 1.f));
    uint r = rgb.x * USHRT_MAX;
    uint g = rgb.y * USHRT_MAX;
    uint b = rgb.z * USHRT_MAX;
    BufferOut[DTid.x].RG = g << 16 | r;
    BufferOut[DTid.x].BA = USHRT_MAX << 16 | b;
}