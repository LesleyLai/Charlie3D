#version 460

layout (location = 0) in vec3 in_pos;

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

out gl_PerVertex
{
    vec4 gl_Position;
};


void main()
{
    mat4 model = object_buffer.objects[gl_BaseInstance].model;
    gl_Position = scene_data.sunlight_view_proj * model * vec4(in_pos, 1.0);
}