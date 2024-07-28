#version 440

// Includes
#include <Shaders/OpenGL/Common/common.glsl>

// Various texture maps
layout (binding = TEXTURE_ALBEDO_MAP) uniform sampler2DArray sScene;

// Input attribus
in GeometryData
{
    vec2 vUv;
} g_out;

// Input textures
layout (binding = TEXTURE_DEPTH) uniform sampler2DMSArray sDepthMap;
layout (binding = TEXTURE_ALBEDO_MAP) uniform sampler2DMSArray sColorMap;
layout (binding = TEXTURE_NORMAL_MAP) uniform sampler2DMSArray sNormalMap;
layout (binding = TEXTURE_SPECULAR_MAP) uniform sampler2DMSArray sSpecularMap;

// Render targets
layout (location = 0) out float depthBuffer;
layout (location = 1) out vec4 colorBuffer;
layout (location = 2) out vec4 normalBuffer;
layout (location = 3) out vec4 specularBuffer;

void main()
{
    // Accumulate the gbuffer contents
    float depth = 0;
    vec4 albedo = vec4(0);
    vec4 normal = vec4(0);
    vec4 specular = vec4(0);

    for (int sampleId = 0; sampleId < max(1, sRenderData.iMSAA); ++sampleId)
    {
        const ivec3 sampleCoords = ivec3(gl_FragCoord.xy, gl_Layer);
        depth += texelFetch(sDepthMap, sampleCoords, sampleId).r;
        albedo += texelFetch(sColorMap, sampleCoords, sampleId);
        normal += texelFetch(sNormalMap, sampleCoords, sampleId);
        specular += texelFetch(sSpecularMap, sampleCoords, sampleId);
    }
    
    // Write out the results
    const float normFactor = 1.0 / float(max(1, sRenderData.iMSAA));
    depthBuffer = depth * normFactor;
    colorBuffer = albedo * normFactor;
    normalBuffer = normal * normFactor;
    specularBuffer = specular * normFactor;
}