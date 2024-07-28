#version 440

// Includes
#include <Shaders/OpenGL/Common/common.glsl>
#include <Shaders/OpenGL/LensFlare/RayTraceLensFlare/RenderGhostsTiled/common.glsl>

// Kernel size
layout(local_size_x = TRACE_RAYS_GROUP_SIZE, local_size_y = TRACE_RAYS_GROUP_SIZE, local_size_z = 1) in;

// List of all the rays for the current group
shared GhostSingleTracedRay groupRays[TRACE_RAYS_GROUP_SIZE * TRACE_RAYS_GROUP_SIZE];

// Sizes for each worker in the group
shared int quadSizes[TRACE_RAYS_GROUP_SIZE * TRACE_RAYS_GROUP_SIZE];

////////////////////////////////////////////////////////////////////////////////
int rayId(const ivec2 base, const ivec2 offset)
{
    return (base.y + offset.y) * TRACE_RAYS_GROUP_SIZE + base.x + offset.x;
}

////////////////////////////////////////////////////////////////////////////////
ivec4 getVertIds(const ivec2 base, const int quadSize)
{
    return ivec4
    (
        rayId(base, ivec2(       0,        0)),
        rayId(base, ivec2(quadSize,        0)),
        rayId(base, ivec2(quadSize, quadSize)),
        rayId(base, ivec2(       0, quadSize))
    );
}

////////////////////////////////////////////////////////////////////////////////
bool isPrimitiveValid(const ivec4 vertIds)
{
    return sLensFlareData.iClipRays == 0 || 
    (
        isRayValid(groupRays[vertIds[0]]) ||
        isRayValid(groupRays[vertIds[1]]) ||
        isRayValid(groupRays[vertIds[2]]) ||
        isRayValid(groupRays[vertIds[3]])
    );
}

////////////////////////////////////////////////////////////////////////////////
bool isPrimitiveValid(const ivec2 workerId, const int quadSize)
{
    return isPrimitiveValid(getVertIds(workerId, quadSize));
}

////////////////////////////////////////////////////////////////////////////////
bool edgesSameFacing(const ivec4 quad1, const ivec4 quad2)
{
    /*
    const vec2 edges1[4] = 
    {
        normalize(groupRays[quad1[3]].vSensorPos.xy - groupRays[quad1[0]].vSensorPos.xy),
        normalize(groupRays[quad1[1]].vSensorPos.xy - groupRays[quad1[0]].vSensorPos.xy),
        normalize(groupRays[quad1[2]].vSensorPos.xy - groupRays[quad1[1]].vSensorPos.xy),
        normalize(groupRays[quad1[3]].vSensorPos.xy - groupRays[quad1[2]].vSensorPos.xy)
    };
    
    const vec2 edges2[4] = 
    {
        normalize(groupRays[quad2[3]].vSensorPos.xy - groupRays[quad2[0]].vSensorPos.xy),
        normalize(groupRays[quad2[1]].vSensorPos.xy - groupRays[quad2[0]].vSensorPos.xy),
        normalize(groupRays[quad2[2]].vSensorPos.xy - groupRays[quad2[1]].vSensorPos.xy),
        normalize(groupRays[quad2[3]].vSensorPos.xy - groupRays[quad2[2]].vSensorPos.xy)
    };*/

    const vec2 edges1[4] = 
    {
        abs(groupRays[quad1[3]].vSensorPos.xy - groupRays[quad1[0]].vSensorPos.xy),
        abs(groupRays[quad1[1]].vSensorPos.xy - groupRays[quad1[0]].vSensorPos.xy),
        abs(groupRays[quad1[2]].vSensorPos.xy - groupRays[quad1[1]].vSensorPos.xy),
        abs(groupRays[quad1[3]].vSensorPos.xy - groupRays[quad1[2]].vSensorPos.xy)
    };
    
    const vec2 edges2[4] = 
    {
        abs(groupRays[quad2[3]].vSensorPos.xy - groupRays[quad2[0]].vSensorPos.xy),
        abs(groupRays[quad2[1]].vSensorPos.xy - groupRays[quad2[0]].vSensorPos.xy),
        abs(groupRays[quad2[2]].vSensorPos.xy - groupRays[quad2[1]].vSensorPos.xy),
        abs(groupRays[quad2[3]].vSensorPos.xy - groupRays[quad2[2]].vSensorPos.xy)
    };
    
    return 
        dot(edges1[0] - edges2[0], vec2(1.0)) < sLensFlareDataTiled.fQuadMergeEdgeThreshold &&
        dot(edges1[1] - edges2[1], vec2(1.0)) < sLensFlareDataTiled.fQuadMergeEdgeThreshold &&
        dot(edges1[2] - edges2[2], vec2(1.0)) < sLensFlareDataTiled.fQuadMergeEdgeThreshold &&
        dot(edges1[3] - edges2[3], vec2(1.0)) < sLensFlareDataTiled.fQuadMergeEdgeThreshold;
}

////////////////////////////////////////////////////////////////////////////////
bool canMergeQuads(const ivec2 workerId, const int quadSize, const int mergedQuadSize)
{
    // Indices of the four merged quad verts
    const ivec4 vertIds = getVertIds(workerId, mergedQuadSize);

    // Indices of all the relevant quads
    const ivec4 vertIds0 = getVertIds(workerId + ivec2(       0,        0), quadSize);
    const ivec4 vertIds1 = getVertIds(workerId + ivec2(quadSize,        0), quadSize);
    const ivec4 vertIds2 = getVertIds(workerId + ivec2(quadSize, quadSize), quadSize);
    const ivec4 vertIds3 = getVertIds(workerId + ivec2(       0, quadSize), quadSize);

    // Indices of the four quads
    const ivec4 quadIds = getVertIds(workerId, quadSize);

    // Do not merge rays on the edge of the worker group
    if (any(greaterThanEqual(workerId, ivec2(gl_WorkGroupSize.xy - mergedQuadSize))))
        return false;

    // Only merge quads where all vertices that are valid
    if (!isRayValid(groupRays[vertIds[0]]) ||
        !isRayValid(groupRays[vertIds[1]]) ||
        !isRayValid(groupRays[vertIds[2]]) ||
        !isRayValid(groupRays[vertIds[3]]))
        return false;
    
    // We can only merge quads of the same size
    if (quadSizes[quadIds[0]] != quadSizes[quadIds[1]] ||
        quadSizes[quadIds[1]] != quadSizes[quadIds[2]] ||
        quadSizes[quadIds[2]] != quadSizes[quadIds[3]])
        return false;

    // We can only merge quads that have a similar edge facing
    if (!edgesSameFacing(vertIds0, vertIds1) ||
        !edgesSameFacing(vertIds1, vertIds2) ||
        !edgesSameFacing(vertIds2, vertIds3) ||
        !edgesSameFacing(vertIds3, vertIds0))
        return false;

    // We can merge the quad if all conditions are met
    return true;
}

////////////////////////////////////////////////////////////////////////////////
GhostSinglePrimitive createPrimitive(const ivec4 vertIds)
{
    GhostSinglePrimitive result;
    for (int i = 0; i < 4; ++i)
    {
        result.vSensorPos[i] = groupRays[vertIds[i]].vSensorPos;
        result.vSensorValues[i] = vec2(groupRays[vertIds[i]].fIntensity, groupRays[vertIds[i]].fClipFactor);
    }
    result.vColor = lambda2RGB(sLensFlareLensData.vWavelengths[sGhostParameters.iChannelID].x, 1.0) * sLensFlareLensData.vColor;
    return result;
}

////////////////////////////////////////////////////////////////////////////////
// Computes the aabb for the given quad
vec4 primitiveAABB(const ivec4 vertIds)
{
    vec4 result;
    result.xy = min(min(groupRays[vertIds[0]].vSensorPos, groupRays[vertIds[1]].vSensorPos), min(groupRays[vertIds[2]].vSensorPos, groupRays[vertIds[3]].vSensorPos));
    result.zw = max(max(groupRays[vertIds[0]].vSensorPos, groupRays[vertIds[1]].vSensorPos), max(groupRays[vertIds[2]].vSensorPos, groupRays[vertIds[3]].vSensorPos));
    return result;
}

////////////////////////////////////////////////////////////////////////////////
void main()
{
	// Compute the coordinates of the current ray
    const ivec2 rayCoord = ivec2(gl_WorkGroupID * (gl_WorkGroupSize - 1) + gl_LocalInvocationID); // Ray indices
    const ivec2 workerId = ivec2(gl_LocalInvocationID.xy); // ID of the worker within the group
    const int workerIndex = int(gl_LocalInvocationIndex); // Index of the worker within the group

    // Init the necessary groupshared elements
    initGhostRayGroupSharedMemory(workerIndex, TRACE_RAYS_GROUP_SIZE * TRACE_RAYS_GROUP_SIZE);

    // Skip doing anything if we are outside the ray grid size
    if (any(greaterThanEqual(rayCoord, ivec2(sGhostParameters.iRayCount))))
        return;
    
	// Spawn a ray on the input ray grid for this shader invocation
	const Ray ray = generateRayOnGrid(rayCoord);

	// Trace the ray through the optical system
	const Ray tracedRay = traceRay(ray);

	// Compute the resulting geometry entry
	GhostSingleTracedRay result;
    if (sLensFlareData.iProjectionMethod == ProjectionMethod_PupilGrid)
        result.vSensorPos = ray.pos.xy / (sLensFlareLensData.fOuterPupilHeight) * vec2(1.0 / sCameraData.fAspect, 1.0) * sLensFlareData.fFilmStretch;
    else if (sLensFlareData.iProjectionMethod == ProjectionMethod_SensorGrid)
        result.vSensorPos = tracedRay.pos.xy / (sLensFlareLensData.vFilmSize * 0.5) * sLensFlareData.fFilmStretch;
	result.fIntensity = saturate(tracedRay.intensity) * sGhostParameters.fIntensityScale;
    result.fLambda = sGhostParameters.fLambda;
    result.fClipFactor = tracedRay.clipFactor;

	// Store the result in the output buffer
    groupRays[workerIndex] = result;

    // Write out the quad size (skip quads on the edge)
    int quadSize;
    if (any(greaterThanEqual(workerId, ivec2(gl_WorkGroupSize.xy - 1))) ||  
        any(greaterThanEqual(rayCoord, ivec2(sGhostParameters.iRayCount - 1))))
        quadSize = 0;
    else
        quadSize = 1;
    quadSizes[workerIndex] = quadSize;

    memoryBarrier();
    barrier();

    // Skip invalid primitives
    if (!isPrimitiveValid(getVertIds(workerId, 1)))
    {
        quadSizes[workerIndex] = 0;
        return;
    }

    memoryBarrier();
    barrier();

    /*
    const GhostSinglePrimitive primitive_ = createPrimitive(getVertIds(workerId, quadSize));
    const vec4 aabb_ = primitiveAABB(primitive_);
    const uint outPrimitiveId_ = atomicAdd(sGlobalPropertiesBuffer.uiNumPrimitives, 1);
    sGhostTracedPrimitivesBuffer[outPrimitiveId_] = primitive_;
    sGhostTracedPrimitivePropertiesBuffer[outPrimitiveId_].vAABB = aabb_;
    return;
    */

    // Perform a number of merge steps
    for (int mergeId = 0; mergeId < sLensFlareDataTiled.iNumQuadMergeSteps; ++mergeId)
    {
        // Skip merged/non-existent quads
        if (quadSize == 0) return;

        // Size of the quad (in terms of neighboring vertices)
        const int nextQuadSize = quadSize * 2;

        // Base vertex coords (coordinate of the left vertex)
        const ivec2 baseWorkerId = (workerId / nextQuadSize) * nextQuadSize;

        // Determine whether we can be merged or not
        const bool canMerge = canMergeQuads(baseWorkerId, quadSize, nextQuadSize);
        
        memoryBarrier();
        barrier();

        // Compute the new quad sizes
        quadSize = !canMerge ? quadSize :
            (any(greaterThan(workerId % nextQuadSize, ivec2(0))) ? 0 : nextQuadSize);
        quadSizes[workerIndex] = quadSize;

        memoryBarrier();
        barrier();
    }

    // Skip merged/non-existent quads
    if (quadSize == 0) return;
    
    // Construct the resulting primitive
    const GhostSinglePrimitive primitive = createPrimitive(getVertIds(workerId, quadSize));
    const vec4 aabb = primitiveAABB(primitive);

    // Write out the generated primitive
    const uint outPrimitiveId = atomicAdd(sGlobalPropertiesBuffer.uiNumPrimitives, 1);
    sGhostTracedPrimitivesBuffer[outPrimitiveId] = primitive;
    sGhostTracedPrimitivePropertiesBuffer[outPrimitiveId].vAABB = aabb; 
    
    // Determine the tile start and end ids
    const vec4 aabbPixels = ndcAABBToPixels(aabb);
    const ivec2 coarseTileStartIds = tileCheckStartIdsCoarseTile(aabbPixels);
    const ivec2 coarseTileEndIds = tileCheckEndIdsCoarseTile(aabbPixels);

    // Insert the primitive into the relevant coarse tiles
    for (int coarseTileIdX = coarseTileStartIds[0]; coarseTileIdX < coarseTileEndIds[0]; ++coarseTileIdX)
    for (int coarseTileIdY = coarseTileStartIds[1]; coarseTileIdY < coarseTileEndIds[1]; ++coarseTileIdY)
    {
        // Tile IDs
        const ivec2 coarseTileID = ivec2(coarseTileIdX, coarseTileIdY);

        // Add the index of the primitive to the list of primitives in the current coarse tile if it intersects the tile
        if (intersectsCoarseTile(aabb.xy, aabb.zw, coarseTileID))
        {
            // Array indices
            const uint coarseTileArrayId = coarseTileArrayIndex(coarseTileID);
            const uint coarseTileParamArrayId = coarseTileParamArrayIndex(coarseTileID);

            // Increment the number of primitives in the tile
            const uint bufferIndex = atomicAdd(sGhostCoarseTilesPropertiesBuffer[coarseTileParamArrayId].uiNumPrimitives, 1);

            // Write out the primitiveID ID
            sGhostCoarseTilesBuffer[coarseTileArrayId + bufferIndex].uiPrimitiveId = outPrimitiveId;
        }
    }
}