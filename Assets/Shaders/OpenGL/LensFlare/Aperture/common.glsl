// Polygonal smoothed min 
// https://iquilezles.org/articles/smin/
float smin_polynomial(const float a, const float b, const float k)
{
    const float diff = b - a;
    const float h = clamp(0.5 + 0.5 * diff / k, 0.0, 1.0);
    return b - h * (diff + k * (1.0 - h));
}

// Polygonal smoothed max
// https://iquilezles.org/articles/smin/
float smax_polynomial(const float a, const float b, const float k)
{
    const float diff = a - b;
    const float h = clamp(0.5 + 0.5 * diff / k, 0.0, 1.0);
    return b + h * (diff + k * (1.0 - h));    
}

float hardMask(const float dist)
{
    return 1.0 - step(0.95, dist);
}

float smoothMask(const float dist)
{
    return smoothstep(0.95, 0.8, dist);
}

float circleDist(const vec2 pos)
{
    return length(pos);
}

float octagonDist(const vec2 pos)
{
    const vec2 absPos = abs(pos);
    return smax_polynomial(max(absPos.x, absPos.y), dot(absPos, vec2(0.70710678118)), 0.1);
}