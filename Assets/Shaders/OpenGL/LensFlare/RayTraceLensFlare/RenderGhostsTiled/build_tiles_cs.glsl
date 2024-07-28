#version 440

// Includes
#include <Shaders/OpenGL/Common/common.glsl>
#include <Shaders/OpenGL/LensFlare/RayTraceLensFlare/RenderGhostsTiled/common.glsl>

layout(local_size_x = TILE_SPLAT_GROUP_SIZE, local_size_y = 1, local_size_z = 1) in;

void main()
{
    // Index of the current primitive
    const int primitiveID = int(gl_GlobalInvocationID.x);
    const ivec2 coarseTileID = ivec2(gl_WorkGroupID.yz);
    const int tilesPerCoarseTile = sLensFlareDataTiled.iCoarseTileSize / sLensFlareDataTiled.iTileSize;
    const ivec2 baseTileID = coarseTileID * tilesPerCoarseTile;

    // Tile array indices
    const uint coarseTileArrayId = coarseTileArrayIndex(coarseTileID); // Index into the coarse tile buffer (that stores primitives)
    const uint coarseTileParamArrayId = coarseTileParamArrayIndex(coarseTileID); // Index into the coarse tile property buffer (which stores the num. primitives)
    const int numTotalPrimitives = int(sGhostCoarseTilesPropertiesBuffer[coarseTileParamArrayId].uiNumPrimitives);
    
    // Skip irrelevant primitives
    if (primitiveID >= numTotalPrimitives) return;
    
    // AABB of the primitive
    const uint globalPrimitiveID = sGhostCoarseTilesBuffer[coarseTileArrayId + primitiveID].uiPrimitiveId;
    const vec4 aabb = sGhostTracedPrimitivePropertiesBuffer[globalPrimitiveID].vAABB;

    // Try to insert the primitive into all the relevant sub-tiles
    for (int localTileX = 0, tileX = baseTileID.x; localTileX < tilesPerCoarseTile && tileX < sLensFlareDataTiled.vNumTiles.x; ++localTileX, ++tileX)
    for (int localTileY = 0, tileY = baseTileID.y; localTileY < tilesPerCoarseTile && tileY < sLensFlareDataTiled.vNumTiles.y; ++localTileY, ++tileY)
    {
        // Indices of the current tile
        const ivec2 tileID = ivec2(tileX, tileY);

        // Add the index of the primitive to the list of primitives in the current tile intersects the tile
        if (intersectsTile(aabb.xy, aabb.zw, tileID))
        {
            // Tile array indices
            const uint tileArrayId = tileArrayIndex(tileID); // Index into the tile buffer (that stores primitives)
            const uint tileParamArrayId = tileParamArrayIndex(tileID); // Index into the tile property buffer (which stores the num. primitives)

            // Increment the number of primitives in the tile
            const uint bufferIndex = atomicAdd(sGhostTilesPropertiesBuffer[tileParamArrayId].uiNumPrimitives, 1);

            // Write out the primitiveID ID
            if (bufferIndex < sLensFlareDataTiled.iMaxTileEntries)
                sGhostTilesBuffer[tileArrayId + bufferIndex].uiPrimitiveId = globalPrimitiveID;
        }
    }
}