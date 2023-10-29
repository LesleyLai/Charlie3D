#version 460

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec2 in_tex_coord;
layout (location = 3) in vec4 in_tangent;

#include "camera.h.glsl"
#include "scene_data.h.glsl"
#include "object_data.h.glsl"

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
    mat4 transform_matrix = camera.view_proj * model;
    gl_Position = transform_matrix * vec4(in_position, 1.0f);
    out_tex_coord = in_tex_coord;

    out_normal = normalize(inverse(mat3(model)) * in_normal);
    out_tangent = normalize(mat3(model) * in_tangent.xyz);
    out_bi_tangent = cross(out_normal, out_tangent) * in_tangent.w;

    out_shadow_coord = bias_mat * scene_data.sunlight_view_proj * model * vec4(in_position, 1.0);
}
