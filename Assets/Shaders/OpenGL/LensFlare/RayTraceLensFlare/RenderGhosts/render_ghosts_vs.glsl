#version 440

// Includes
#include <Shaders/OpenGL/Common/common.glsl>
#include <Shaders/OpenGL/LensFlare/RayTraceLensFlare/RenderGhosts/common.glsl>

// Outputs
out vec2 vPupilPos;
out vec2 vAperturePos;
out vec2 vSensorPos;
out float fRadius;
out float fIntensity;
out float fApertureDist;
out float fClipFactor;

void main()
{
    // Get the corresponding ray grid entry
    const GridEntry gridEntry = computeGridEntry(gl_VertexID);
    
    // Write out the output values
    vPupilPos = gridEntry.vPupilPos / (sLensFlareLensData.fOuterPupilHeight);
    vAperturePos = gridEntry.vAperturePos;
    vSensorPos = gridEntry.vSensorPos / (sLensFlareLensData.vFilmSize * 0.5);
    fApertureDist = gridEntry.fApertureDist;
    fRadius = gridEntry.fRelativeRadius;
    fIntensity = gridEntry.fIntensity;
    fClipFactor = gridEntry.fClipFactor;
    if (sLensFlareData.iProjectionMethod == ProjectionMethod_PupilGrid)
        gl_Position.xy = vPupilPos * vec2(1.0 / sCameraData.fAspect, 1.0) * sLensFlareData.fFilmStretch;
    else if (sLensFlareData.iProjectionMethod == ProjectionMethod_SensorGrid)
        gl_Position.xy = vSensorPos * sLensFlareData.fFilmStretch;
    gl_Position.zw = vec2(0, 1);
}