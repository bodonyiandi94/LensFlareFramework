#version 440

// Includes
#include <Shaders/OpenGL/Common/common.glsl>
#include <Shaders/OpenGL/LensFlare/RayTraceLensFlare/common.glsl>

// Kernel size
layout(local_size_x = BAKE_POLYNOMIALS_GROUP_SIZE, local_size_y = 1, local_size_z = 1) in;

void main()
{
#if BAKE_POLYNOMIAL_INVARIANTS == 1 && (RAY_TRACE_METHOD == RaytraceMethod_PolynomialFullFit || RAY_TRACE_METHOD == RaytraceMethod_PolynomialPartialFit)
	// Skip if we are outside the valid range of ghosts
	if (int(gl_GlobalInvocationID.x) > sLensFlareData.iMaxNumGhosts * sLensFlareData.iMaxNumChannels)
		return;

	// Current indices
    const int ghostID = int(gl_GlobalInvocationID.x / 3);
    const int wavelengthID = int(gl_GlobalInvocationID.x % 3);

	// Compute the partials
	bakePolynomialInvariants(ghostID, wavelengthID, sLensFlareLensData.fLightRotation, sLensFlareLensData.fLightAngle);
#endif
}