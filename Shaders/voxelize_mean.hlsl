cbuffer CS_CONSTANT_BUFFER : register(b0)
{
    uint INPUT_R;
    uint INPUT_C;
    uint INPUT_D;
    uint OUTPUT_R;
    uint OUTPUT_C;
    uint OUTPUT_D;
    float SPACING_X;
    float SPACING_Y;
    float SPACING_Z;
    float VOXEL_SPACING_X;
    float VOXEL_SPACING_Y;
    float VOXEL_SPACING_Z;
    uint MODE;
    uint UNUSED[3];
};

StructuredBuffer<uint> BufferIn : register(t0);
RWStructuredBuffer<float> BufferOut : register(u0);
RWStructuredBuffer<uint> BufferCountsOut : register(u1);

uint3 GetImageForIndex(uint index)
{
    uint stride = OUTPUT_C * OUTPUT_D;
    uint3 rcd;
    rcd.x = floor(index / (float)stride); // row
    rcd.y = (index % stride) % OUTPUT_C; // column
    rcd.z = floor((index % stride) / (float)OUTPUT_C); // depth
    return rcd;
}

[numthreads(1, 1, 1)]
void CSMain( uint3 DTid : SV_DispatchThreadID )
{
    uint3 rcd = GetImageForIndex(DTid.x);

    // Ensure that the slice image being processed is relevant to the current pixel.
    uint inputVoxelDepth = floor(INPUT_D * SPACING_Z / VOXEL_SPACING_Z);
    if (inputVoxelDepth != rcd.z)
    {
        return;
    }

    float aggregator = 0;
    uint inputStartRow    = floor(rcd.x * VOXEL_SPACING_Y / SPACING_Y);
    uint inputEndRow      = floor((rcd.x + 1) * VOXEL_SPACING_Y / SPACING_Y);
    uint inputStartColumn = floor(rcd.y * VOXEL_SPACING_X / SPACING_X);
    uint inputEndColumn   = floor((rcd.y + 1) * VOXEL_SPACING_X / SPACING_X);

    for (uint r = inputStartRow; r < inputEndRow; r++)
    {
        for (uint c = inputStartColumn; c < inputEndColumn; c++)
        {
            uint bufferIndex = floor((r * INPUT_C + c) / 2);
            uint bufferOffset = (r * INPUT_C + c) % 2;

            float value;
            if (bufferOffset == 0)
            {
                value = (float)((BufferIn[bufferIndex] << 16) >> 16) / (float)0xFFFF;
            }
            else
            {
                value = (float)(BufferIn[bufferIndex] >> 16) / (float)0xFFFF;
            }

            if (MODE == 0)
            {
                aggregator += value;
            }
            else if (MODE == 1)
            {
                aggregator += (value * value);
            }
        }
    }

    uint nCount = (inputEndRow - inputStartRow) * (inputEndColumn - inputStartColumn);
    BufferOut[DTid.x] = ((BufferCountsOut[DTid.x] * BufferOut[DTid.x]) + aggregator) / (BufferCountsOut[DTid.x] + nCount);
    BufferCountsOut[DTid.x] += nCount;
}