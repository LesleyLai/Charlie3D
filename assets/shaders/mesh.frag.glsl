#version 460

layout (location = 0) in vec2 inTexCoord;

layout (location = 0) out vec4 outFragColor;

layout (set = 0, binding = 1) uniform SceneData {
    vec4 sunlightDirection;
    vec4 sunlightColor;
} sceneData;

layout (set = 2, binding = 0) uniform sampler2D diffuse_texture;

void main()
{
    vec3 diffuse = texture(diffuse_texture, inTexCoord).xyz * sceneData.sunlightColor.xyz * sceneData.sunlightColor.w;
    outFragColor = vec4(diffuse, 1.0f);
}
