#ifndef CHARLIE3D_SCENE_DATA_GLSL
#define CHARLIE3D_SCENE_DATA_GLSL

layout (std140, set = 0, binding = 1) uniform SceneData {
    vec4 sunlight_direction; // w is used for ambient strength
    vec4 sunlight_color;
    mat4 sunlight_view_proj;

    uint sunlight_shadow_mode;
} scene_data;

#endif