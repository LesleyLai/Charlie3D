#version 460

layout (location = 0) in vec2 in_tex_coord;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec3 in_tangent;
layout (location = 3) in vec3 in_bi_tangent;
layout (location = 4) in vec4 in_shadow_coord;

//layout (constant_id = 0) const int shadow_mode = 0;

layout (location = 0) out vec4 out_frag_color;

#include "scene_data.h.glsl"

layout (set = 0, binding = 2) uniform sampler2D shadow_map;

layout (set = 2, binding = 0) uniform sampler2D albedo_texture;
layout (set = 2, binding = 1) uniform sampler2D normal_texture;
layout (set = 2, binding = 2) uniform sampler2D occlusion_texture;

// #define VISUALIZE_SHADOW_MAP
#define NUM_SAMPLES 16
const vec2 poisson_disk_16[NUM_SAMPLES] = vec2[](
    vec2(-0.680088, -0.731923),
    vec2(-0.957909, -0.247622),
    vec2(0.0948045, -0.992508),
    vec2(-0.316418, -0.93561),
    vec2(0.508091, -0.270309),
    vec2(0.986855, -0.161122),
    vec2(-0.0783372, -0.377044),
    vec2(0.678299, -0.730012),
    vec2(0.264303, 0.150586),
    vec2(0.375585, 0.926198),
    vec2(0.869216, 0.485342),
    vec2(-0.0303609, 0.582547),
    vec2(-0.456659, 0.886469),
    vec2(-0.0502123, 0.998242),
    vec2(-0.522737, 0.0698312),
    vec2(-0.857891, 0.512805)
);

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
    //float bias = 1e-3 * (1.1 - max(dot(-sunlight_direction, in_normal), 0.0));

    float visibility = 1.0;
    if (0.0 < shadow_coord.z && shadow_coord.z < 1.0)
    {
        float z = texture(shadow_map, shadow_coord.xy).r;
        if (shadow_coord.w > 0.0 && z < shadow_coord.z/** - bias*/)
        {
            visibility = 0.0;
        }
    }
    return visibility;
}

// percentage closer filter
float PCF(vec4 shadow_coord, float scale) {
    ivec2 tex_dim = textureSize(shadow_map, 0);
    vec2 delta = scale * vec2(1.0) / tex_dim;

    float visibility = 0.0;

    for (int i = 0; i < NUM_SAMPLES; ++i) {
        vec2 offset = poisson_disk_16[i] * delta;
        visibility += shadow_map_proj(shadow_coord + vec4(offset, 0.0, 0.0));
    }
    return visibility / float(NUM_SAMPLES);
}

// Calculate the average depth of occluders
// return false if no blocker
bool estimate_blocker_depth(vec2 uv, float z_receiver, out float blocker_depth) {
    int blocker_count = 0;
    float blocker_depth_sum = 0.0;

    ivec2 tex_dim = textureSize(shadow_map, 0);
    vec2 delta = vec2(1.0) / tex_dim * 20.0;

    for (int i = 0; i < NUM_SAMPLES; i++) {
        vec2 sample_uv = uv + poisson_disk_16[i] * delta;
        float depth_on_shadow_map = texture(shadow_map, sample_uv).r;
        if (depth_on_shadow_map /**+ 1e-3*/ < z_receiver) {
            blocker_count++;
            blocker_depth_sum += depth_on_shadow_map;
        }
    }

    // No blockers
    if (blocker_count == 0)  return false;

    blocker_depth = blocker_depth_sum / float(blocker_count);
    return true;
}

const float light_width = 50.0f;
float PCSS(vec4 shadow_coord) {
    float blocker_depth;
    bool has_blocker = estimate_blocker_depth(shadow_coord.xy, shadow_coord.z, blocker_depth);
    if (!has_blocker) { return 1.0; } // Fully visible if no blockers

    float penumbra_size = (shadow_coord.z - blocker_depth) * light_width / blocker_depth;
    return PCF(shadow_coord, penumbra_size);
}

void main()
{
    //const bool is_shadow_map_enabled = shadow_mode == 1;
    //float visibility = is_shadow_map_enabled ? PCF(in_shadow_coord / in_shadow_coord.w) : 1.0;
    //float visibility = PCF(in_shadow_coord / in_shadow_coord.w, 1.0);
    float visibility = PCSS(in_shadow_coord / in_shadow_coord.w);

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
