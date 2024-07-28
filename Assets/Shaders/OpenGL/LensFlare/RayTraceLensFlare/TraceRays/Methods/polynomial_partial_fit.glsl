////////////////////////////////////////////////////////////////////////////////
// Polynomial term baking
////////////////////////////////////////////////////////////////////////////////

MonomialBaked bakePolynomialTerm(const int ghostId, const int wavelengthId, const float rotation, const float angle, const uint termId)
{
//   Non-lerped coefficients
#if LERP_POLYNOMIAL_COEFFICIENTS == 0
    return bakePolynomialTermDefault(ghostId, wavelengthId, rotation, angle, calculateTermId(ghostId, wavelengthId, int(angleIndex(angle)), 0, termId));

//   Lerped coefficients
#else
    // Angle-based term indices
    const vec3 angleTermIndices = angleIndices(angle);
    const uint baseTermId = calculateBaseTermId(ghostId, wavelengthId);
    
    // Extract the relevant angle-based terms
    const MonomialFull terms[2] = 
    {
        sGhostWeightsBufferFull[calculateTermId(baseTermId, int(angleTermIndices[0]), 0, termId)],
        sGhostWeightsBufferFull[calculateTermId(baseTermId, int(angleTermIndices[1]), 0, termId)],
    };
    
    // Polynomial inputs
    const float[NUM_INPUT_VARIABLES_TO_BAKE] polynomialInput = POLYNOMIAL_INPUTS_BAKE;
    const float[2][NUM_INPUT_VARIABLES_TO_BAKE] polynomialInputs = 
    {
        { angleTermIndices[0] * sLensFlareData.fPolynomialAnglesStep, polynomialInput[1] },
        { angleTermIndices[1] * sLensFlareData.fPolynomialAnglesStep, polynomialInput[1] },
    };
    
    // Interpolate the terms and degrees
    MonomialBaked result;
    for (int arrayId = 0; arrayId < 2; ++arrayId)
    {
        const ivec3 degreesToBake[NUM_INPUT_VARIABLES_TO_BAKE] = { POLYNOMIAL_TERMS_TO_BAKE(terms[0].vDegrees[arrayId]) };
        result.vCoefficients[arrayId] = lerp
        (
            spowprod(polynomialInputs[0], degreesToBake) * terms[0].vCoefficients[arrayId], 
            spowprod(polynomialInputs[1], degreesToBake) * terms[1].vCoefficients[arrayId], 
            fract(angleTermIndices[2])
        );
        for (int c = 0; c < NUM_INPUT_VARIABLES_BAKED; ++c)
            result.vDegrees[arrayId][c] = terms[0].vDegrees[arrayId][c];
    }
    
    return result;
#endif
}

////////////////////////////////////////////////////////////////////////////////
// Polynomial evaluation
////////////////////////////////////////////////////////////////////////////////

// Traces a ray from the entrance plane up until the sensor.
Ray traceGhostRayPolynomialPartialFit(const float radiusClip, const float irisClip, const int ghostId, const int wavelengthId, const float wavelength, const float rotation, const float angle, const Ray ray)
{
//   Baked terms; lerped coefficients
#if BAKE_POLYNOMIAL_INVARIANTS == 1 && LERP_POLYNOMIAL_COEFFICIENTS == 1
    return evalPolynomial(radiusClip, irisClip, ghostId, wavelengthId, wavelength, rotation, angle, ray);

//   Baked terms; non-lerped coefficients
#elif BAKE_POLYNOMIAL_INVARIANTS == 1 && LERP_POLYNOMIAL_COEFFICIENTS == 0
    // Handle zero terms
    if (NUM_POLYNOMIAL_TERMS_GHOST == 0) 
    {
        Ray result;
        invalidateRay(result);
        return result;
    }

    // Declare the pupil coordinates
    DECLARE_PUPIL_COORDINATES;
    
    // Angle-based term indices
    const vec3 angleTermIndices = angleIndices(angle);

    // Polynomial input
    const float[NUM_INPUT_VARIABLES] polynomialInput = POLYNOMIAL_INPUT;

    // Evaluate the polynomial
    vec3 polynomial[2] = { vec3(0.0), vec3(0.0) };
    for (uint i = 0, idx = GHOST_TERMS_START_ID; i < NUM_POLYNOMIAL_TERMS_GHOST; ++i, ++idx)
    {
        const MONOMIAL_TYPE term = GHOST_TERMS_BUFFER[idx];
        for (int arrayId = 0; arrayId < 2; ++arrayId)
            polynomial[arrayId] += lerp
            (
                term.vCoefficients[arrayId] * spowprod(polynomialInput, term.vDegrees[arrayId]),
                term.vCoefficients[arrayId] * spowprod(polynomialInput, term.vDegrees[arrayId]),
                fract(angleTermIndices[2])
            );
    }

    // Return the results
    return resolvePolynomialResult(polynomial, angle, rotation, radiusClip, irisClip);

//   Non-baked terms; non-lerped coefficients
#else
    // Handle zero terms
    if (NUM_POLYNOMIAL_TERMS_GHOST == 0) 
    {
        Ray result;
        invalidateRay(result);
        return result;
    }

    // Declare the pupil coordinates
    DECLARE_PUPIL_COORDINATES;
    
    // Angle-based term indices
    const vec3 angleTermIndices = angleIndices(angle);
    const uint baseTermIds[2] = 
    {
        calculateBaseTermId(int(ghostId), wavelengthId, int(angleTermIndices[0]), 0),
        calculateBaseTermId(int(ghostId), wavelengthId, int(angleTermIndices[1]), 0),
    };

    // Polynomial input
    const float[NUM_INPUT_VARIABLES] polynomialInput = POLYNOMIAL_INPUT;
    const float inputAngles[2] = 
    {
        POLYNOMIAL_INPUT_ANGLE(angleTermIndices[0] * sLensFlareData.fPolynomialAnglesStep),
        POLYNOMIAL_INPUT_ANGLE(angleTermIndices[1] * sLensFlareData.fPolynomialAnglesStep)
    };
    float[2][NUM_INPUT_VARIABLES] polynomialInputs = 
    {
        { polynomialInput[0], polynomialInput[1], polynomialInput[2], polynomialInput[3], inputAngles[0], polynomialInput[5] },
        { polynomialInput[0], polynomialInput[1], polynomialInput[2], polynomialInput[3], inputAngles[1], polynomialInput[5] },
    };

    // Evaluate the polynomial
    vec3 polynomial[2] = { vec3(0.0), vec3(0.0) };
    for (uint i = 0; i < NUM_POLYNOMIAL_TERMS_GHOST; ++i)
    {
        // Extract the four relevant angle-based terms
        const MONOMIAL_TYPE terms[2] = 
        {
            sGhostWeightsBufferFull[calculateTermId(baseTermIds[0], i)],
            sGhostWeightsBufferFull[calculateTermId(baseTermIds[1], i)],
        };
        for (int arrayId = 0; arrayId < 2; ++arrayId)
            polynomial[arrayId] += lerp
            (
                spowprod(polynomialInputs[0], terms[0].vDegrees[arrayId]) * terms[0].vCoefficients[arrayId], 
                spowprod(polynomialInputs[1], terms[1].vDegrees[arrayId]) * terms[1].vCoefficients[arrayId], 
                fract(angleTermIndices[2])
            );
    }

    // Return the results
    return resolvePolynomialResult(polynomial, angle, rotation, radiusClip, irisClip);

#endif
}