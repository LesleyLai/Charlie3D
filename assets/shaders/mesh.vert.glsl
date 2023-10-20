#version 460

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec2 in_tex_coord;
layout (location = 3) in vec4 in_tangent;

layout (set = 0, binding = 0) uniform CameraBuffer {
    mat4 view;
    mat4 proj;
    mat4 view_proj;
} camera_data;

layout (std140, set = 0, binding = 1) uniform SceneData {
    vec4 sunlight_direction; // w is used for ambient strength
    vec4 sunlight_color;
    mat4 sunlight_view_proj;
} scene_data;

struct ObjectData {
    mat4 model;
};

layout (std140, set = 1, binding = 0) readonly buffer ObjectBuffer {
    ObjectData objects[];
} object_buffer;

layout (location = 0) out vec2 out_tex_coord;
layout (location = 1) out vec3 out_normal;
layout (location = 2) out vec3 out_tangent;
layout (location = 3) out vec3 out_bi_tangent;
layout (location = 4) out vec4 out_shadow_coord;

const mat4 bias_mat = mat4(
    0.5, 0.0, 0.0, 0.0,
    0.0, 0.5, 0.0, 0.0,
    0.0, 0.0, 1.0, 0.0,
    0.5, 0.5, 0.0, 1.0);

void main()
{
    mat4 model = object_buffer.objects[gl_BaseInstance].model;
    mat4 transform_matrix = camera_data.view_proj * model;
    gl_Position = transform_matrix * vec4(in_position, 1.0f);
    out_tex_coord = in_tex_coord;

    out_normal = normalize(inverse(mat3(model)) * in_normal);
    out_tangent = normalize(mat3(model) * in_tangent.xyz);
    out_bi_tangent = cross(out_normal, out_tangent) * in_tangent.w;

    out_shadow_coord = bias_mat * scene_data.sunlight_view_proj * model * vec4(in_position, 1.0);
}
