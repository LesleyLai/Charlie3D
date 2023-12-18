#version 460
#extension GL_EXT_nonuniform_qualifier: require

#include "scene_data.h.glsl"
#include "pbr.h.glsl"

layout (location = 0) in vec3 in_world_pos;
layout (location = 1) in vec2 in_tex_coord;
layout (location = 2) in vec3 in_normal;
layout (location = 3) in vec3 in_tangent;
layout (location = 4) in vec3 in_bi_tangent;
layout (location = 5) in vec4 in_shadow_coord;
layout (location = 6) in flat int in_material_index;

layout (constant_id = 0) const int shadow_mode = 0;

layout (location = 0) out vec4 out_frag_color;

layout (set = 0, binding = 2) uniform sampler2D shadow_map;

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
layout (std430, set = 2, binding = 0) readonly buffer MaterialBuffer {
    Material materials[];
};

layout (set = 3, binding = 10) uniform sampler2D global_textures[];


// #define VISUALIZE_SHADOW_MAP
#define SHADOW_SAMPLE_COUNT 16
const vec2 poisson_disk_16[SHADOW_SAMPLE_COUNT] = vec2[](
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

float shadow_map_proj(vec4 shadow_coord)
{
    if (0.0 < shadow_coord.z && shadow_coord.z < 1.0)
    {
        float z = texture(shadow_map, shadow_coord.xy).r;
        if (shadow_coord.w > 0.0 && z < shadow_coord.z)
        {
            return 0.0;
        }
    }
    return 1.0;
}


// Returns a uniform random number from [0, 1] base on a vec4
float random4(vec4 seed) {
    float dot_product = dot(seed, vec4(12.9898, 78.233, 45.164, 94.673));
    return fract(sin(dot_product) * 43758.5453);
}

mat2 rotation2d(float angle) {
    float s = sin(angle);
    float c = cos(angle);

    return mat2(
        c, -s,
        s, c
    );
}


// percentage closer filter
float shadow_PCF(vec4 shadow_coord, vec2 texel_size, float scale) {
    float visibility = 0.0;
    for (int i = 0; i < SHADOW_SAMPLE_COUNT; ++i) {
        int index = int(random4(vec4(shadow_coord.xyz, i)) * 16.0) % 16;
        float rotate = random4(vec4(shadow_coord.xyz, i)) * 3.1415926 * 2.0;

        vec2 offset = poisson_disk_16[i] * scale * texel_size;
        offset = rotation2d(rotate) * offset;
        visibility += shadow_map_proj(shadow_coord + vec4(offset, 0.0, 0.0));
    }
    return visibility / float(SHADOW_SAMPLE_COUNT);
}

// Calculate the average depth of occluders
// return false if no blocker
bool estimate_blocker_depth(vec2 uv, float z_receiver, out float z_blocker) {
    int blocker_count = 0;
    float blocker_depth_sum = 0.0;

    ivec2 tex_dim = textureSize(shadow_map, 0);
    vec2 delta = vec2(1.0) / tex_dim * 5.0;

    for (int i = 0; i < 4; i++) {
        int index = int(random4(vec4(uv, z_receiver, i)) * 16.0) % 16;
        vec2 offset = poisson_disk_16[index] * delta;
        float depth_on_shadow_map = texture(shadow_map, uv + offset).r;
        if (depth_on_shadow_map < z_receiver) {
            blocker_count++;
            blocker_depth_sum += depth_on_shadow_map;
        }
    }

    // No blockers
    if (blocker_count == 0)  return false;

    z_blocker = blocker_depth_sum / float(blocker_count);
    return true;
}

float shadow_PCSS(vec4 shadow_coord, vec2 texel_size, float light_size) {
    float z_receiver = shadow_coord.z;

    float z_blocker;
    bool has_blocker = estimate_blocker_depth(shadow_coord.xy, z_receiver, z_blocker);
    if (!has_blocker) { return 1.0; } // Fully visible if no blockers

    float penumbra_size = (z_receiver - z_blocker) * light_size / z_blocker;
    return shadow_PCF(shadow_coord, texel_size, penumbra_size);
}

// Returns a visibility between [0, 1]
float shadow_mapping() {
    ivec2 shadow_map_size = textureSize(shadow_map, 0);
    vec2 shadow_texel_size = vec2(1.0) / shadow_map_size;

    //return shadow_map_proj(in_shadow_coord);
    //return shadow_PCF(in_shadow_coord, shadow_map_size, 0.0);
    const float light_size = 100.0f;
    return shadow_PCSS(in_shadow_coord, shadow_texel_size, light_size);
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
    if (albedo.a < 0.1) {
        discard;
    }
    vec3 base_color = albedo.rgb;

    vec2 metallic_roughness = texture(global_textures[nonuniformEXT(material.metallic_roughness_texture_index)], in_tex_coord).bg;
    float metallic = metallic_roughness.r * material.metallic_factor;
    float perceptual_roughness = metallic_roughness.g * material.roughness_factor;

    //vec3 normal = calculate_pixel_normal();
    vec3 normal = in_normal;

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
    if (shadow_mode == 1) {
        visibility = shadow_mapping();
    }

    vec3 Lo = ambient + luminance * visibility;

    vec3 color = reinhard_tone_mapping(Lo);
    out_frag_color = vec4(color, 1.0f);

    #endif
}
