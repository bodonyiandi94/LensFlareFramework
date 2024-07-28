#version 440

// Includes
#include <Shaders/OpenGL/Common/common.glsl>
#include <Shaders/OpenGL/Skybox/common.glsl>

// Input attribs
layout (location = VERTEX_ATTRIB_POSITION) in vec2 vPosition;

// Output attribs
out VertexData
{
    vec3 vPosition;
	vec3 vUv;
} v_out;

// Entry point
void main()
{
    // Calculate the cubemap texture coordinates
    const vec4 wsPosition = clipToWorldSpace(vec3(vPosition, 0), sCameraData.mInverseProjection, mat4(mat3(sCameraData.mInverseView)));
    v_out.vUv = vec3(wsPosition.x, -wsPosition.y, -wsPosition.z);
    v_out.vPosition = clipToWorldSpace(vec3(vPosition, 1)).xyz;

    // Write out the positions
    gl_Position = vec4(vPosition.xy, 1, 1);
}