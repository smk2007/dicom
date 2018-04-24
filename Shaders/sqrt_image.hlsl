RWStructuredBuffer<float> BufferOut : register(u0);

[numthreads(1, 1, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
    BufferOut[DTid.x] = sqrt(BufferOut[DTid.x]);
}