#version 460

layout (location = 0) in vec2 in_tex_coord;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec3 in_tangent;
layout (location = 3) in vec3 in_bi_tangent;

layout (location = 0) out vec4 out_frag_color;

layout (set = 0, binding = 1) uniform SceneData {
    vec4 sunlight_direction; // w is used for ambient strength
    vec4 sunlight_color;
} scene_data;

layout (set = 2, binding = 0) uniform sampler2D albedo_texture;
layout (set = 2, binding = 1) uniform sampler2D normal_texture;
layout (set = 2, binding = 2) uniform sampler2D occlusion_texture;

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

void main()
{
    float ambient_occlusion = texture(occlusion_texture, in_tex_coord).r;

    vec3 sunlight_direction = scene_data.sunlight_direction.xyz;
    vec3 sunlight_color = scene_data.sunlight_color.xyz * scene_data.sunlight_color.w;

    vec3 albedo = texture(albedo_texture, in_tex_coord).xyz;
    vec3 normal = calculate_pixel_normal();

    // lighting
    float ambient_strength = scene_data.sunlight_direction.w;
    vec3 ambient = albedo * ambient_occlusion * ambient_strength;
    vec3 diffuse = albedo * sunlight_color * max(dot(sunlight_direction, normal), 0.0);

    vec3 li = ambient + diffuse;

    vec3 color = reinhard_tone_mapping(li);

    out_frag_color = vec4(color, 1.0f);
}
