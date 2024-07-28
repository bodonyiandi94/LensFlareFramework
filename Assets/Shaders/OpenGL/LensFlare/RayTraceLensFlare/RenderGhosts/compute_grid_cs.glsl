#version 440

// Includes
#include <Shaders/OpenGL/Common/common.glsl>
#include <Shaders/OpenGL/LensFlare/RayTraceLensFlare/RenderGhosts/common.glsl>

// Kernel size
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

void main()
{
    // Init the necessary groupshared elements
    initGhostRayGroupSharedMemory(int(gl_LocalInvocationIndex), int(gl_WorkGroupSize.x * gl_WorkGroupSize.y));

	// Compute the coordinates of the current ray
	const ivec2 rayGridCoord = ivec2(gl_GlobalInvocationID.xy);

	// Skip if we are outside the actual ray grid
	if (any(greaterThanEqual(rayGridCoord, ivec2(sGhostParameters.iRayCount))))
		return;

	// Compute the grid entry
	sGhostGridEntryData[getRayGridEntryID(rayGridCoord)] = traceGridEntry(rayGridCoord);
}