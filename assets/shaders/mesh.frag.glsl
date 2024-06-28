#version 460
#extension GL_EXT_nonuniform_qualifier: require

#include "scene_data.h.glsl"
#include "pbr.h.glsl"
#include "shadow.h.glsl"

layout (location = 0) in vec3 in_world_pos;
layout (location = 1) in vec2 in_tex_coord;
layout (location = 2) in vec3 in_normal;
layout (location = 3) in vec3 in_tangent;
layout (location = 4) in vec3 in_bi_tangent;
layout (location = 5) in vec4 in_shadow_coord;
layout (location = 6) in flat int in_material_index;

layout (location = 0) out vec4 out_frag_color;

layout (set = 0, binding = 0) uniform CameraBuffer {
    mat4 view;
    mat4 proj;
    mat4 view_proj;
    vec3 position;
} camera;

struct Material {
    vec4 base_color_factor;
    uint albedo_texture_index;
    uint normal_texture_index;
    uint metallic_roughness_texture_index;
    uint occlusion_texture_index;
    float metallic_factor;
    float roughness_factor;
};
layout (std430, set = 2, binding = 0) readonly restrict buffer MaterialBuffer {
    Material materials[];
};

layout (set = 3, binding = 10) uniform sampler2D global_textures[];


// #define VISUALIZE_SHADOW_MAP

Material current_material() {
    return materials[in_material_index];
}

vec3 calculate_pixel_normal() {
    mat3 TNB = mat3(normalize(in_tangent), normalize(in_bi_tangent), normalize(in_normal));

    // obtain normal from normal map in range [0,1]
    uint normal_texture_index = current_material().normal_texture_index;
    vec3 tangent_space_normal = texture(global_textures[nonuniformEXT(normal_texture_index)], in_tex_coord).xyz;
    // transform normal vector to range [-1,1]
    tangent_space_normal = normalize(tangent_space_normal * 2.0 - 1.0);

    vec3 pixel_normal = normalize(TNB * tangent_space_normal);
    return pixel_normal;
}

vec3 reinhard_tone_mapping(vec3 radiance) {
    return radiance / (radiance + vec3(1.0));
}




void main()
{
    #ifdef VISUALIZE_SHADOW_MAP

    float color = texture(shadow_map, in_shadow_coord.xy).r;
    out_frag_color = vec4(color, color, color, 1.0);

    #else

    Material material = current_material();

    uint occlusion_texture_index = material.occlusion_texture_index;
    float ambient_occlusion = texture(global_textures[nonuniformEXT(occlusion_texture_index)], in_tex_coord).r;

    uint albedo_texture_index = material.albedo_texture_index;
    vec4 albedo = material.base_color_factor * texture(global_textures[nonuniformEXT(albedo_texture_index)], in_tex_coord);
    // alpha cutoff
    //    if (albedo.a < 0.1) {
    //        discard;
    //    }
    vec3 base_color = albedo.rgb;

    vec2 metallic_roughness = texture(global_textures[nonuniformEXT(material.metallic_roughness_texture_index)], in_tex_coord).bg;
    float metallic = metallic_roughness.r * material.metallic_factor;
    float perceptual_roughness = metallic_roughness.g * material.roughness_factor;

    vec3 normal = calculate_pixel_normal();
    //vec3 normal = in_normal;

    // lighting
    vec3 sunlight_direction = scene_data.sunlight_direction.xyz;
    float ambient_strength = scene_data.sunlight_direction.w;
    vec3 ambient = base_color * ambient_occlusion * ambient_strength;

    vec3 view_direction = normalize(camera.position - in_world_pos); // view unit vector
    vec3 light_direction = normalize(-sunlight_direction);

    Surface surface;
    surface.base_color = base_color;
    surface.normal = normal;
    surface.reflectance = 0.5;
    surface.perceptual_roughness = perceptual_roughness;
    surface.metallic = metallic;

    vec3 F = BRDF(view_direction, light_direction, surface);

    float NoL = clamp(dot(normal, light_direction), 0.0, 1.0);

    // lightIntensity is the illuminance
    // at perpendicular incidence in lux
    vec3 sunlight_color = scene_data.sunlight_color.xyz;
    float sunlight_intensity = scene_data.sunlight_color.w;

    float illuminance = sunlight_intensity * NoL;
    vec3 luminance = F * sunlight_color * illuminance;

    float visibility = 1.0;

    uint shadow_mode = scene_data.sunlight_shadow_mode;
    if (shadow_mode > 0) {
        visibility = shadow_mapping(in_shadow_coord, shadow_mode);
    }

    vec3 Lo = ambient + luminance * visibility;

    vec3 color = reinhard_tone_mapping(Lo);
    out_frag_color = vec4(color, 1.0f);

    #endif
}
