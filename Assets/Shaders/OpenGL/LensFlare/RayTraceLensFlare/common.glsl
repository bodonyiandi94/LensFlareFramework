////////////////////////////////////////////////////////////////////////////////
// Constants
////////////////////////////////////////////////////////////////////////////////

#define MIN_INTENSITY 1e-8
//#define MIN_INTENSITY 1e-12

////////////////////////////////////////////////////////////////////////////////
// Ray-tracing structures
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Structure describing a lens interface
struct Lens
{
    // Center of the lens interface
    vec3 center;
    
    // Radius of curvature
    float radius;
    
    // Refraction indices
    vec4 n[2];
    
    // Height of the lens element
    float height;
    
    // Aperture height
    float aperture;
};

////////////////////////////////////////////////////////////////////////////////
// Structure describing a ray
struct Ray
{
    // Ray position
    vec3 pos;
    
    // Ray direction
    vec3 dir;
    
    // Position on the aperture (plain, unclamped)
    vec2 aperturePos;

    // Relative radius
    float radius;
    
    // Accumulated intensity
    float intensity;

    // Distance from the aperture (analytical value, using ray-tracing)
    float apertureDistAnalytical;

    // Distance from the aperture
    float apertureDist;

    // Clip factor used to clip the ray
    float clipFactor;
};

////////////////////////////////////////////////////////////////////////////////
// Uniforms
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Describes the parameters of a single ghost
struct GhostParameters
{
    ivec2 vGhostIndices;
    vec2 vPupilMin;
    vec2 vPupilMax;
    vec2 vPupilCenter;
    vec2 vPupilHalfSize;
    vec2 vSensorMin;
    vec2 vSensorMax;

    int iRayCount;
    int iGhostID;
    int iChannelID;
    float fLambda;
    float fIntensityScale;
    
    int iGridStartID;
    int iNumPolynomialTerms;
};

// Ghost geometry output buffer
TYPED_ARRAY_BUFFER(std140, UNIFORM_BUFFER_GENERIC_1, sGhostParametersBuffer_, GhostParameters);
#define sGhostParametersBuffer sGhostParametersBuffer_.sData

// parameters for the current ghost
#ifdef SHADER_TYPE_COMPUTE_SHADER
#define sGhostParameters sGhostParametersBuffer[gl_WorkGroupID.z]
#else
layout (location = 0) uniform int iGhostParamID = 0;
#define sGhostParameters sGhostParametersBuffer[iGhostParamID]
#endif

////////////////////////////////////////////////////////////////////////////////
// Len's flare uniform buffer
layout (std140, binding = UNIFORM_BUFFER_GENERIC_2) uniform RayTraceLensFlareUniformDataLens
{
    vec3 vColor;
    vec3 vRayDir;
    float fRayDistance;
    
    float fLightRotation;
    float fLightAngle;

    float fRadiusClip;
    float fIrisClip;
    float fIntensityClip;
    float fRefractionClip;

    float fOuterPupilHeight;
    float fApertureHeight;
    vec2 vFilmSize;

    int iNumLenses;
    
    vec4 vWavelengths[MAX_CHANNELS];
    Lens sLenses[MAX_CHANNELS][MAX_LENSES];
} sLensFlareLensData;

////////////////////////////////////////////////////////////////////////////////
// Len's flare uniform buffer
layout (std140, binding = UNIFORM_BUFFER_GENERIC_3) uniform RayTraceLensFlareUniformDataCommon
{
    ivec2 vRenderResolution;

    int iGridMethod;
    int iRaytraceMethod;
    int iProjectionMethod;
    int iShadingMethod;
    int iOverlayMethod;
    int iIsWireFrame;
    int iClipRays;
    int iClipPixels;
    float fFilmStretch;

    int iNumGhosts;
    int iNumChannels;
    int iMaxNumGhosts;
    int iMaxNumChannels;
    int iMaxRayGridSize;

    int iNumPolynomialAngles;
    int iNumPolynomialRotations;
    float fMaxPolynomialAngles;
    float fMaxPolynomialRotations;
    float fPolynomialAnglesStep;
    float fPolynomialRotationsStep;
} sLensFlareData;

// Iris mask texture
layout (binding = TEXTURE_POST_PROCESS_1) uniform sampler2D sAperture;

////////////////////////////////////////////////////////////////////////////////
// Ray tracing functions
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
Ray createRay(const vec3 rayPosition, const vec3 rayDirection)
{
    Ray result;
    
    result.pos = rayPosition;
    result.dir = rayDirection;
    result.aperturePos = vec2(1.0);
    result.radius = 0.0;
    result.intensity = 1.0;
    result.apertureDist = 0.0;
    result.apertureDistAnalytical = 0.0;
    result.clipFactor = 0.0;
    
    return result;
}

////////////////////////////////////////////////////////////////////////////////
void invalidateRay(inout Ray ray)
{
    ray.intensity = -1e6; // Zero out the intensity
    ray.radius = 5; // 'Zero out' the radius
    ray.apertureDist = 5; // 'Zero out' the aperture distance
    ray.apertureDistAnalytical = 5;
    ray.clipFactor = 5; // Zero out the clip factor
    ray.aperturePos = vec2(1.0); // Zero out the aperture pos
}

////////////////////////////////////////////////////////////////////////////////
ivec2 getRayID(const int rayCount, const int vertexID)
{
    const int SUBDIVISION = rayCount - 1;

    // Quad indices
    const ivec2 QUAD_IDS[] =
    {
        ivec2(0, 0),
        ivec2(1, 0),
        ivec2(1, 1),

        ivec2(1, 1),
        ivec2(0, 1),
        ivec2(0, 0),
    };
    
    // Column id
    const int col = (vertexID / 6) % SUBDIVISION;
    const int row = (vertexID / 6) / SUBDIVISION;
    const int vert = vertexID % 6;

    return ivec2(col, row) + QUAD_IDS[vert];   
}

////////////////////////////////////////////////////////////////////////////////
ivec2 getRayID(const int vertexID)
{
    return getRayID(sGhostParameters.iRayCount, vertexID);
}

////////////////////////////////////////////////////////////////////////////////
int getRayGridEntryID(const ivec2 rayGridCoord)
{
    return sGhostParameters.iGridStartID + rayGridCoord.y * sGhostParameters.iRayCount + rayGridCoord.x;
}

////////////////////////////////////////////////////////////////////////////////
vec3 generateRayStartPos(const float rayDistance, const int rayCount, const vec2 pupilMin, const vec2 pupilMax, const ivec2 rayID)
{
    return vec3
    (
        pupilMin + ((vec2(rayID) / vec2(rayCount - 1)) * (pupilMax - pupilMin)),
        rayDistance
    );
}

////////////////////////////////////////////////////////////////////////////////
Ray generateRayOnGrid(const float rayDistance, const vec3 rayDir, const int rayCount, const vec2 pupilMin, const vec2 pupilMax, const ivec2 rayID)
{
    return createRay(generateRayStartPos(rayDistance, rayCount, pupilMin, pupilMax, rayID), rayDir);
}

////////////////////////////////////////////////////////////////////////////////
Ray generateRayOnGrid(const ivec2 rayID)
{
    return generateRayOnGrid(
        sLensFlareLensData.fRayDistance, 
        sLensFlareLensData.vRayDir, 
        sGhostParameters.iRayCount, 
        sGhostParameters.vPupilMin, 
        sGhostParameters.vPupilMax, 
        rayID);
}

////////////////////////////////////////////////////////////////////////////////
Ray generateRayOnGrid(const int vertexID)
{
    return generateRayOnGrid(getRayID(vertexID));
}

////////////////////////////////////////////////////////////////////////////////
// Determines if the parameter ray is tractable (not inf/nan)
bool isRayTractable(const float intensity, const float fClipFactor)
{
    return !isinf(intensity) && !isnan(intensity) && !isinf(fClipFactor) && !isnan(fClipFactor);
}

////////////////////////////////////////////////////////////////////////////////
// Determines if the parameter ray is valid
bool isRayValid(const float intensity, const float clipFactor)
{
    return clipFactor <= 1.0 && intensity >= MIN_INTENSITY;
}

////////////////////////////////////////////////////////////////////////////////
// Determines if the parameter ray is valid
bool isRayValid(const float intensity, const float radius, const float iris)
{
    return radius <= sLensFlareLensData.fRadiusClip && intensity >= MIN_INTENSITY && iris <= sLensFlareLensData.fIrisClip;
}

////////////////////////////////////////////////////////////////////////////////
float rayApertureDist(const vec2 aperturePosNormalised)
{
    return any(lessThanEqual(aperturePosNormalised, vec2(-1.0)) || greaterThanEqual(aperturePosNormalised, vec2(1.0))) ? 2.0 : texture(sAperture, aperturePosNormalised * 0.5 + 0.5).r;
}

////////////////////////////////////////////////////////////////////////////////
float rayApertureDist(const vec2 aperturePos, const float apertureHeight)
{
    return rayApertureDist(aperturePos / apertureHeight);
}

////////////////////////////////////////////////////////////////////////////////
float rayApertureDist(const Ray ray)
{
    return rayApertureDist(ray.aperturePos / sLensFlareLensData.fApertureHeight);
}

////////////////////////////////////////////////////////////////////////////////
float rayClipFactor(const Ray ray, const float radiusClip, const float irisClip)
{
    return max(ray.radius / radiusClip, ray.apertureDist / irisClip);
}

////////////////////////////////////////////////////////////////////////////////
// Ghost-ray tracing functionality
////////////////////////////////////////////////////////////////////////////////

#include <Shaders/OpenGL/LensFlare/RayTraceLensFlare/TraceRays/trace_rays.glsl>

////////////////////////////////////////////////////////////////////////////////
bool conditionalTransformToEntrancePupilCoords(const Ray ray, out vec2 entranceCoords)
{
    const Ray entranceRay = traceRayEntrance(ray);
    entranceCoords = entranceRay.pos.xy;
    return entranceRay.intensity >= MIN_INTENSITY;
}

////////////////////////////////////////////////////////////////////////////////
vec2 transformToEntrancePupilCoords(const Ray ray)
{
    return traceRayEntrance(ray).pos.xy;
    // TODO: rotate ray.pos.xy using theta to obtain these coordinates
}