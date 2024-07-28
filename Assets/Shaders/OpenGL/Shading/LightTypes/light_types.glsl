
////////////////////////////////////////////////////////////////////////////////
// Define the various light types available
#define DIRECTIONAL 0
#define POINT 1
#define SPOT 2

////////////////////////////////////////////////////////////////////////////////
// Include the common headers
#include <Shaders/OpenGL/Shading/LightTypes/common.glsl>

////////////////////////////////////////////////////////////////////////////////
// Include the appropriate light header
#if LIGHT_TYPE == DIRECTIONAL
    #include <Shaders/OpenGL/Shading/LightTypes/directional_light.glsl>
    #define computeLight computeDirectionalLight
#elif LIGHT_TYPE == POINT
    #include <Shaders/OpenGL/Shading/LightTypes/point_light.glsl>
    #define computeLight computePointLight
#elif LIGHT_TYPE == SPOT
    #include <Shaders/OpenGL/Shading/LightTypes/spot_light.glsl>
    #define computeLight computeSpotLight
#endif

////////////////////////////////////////////////////////////////////////////////
vec4 computeLight(const SurfaceInfo surface, const MaterialInfo material)
{
    return computeLight(surface, material, 0.0, 1.0, 1.0, 1.0);
}