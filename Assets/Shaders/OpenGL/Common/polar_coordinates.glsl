const vec3 sph2cart(const float azimuth, const float elevation, const float r)
{
    return r * vec3
    (
        sin(elevation) * cos(azimuth),
        cos(elevation),
        sin(elevation) * sin(azimuth)
    );
}

const vec2 cart2pol(const vec2 cart)
{
    return vec2(length(cart), atan(cart.y, cart.x));
}