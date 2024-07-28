////////////////////////////////////////////////////////////////////////////////
// Polynomial term baking
////////////////////////////////////////////////////////////////////////////////

MonomialBaked bakePolynomialTerm(const int ghostId, const int wavelengthId, const float rotation, const float angle, const uint termId)
{
    return bakePolynomialTermDefault(ghostId, wavelengthId, rotation, angle, GHOST_TERMS_START_ID_BUFFER_FULL + termId);
}

////////////////////////////////////////////////////////////////////////////////
// Polynomial evaluation
////////////////////////////////////////////////////////////////////////////////

// Traces a ray from the entrance plane up until the sensor.
Ray traceGhostRayPolynomialFullFit(const float radiusClip, const float irisClip, const int ghostId, const int wavelengthId, const float wavelength, const float rotation, const float angle, const Ray ray)
{    
//   Baked terms; lerped coefficients
#if BAKE_POLYNOMIAL_INVARIANTS == 1
    return evalPolynomial(radiusClip, irisClip, ghostId, wavelengthId, wavelength, rotation, angle, ray);

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
    
    // Polynomial input
    const float[NUM_INPUT_VARIABLES] polynomialInput = POLYNOMIAL_INPUT;

    // Evaluate the polynomial
    vec3 polynomial[2] = { vec3(0.0), vec3(0.0) };
    for (uint i = 0, idx = GHOST_TERMS_START_ID_BUFFER_FULL; i < NUM_POLYNOMIAL_TERMS_GHOST; ++i, ++idx)
    {
        // Extract the four relevant angle-based terms
        const MONOMIAL_TYPE term = sGhostWeightsBufferFull[idx];
        for (int arrayId = 0; arrayId < 2; ++arrayId)
            polynomial[arrayId] += term.vCoefficients[arrayId] * spowprod(polynomialInput, term.vDegrees[arrayId]);
    }

    // Return the results
    return resolvePolynomialResult(polynomial, angle, rotation, radiusClip, irisClip);

#endif
}