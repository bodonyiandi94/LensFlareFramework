#if RAY_TRACE_METHOD == RaytraceMethod_PolynomialFullFit
    #undef BAKE_POLYNOMIAL_INVARIANTS
    #define BAKE_POLYNOMIAL_INVARIANTS 0
#endif

////////////////////////////////////////////////////////////////////////////////
// Polynomial defines
////////////////////////////////////////////////////////////////////////////////

// Pupil coordinates
#define PUPIL_COORDINATES_CARTESIAN_HALF_LENGTH vec2(sLensFlareLensData.fOuterPupilHeight)
#define PUPIL_COORDINATES_CARTESIAN ray.pos.xy
#define PUPIL_COORDINATES_CARTESIAN_NORMALIZED (PUPIL_COORDINATES_CARTESIAN / PUPIL_COORDINATES_CARTESIAN_HALF_LENGTH)
#define PUPIL_COORDINATES_POLAR cart2pol(PUPIL_COORDINATES_CARTESIAN)
#define PUPIL_COORDINATES_POLAR_NORMALIZED cart2pol(PUPIL_COORDINATES_CARTESIAN / PUPIL_COORDINATES_CARTESIAN_HALF_LENGTH)

#define DECLARE_PUPIL_COORDINATES \
    const vec2 pupilCoordinatesCartesian = PUPIL_COORDINATES_CARTESIAN; \
    const vec2 pupilCoordinatesPolar = PUPIL_COORDINATES_POLAR_NORMALIZED; \
    const vec2 pupilCoordinatesPolarInverted = vec2(1.0 - pupilCoordinatesPolar.x, pupilCoordinatesPolar.y) \

// Inputs
#define POLYNOMIAL_INPUT_ANGLE(X) degrees(X)
#define POLYNOMIAL_INPUT_0 pupilCoordinatesCartesian.x
#define POLYNOMIAL_INPUT_1 pupilCoordinatesCartesian.y
#define POLYNOMIAL_INPUT_2 pupilCoordinatesPolar.x
#define POLYNOMIAL_INPUT_3 pupilCoordinatesPolarInverted.x
#define POLYNOMIAL_INPUT_4 POLYNOMIAL_INPUT_ANGLE(angle)
#define POLYNOMIAL_INPUT_5 wavelength * 1e-3

#define POLYNOMIAL_INPUTS_1 { POLYNOMIAL_INPUT_0 }
#define POLYNOMIAL_INPUTS_2 { POLYNOMIAL_INPUT_0, POLYNOMIAL_INPUT_1}
#define POLYNOMIAL_INPUTS_3 { POLYNOMIAL_INPUT_0, POLYNOMIAL_INPUT_1, POLYNOMIAL_INPUT_2 }
#define POLYNOMIAL_INPUTS_4 { POLYNOMIAL_INPUT_0, POLYNOMIAL_INPUT_1, POLYNOMIAL_INPUT_2, POLYNOMIAL_INPUT_3 }
#define POLYNOMIAL_INPUTS_5 { POLYNOMIAL_INPUT_0, POLYNOMIAL_INPUT_1, POLYNOMIAL_INPUT_2, POLYNOMIAL_INPUT_3, POLYNOMIAL_INPUT_4 }
#define POLYNOMIAL_INPUTS_6 { POLYNOMIAL_INPUT_0, POLYNOMIAL_INPUT_1, POLYNOMIAL_INPUT_2, POLYNOMIAL_INPUT_3, POLYNOMIAL_INPUT_4, POLYNOMIAL_INPUT_5 }
#define POLYNOMIAL_INPUTS_7 { POLYNOMIAL_INPUT_0, POLYNOMIAL_INPUT_1, POLYNOMIAL_INPUT_2, POLYNOMIAL_INPUT_3, POLYNOMIAL_INPUT_4, POLYNOMIAL_INPUT_5, POLYNOMIAL_INPUT_6 }
#define POLYNOMIAL_INPUTS_8 { POLYNOMIAL_INPUT_0, POLYNOMIAL_INPUT_1, POLYNOMIAL_INPUT_2, POLYNOMIAL_INPUT_3, POLYNOMIAL_INPUT_4, POLYNOMIAL_INPUT_5, POLYNOMIAL_INPUT_6, POLYNOMIAL_INPUT_7 }

#define POLYNOMIAL_INPUTS_PARAMETRIC_1 { POLYNOMIAL_INPUT_0 }
#define POLYNOMIAL_INPUTS_PARAMETRIC_2 { POLYNOMIAL_INPUT_0, POLYNOMIAL_INPUT_1}
#define POLYNOMIAL_INPUTS_PARAMETRIC_3 { POLYNOMIAL_INPUT_0, POLYNOMIAL_INPUT_1, POLYNOMIAL_INPUT_2 }
#define POLYNOMIAL_INPUTS_PARAMETRIC_4 { POLYNOMIAL_INPUT_0, POLYNOMIAL_INPUT_1, POLYNOMIAL_INPUT_2, POLYNOMIAL_INPUT_3 }
#define POLYNOMIAL_INPUTS_PARAMETRIC_5 { POLYNOMIAL_INPUT_0, POLYNOMIAL_INPUT_1, POLYNOMIAL_INPUT_2, POLYNOMIAL_INPUT_3, POLYNOMIAL_INPUT_4 }
#define POLYNOMIAL_INPUTS_PARAMETRIC_6 { POLYNOMIAL_INPUT_0, POLYNOMIAL_INPUT_1, POLYNOMIAL_INPUT_2, POLYNOMIAL_INPUT_3, POLYNOMIAL_INPUT_4, POLYNOMIAL_INPUT_5 }
#define POLYNOMIAL_INPUTS_PARAMETRIC_7 { POLYNOMIAL_INPUT_0, POLYNOMIAL_INPUT_1, POLYNOMIAL_INPUT_2, POLYNOMIAL_INPUT_3, POLYNOMIAL_INPUT_4, POLYNOMIAL_INPUT_5, POLYNOMIAL_INPUT_6 }
#define POLYNOMIAL_INPUTS_PARAMETRIC_8 { POLYNOMIAL_INPUT_0, POLYNOMIAL_INPUT_1, POLYNOMIAL_INPUT_2, POLYNOMIAL_INPUT_3, POLYNOMIAL_INPUT_4, POLYNOMIAL_INPUT_5, POLYNOMIAL_INPUT_6, POLYNOMIAL_INPUT_7 }

#define POLYNOMIAL_TERMS_TO_BAKE_1(DEGREES) DEGREES[NUM_INPUT_VARIABLES_BAKED + 0]
#define POLYNOMIAL_TERMS_TO_BAKE_2(DEGREES) DEGREES[NUM_INPUT_VARIABLES_BAKED + 0], DEGREES[NUM_INPUT_VARIABLES_BAKED + 1]
#define POLYNOMIAL_TERMS_TO_BAKE_3(DEGREES) DEGREES[NUM_INPUT_VARIABLES_BAKED + 0], DEGREES[NUM_INPUT_VARIABLES_BAKED + 1], DEGREES[NUM_INPUT_VARIABLES_BAKED + 2]
#define POLYNOMIAL_TERMS_TO_BAKE_4(DEGREES) DEGREES[NUM_INPUT_VARIABLES_BAKED + 0], DEGREES[NUM_INPUT_VARIABLES_BAKED + 1], DEGREES[NUM_INPUT_VARIABLES_BAKED + 2], DEGREES[NUM_INPUT_VARIABLES_BAKED + 3]

// Full, non-baked
#define NUM_INPUT_VARIABLES_FULL 6
#define NUM_OUTPUT_VARIABLES_FULL 6
#define NUM_WAVELENGTHS_FULL 3
#define GHOST_TERMS_BUFFER_FULL sGhostWeightsBufferFull
#define GHOST_TERMS_START_ID_BUFFER_FULL (ghostId * NUM_POLYNOMIAL_TERMS)
#define MONOMIAL_TYPE_FULL MonomialFull
#define POLYNOMIAL_INPUT_FULL CONCAT(POLYNOMIAL_INPUTS_, NUM_INPUT_VARIABLES_FULL)

// Partial, baked
#define NUM_INPUT_VARIABLES_BAKED 4
#define NUM_INPUT_VARIABLES_TO_BAKE 2
#define NUM_OUTPUT_VARIABLES_BAKED NUM_OUTPUT_VARIABLES_FULL
#define NUM_WAVELENGTHS_BAKED NUM_WAVELENGTHS_FULL
#define GHOST_TERMS_BUFFER_BAKED sGhostWeightsBufferBaked
#define GHOST_TERMS_START_ID_BUFFER_BAKED ((ghostId * NUM_WAVELENGTHS_BAKED + wavelengthId) * NUM_POLYNOMIAL_TERMS)
#define MONOMIAL_TYPE_BAKED MonomialBaked
#define POLYNOMIAL_INPUTS_BAKE { POLYNOMIAL_INPUT_ANGLE(sLensFlareLensData.fLightAngle), sLensFlareLensData.vWavelengths[wavelengthId].x * 1e-3 }
#define POLYNOMIAL_INPUT_BAKED CONCAT(POLYNOMIAL_INPUTS_, NUM_INPUT_VARIABLES_BAKED)
#define POLYNOMIAL_TERMS_TO_BAKE CONCAT(POLYNOMIAL_TERMS_TO_BAKE_, NUM_INPUT_VARIABLES_TO_BAKE)

// Delegate to the proper enums
#if BAKE_POLYNOMIAL_INVARIANTS == 1
    #define NUM_INPUT_VARIABLES NUM_INPUT_VARIABLES_BAKED
    #define NUM_OUTPUT_VARIABLES NUM_OUTPUT_VARIABLES_BAKED
    #define NUM_WAVELENGTHS NUM_WAVELENGTHS_BAKED
    #define GHOST_TERMS_BUFFER_BUFFER GHOST_TERMS_BUFFER_BAKED
    #define GHOST_TERMS_START_ID_BUFFER GHOST_TERMS_START_ID_BUFFER_BAKED
    #define MONOMIAL_TYPE MONOMIAL_TYPE_BAKED
    #define POLYNOMIAL_INPUT POLYNOMIAL_INPUT_BAKED
#else
    #define NUM_INPUT_VARIABLES NUM_INPUT_VARIABLES_FULL
    #define NUM_OUTPUT_VARIABLES NUM_OUTPUT_VARIABLES_FULL
    #define NUM_WAVELENGTHS NUM_WAVELENGTHS_FULL
    #define GHOST_TERMS_BUFFER_BUFFER GHOST_TERMS_BUFFER_FULL
    #define GHOST_TERMS_START_ID_BUFFER GHOST_TERMS_START_ID_BUFFER_FULL
    #define MONOMIAL_TYPE MONOMIAL_TYPE_FULL
    #define POLYNOMIAL_INPUT POLYNOMIAL_INPUT_FULL
#endif

// Determine which term count to use
#if USE_DYNAMIC_POLYNOMIAL_TERM_COUNTS == 1
    #define NUM_POLYNOMIAL_TERMS_GHOST sGhostParameters.iNumPolynomialTerms
#else
    #define NUM_POLYNOMIAL_TERMS_GHOST NUM_POLYNOMIAL_TERMS
#endif

////////////////////////////////////////////////////////////////////////////////
// Fit sparse polynomial terms
////////////////////////////////////////////////////////////////////////////////

// Monomial with a fully fit polynomial
struct MonomialFull
{
    vec3 vCoefficients[2];
    ivec3 vDegrees[2][NUM_INPUT_VARIABLES_FULL];
};

TYPED_ARRAY_BUFFER(std430, UNIFORM_BUFFER_GENERIC_5, sGhostWeightsBufferFull_, MonomialFull);
#define sGhostWeightsBufferFull sGhostWeightsBufferFull_.sData

// Monomial with invariants baked
struct MonomialBaked
{
    vec3 vCoefficients[2];
    ivec3 vDegrees[2][NUM_INPUT_VARIABLES_BAKED];
};

TYPED_ARRAY_BUFFER(std430, UNIFORM_BUFFER_GENERIC_6, sGhostWeightsBufferBaked_, MonomialBaked);
#define sGhostWeightsBufferBaked sGhostWeightsBufferBaked_.sData

////////////////////////////////////////////////////////////////////////////////
// Memory access
////////////////////////////////////////////////////////////////////////////////

#ifdef TRACE_RAYS_POLY_USE_SHARED_MEMORY
    shared MONOMIAL_TYPE gsPolynomialTerms[NUM_POLYNOMIAL_TERMS]; // Groupshared polynomial terms
    #define GHOST_TERMS_BUFFER gsPolynomialTerms
    #define GHOST_TERMS_START_ID (0)
#else
    #define GHOST_TERMS_BUFFER GHOST_TERMS_BUFFER_BUFFER
    #define GHOST_TERMS_START_ID (GHOST_TERMS_START_ID_BUFFER)
#endif

// Initialize the shared memory
void initGhostRayGroupSharedMemory(const int ghostId, const int wavelengthId, const int workerIdx, const int workersPerGroup)
{
#ifdef TRACE_RAYS_POLY_USE_SHARED_MEMORY
    const int weightsPerWorker = ROUNDED_DIV(sGhostParameters.iNumPolynomialTerms, workersPerGroup);
    for (int i = 0, weightId = workerIdx * weightsPerWorker, bufferId = GHOST_TERMS_START_ID_BUFFER_BAKED + weightId; i < weightsPerWorker && weightId < sGhostParameters.iNumPolynomialTerms; ++i, ++weightId, ++bufferId)
        gsPolynomialTerms[weightId] = GHOST_TERMS_BUFFER_BUFFER[bufferId];
    barrier();
#endif
}

////////////////////////////////////////////////////////////////////////////////
// Indexing
////////////////////////////////////////////////////////////////////////////////

// Converts a single index to a collection of indices in the form (floor(id), ceil(id), id)
vec3 makeIndices(float index)
{
	return vec3(floor(index), ceil(index), index);
}

// Clamps the input indices to a valid range
float clampIndex(const float index, const uint numValues)
{
	return clamp(index, 0.0, float(numValues - 1));
}

// Clamps the input indices to a valid range
vec3 clampIndices(const vec3 indices, const uint numValues)
{
	return clamp(indices, vec3(0.0), vec3(float(numValues - 1)));
}

// Clamps the input index to a valid range
vec3 clampIndices(const float index, const uint numValues)
{
	return clampIndices(makeIndices(index), numValues);
}

// Helper macro to generate the various index calculators
#define PARAM_INDEX(FNAME, PNAME) \
	float FNAME##IndexUnclamped(const float val) \
	{ \
		return (val) / sLensFlareData.f##PNAME##Step; \
	} \
	float FNAME##Index(const float val) \
	{ \
		return clampIndex(FNAME##IndexUnclamped(val), sLensFlareData.iNum##PNAME); \
	} \
	vec3 FNAME##IndicesFromIndex(const float index) \
	{ \
		return clampIndices(index, sLensFlareData.iNum##PNAME); \
	} \
	vec3 FNAME##Indices(const float val) \
	{ \
		return FNAME##IndicesFromIndex(FNAME##IndexUnclamped(val)); \
	} \

// Instantiate the index calculator functions
PARAM_INDEX(angle, PolynomialAngles)
PARAM_INDEX(rotation, PolynomialRotations)

uint calculateTermId(const int ghostId, const int wavelengthId, const int angleId, const int rotationId, const uint termId)
{
    return uint(termId + NUM_POLYNOMIAL_TERMS * (
        ghostId * sLensFlareData.iMaxNumChannels * sLensFlareData.iNumPolynomialAngles * sLensFlareData.iNumPolynomialRotations +
        wavelengthId * sLensFlareData.iNumPolynomialAngles * sLensFlareData.iNumPolynomialRotations +
        angleId * sLensFlareData.iNumPolynomialRotations +
        rotationId
    ));
}

uint calculateBaseTermId(const int ghostId, const int wavelengthId)
{
    return uint(
        ghostId * sLensFlareData.iMaxNumChannels * sLensFlareData.iNumPolynomialAngles * sLensFlareData.iNumPolynomialRotations +
        wavelengthId * sLensFlareData.iNumPolynomialAngles * sLensFlareData.iNumPolynomialRotations
    );
}

uint calculateBaseTermId(const int ghostId, const int wavelengthId, const int angleId, const int rotationId)
{
    return uint(
        ghostId * sLensFlareData.iMaxNumChannels * sLensFlareData.iNumPolynomialAngles * sLensFlareData.iNumPolynomialRotations +
        wavelengthId * sLensFlareData.iNumPolynomialAngles * sLensFlareData.iNumPolynomialRotations +
        angleId * sLensFlareData.iNumPolynomialRotations +
        rotationId
    );
}

uint calculateTermId(const uint baseTermId, const int angleId, const int rotationId, const uint termId)
{
    return uint(termId + NUM_POLYNOMIAL_TERMS * (baseTermId + angleId * sLensFlareData.iNumPolynomialRotations + rotationId));
}

uint calculateTermId(const uint baseTermId, const uint termId)
{
    return uint(termId + NUM_POLYNOMIAL_TERMS * baseTermId);
}

////////////////////////////////////////////////////////////////////////////////
// Signed pow functions
////////////////////////////////////////////////////////////////////////////////

// Signed pow function
float spow(const float x, const int y)
{
    //return exp2(y * log2(abs(x))) * mix(1, sign(x), y % 2);
    return (y == 0) ? 1.0 : pow(max(abs(x), 1e-6), y) * ((y % 2 == 1 && x < 0.0) ? -1 : 1);
}

vec3 spow_v(const vec3 x, const ivec3 y)
{
    return vec3(spow(x[0], y[0]), spow(x[1], y[1]), spow(x[2], y[2]));

    //return exp2(y * log2(abs(x))) * mix(vec3(1), sign(x), vec3(y % 2));

    //return pow(vec3(x[i]), y[i])
}

// Product of signed pows
#define INSTANTIATE_SPOWPROD(N) \
    vec3 spowprod(const float[N] x, const ivec3[N] y) \
    { \
        vec3 result = vec3(1.0); \
        for (int i = 0; i < N; ++i) \
            result *= spow_v(vec3(x[i]), y[i]); \
        return result; \
    }

// Instantiations
INSTANTIATE_SPOWPROD(2)
INSTANTIATE_SPOWPROD(3)
INSTANTIATE_SPOWPROD(4)
INSTANTIATE_SPOWPROD(5)
INSTANTIATE_SPOWPROD(6)

////////////////////////////////////////////////////////////////////////////////
// Evaluation
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
Ray resolvePolynomialResult(const vec3[2] polynomial, const float angle, const float rotation, const float radiusClip, const float irisClip)
{
    // Write out the results
    Ray result;
    #if NUM_OUTPUT_VARIABLES == 4
        result.pos.xy = polynomial[0].xy;
        result.clipFactor = polynomial[0].z;
        result.intensity = polynomial[0].w;
    #elif NUM_OUTPUT_VARIABLES == 6
        result.aperturePos = polynomial[0].xy;
        result.intensity = polynomial[0].z;
        result.radius = polynomial[1].z;
        result.pos.xy = polynomial[1].xy;
    #endif

    // Rotate the aperture and sensor position
    if (sLensFlareData.iNumPolynomialRotations <= 1)
    {
        result.pos = rotationMatrix2D(rotation) * result.pos;
        result.aperturePos = vec2(rotationMatrix2D(rotation) * vec3(result.aperturePos, 1));
    }

    // Sample the aperture texture
    #if NUM_OUTPUT_VARIABLES == 6
        result.apertureDist = rayApertureDist(result);
        result.clipFactor = rayClipFactor(result, radiusClip, irisClip);
    #endif
    
    // Return the resulting ray
    return result;    
}

Ray evalPolynomial(const float radiusClip, const float irisClip, const int ghostId, const int wavelengthId, const float wavelength, const float rotation, const float angle, const Ray ray)
{
    // Handle zero terms
    if (NUM_POLYNOMIAL_TERMS_GHOST == 0) 
    {
        Ray result;
        invalidateRay(result);
        return result;
    }

    // Declare the pupil coordinates
    DECLARE_PUPIL_COORDINATES;

    // Input variables
    const float[NUM_INPUT_VARIABLES] polynomialInput = POLYNOMIAL_INPUT;

    // Evaluate the polynomial
    vec3 polynomial[2] = { vec3(0.0), vec3(0.0) };
    for (uint i = 0, idx = GHOST_TERMS_START_ID; i < NUM_POLYNOMIAL_TERMS_GHOST; ++i, ++idx)
    {
        const MONOMIAL_TYPE term = GHOST_TERMS_BUFFER[idx];
        for (int arrayId = 0; arrayId < 2; ++arrayId)
            polynomial[arrayId] += term.vCoefficients[arrayId] * spowprod(polynomialInput, term.vDegrees[arrayId]);
    }
    
    // Return the results
    return resolvePolynomialResult(polynomial, angle, rotation, radiusClip, irisClip);
}

////////////////////////////////////////////////////////////////////////////////
// Term baking
////////////////////////////////////////////////////////////////////////////////

MonomialBaked bakePolynomialTermDefault(const int ghostId, const int wavelengthId, const float rotation, const float angle, const uint termIndex)
{
    const float[NUM_INPUT_VARIABLES_TO_BAKE] polynomialInput = POLYNOMIAL_INPUTS_BAKE;
    const MonomialFull term = sGhostWeightsBufferFull[termIndex];
    
    MonomialBaked result;
    for (int arrayId = 0; arrayId < 2; ++arrayId)
    {
        const ivec3 degreesToBake[NUM_INPUT_VARIABLES_TO_BAKE] = { POLYNOMIAL_TERMS_TO_BAKE(term.vDegrees[arrayId]) };
        result.vCoefficients[arrayId] = term.vCoefficients[arrayId] * spowprod(polynomialInput, degreesToBake);
        for (int c = 0; c < NUM_INPUT_VARIABLES_BAKED; ++c)
            result.vDegrees[arrayId][c] = term.vDegrees[arrayId][c];
    }

    return result;
}

MonomialBaked bakePolynomialTerm(const int ghostId, const int wavelengthId, const float rotation, const float angle, const uint termId);

// Bakes the invariants into the polynomials
void bakePolynomialInvariants(const int ghostId, const int wavelengthId, const float rotation, const float angle)
{
    for (uint i = 0; i < NUM_POLYNOMIAL_TERMS; ++i)
        sGhostWeightsBufferBaked[GHOST_TERMS_START_ID_BUFFER_BAKED + i] = bakePolynomialTerm(ghostId, wavelengthId, rotation, angle, i);
}