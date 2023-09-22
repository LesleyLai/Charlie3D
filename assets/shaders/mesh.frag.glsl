#version 460

layout (location = 0) in vec2 inTexCoord;

layout (location = 0) out vec4 outFragColor;

layout (set = 0, binding = 1) uniform SceneData {
    vec4 sunlightDirection;
    vec4 sunlightColor;
} sceneData;

layout (set = 2, binding = 0) uniform sampler2D diffuse_texture;

vec3 reinhard_tone_mapping(vec3 radiance) {
    return radiance / (radiance + vec3(1.0));
}

void main()
{
    vec3 diffuse = texture(diffuse_texture, inTexCoord).xyz * sceneData.sunlightColor.xyz * sceneData.sunlightColor.w;
    outFragColor = vec4(reinhard_tone_mapping(diffuse), 1.0f);
}
