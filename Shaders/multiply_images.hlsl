StructuredBuffer<float> BufferIn1 : register(t0);
StructuredBuffer<float> BufferIn2 : register(t1);
RWStructuredBuffer<float> BufferOut : register(u0);

[numthreads(1, 1, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
    BufferOut[DTid.x] = BufferIn1[DTid.x] * BufferIn2[DTid.x];
}