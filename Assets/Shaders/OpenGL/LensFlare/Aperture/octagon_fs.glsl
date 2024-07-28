#version 440

// Includes
#include <Shaders/OpenGL/Common/common.glsl>
#include <Shaders/OpenGL/LensFlare/Aperture/common.glsl>

// Input attribs
in vec2 vPos;

// Render targets
layout (location = 0) out vec4 colorBuffer;

void main()
{
    colorBuffer = vec4(vec3(hardMask(octagonDist(vPos))), 1.0);
}