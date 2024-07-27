#ifndef CHARLIE3D_OCTAHEDRAON_ENCODING_GLSL
#define CHARLIE3D_OCTAHEDRAON_ENCODING_GLSL

vec2 sign_not_zero(vec2 v)
{
    return vec2((v.x >= 0.0) ? + 1.0 : -1.0, (v.y >= 0.0) ? + 1.0 : -1.0);
}

vec3 vec3_from_oct(vec2 e)
{
    vec3 v = vec3(e.xy, 1.0 - abs(e.x) - abs(e.y));
    if (v.z < 0) v.xy = (1.0 - abs(v.yx)) * sign_not_zero(v.xy);
    return normalize(v);
}

#endif // CHARLIE3D_OCTAHEDRAON_ENCODING_GLSL