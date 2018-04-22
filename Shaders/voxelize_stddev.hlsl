cbuffer CS_CONSTANT_BUFFER : register(b0)
{
    float ZPosition;
    uint Width;
    uint Height;
    uint Depth;
};

struct InOutType
{
    uint RightLeft;
    int three;
};


StructuredBuffer<InOutType> Buffer0 : register(t0);
RWStructuredBuffer<float> BufferOut : register(u0);

[numthreads(1, 1, 1)]
void CSMain( uint3 DTid : SV_DispatchThreadID )
{
    BufferOut[DTid.x] = .5100;
    //BufferOut[DTid.x].Right = 1024;
}