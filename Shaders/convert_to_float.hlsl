cbuffer CS_CONSTANT_BUFFER : register(b0)
{
    uint Columns;
    uint UNUSED[3];
}; 

StructuredBuffer<uint> BufferIn : register(t0);
RWStructuredBuffer<float> BufferOut : register(u0);

[numthreads(1, 1, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
    uint index = (DTid.x * Columns + DTid.y);
    uint bufferIndex = floor(index / 2);
    uint bufferOffset = index % 2;

    if (bufferOffset == 0)
    {
        BufferOut[index] = (float)((BufferIn[bufferIndex] << 16) >> 16);
    }
    else
    {
        BufferOut[index] = (float)(BufferIn[bufferIndex] >> 16);
    }
}