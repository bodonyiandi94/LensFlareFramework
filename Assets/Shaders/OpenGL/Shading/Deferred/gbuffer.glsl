#include <Shaders/OpenGL/Shading/surface_descriptors.glsl>

////////////////////////////////////////////////////////////////////////////////
// No MSAA
#if !defined(MSAA) || MSAA == 0
    #define SAMPLER_TYPE sampler2DArray
    #define sampleGbuffer(smpler, uv, layerId, sampleId) textureDR(smpler, uv, layerId)

////////////////////////////////////////////////////////////////////////////////
// MSAA
#elif MSAA == 1
    #define SAMPLER_TYPE sampler2DMSArray
    #define sampleGbuffer(smpler, uv, layerId, sampleId) texelFetch(smpler, ivec3(uv * sRenderData.vResolution, layerId), sampleId)
#endif

////////////////////////////////////////////////////////////////////////////////
// Various texture maps
layout (binding = TEXTURE_ALBEDO_MAP) uniform SAMPLER_TYPE sColorMap;
layout (binding = TEXTURE_NORMAL_MAP) uniform SAMPLER_TYPE sNormalMap;
layout (binding = TEXTURE_SPECULAR_MAP) uniform SAMPLER_TYPE sSpecularMap;
layout (binding = TEXTURE_DEPTH) uniform SAMPLER_TYPE sDepthMap;

////////////////////////////////////////////////////////////////////////////////
#define DECLARE_COMPONENT(NAME, TYPE, TEXTURE, CHANNELS, FN) \
    TYPE CONCAT(gbuffer, NAME)(const vec2 uv, const int layerId, const int sampleId) \
    { return FN(sampleGbuffer(TEXTURE, uv, layerId, sampleId).CHANNELS); } \
    TYPE CONCAT(gbuffer, NAME)(const vec2 uv, const int sampleId) \
    { return FN(sampleGbuffer(TEXTURE, uv, 0, sampleId).CHANNELS); }

////////////////////////////////////////////////////////////////////////////////
DECLARE_COMPONENT(Depth, float, sDepthMap, r, )
DECLARE_COMPONENT(LinearDepth, float, sDepthMap, r, linearDepth)
DECLARE_COMPONENT(Albedo, vec3, sColorMap, rgb, )
DECLARE_COMPONENT(Revealage, float, sColorMap, a, )
DECLARE_COMPONENT(Normal, vec3, sNormalMap, rgb, normalize)
DECLARE_COMPONENT(Specular, float, sNormalMap, a, )
DECLARE_COMPONENT(Metallic, float, sSpecularMap, r, )
DECLARE_COMPONENT(Roughness, float, sSpecularMap, g, )
DECLARE_COMPONENT(Velocity, vec2, sSpecularMap, ba, )

////////////////////////////////////////////////////////////////////////////////
#undef DECLARE_COMPONENT
#undef SAMPLER_TYPE
#undef sampleGbuffer

////////////////////////////////////////////////////////////////////////////////
// Extract the surface information from the GBuffer
SurfaceInfo sampleGbufferSurface(const vec2 uv, const int layerId, const int sampleId)
{
    SurfaceInfo surface;
    surface.position = reconstructPositionWS(uv, gbufferDepth(uv, layerId, sampleId));
    surface.normal = gbufferNormal(uv, layerId, sampleId);
    return surface;
}

////////////////////////////////////////////////////////////////////////////////
// Extract the material information from the GBuffer
MaterialInfo sampleGbufferMaterial(const vec2 uv, const int layerId, const int sampleId)
{
    MaterialInfo material;
    material.albedo = gbufferAlbedo(uv, layerId, sampleId);
    material.roughness = gbufferRoughness(uv, layerId, sampleId);
    material.metallic = gbufferMetallic(uv, layerId, sampleId);
    material.specular = gbufferSpecular(uv, layerId, sampleId);
    return material;
}