#version 440

// Includes
#include <Shaders/OpenGL/Common/common.glsl>
#include <Shaders/OpenGL/LensFlare/RayTraceLensFlare/RenderGhostsTiled/common.glsl>

#define RESOLVE_GROUP_SIZE TILE_SIZE
layout(local_size_x = RESOLVE_GROUP_SIZE, local_size_y = RESOLVE_GROUP_SIZE, local_size_z = 1) in;

// List of all the primitive rays for the current batch in the tile
shared GhostSinglePrimitive tilePrimitives[RESOLVE_GROUP_SIZE * RESOLVE_GROUP_SIZE];

void main()
{
    // Compute the coordinates of the fragment and the ID of the current tile based on the global invocation ID
	const ivec2 fragmentCoord = ivec2(gl_GlobalInvocationID.xy); // 2D coords of the output fragment that this invocation belongs to
	const ivec2 tileId = ivec2(fragmentCoord / TILE_SIZE); // Index of the tile that the output fragment belongs to
    const vec2 fragmentNDC = pixelPosToNDC(fragmentCoord); // NDC coordinates for the current fragment
    const uint tileArrayId = tileArrayIndex(tileId); // Index into the tile buffer (that stores the primitives)
    const uint tileParamArrayId = tileParamArrayIndex(tileId); // Index into the tile property buffer (which stores the num. primitives)

    // Batch-related variables
    const int numTilePrimitives = min(sLensFlareDataTiled.iMaxTileEntries, int(sGhostTilesPropertiesBuffer[tileParamArrayId].uiNumPrimitives));
    const int workerIndex = int(gl_LocalInvocationIndex); // Index of the worker within the group
    const int numGroupWorkers = int(gl_WorkGroupSize.x * gl_WorkGroupSize.y); // Number of workers in this group in total
    const int numTotalBatches = (numTilePrimitives + numGroupWorkers - 1) / numGroupWorkers;

    // Output of the shader
    vec3 result = vec3(0.0);

    // Go through all the batches
    int batchBaseId = 0; // Batch base index
    int tilePrimitiveID = int(tileArrayId + workerIndex);
    for (int batchID = 0; batchID < numTotalBatches; ++batchID, batchBaseId += numGroupWorkers, tilePrimitiveID += numGroupWorkers)
    {
        // Extract the corresponding primitive
        const uint globalPrimitiveID = sGhostTilesBuffer[tilePrimitiveID].uiPrimitiveId;
        tilePrimitives[workerIndex] = sGhostTracedPrimitivesBuffer[globalPrimitiveID];

        // Wait until all the workers extracted their primitives into the shared buffer
        memoryBarrier();
        barrier();

        // Process all the primitives in the batch
        for (int primitiveID = 0; primitiveID < numGroupWorkers && (batchBaseId + primitiveID) < numTilePrimitives; ++primitiveID)
            result += processPrimitive(fragmentNDC, tilePrimitives[primitiveID]);
        
        // Wait until all the workers in the tile are finished processing the primitives
        memoryBarrier();
        barrier();
    }

    // Skip non-existent fragments
    if (any(greaterThanEqual(vec2(fragmentCoord), sLensFlareData.vRenderResolution)))
        return;

    // Sample the scene texture if needed (if applying results directly on top of the existing render)
    if (sLensFlareDataTiled.iSampleBaseScene == 1)
        result += texelFetch(sSceneColor, ivec3(fragmentCoord, 0), 0).rgb;
    
	// Write out the convolution result
	imageStore(sResult, ivec2(fragmentCoord), vec4(result, 1.0));
}