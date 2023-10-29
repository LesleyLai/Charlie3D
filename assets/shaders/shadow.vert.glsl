#version 460

layout (location = 0) in vec3 in_pos;

#include "scene_data.h.glsl"
#include "object_data.h.glsl"

out gl_PerVertex
{
    vec4 gl_Position;
};

void main()
{
    mat4 model = object_buffer.objects[gl_BaseInstance].model;
    gl_Position = scene_data.sunlight_view_proj * model * vec4(in_pos, 1.0);
}