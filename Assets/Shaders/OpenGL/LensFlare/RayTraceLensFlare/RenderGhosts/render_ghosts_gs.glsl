#version 440

// Includes
#include <Shaders/OpenGL/Common/common.glsl>
#include <Shaders/OpenGL/LensFlare/RayTraceLensFlare/RenderGhosts/common.glsl>

layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

// Inputs
in vec2 vPupilPos[];
in vec2 vAperturePos[];
in vec2 vSensorPos[];
in float fRadius[];
in float fIntensity[];
in float fApertureDist[];
in float fClipFactor[];

// Outputs
out vec2 vPupilPosGS;
out vec2 vAperturePosGS;
out vec2 vSensorPosGS;
out float fRadiusGS;
out float fIntensityGS;
out float fApertureDistGS;
out float fClipFactorGS;

////////////////////////////////////////////////////////////////////////////////
bool isPrimitiveValid()
{
    return sLensFlareData.iClipRays == 0 || 
    (
        isRayValid(fIntensity[0], fClipFactor[0]) ||
        isRayValid(fIntensity[1], fClipFactor[1]) ||
        isRayValid(fIntensity[2], fClipFactor[2])
    );
}

void main()
{
    if (!isPrimitiveValid()) return;

    for (int vertexId = 0; vertexId < 3; ++vertexId)
    {
        gl_Position = gl_in[vertexId].gl_Position; 
        vPupilPosGS = vPupilPos[vertexId];
        vAperturePosGS = vAperturePos[vertexId];
        vSensorPosGS = vSensorPos[vertexId];
        fRadiusGS = fRadius[vertexId];
        fIntensityGS = fIntensity[vertexId];
        fApertureDistGS = fApertureDist[vertexId];
        fClipFactorGS = fClipFactor[vertexId];
        EmitVertex();
    }    
    EndPrimitive();
}