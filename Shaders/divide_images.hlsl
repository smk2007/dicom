StructuredBuffer<float> BufferIn1 : register(t0);
StructuredBuffer<float> BufferIn2 : register(t1);
RWStructuredBuffer<float> BufferOut : register(u0);

[numthreads(1, 1, 1)]
void CSMain( uint3 DTid : SV_DispatchThreadID )
{
    float epsilon = .000000000000000001;
    float in1 = log(BufferIn1[DTid.x] + epsilon);
    float in2 = log(BufferIn2[DTid.x] + epsilon);
    BufferOut[DTid.x] = exp(in1 - in2);
}