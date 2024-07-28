////////////////////////////////////////////////////////////////////////////////
// Structure describing an intersection
struct Intersection
{
    // Position of intersection
    vec3 pos;
    
    // Normal of intersection
    vec3 normal;
    
    // Incident angle
    float theta;
    
    // Whether it was a successful hit
    bool hit;
};

////////////////////////////////////////////////////////////////////////////////
// Performs a ray-plane intersection
Intersection intersectPlane(const Lens lens, const Ray ray)
{
    Intersection i;
    
    // Calculate the point of intersection
    i.pos = ray.pos + (ray.dir * ((lens.center.z - ray.pos.z) / ray.dir.z));
    
    // Calculate the normal of intersection
    i.normal = vec3(0.0, 0.0, ray.dir.z > 0.0 ? -1.0 : 1.0);
    
    // Irrelevant
    i.theta = acos(dot(-ray.dir, i.normal));
    
    // It's always a hit
    i.hit = true;
    
    return i;
}

////////////////////////////////////////////////////////////////////////////////
// Performs a ray-sphere intersection
Intersection intersectSphere(const Lens lens, const Ray ray)
{
    Intersection i;
    
    // Vector pointing from the ray to the sphere center
    const vec3 D = ray.pos - lens.center;

    // Solution terms
    const float B = dot(D, ray.dir);
    const float C = dot(D, D) - (lens.radius * lens.radius);
    const float B2_C = B * B - C; // Discriminant
    
    // No hit if the discriminant is negative
    if (B2_C <= 0.0) { i.hit = false; return i; }
    
    // The ray is inside the virtual sphere if multiplying its Z coordinate by 
    // the lens radius is negative, and it is outside if the result is positive
    const float inside = -sign(lens.radius * ray.dir.z);
    const float t = -B + sqrt(B2_C) * inside; // inside:  -B + sqrt(B2_C), outside: -B - sqrt(B2_C)    
    i.pos = ray.pos + t * ray.dir; // Hit position
    i.normal = normalize(i.pos - lens.center) * -inside; // Hit normal (flipped if the ray is inside the sphere)
    i.theta = acos(dot(-ray.dir, i.normal)); // Hit angle
    i.hit = true; // It was a hit
    
    return i;
}

////////////////////////////////////////////////////////////////////////////////
// Perform the appropriate intersection with the lens
Intersection intersectLens(const Lens lens, const Ray ray)
{
    return (lens.radius == 0.0) ? intersectPlane(lens, ray) : intersectSphere(lens, ray);
}

////////////////////////////////////////////////////////////////////////////////
// Fresnel equation
// theta0: incident angle (theta_i)
// theta1: refracted angle in coating (theta_)
// theta2: refracted angle in second medium (theta_)
float fresnelAR(float theta0, const float lambda, const float n0, const float n1, const float n2, const float d)
{
    //theta0 = max(theta0, 1e-4);
    //theta0 += 1e-5;
    //if (theta0 == 0) theta0 = 1e-4;
    //if (abs(theta0) < 1e-4) theta0 = 1e-3;

    if (abs(theta0) < 1e-3) theta0 = 1e-3;

	// Apply Snell's law to get the other angles
    const float st0 = sin(theta0);
	const float theta1 = asin(st0 * n0 / n1);
	const float theta2 = asin(st0 * n0 / n2);

    const float st01 = sin(theta0 + theta1);
    const float tt01 = tan(theta0 + theta1);

    // amplitude for outer reflection/transmission on topmost interface
	const float rs01 = -sin(theta0 - theta1) / st01;
	const float rp01 = tan(theta0 - theta1) / tt01;
	const float ts01 = 2.0 * sin(theta1) * cos(theta0) / st01;
	const float tp01 = ts01 * cos(theta0 - theta1);

    // amplitude for inner reflection
	const float rs12 = -sin(theta1 - theta2) / sin(theta1 + theta2);
	const float rp12 = tan(theta1 - theta2) / tan(theta1 + theta2);

    // after passing through first surface twice:
    // 2 transmissions and 1 reflection
	const float ris = ts01 * ts01 * rs12;
	const float rip = tp01 * tp01 * rp12;

    // phase difference between outer and inner reflections
	const float dy = d * n1;
	const float dx = tan(theta1) * dy;
	const float delay = sqrt(dx * dx + dy * dy);
	const float relPhase = 4.0 * PI / lambda * (delay - dx * st0);
    const float crp = cos(relPhase);

    // add up sinces of different phase and amplitude
	const float out_s2 = rs01 * rs01 + ris * ris + 2.0 * rs01 * ris * crp;
	const float out_p2 = rp01 * rp01 + rip * rip + 2.0 * rp01 * rip * crp;

	return (out_s2 + out_p2) * 0.5;
}

////////////////////////////////////////////////////////////////////////////////
// Ray-tracing implementation
////////////////////////////////////////////////////////////////////////////////

Ray traceGhostRayAnalytical(const float radiusClip, const float irisClip, const int ghostID, const int channelID, const int startSurfaceId, const int numLenses, const ivec2 ghostIndices, const float lambda, Ray ray)
{
    int phase = 0; // Current phase of testing (0: forward #1, 1: backward, 2: forward #2)
    int delta = 1; // Tracing direction, how much to increment the lens id when going from one surface to the next
    for (int surfaceId = startSurfaceId; surfaceId < startSurfaceId + numLenses; surfaceId += delta)
    {
        // Extract the current lens
        const Lens lens = sLensFlareLensData.sLenses[channelID][surfaceId]; 
    
        // Determine the intersection
        const Intersection i = intersectLens(lens, ray);
        if (!i.hit) { invalidateRay(ray); break; } // Stop if we couldn't hit anything
        
        // update the ray attributes
        ray.pos = i.pos;
        if (lens.aperture > 0.0) // Save the UV upon reaching the aperture
        {
            //if (abs(i.theta - sLensFlareLensData.fLightAngle) > acos(sLensFlareLensData.fRefractionClip)) { invalidateRay(ray); break; } // Stop if we couldn't hit anything
        
            //ray.aperturePos = abs(ray.pos.xy); // Plain UV coordinates
            ray.aperturePos = ray.pos.xy; // Plain UV coordinates
            ray.apertureDistAnalytical = length(ray.aperturePos / lens.aperture); // Analytical, ray-tracing aperture dist
            ray.apertureDist = rayApertureDist(ray); // Sample the aperture texture
            continue; // Don't reflect/refract on the aperture
        }
        else
        {
            ray.radius = max(ray.radius, length(ray.pos.xy / lens.height));
        }

        // Get the refractive indices
        const vec4 n = lens.n[phase % 2];
        
        if (phase < 2 && surfaceId == ghostIndices[phase]) // Are we reflecting?
        {
            ray.dir = normalize(reflect(ray.dir, i.normal)); // Compute the reflected direction
            ray.intensity *= fresnelAR(i.theta, lambda, n[0], n[1], n[2], n[3]); // Calculate the Fresnel reflectivity (R) term (how much energy is reflected) and accummulate
            delta = -delta; // Change the iteration direction
            ++phase; // Increment the phase counter
        }
        else // We are refracting
        {
            ray.dir = refract(ray.dir, i.normal, n[0] / n[2]); // Compute the refracted direction
            if (ray.dir == vec3(0.0)) { invalidateRay(ray); break; } // Stop if total internal reflection occurs
            ray.dir = normalize(ray.dir);
            
            /*
            const vec3 newDir = refract(ray.dir, i.normal, n[0] / n[2]); // Compute the refracted direction
            if (newDir == vec3(0.0)) { invalidateRay(ray); break; } // Stop if total internal reflection occurs
            const vec3 newDirNormalized = normalize(newDir); // Compute the refracted direction
            if (dot(ray.dir, newDirNormalized) < sLensFlareLensData.fRefractionClip) { invalidateRay(ray); break; } // Stop if the refracted angle is over a threshold
            ray.dir = newDirNormalized;
            */
        }
    }

    // Calculate the clip factor
    ray.clipFactor = rayClipFactor(ray, radiusClip, irisClip);
    
    // Return the modified ray
    return ray;
}