#version 440

// Includes
#include <Shaders/OpenGL/Common/common.glsl>
#include <Shaders/OpenGL/LensFlare/RayTraceLensFlare/PrecomputeGhosts/common.glsl>

// Kernel size
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

void main()
{
	// Compute the coordinates of the current ray
	const ivec2 rayGridCoord = ivec2(gl_GlobalInvocationID.xy);

	// Skip if we are outside the actual image
	if (any(greaterThanEqual(rayGridCoord, ivec2(sGhostParameters.iRayCount))))
		return;

	// Spawn a ray on the input ray grid for this shader invocation
	const Ray ray = generateRayOnGrid(rayGridCoord);

	// Trace the ray through the optical system
	const Ray tracedRayEntrance = traceRayEntrance(ray);
	const Ray tracedRay = traceRay(ray);

	// Compute the resulting geometry entry
	GhostGeometryEntry result;
	// - pupil pos
    result.vPupilPosCartesian = ray.pos.xy;
    result.vPupilPosCartesianNormalized = result.vPupilPosCartesian / sLensFlareLensData.fOuterPupilHeight;
    result.vPupilPosPolar = cart2pol(result.vPupilPosCartesian);
    result.vPupilPosPolarNormalized = cart2pol(result.vPupilPosCartesian / sLensFlareLensData.fOuterPupilHeight);
	// - centered pupil pos
    result.vCenteredPupilPosCartesian = ray.pos.xy - sGhostParameters.vPupilCenter;
    result.vCenteredPupilPosCartesianNormalized = result.vCenteredPupilPosCartesian / sGhostParameters.vPupilHalfSize;
    result.vCenteredPupilPosPolar = cart2pol(result.vCenteredPupilPosCartesian);
    result.vCenteredPupilPosPolarNormalized = cart2pol(result.vCenteredPupilPosCartesian / sGhostParameters.vPupilHalfSize);
	// - entrance pupil pos
    result.vEntrancePupilPosCartesian = tracedRayEntrance.pos.xy;
    result.vEntrancePupilPosCartesianNormalized = result.vEntrancePupilPosCartesian / sLensFlareLensData.fOuterPupilHeight;
    result.vEntrancePupilPosPolar = cart2pol(result.vEntrancePupilPosCartesian);
    result.vEntrancePupilPosPolarNormalized = cart2pol(result.vEntrancePupilPosCartesian / sLensFlareLensData.fOuterPupilHeight);
	// - aperture pos
	result.vAperturePos = tracedRay.aperturePos;
	result.vAperturePosNormalized = tracedRay.aperturePos / sLensFlareLensData.fOuterPupilHeight;
	// - sensor pos
	result.vSensorPos = tracedRay.pos.xy;
	result.vSensorPosNormalized = tracedRay.pos.xy / (sLensFlareLensData.vFilmSize * 0.5);
	// - aperture dist
    result.fApertureDistAnalytical = tracedRay.apertureDistAnalytical;
	result.fApertureDistTexture = tracedRay.apertureDist;
	result.fApertureDistPupilAbsolute = pupilApertureDistAbsolute(tracedRay);
	result.fApertureDistPupilBounded = pupilApertureDistBounded(tracedRay);
	// - other ray-traced attributes
	result.fIntensity = tracedRay.intensity;
	result.fRelativeRadius = tracedRay.radius;
	result.fClipFactor = tracedRay.clipFactor;

	// Store the result in the output buffer
	const int outputBufferID = rayGridCoord.y * sGhostParameters.iRayCount + rayGridCoord.x;
	sGhostGeometryData.sGhostGeometries[outputBufferID] = result;
}