cbuffer CS_CONSTANT_BUFFER : register(b0)
{
    uint INPUT_NUM;
    uint UNUSED[3];
};

StructuredBuffer<float> BufferIn : register(t0);
RWStructuredBuffer<float> BufferOut : register(u0);

[numthreads(1, 1, 1)]
void CSMain( uint3 DTid : SV_DispatchThreadID )
{
    BufferOut[DTid.x] = ((INPUT_NUM * BufferOut[DTid.x]) + BufferIn[DTid.x]) / (INPUT_NUM + 1);
}