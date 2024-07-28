#include <Shaders/OpenGL/LensFlare/RayTraceLensFlare/common.glsl>

// Describes the output attributes of a single ray
struct GhostGeometryEntry
{
    vec2 vPupilPosCartesian;
    vec2 vPupilPosCartesianNormalized;
    vec2 vPupilPosPolar;
    vec2 vPupilPosPolarNormalized;
    vec2 vCenteredPupilPosCartesian;
    vec2 vCenteredPupilPosCartesianNormalized;
    vec2 vCenteredPupilPosPolar;
    vec2 vCenteredPupilPosPolarNormalized;
    vec2 vEntrancePupilPosCartesian;
    vec2 vEntrancePupilPosCartesianNormalized;
    vec2 vEntrancePupilPosPolar;
    vec2 vEntrancePupilPosPolarNormalized;
    vec2 vAperturePos;
    vec2 vAperturePosNormalized;
    vec2 vSensorPos;
    vec2 vSensorPosNormalized;
    float fApertureDistAnalytical;
    float fApertureDistTexture;
    float fApertureDistPupilAbsolute;
    float fApertureDistPupilBounded;
    float fIntensity;
    float fRelativeRadius;
    float fClipFactor;
};

// Ghost geometry output buffer
layout (std140, binding = UNIFORM_BUFFER_GENERIC_4) buffer GhostGeometryData
{
    GhostGeometryEntry sGhostGeometries[];
} sGhostGeometryData;

////////////////////////////////////////////////////////////////////////////////
// Ray generation and other helpers
////////////////////////////////////////////////////////////////////////////////

vec2 pupilUvAbsolute(const Ray ray)
{
    return (ray.pos.xy + vec2(sLensFlareLensData.fOuterPupilHeight)) / vec2(2 * sLensFlareLensData.fOuterPupilHeight);
}

vec2 pupilUvBounded(const Ray ray)
{
    return (ray.pos.xy - sGhostParameters.vPupilMin) / (sGhostParameters.vPupilMax - sGhostParameters.vPupilMin);
}

float pupilApertureDistAbsolute(const Ray ray)
{
    return texture(sAperture, pupilUvAbsolute(ray)).r;
}

float pupilApertureDistBounded(const Ray ray)
{
    return texture(sAperture, pupilUvBounded(ray)).r;
}