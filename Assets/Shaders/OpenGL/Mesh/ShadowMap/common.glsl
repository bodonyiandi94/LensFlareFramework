
#include <Shaders/OpenGL/Common/Optional/material.glsl>
#include <Shaders/OpenGL/Shading/Shadows/shadow_maps.glsl>

// Model matrix
layout (location = 16) uniform mat4 mModel;

// Common, light-specific attributes
layout (location = 17) uniform uint uiShadowMapAlgorithm;
layout (location = 18) uniform uint uiShadowMapPrecision;
layout (location = 19) uniform vec2 vExponentialConstants;

// Transformation properties
layout (location = 20) uniform mat4 mLight;
layout (location = 21) uniform float fIsPerspective;
layout (location = 22) uniform float fNear;
layout (location = 23) uniform float fFar;

////////////////////////////////////////////////////////////////////////////////
// Helper function for obtaining the fragment depth for a shadow map
float extractShadowMapDepth(const vec4 fragCoord)
{
    const float depth = fIsPerspective == 0.0 ? fragCoord.z : linearDepth(fragCoord.z, fNear, fFar);
    return transformShadowMapDepth(uiShadowMapAlgorithm, depth);
}