#version 440

// Includes
#include <Shaders/OpenGL/Common/common.glsl>
#include <Shaders/OpenGL/LensFlare/RayTraceLensFlare/RenderGhostsTiled/common.glsl>

// Kernel size
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

void main()
{
    // Extract the total number of coarse tiles
    const uvec2 numCoarseTiles = uvec2(sLensFlareDataTiled.vNumCoarseTiles);

    // Figure out the maximum number of primitives
    uint numMaxPrimitives = 0;
    for (int coarseTileIdX = 0; coarseTileIdX < numCoarseTiles[0]; ++coarseTileIdX)
    for (int coarseTileIdY = 0; coarseTileIdY < numCoarseTiles[1]; ++coarseTileIdY)
    {
        const ivec2 coarseTileID = ivec2(coarseTileIdX, coarseTileIdY);
        const uint coarseTileParamArrayId = coarseTileParamArrayIndex(coarseTileID);
        numMaxPrimitives = max(numMaxPrimitives, sGhostCoarseTilesPropertiesBuffer[coarseTileParamArrayId].uiNumPrimitives);
    }

    // Compute the number of groups required
    const uint numGroups = ROUNDED_DIV(numMaxPrimitives, TILE_SPLAT_GROUP_SIZE);

    // Write out the dispatch parameters
    sDispatchIndirectBuffer.vNumGroups = uvec3(numGroups, numCoarseTiles);
}