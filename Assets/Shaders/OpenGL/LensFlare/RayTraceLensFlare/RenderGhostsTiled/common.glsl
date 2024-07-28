#include <Shaders/OpenGL/LensFlare/RayTraceLensFlare/common.glsl>

////////////////////////////////////////////////////////////////////////////////
// Uniforms
////////////////////////////////////////////////////////////////////////////////

// Len's flare uniform buffer
layout (std140, binding = UNIFORM_BUFFER_GENERIC_4) uniform RayTraceLensFlareUniformDataTiledCommon
{
    ivec2 vNumCoarseTiles;
    int iCoarseTileSize;
    int iMaxCoarseTileEntries;

    ivec2 vNumTiles;
    int iTileSize;
    int iMaxTileEntries;

    int iNumQuadMergeSteps;
    float fQuadMergeEdgeThreshold;

    int iSampleBaseScene;
} sLensFlareDataTiled;

////////////////////////////////////////////////////////////////////////////////
// Dispatch indirect buffer
////////////////////////////////////////////////////////////////////////////////

layout (std140, binding = UNIFORM_BUFFER_GENERIC_7) buffer DispatchIndirectBuffer
{
	uvec3 vNumGroups;
	uint _padding;
} sDispatchIndirectBuffer;

////////////////////////////////////////////////////////////////////////////////
// Global properties buffer
////////////////////////////////////////////////////////////////////////////////

layout (std140, binding = UNIFORM_BUFFER_GENERIC_8) buffer GlobalPropertiesBuffer
{
    uint uiNumPrimitives;
} sGlobalPropertiesBuffer;

////////////////////////////////////////////////////////////////////////////////
// Tile rays
////////////////////////////////////////////////////////////////////////////////

// Describes the output attributes of a single ray
struct GhostSingleTracedRay
{
    vec2 vSensorPos;
    float fLambda;
    float fIntensity;
    float fClipFactor;
};

// Describes the output attributes of a single primitive
struct GhostSinglePrimitive
{
    vec2 vSensorPos[4];
    vec2 vSensorValues[4];
    vec3 vColor;
};

// Ghost geometry output buffer
TYPED_ARRAY_BUFFER(std140, UNIFORM_BUFFER_GENERIC_9, sGhostTracedPrimitivesBuffer_, GhostSinglePrimitive);
#define sGhostTracedPrimitivesBuffer sGhostTracedPrimitivesBuffer_.sData

// Describes the attributes of a single primitive
struct GhostSinglePrimitiveProperty
{    
    vec4 vAABB;
};

// Ghost property buffer
TYPED_ARRAY_BUFFER(std140, UNIFORM_BUFFER_GENERIC_10, sGhostTracedPrimitivePropertiesBuffer_, GhostSinglePrimitiveProperty);
#define sGhostTracedPrimitivePropertiesBuffer sGhostTracedPrimitivePropertiesBuffer_.sData

////////////////////////////////////////////////////////////////////////////////
// Tile entries
////////////////////////////////////////////////////////////////////////////////

struct GhostSingleTileEntry
{    
    uint uiPrimitiveId;
};

// Coarse
TYPED_ARRAY_BUFFER(std430, UNIFORM_BUFFER_GENERIC_11, sGhostCoarseTilesBuffer_, GhostSingleTileEntry);
#define sGhostCoarseTilesBuffer sGhostCoarseTilesBuffer_.sData

// Dense
TYPED_ARRAY_BUFFER(std430, UNIFORM_BUFFER_GENERIC_13, sGhostTilesBuffer_, GhostSingleTileEntry);
#define sGhostTilesBuffer sGhostTilesBuffer_.sData

////////////////////////////////////////////////////////////////////////////////
// Tile properties
////////////////////////////////////////////////////////////////////////////////

struct GhostSingleTileProperties
{    
    uint uiNumPrimitives;
};

// Coarse
TYPED_ARRAY_BUFFER(std140, UNIFORM_BUFFER_GENERIC_12, sGhostCoarseTilesPropertiesBuffer_, GhostSingleTileProperties);
#define sGhostCoarseTilesPropertiesBuffer sGhostCoarseTilesPropertiesBuffer_.sData

// Dense
TYPED_ARRAY_BUFFER(std140, UNIFORM_BUFFER_GENERIC_14, sGhostTilesPropertiesBuffer_, GhostSingleTileProperties);
#define sGhostTilesPropertiesBuffer sGhostTilesPropertiesBuffer_.sData

////////////////////////////////////////////////////////////////////////////////
// Various texture maps
layout (binding = TEXTURE_ALBEDO_MAP) uniform sampler2DArray sSceneColor;

////////////////////////////////////////////////////////////////////////////////
//  Output image buffer
layout (binding = 0, rgba16f) uniform restrict writeonly image2D sResult;

////////////////////////////////////////////////////////////////////////////////
// Common array indexing function IDs
////////////////////////////////////////////////////////////////////////////////

// Converts the parameter pixel coordinates to NDC
vec2 pixelPosToNDC(const ivec2 pixelCords)
{
    return sreenToNdc(vec2(pixelCords) / vec2(sLensFlareData.vRenderResolution - 1));
}

// Array index for the per-tile buffer
uint tileArrayIndex(const ivec2 tileId)
{
	return uint(tileId.y * sLensFlareDataTiled.vNumTiles.x + tileId.x) * uint(sLensFlareDataTiled.iMaxTileEntries);
}

// Array index for the per-tile buffer
uint tileParamArrayIndex(const ivec2 tileId)
{
	return uint(tileId.y * sLensFlareDataTiled.vNumTiles.x + tileId.x);
}

// Array index for the coarse per-tile buffer
uint coarseTileArrayIndex(const ivec2 tileId)
{
	return uint(tileId.y * sLensFlareDataTiled.vNumCoarseTiles.x + tileId.x) * uint(sLensFlareDataTiled.iMaxCoarseTileEntries);
}

// Array index for the coarse per-tile buffer
uint coarseTileParamArrayIndex(const ivec2 tileId)
{
	return uint(tileId.y * sLensFlareDataTiled.vNumCoarseTiles.x + tileId.x);
}

////////////////////////////////////////////////////////////////////////////////
// Barycentric-related functions
////////////////////////////////////////////////////////////////////////////////

// Compute the barycentric coordinates of a 2D points inside a triangle
vec3 getBarycentricCoordinatesNaive(const vec2 p, const vec2 a, const vec2 b, const vec2 c)
{
    const vec2 v0 = b - a, v1 = c - a, v2 = p - a;
    const float d00 = dot(v0, v0);
    const float d01 = dot(v0, v1);
    const float d11 = dot(v1, v1);
    const float d20 = dot(v2, v0);
    const float d21 = dot(v2, v1);
    const float denom = d00 * d11 - d01 * d01;

    if (denom == 0 || isinf(denom) || isnan(denom))
        return vec3(-100);

    vec3 result;
    
    result.y = (d11 * d20 - d01 * d21) / denom;
    result.z = (d00 * d21 - d01 * d20) / denom;
    result.x = 1.0 - result.y - result.z;

    return result;
}

// Cross product in 4D
vec4 cross_4D(const vec4 x1, const vec4 x2, const vec4 x3)
{
    return vec4(
         dot(x1.yzw, cross(x2.yzw, x3.yzw)),
        -dot(x1.xzw, cross(x2.xzw, x3.xzw)),
         dot(x1.xyw, cross(x2.xyw, x3.xyw)),
        -dot(x1.xyz, cross(x2.xyz, x3.xyz))
    );
}

vec4 getBarycentricCoordinatesHomogenous(const vec2 p, const vec2 a, const vec2 b, const vec2 c)
{
    const vec4 v0 = vec4(a.x, b.x, c.x, p.x);
    const vec4 v1 = vec4(a.y, b.y, c.y, p.y);
    const vec4 v2 = vec4(1.0);
    return cross_4D(v0, v1, v2);
}

vec3 getBarycentricCoordinates(const vec4 t)
{
    return -t.xyz / t.w;
}

vec3 getBarycentricCoordinates(const vec2 p, const vec2 a, const vec2 b, const vec2 c)
{
    //return getBarycentricCoordinatesNaive(p, a, b, c);
    return getBarycentricCoordinates(getBarycentricCoordinatesHomogenous(p, a, b, c));
}

float triArea(const vec2 v, const vec2 w)
{
    //0.5 * cross(v, w).z
    return 0.5 * (v.x * w.y - v.y * w.x);
}

vec4 getBarycentricCoordinatesNaive(const vec2 p, const vec2 a, const vec2 b, const vec2 c, const vec2 d)
{
    const vec2 s0 = a - p;
    const vec2 s1 = b - p;
    const vec2 s2 = c - p;
    const vec2 s3 = d - p;

    const float A0 = triArea(s0, s1);
    const float A1 = triArea(s1, s2);
    const float A2 = triArea(s2, s3);
    const float A3 = triArea(s3, s0);

    const float D0 = dot(s0, s1);
    const float D1 = dot(s1, s2);
    const float D2 = dot(s2, s3);
    const float D3 = dot(s3, s0);

    const float r0 = length(s0);
    const float r1 = length(s1);
    const float r2 = length(s2);
    const float r3 = length(s3);

    const float t0 = -(r0 * r1 - D0) / A0;
    const float t1 = -(r1 * r2 - D1) / A1;
    const float t2 = -(r2 * r3 - D2) / A2;
    const float t3 = -(r3 * r0 - D3) / A3;

    const float mu0 = (t3 + t0) / r0;
    const float mu1 = (t0 + t1) / r1;
    const float mu2 = (t1 + t2) / r2;
    const float mu3 = (t2 + t3) / r3;

    const float sum = (mu0 + mu1 + mu2 + mu3);
    if (sum == 0 || isinf(sum) || isnan(sum))
        return vec4(-100);

    return vec4(mu0, mu1, mu2, mu3) / sum;
}

// Compute the barycentric coordinates of a 2D points inside a quad
vec4 getBarycentricCoordinatesGeneralized(const vec2 p, const vec2 a, const vec2 b, const vec2 c, const vec2 d)
{
    const mat4x2 s = mat4x2(a, b, c, d) - mat4x2(p, p, p, p);
    const vec4 A = vec4(triArea(s[0], s[1]), triArea(s[1], s[2]), triArea(s[2], s[3]), triArea(s[3], s[0]));
    const vec4 D = vec4(dot(s[0], s[1]), dot(s[1], s[2]), dot(s[2], s[3]), dot(s[3], s[0]));
    const vec4 r = vec4(length(s[0]), length(s[1]), length(s[2]), length(s[3]));
    const vec4 t = vec4(-(r[0] * r[1] - D[0]), -(r[1] * r[2] - D[1]), -(r[2] * r[3] - D[2]), -(r[3] * r[0] - D[3])) / A;
    const vec4 mu = vec4(t[3] + t[0], t[0] + t[1], t[1] + t[2], t[2] + t[3]) / r;
    const float sum = dot(mu, vec4(1.0));
    return mu / sum;
}

// Compute the barycentric coordinates of a 2D points inside a quad
vec4 getBarycentricCoordinatesWachspress(const vec2 p, const vec2 a, const vec2 b, const vec2 c, const vec2 d)
{
    const mat4x2 sp = mat4x2(a, b, c, d) - mat4x2(d, a, b, c);
    const mat4x2 s = mat4x2(a, b, c, d) - mat4x2(p, p, p, p);
    const vec4 Ap = vec4(triArea(sp[0], sp[1]), triArea(sp[1], sp[2]), triArea(sp[2], sp[3]), triArea(sp[3], sp[0]));
    const vec4 A = vec4(triArea(s[0], s[1]), triArea(s[1], s[2]), triArea(s[2], s[3]), triArea(s[3], s[0]));
    const vec4 mu = vec4(A.y * A.z, A.z * A.w, A.w * A.x, A.x * A.y);
    const vec4 w = Ap * mu;
    const float sum = dot(w, vec4(1.0));
    return w / sum;
}

// Compute the barycentric coordinates of a 2D points inside a quad
vec4 getBarycentricCoordinatesLoopDeRose(const vec2 p, const vec2 a, const vec2 b, const vec2 c, const vec2 d)
{
    const mat4x2 s = mat4x2(a, b, c, d) - mat4x2(p, p, p, p);
    const vec4 A = vec4(triArea(s[0], s[1]), triArea(s[1], s[2]), triArea(s[2], s[3]), triArea(s[3], s[0]));
    const vec4 w = vec4(A.y * A.z, A.z * A.w, A.w * A.x, A.x * A.y);
    const float sum = dot(w, vec4(1.0));
    return w / sum;
}

// Compute the barycentric coordinates of a 2D points inside a quad
vec4 getBarycentricCoordinates(const vec2 p, const vec2 a, const vec2 b, const vec2 c, const vec2 d)
{
    //return getBarycentricCoordinatesLoopDeRose(p, a, b, c, d);
    return getBarycentricCoordinatesWachspress(p, a, b, c, d);
    //return getBarycentricCoordinatesGeneralized(p, a, b, c, d);
    //return getBarycentricCoordinatesNaive(p, a, b, c, d);
}

// Determines if the point corresponding to the parameter barycentric coordinates is inside the triangle
bool isBarycentricInside(const vec3 barycentric)
{
    return (all(greaterThanEqual(barycentric, vec3(0.0))) && all(lessThanEqual(barycentric, vec3(1.0))));
}

// Determines if the point corresponding to the parameter barycentric coordinates is on the edge of the triangle
bool isBarycentricOnEdge(const vec3 barycentric)
{
    return (all(greaterThanEqual(barycentric, vec3(0.0))) && all(lessThanEqual(barycentric, vec3(1.0)))) && any(lessThanEqual(barycentric, vec3(2e-2)));
}

// Determines if the point corresponding to the parameter barycentric coordinates is outside the triangle
bool isBarycentricOutside(const vec3 barycentric)
{
    return (any(lessThan(barycentric, vec3(0.0))) || any(greaterThan(barycentric, vec3(1.0))));
    //return (any(lessThan(barycentric, vec3(-1e-5))) || any(greaterThan(barycentric, vec3(1.0 + 1e-5))));
}

// Determines if the point corresponding to the parameter barycentric coordinates is inside the triangle
bool isBarycentricInside(const vec4 barycentric)
{
    return (all(greaterThanEqual(barycentric, vec4(0.0))) && all(lessThanEqual(barycentric, vec4(1.0))));
}

// Determines if the point corresponding to the parameter barycentric coordinates is on the edge of the triangle
bool isBarycentricOnEdge(const vec4 barycentric)
{
    return (all(greaterThanEqual(barycentric, vec4(-5e-3))) && all(lessThanEqual(barycentric, vec4(1.0 + 5e-3)))) && 
        (any(lessThanEqual(barycentric, vec4(5e-3))) || any(lessThanEqual(barycentric, vec4(-5e-3))));
    //return (all(greaterThanEqual(barycentric, vec4(0.0))) && all(lessThanEqual(barycentric, vec4(1.0)))) && any(lessThanEqual(barycentric, vec4(5e-2)));
}

// Determines if the point corresponding to the parameter barycentric coordinates is outside the triangle
bool isBarycentricOutside(const vec4 barycentric)
{
    return (any(lessThan(barycentric, vec4(0.0))) || any(greaterThan(barycentric, vec4(1.0))));
    //return (any(lessThan(barycentric, vec4(-1e-5))) || any(greaterThan(barycentric, vec4(1.0 + 1e-5))));
}

// define barycentric interpolation functions
#define LERP_BARYCENTRIC(GENTYPE, BASE) \
    GENTYPE lerpBarycentric(const vec3 barycentricCoords, const GENTYPE a, const GENTYPE b, const GENTYPE c) \
    { return GENTYPE(barycentricCoords.x) * a + GENTYPE(barycentricCoords.y) * b + GENTYPE(barycentricCoords.z) * c; } \
    GENTYPE lerpBarycentric(const vec4 barycentricCoords, const GENTYPE a, const GENTYPE b, const GENTYPE c, const GENTYPE d) \
    { return GENTYPE(barycentricCoords.x) * a + GENTYPE(barycentricCoords.y) * b + GENTYPE(barycentricCoords.z) * c + GENTYPE(barycentricCoords.w) * d; } \

DEF_GENFTYPE(LERP_BARYCENTRIC)
DEF_GENDTYPE(LERP_BARYCENTRIC)

#undef LERP_BARYCENTRIC

////////////////////////////////////////////////////////////////////////////////
// Tile intersection
////////////////////////////////////////////////////////////////////////////////

// Computes the aabb for the given quad
vec4 primitiveAABB(const GhostSinglePrimitive p)
{
    vec4 result;
    result.xy = min(min(p.vSensorPos[0], p.vSensorPos[1]), min(p.vSensorPos[2], p.vSensorPos[3]));
    result.zw = max(max(p.vSensorPos[0], p.vSensorPos[1]), max(p.vSensorPos[2], p.vSensorPos[3]));
    return result;
}

// Converts the parameter NDC AABB to window-space AABB
vec4 ndcAABBToPixels(const vec4 aabb)
{
    return (aabb * 0.5 + 0.5) * vec4(sLensFlareData.vRenderResolution - 1, sLensFlareData.vRenderResolution - 1);
}

// Determines the starting tile IDs for checking tile intersection with the parameter primitive AABB and tile size
ivec2 tileCheckStartIds(const vec4 aabb, const int tileSize, const ivec2 numTiles)
{
    //return ivec2(0);
    return max(ivec2(0), ivec2(floor(aabb.xy / tileSize)));
}

// Determines the starting tile IDs for checking tile intersection with the parameter primitive AABB and tile size
ivec2 tileCheckEndIds(const vec4 aabb, const int tileSize, const ivec2 numTiles)
{
    //return numTiles;
    return min(numTiles, ivec2(ceil(aabb.zw / tileSize)));
}

// Determines the starting tile IDs for checking tile intersection with the parameter primitive AABB and tile size
ivec2 tileCheckStartIdsCoarseTile(const vec4 aabb)
{
    return tileCheckStartIds(aabb, sLensFlareDataTiled.iCoarseTileSize, sLensFlareDataTiled.vNumCoarseTiles);
}

// Determines the starting tile IDs for checking tile intersection with the parameter primitive AABB and tile size
ivec2 tileCheckStartIdsDenseTile(const vec4 aabb)
{
    return tileCheckStartIds(aabb, sLensFlareDataTiled.iTileSize, sLensFlareDataTiled.vNumTiles);
}

// Determines the starting tile IDs for checking tile intersection with the parameter primitive AABB and tile size
ivec2 tileCheckEndIdsCoarseTile(const vec4 aabb)
{
    return tileCheckEndIds(aabb, sLensFlareDataTiled.iCoarseTileSize, sLensFlareDataTiled.vNumCoarseTiles);
}

// Determines the starting tile IDs for checking tile intersection with the parameter primitive AABB and tile size
ivec2 tileCheckEndIdsDenseTile(const vec4 aabb)
{
    return tileCheckEndIds(aabb, sLensFlareDataTiled.iTileSize, sLensFlareDataTiled.vNumTiles);
}

// Intersects a tile with a given AABB
bool intersectsTile(const vec2 aabbMin, const vec2 aabbMax, const vec2 tileStartNDC, const vec2 tileEndNDC)
{
    return !((aabbMax.x < tileStartNDC.x || aabbMin.x > tileEndNDC.x) ||
             (aabbMax.y < tileStartNDC.y || aabbMin.y > tileEndNDC.y));
}

// Intersects a tile with a given AABB
bool intersectsTile(const vec2 aabbMin, const vec2 aabbMax, const ivec2 tileID)
{
    // Tile start and end NDC
    const vec2 tileStartNDC = pixelPosToNDC(tileID * sLensFlareDataTiled.iTileSize);
    const vec2 tileEndNDC = pixelPosToNDC((tileID + 1) * sLensFlareDataTiled.iTileSize - 1);

    return intersectsTile(aabbMin, aabbMax, tileStartNDC, tileEndNDC);
}

// Intersects a tile with a given AABB
bool intersectsCoarseTile(const vec2 aabbMin, const vec2 aabbMax, const ivec2 coarseTileID)
{
    // Tile start and end NDC
    const vec2 coarseTileStartNDC = pixelPosToNDC(coarseTileID * sLensFlareDataTiled.iCoarseTileSize);
    const vec2 coarseTileEndNDC = pixelPosToNDC((coarseTileID + 1) * sLensFlareDataTiled.iCoarseTileSize - 1);

    return intersectsTile(aabbMin, aabbMax, coarseTileStartNDC, coarseTileEndNDC);
}

// Intersects a tile with a given AABB
bool isPixelInside(const vec2 aabbMin, const vec2 aabbMax, const vec2 pixelNDC)
{
    return ((aabbMax.x >= pixelNDC.x && aabbMin.x <= pixelNDC.x) &&
            (aabbMax.y >= pixelNDC.y && aabbMin.y <= pixelNDC.y));
}

////////////////////////////////////////////////////////////////////////////////
// Ray evaluation
////////////////////////////////////////////////////////////////////////////////

// Determines if the parameter ray is valid
bool isRayValid(const GhostSingleTracedRay ray)
{
    return isRayValid(ray.fIntensity, ray.fClipFactor);
}

// Evaluates the parameter ray
vec3 evalRay(const vec3 color, const float intensity, const float clipFactor)
{
    return vec3(color * saturate(intensity)) * le(clipFactor, 1.0) * ge(intensity, MIN_INTENSITY);
}

// Determines whether a sample should be considered based on its barycentric coordinates
bool isSampleValid(const vec3 barycentric)
{
    if (sLensFlareData.iIsWireFrame == 1)
        return isBarycentricOnEdge(barycentric);
    return !isBarycentricOutside(barycentric);
}

// Determines whether a sample should be considered based on its barycentric coordinates
bool isSampleValid(const vec4 barycentric)
{
    if (sLensFlareData.iIsWireFrame == 1)
        return isBarycentricOnEdge(barycentric);
    return !isBarycentricOutside(barycentric);
}

// Determines whether a sample should be considered based on its barycentric coordinates
bool isSampleValidHomogenous(const vec4 barycentric)
{
    if (sLensFlareData.iIsWireFrame == 1)
        return isBarycentricOnEdge(getBarycentricCoordinates(barycentric));
    const vec2 limits = vec2(gt(barycentric.w, 0.0)) * vec2(0.0, barycentric.w) + vec2(lt(barycentric.w, 0.0)) * vec2(barycentric.w, 0.0);
    return all(greaterThanEqual(-barycentric.xyz, vec3(limits.x))) && all(lessThanEqual(-barycentric.xyz, vec3(limits.y)));
}

vec3 processTriangle(const vec2 fragmentNDC, const GhostSinglePrimitive p, const ivec3 vertIds)
{
    // Find the primitive's barycentric coordinates
    const vec3 barycentric = getBarycentricCoordinates(fragmentNDC, p.vSensorPos[vertIds[0]], p.vSensorPos[vertIds[1]], p.vSensorPos[vertIds[2]]);

    // Make sure the center fragment is actually inside the primitive
    if (!isSampleValid(barycentric)) return vec3(0.0);
    
    // Interpolate the ray properties
    const vec2 sensorValues = lerpBarycentric(barycentric, p.vSensorValues[vertIds[0]], p.vSensorValues[vertIds[1]], p.vSensorValues[vertIds[2]]);

    // Evaluate the ray
    return evalRay(p.vColor, sensorValues[0], sensorValues[1]).rgb;
}

vec3 processQuad(const vec2 fragmentNDC, const GhostSinglePrimitive p)
{
    // Find the primitive's barycentric coordinates
    const vec4 barycentric = getBarycentricCoordinates(fragmentNDC, p.vSensorPos[0], p.vSensorPos[1], p.vSensorPos[2], p.vSensorPos[3]);

    // Make sure the center fragment is actually inside the primitive
    if (!isSampleValid(barycentric)) return vec3(0.0);
     
    // Interpolate the ray properties
    const vec2 sensorValues = lerpBarycentric(barycentric, p.vSensorValues[0], p.vSensorValues[1], p.vSensorValues[2], p.vSensorValues[3]);

    // Evaluate the ray
    return evalRay(p.vColor, sensorValues[0], sensorValues[1]).rgb;
}

// Evaluates the primitive for current output fragment
vec3 processPrimitive(const vec2 fragmentNDC, const GhostSinglePrimitive primitive)
{
    //const vec4 aabb = primitiveAABB(primitive);
    //if (!isPixelInside(aabb.xy, aabb.zw, fragmentNDC)) return vec3(0.0);

    return processQuad(fragmentNDC, primitive);
    //return processTriangle(fragmentNDC, primitive, ivec3(0, 1, 2)) + processTriangle(fragmentNDC, primitive, ivec3(0, 2, 3));
}