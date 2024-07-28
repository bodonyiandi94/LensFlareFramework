#include <Shaders/OpenGL/Common/Rendering/camera.glsl>

// convert from [0, 1] to [-1, 1]
vec2 sreenToNdc(const vec2 v)
{
	return v * 2.0 - 1.0;
}

// convert from [0, 1] to [-1, 1]
vec3 sreenToNdc(const vec3 v)
{
	return v * 2.0 - 1.0;
}

// convert from [-1, 1] to [0, 1]
vec2 ndcToScreen(const vec2 v)
{
	return v * 0.5 + 0.5;
}

// convert from [-1, 1] to [0, 1]
vec3 ndcToScreen(const vec3 v)
{
	return v * 0.5 + 0.5;
}

// Convert back from clip-space using the parameter inverse matrix
vec3 reconstructPosition(const vec2 v, const float d, const mat4 im)
{
    const vec4 p = im * vec4(sreenToNdc(vec3(v, d)), 1.0);
	return p.xyz / p.w;
}

// Convert from clip-space to world space
vec3 reconstructPositionWS(const vec2 v, const float d)
{
    return reconstructPosition(v, d, sCameraData.mInverseViewProjection);
}

// Convert from clip-space to camera space
vec3 reconstructPositionCS(const vec2 v, const float d)
{
    return reconstructPosition(v, d, sCameraData.mInverseProjection);
}