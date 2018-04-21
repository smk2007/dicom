cbuffer CS_CONSTANT_BUFFER : register(b0)
{
    float min;
    float max;
    double unused;
};

RWStructuredBuffer<float> BufferOut : register(u0);

[numthreads(1, 1, 1)]
void CSMain( uint3 DTid : SV_DispatchThreadID )
{
    BufferOut[DTid.x] = (max - BufferOut[DTid.x]) / (max - min);
}