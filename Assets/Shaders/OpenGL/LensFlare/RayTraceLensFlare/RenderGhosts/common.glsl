#include <Shaders/OpenGL/LensFlare/RayTraceLensFlare/common.glsl>

////////////////////////////////////////////////////////////////////////////////
// Uniforms
////////////////////////////////////////////////////////////////////////////////

// Len's flare uniform buffer
layout (std140, binding = UNIFORM_BUFFER_GENERIC_3) uniform RayTraceLensFlareUniformDataDirectCommon
{
    int _;
} sLensFlareDataDirect;

////////////////////////////////////////////////////////////////////////////////
// Structure describing a ray in the grid
struct GridEntry
{
    vec2 vPupilPos;
    vec2 vSensorPos;
    vec2 vAperturePos;
    float fIntensity;
    float fRelativeRadius;
    float fApertureDist;
    float fClipFactor;
};

// Ghost geometry output buffer
TYPED_ARRAY_BUFFER(std140, UNIFORM_BUFFER_GENERIC_7, sGhostGridEntryData_, GridEntry);
#define sGhostGridEntryData sGhostGridEntryData_.sData

////////////////////////////////////////////////////////////////////////////////
// Ray grid management
////////////////////////////////////////////////////////////////////////////////

GridEntry traceGridEntry(const ivec2 rayID)
{
	const Ray ray = generateRayOnGrid(rayID);
	const Ray tracedRay = traceRay(ray);
	GridEntry result;
	result.vPupilPos = ray.pos.xy;
	result.vSensorPos = tracedRay.pos.xy;
	result.vAperturePos = tracedRay.aperturePos;
	result.fIntensity = saturate(tracedRay.intensity) * sGhostParameters.fIntensityScale;
	result.fRelativeRadius = tracedRay.radius;
	result.fClipFactor = tracedRay.clipFactor;
    result.fApertureDist = tracedRay.apertureDist;
    return result;
}

GridEntry traceGridEntry(const int vertexID)
{
    return traceGridEntry(getRayID(vertexID));
}

GridEntry computeGridEntry(const int vertexID)
{
    if (sLensFlareData.iGridMethod == GridMethod_FixedGrid)
        return traceGridEntry(vertexID);
    else if (sLensFlareData.iGridMethod == GridMethod_AdaptiveGrid)
	    return sGhostGridEntryData[getRayGridEntryID(getRayID(vertexID))];
    return traceGridEntry(vertexID);
}