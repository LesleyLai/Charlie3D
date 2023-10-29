#version 460

layout (location = 0) in vec2 in_tex_coord;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec3 in_tangent;
layout (location = 3) in vec3 in_bi_tangent;
layout (location = 4) in vec4 in_shadow_coord;

layout (constant_id = 0) const int shadow_mode = 0;

layout (location = 0) out vec4 out_frag_color;

#include "scene_data.h.glsl"

layout (set = 0, binding = 2) uniform sampler2D shadow_map;

layout (set = 2, binding = 0) uniform sampler2D albedo_texture;
layout (set = 2, binding = 1) uniform sampler2D normal_texture;
layout (set = 2, binding = 2) uniform sampler2D occlusion_texture;

//#define VISUALIZE_SHADOW_MAP

vec3 calculate_pixel_normal() {
    mat3 TNB = mat3(normalize(in_tangent), normalize(in_bi_tangent), normalize(in_normal));

    // obtain normal from normal map in range [0,1]
    vec3 tangent_space_normal = texture(normal_texture, in_tex_coord).xyz;
    // transform normal vector to range [-1,1]
    tangent_space_normal = normalize(tangent_space_normal * 2.0 - 1.0);

    vec3 pixel_normal = normalize(TNB * tangent_space_normal);

    return pixel_normal;
}

vec3 reinhard_tone_mapping(vec3 radiance) {
    return radiance / (radiance + vec3(1.0));
}

float shadow_map_proj(vec4 shadow_coord)
{
    // Need larger z bias when the light is more parallel to the surface
    vec3 sunlight_direction = scene_data.sunlight_direction.xyz;
    //float bias = 3e-2 * (1.1 - max(dot(-sunlight_direction, in_normal), 0.0));

    float visibility = 1.0;
    if (0.0 < shadow_coord.z && shadow_coord.z < 1.0)
    {
        float z = texture(shadow_map, shadow_coord.xy).r;
        if (shadow_coord.w > 0.0 && z < shadow_coord.z /**- bias*/)
        {
            visibility = 0.0;
        }
    }
    return visibility;
}

// percentage closer filter
float PCF(vec4 shadow_coord) {
    const float scale = 0.5;
    const int filter_range = 4;
    const int filter_size = filter_range * 2 + 1;
    const int filer_size_square = filter_size * filter_size;

    ivec2 tex_dim = textureSize(shadow_map, 0);
    vec2 delta = scale * vec2(1.0) / tex_dim;

    float visibility = 0.0;
    for (int x = -filter_range; x <= filter_range; ++x) {
        for (int y = -filter_range; y <= filter_range; ++y) {
            vec2 offset = vec2(x, y) * delta;
            visibility += shadow_map_proj(shadow_coord + vec4(offset, 0.0, 0.0));
        }
    }
    return visibility / filer_size_square;
}

void main()
{
    const bool is_shadow_map_enabled = shadow_mode == 1;
    float visibility = is_shadow_map_enabled ? PCF(in_shadow_coord / in_shadow_coord.w) : 1.0;

    #ifdef VISUALIZE_SHADOW_MAP

    vec4 shadow_coord = in_shadow_coord / in_shadow_coord.w;
    float color = texture(shadow_map, shadow_coord.xy).r;
    out_frag_color = vec4(color, color, color, 1.0);

    #else

    float ambient_occlusion = texture(occlusion_texture, in_tex_coord).r;

    vec3 sunlight_direction = scene_data.sunlight_direction.xyz;
    vec3 sunlight_color = scene_data.sunlight_color.xyz * scene_data.sunlight_color.w;

    vec3 albedo = texture(albedo_texture, in_tex_coord).xyz;
    vec3 normal = calculate_pixel_normal();

    // lighting
    float ambient_strength = scene_data.sunlight_direction.w;
    vec3 ambient = albedo * ambient_occlusion * ambient_strength;
    vec3 diffuse = albedo * sunlight_color * max(dot(-sunlight_direction, in_normal), 0.0);

    vec3 li = ambient + diffuse * visibility;

    vec3 color = reinhard_tone_mapping(li);

    out_frag_color = vec4(color, 1.0f);

    #endif

}
