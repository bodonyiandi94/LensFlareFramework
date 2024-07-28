#include <Shaders/OpenGL/Generated/resource_indices.glsl>
#include <Shaders/OpenGL/Common/Rendering/render_settings.glsl>
#include <Shaders/OpenGL/Common/Noise/value.glsl>

// Depth level that is considered to be in the background
#define BACKGROUND_DEPTH (1.0 - 1e-6)

// Camera uniform buffer
layout (std140, binding = UNIFORM_BUFFER_CAMERA) buffer CameraData
{
    mat4 mView;
    mat4 mProjection;
    mat4 mViewProjection;
    mat4 mInverseView;
    mat4 mInverseProjection;
    mat4 mInverseViewProjection;
    mat4 mPrevView;
    mat4 mPrevProjection;
    mat4 mPrevViewProjection;
    mat4 mPrevInverseView;
    mat4 mPrevInverseProjection;
    mat4 mPrevInverseViewProjection;
    vec3 vEye;
    vec2 vFov;
    vec2 vFovDegrees;
    float fNear;
    float fFar;
    float fAspect;
	uint uiApertureMethod;
    float fApertureMin;
    float fApertureMax;
    float fAperture;
	int iApertureNoiseOctaves;
	float fApertureNoiseAmplitude;
	float fApertureNoisePersistance;
	float fApertureAdaptationRate;
    float fFocusDistanceMin;
    float fFocusDistanceMax;
    float fFocusDistance;
} sCameraData;

////////////////////////////////////////////////////////////////////////////////
float getApertureRadiusM()
{
    return sCameraData.fAperture * 0.5 * 1e-3;
}

////////////////////////////////////////////////////////////////////////////////
// Size of one pixel, in the units that Z is in
float pixelSize(const float z, const float fovy, const float height)
{
    return (tan(fovy * 0.5) * z) / (height * 0.5);
}

////////////////////////////////////////////////////////////////////////////////
/* 
 * horizontal: [-] left   [+] right 
 * vertical:   [-] bottom [+] top
 */
vec2 incidentAngle(const vec2 screenPos, const vec2 resolution)
{	
	const vec2 posCentered = screenPos - resolution / 2.0;
	const vec2 f = (resolution * 0.5) / tan(sCameraData.vFov * 0.5);
    return atan(posCentered / f);
}

////////////////////////////////////////////////////////////////////////////////
vec2 incidentAngleDegrees(const vec2 screenPos, const vec2 resolution)
{	
    return degrees(incidentAngle(screenPos, resolution));
}

////////////////////////////////////////////////////////////////////////////////
// Projected z
float projectedZ(float depth, mat4 projection)
{
    return (projection[3][2] / depth + projection[2][2] - 1) / -2;
}

float projectedZ(float depth)
{   
    return projectedZ(depth, sCameraData.mProjection);
}

////////////////////////////////////////////////////////////////////////////////
// Camera-space Z
float camSpaceZ(float depth, mat4 projection)
{
    return projection[3][2] / (depth * -2.0 + 1.0 - projection[2][2]);
}

float camSpaceZ(float depth)
{
    return camSpaceZ(depth, sCameraData.mProjection);
}

////////////////////////////////////////////////////////////////////////////////
// Camera space depth (positive distance, in meters)
float camSpaceDepth(float depth, mat4 projection)
{
    return -meters(camSpaceZ(depth, projection));
}

float camSpaceDepth(float depth)
{
    return -meters(camSpaceZ(depth));
}

////////////////////////////////////////////////////////////////////////////////
// Linear depth values
float linearDepth(const float depth, const float near, const float far)
{
    return (2.0 * near) / (far + near - depth * (far - near));
}

float linearDepth(const float depth)
{
    return linearDepth(depth, sCameraData.fNear, sCameraData.fFar);
}

// Linear camera space depth
float linearCamSpaceDepth(float depth)
{
    return (depth - meters(sCameraData.fNear)) / (meters(sCameraData.fFar) - meters(sCameraData.fNear));
}

////////////////////////////////////////////////////////////////////////////////
// Converts from camera to view space
vec4 clipToViewSpace(const vec3 csPos, const mat4 inverseProjection)
{
    const vec4 csPosition = inverseProjection * vec4(csPos, 1);
    return csPosition / csPosition.w;
}

// Converts from camera to view space
vec4 clipToViewSpace(const vec3 csPos)
{
    return clipToViewSpace(csPos, sCameraData.mInverseProjection);
}

////////////////////////////////////////////////////////////////////////////////
// Converts from camera to world space
vec4 clipToWorldSpace(const vec3 csPos, const mat4 inverseProj, const mat4 inverseView)
{
    const vec4 csPosition = inverseProj * vec4(csPos, 1);
    return inverseView * (csPosition / csPosition.w);
}

// Converts from camera to view space
vec4 clipToWorldSpace(const vec3 csPos)
{
    return clipToWorldSpace(csPos, sCameraData.mInverseProjection, sCameraData.mInverseView);
}