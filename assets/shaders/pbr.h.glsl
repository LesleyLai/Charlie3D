#ifndef CHARLIE3D_PBR_GLSL
#define CHARLIE3D_PBR_GLSL

#include "prelude.h.glsl"

struct Surface {
    vec3 base_color;
    vec3 normal;
    float reflectance;
    float perceptual_roughness;
    float metallic;
};

float Fd_lambertian() {
    return 1.0 / PI;
}

float D_GGX(float NoH, float a) {
    float a2 = a * a;
    float f = (NoH * a2 - NoH) * NoH + 1.0;
    return a2 / (PI * f * f);
}

vec3 F_Schlick(float u, vec3 f0) {
    return f0 + (vec3(1.0) - f0) * pow(1.0 - u, 5.0);
}

float V_SmithGGXCorrelated(float NoV, float NoL, float a) {
    float a2 = a * a;
    float GGXL = NoV * sqrt((-NoL * a2 + NoL) * NoL + a2);
    float GGXV = NoL * sqrt((-NoV * a2 + NoV) * NoV + a2);
    return 0.5 / (GGXV + GGXL);
}

vec3 BRDF(vec3 view_direction, vec3 light_direction, Surface surface) {
    vec3 base_color = surface.base_color;
    vec3 normal = surface.normal;
    float metallic = surface.metallic;
    float roughness = surface.perceptual_roughness * surface.perceptual_roughness;
    float reflectance = surface.reflectance;

    vec3 f0 = mix(vec3(0.16 * reflectance * reflectance), base_color, metallic);
    vec3 h = normalize(view_direction + light_direction);

    float NoV = abs(dot(normal, view_direction)) + 1e-5;
    float NoL = clamp(dot(normal, light_direction), 0.0, 1.0);
    float NoH = clamp(dot(normal, h), 0.0, 1.0);
    float LoH = clamp(dot(light_direction, h), 0.0, 1.0);

    // Mapped reflectance

    float D = D_GGX(NoH, roughness);
    vec3 F = F_Schlick(LoH, f0);
    float V = V_SmithGGXCorrelated(NoV, NoL, roughness);

    // Specular BRDF (Cook-Torrance)
    vec3 Fr = (D * V) * F;

    // diffuse BRDF
    vec3 diffuse_color = (1.0 - metallic) * base_color;
    vec3 Fd = diffuse_color * Fd_lambertian();

    return Fd + Fr;
}

#endif // CHARLIE3D_PBR_GLSL
