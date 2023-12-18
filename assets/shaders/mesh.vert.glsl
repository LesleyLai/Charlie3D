#version 460

#extension GL_EXT_buffer_reference: require
#extension GL_EXT_scalar_block_layout: require

layout (set = 0, binding = 0) uniform CameraBuffer {
    mat4 view;
    mat4 proj;
    mat4 view_proj;
    vec3 position;
} camera;

layout (buffer_reference, scalar) readonly buffer PositionBuffer {
    vec3 position[];
};

struct Vertex {
    vec3 normal;
    vec2 tex_coord;
    vec4 tangent;
};

layout (buffer_reference, std430) readonly buffer VertexBuffer {
    Vertex vertices[];
};


layout (push_constant) uniform constants
{
    PositionBuffer position_buffer;
    VertexBuffer vertex_buffer;
} PushConstants;

#include "scene_data.h.glsl"
#include "object_data.h.glsl"

 layout (location = 0) out vec3 out_world_pos;
layout (location = 1) out vec2 out_tex_coord;
layout (location = 2) out vec3 out_normal;
layout (location = 3) out vec3 out_tangent;
layout (location = 4) out vec3 out_bi_tangent;
layout (location = 5) out vec4 out_shadow_coord;
layout (location = 6) out flat int out_material_index;

const mat4 light_space_to_NDC = mat4(
    0.5, 0.0, 0.0, 0.0,
    0.0, 0.5, 0.0, 0.0,
    0.0, 0.0, 1.0, 0.0,
    0.5, 0.5, 0.0, 1.0);

void main()
{
    vec3 in_position = PushConstants.position_buffer.position[gl_VertexIndex];
    Vertex in_vertex = PushConstants.vertex_buffer.vertices[gl_VertexIndex];
    vec3 in_normal = in_vertex.normal;
    vec2 in_tex_coord = in_vertex.tex_coord;
    vec4 in_tangent = in_vertex.tangent;

    mat4 model = object_buffer.objects[gl_BaseInstance].model;
    mat4 transform_matrix = camera.view_proj * model;
    vec4 world_pos = camera.view * model * vec4(in_position, 1.0f);
    out_world_pos = (world_pos / world_pos.w).xyz;
    gl_Position = transform_matrix * vec4(in_position, 1.0f);
    out_tex_coord = in_tex_coord;

    out_normal = normalize(inverse(mat3(model)) * in_normal);
    out_tangent = normalize(mat3(model) * in_tangent.xyz);
    out_bi_tangent = cross(out_normal, out_tangent) * in_tangent.w;

    out_shadow_coord = light_space_to_NDC * scene_data.sunlight_view_proj * model * vec4(in_position, 1.0);

    out_material_index = material_index_buffer.material_ids[gl_BaseInstance];
}
