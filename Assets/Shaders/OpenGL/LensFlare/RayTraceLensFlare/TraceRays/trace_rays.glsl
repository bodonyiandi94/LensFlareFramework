#include <Shaders/OpenGL/LensFlare/RayTraceLensFlare/TraceRays/Methods/common.glsl>

// Groupshared memory handling
#if USE_GROUPSHARED_MEMORY_POLYNOMIAL == 1
    #ifdef SHADER_TYPE_COMPUTE_SHADER
        #define TRACE_RAYS_POLY_USE_SHARED_MEMORY
    #endif
#endif

#ifdef SHADER_TYPE_COMPUTE_SHADER
    #define TRACE_RAYS_NN_USE_SHARED_MEMORY
#endif

////////////////////////////////////////////////////////////////////////////////
#include <Shaders/OpenGL/LensFlare/RayTraceLensFlare/TraceRays/Methods/analytical.glsl>
Ray traceRayEntrance(const Ray ray)
{
    return traceGhostRayAnalytical(
        sLensFlareLensData.fRadiusClip,
        sLensFlareLensData.fIrisClip,
        sGhostParameters.iGhostID, 
        sGhostParameters.iChannelID,
        1, 
        1, 
        sGhostParameters.vGhostIndices, 
        sLensFlareLensData.vWavelengths[sGhostParameters.iChannelID].x, 
        ray);
}

////////////////////////////////////////////////////////////////////////////////
#if RAY_TRACE_METHOD == RaytraceMethod_PolynomialFullFit || RAY_TRACE_METHOD == RaytraceMethod_PolynomialPartialFit
#include <Shaders/OpenGL/LensFlare/RayTraceLensFlare/TraceRays/Methods/polynomial_common.glsl>
#endif

////////////////////////////////////////////////////////////////////////////////
// Traces a ray from the entrance plane up until the sensor.
#if RAY_TRACE_METHOD == RaytraceMethod_Analytical
    Ray traceRay(const Ray ray)
    {
        return traceGhostRayAnalytical(
            sLensFlareLensData.fRadiusClip,
            sLensFlareLensData.fIrisClip,
            sGhostParameters.iGhostID, 
            sGhostParameters.iChannelID,
            1, 
            sLensFlareLensData.iNumLenses - 1, 
            sGhostParameters.vGhostIndices, 
            sLensFlareLensData.vWavelengths[sGhostParameters.iChannelID].x, 
            ray);
    }
    void initGhostRayGroupSharedMemory(const int workerIdx, const int workersPerGroup)
    {}
#elif RAY_TRACE_METHOD == RaytraceMethod_PolynomialFullFit
    #include <Shaders/OpenGL/LensFlare/RayTraceLensFlare/TraceRays/Methods/polynomial_full_fit.glsl>
    Ray traceRay(const Ray ray)
    {
        return traceGhostRayPolynomialFullFit( 
            sLensFlareLensData.fRadiusClip,
            sLensFlareLensData.fIrisClip,
            sGhostParameters.iGhostID, 
            sGhostParameters.iChannelID,
            sLensFlareLensData.vWavelengths[sGhostParameters.iChannelID].x, 
            sLensFlareLensData.fLightRotation,
            sLensFlareLensData.fLightAngle,
            ray);
    }
    void initGhostRayGroupSharedMemory(const int workerIdx, const int workersPerGroup)
    {
        initGhostRayGroupSharedMemory(sGhostParameters.iGhostID, sGhostParameters.iChannelID, workerIdx, workersPerGroup);
    }
#elif RAY_TRACE_METHOD == RaytraceMethod_PolynomialPartialFit
    #include <Shaders/OpenGL/LensFlare/RayTraceLensFlare/TraceRays/Methods/polynomial_partial_fit.glsl>
    Ray traceRay(const Ray ray)
    {
        return traceGhostRayPolynomialPartialFit( 
            sLensFlareLensData.fRadiusClip,
            sLensFlareLensData.fIrisClip,
            sGhostParameters.iGhostID, 
            sGhostParameters.iChannelID,
            sLensFlareLensData.vWavelengths[sGhostParameters.iChannelID].x, 
            sLensFlareLensData.fLightRotation,
            sLensFlareLensData.fLightAngle,
            ray);
    }
    void initGhostRayGroupSharedMemory(const int workerIdx, const int workersPerGroup)
    {
        initGhostRayGroupSharedMemory(sGhostParameters.iGhostID, sGhostParameters.iChannelID, workerIdx, workersPerGroup);
    }
#endif


void initGhostRayGroupSharedMemory(const uint workerIdx, const uint workersPerGroup)
{
    initGhostRayGroupSharedMemory(int(workerIdx), int(workersPerGroup));
}