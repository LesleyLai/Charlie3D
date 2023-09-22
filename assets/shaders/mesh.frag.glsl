#version 460

layout (location = 0) in vec2 inTexCoord;
layout (location = 1) in vec3 inNormal;

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

    vec3 sunlightDirection = sceneData.sunlightDirection.xyz;
    vec3 sunlightColor = sceneData.sunlightColor.xyz * sceneData.sunlightColor.w;

    vec3 ambient = sceneData.sunlightColor.w * vec3(0.1);

    vec3 diffuseColor = texture(diffuse_texture, inTexCoord).xyz;
    vec3 diffuse = diffuseColor * sunlightColor * max(dot(sunlightDirection, inNormal), 0.0);

    vec3 color = ambient + diffuse;

    outFragColor = vec4(reinhard_tone_mapping(color), 1.0f);
}
