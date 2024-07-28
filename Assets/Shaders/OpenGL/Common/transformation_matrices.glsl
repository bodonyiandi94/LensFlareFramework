mat3 rotationMatrix2D(const float angle)
{
    return mat3(cos(angle), sin(angle), 0, -sin(angle), cos(angle), 0, 0, 0, 1);
}