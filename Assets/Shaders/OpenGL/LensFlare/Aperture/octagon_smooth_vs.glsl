#version 440

// Includes
#include <Shaders/OpenGL/Common/common.glsl>
#include <Shaders/OpenGL/LensFlare/Aperture/common.glsl>

// Input attribs
layout (location = VERTEX_ATTRIB_POSITION) in vec3 vPosition;

// Output attribs
out vec2 vPos;

// Entry point
void main()
{
    vPos = vPosition.xy;
    
    gl_Position = vec4(vPosition, 1);
}