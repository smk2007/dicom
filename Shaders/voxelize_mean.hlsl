cbuffer CS_CONSTANT_BUFFER : register(b0)
{
    float ZPosition;
    min16uint Width;
    min16uint Height;
    min16uint Depth;
    float Unused;
};

struct InOutType
{
    half One;
    half Two;
    int three;
};


StructuredBuffer<InOutType> Buffer0 : register(t0);
RWStructuredBuffer<float> BufferOut : register(u0);

[numthreads(1, 1, 1)]
void CSMain( uint3 DTid : SV_DispatchThreadID )
{
    BufferOut[DTid.x] = (DTid.x % 20)/20.0f /2.0f;
    //BufferOut[DTid.x].Right = 1024;
}