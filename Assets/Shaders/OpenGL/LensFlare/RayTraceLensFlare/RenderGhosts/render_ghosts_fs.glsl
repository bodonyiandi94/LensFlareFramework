#version 440

// Includes
#include <Shaders/OpenGL/Common/common.glsl>
#include <Shaders/OpenGL/LensFlare/RayTraceLensFlare/RenderGhosts/common.glsl>

// Inputs
in vec2 vPupilPosGS;
in vec2 vAperturePosGS;
in vec2 vSensorPosGS;
in float fRadiusGS;
in float fIntensityGS;
in float fApertureDistGS;
in float fClipFactorGS;

// Framebuffer
out vec4 colorBuffer;

vec3 visualizeNormalizedFloat(const float value)
{
    return (value <= 1.0) ? vec3(0, value, 0) : vec3(value, 0, 0);
}

void main()
{
    // Calculate the scaled intensity
    const float intensity = saturate(fIntensityGS);

    // Determine whether the ray is valid or not
    //const bool isRayValid = fRadiusGS <= sLensFlareData.fRadiusClip && intensity >= MIN_INTENSITY;
    //const bool isRayValid = fRadiusGS <= sLensFlareData.fRadiusClip && fApertureDistGS <= sLensFlareData.fIrisClip;
    const bool isRayValid = fClipFactorGS <= 1.0 && intensity >= MIN_INTENSITY;

    // Determine if we are clipping rays or not
    //const bool clipRays = sLensFlareData.iClipPixels == 1 && sLensFlareData.iShadingMethod == ShadingMethod_Shaded;
    const bool clipRays = sLensFlareData.iClipPixels == 1;

    // Apply clipping based on the relative radius
    if (clipRays && !isRayValid)
        discard;
    
    // Write out the final color
    const vec3 color = lambda2RGB(sLensFlareLensData.vWavelengths[sGhostParameters.iChannelID].x, 1.0); 
    colorBuffer.rgb = color * intensity * sLensFlareLensData.vColor;
    colorBuffer.a = 1.0;

    if (sLensFlareData.iShadingMethod == ShadingMethod_Shaded)
    {
        colorBuffer.rgb = color * intensity * sLensFlareLensData.vColor;
    }
    else if (sLensFlareData.iShadingMethod == ShadingMethod_Uncolored)
    {
        colorBuffer.rgb = vec3(intensity);
    }
    else if (sLensFlareData.iShadingMethod == ShadingMethod_Unshaded)
    {
        colorBuffer.rgb = color * sLensFlareLensData.vColor;
    }
    else if (sLensFlareData.iShadingMethod == ShadingMethod_PupilCoords)
    {
        colorBuffer.rgb = vec3(vPupilPosGS, 0);
    }
    else if (sLensFlareData.iShadingMethod == ShadingMethod_SensorCoords)
    {
        colorBuffer.rgb = vec3(vSensorPosGS / (sLensFlareLensData.vFilmSize * 0.5), 0);
    }
    else if (sLensFlareData.iShadingMethod == ShadingMethod_UVCoords)
    {
        colorBuffer.rgb = vec3(vAperturePosGS / sLensFlareLensData.fApertureHeight, 0);
    }
    else if (sLensFlareData.iShadingMethod == ShadingMethod_RelativeRadius)
    {
        colorBuffer.rgb = visualizeNormalizedFloat(fRadiusGS / sLensFlareLensData.fRadiusClip);
    }
    else if (sLensFlareData.iShadingMethod == ShadingMethod_ApertureDist)
    {
        colorBuffer.rgb = visualizeNormalizedFloat(fApertureDistGS / sLensFlareLensData.fIrisClip);
    }
    else if (sLensFlareData.iShadingMethod == ShadingMethod_ClipFactor)
    {
        colorBuffer.rgb = visualizeNormalizedFloat(fClipFactorGS);
    }
    else if (sLensFlareData.iShadingMethod == ShadingMethod_Validity)
    {
        colorBuffer.rgb = vec3(le(fRadiusGS, sLensFlareLensData.fRadiusClip), le(fApertureDistGS, sLensFlareLensData.fIrisClip), le(fClipFactorGS, 1));
    }

    // Handle the overlays
    if (sLensFlareData.iOverlayMethod == OverlayMethod_TractabilityOverlay)
    {
        if (fIntensityGS <= 0.0) 
            colorBuffer.b = 1;
    }
    else if (sLensFlareData.iOverlayMethod == OverlayMethod_ValidityOverlay)
    {
        if (!isRayValid) 
            colorBuffer.b = 1;
    }
    else if (sLensFlareData.iOverlayMethod == OverlayMethod_SensorBoundsOverlay)
    {
        if (any(greaterThan(vSensorPosGS, vec2(1.0))) || any(lessThan(vSensorPosGS, vec2(-1.0))))
            colorBuffer.b = 1;
    }
}