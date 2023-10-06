#version 460

layout (location = 0) in vec2 inTexCoord;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec3 inTangent;
layout (location = 3) in vec3 inBiTangent;

layout (location = 0) out vec4 outFragColor;

layout (set = 0, binding = 1) uniform SceneData {
    vec4 sunlightDirection;
    vec4 sunlightColor;
} sceneData;

layout (set = 2, binding = 0) uniform sampler2D albedoTexture;
layout (set = 2, binding = 1) uniform sampler2D normalTexture;

vec3 calculate_pixel_normal() {
    mat3 TNB = mat3(normalize(inTangent), normalize(inBiTangent), normalize(inNormal));

    // obtain normal from normal map in range [0,1]
    vec3 tangentSpaceNormal = texture(normalTexture, inTexCoord).xyz;
    // transform normal vector to range [-1,1]
    tangentSpaceNormal = normalize(tangentSpaceNormal * 2.0 - 1.0);

    vec3 pixelNormal = normalize(TNB * tangentSpaceNormal);

    return pixelNormal;
}

vec3 reinhard_tone_mapping(vec3 radiance) {
    return radiance / (radiance + vec3(1.0));
}

void main()
{
    vec3 sunlightDirection = sceneData.sunlightDirection.xyz;
    vec3 sunlightColor = sceneData.sunlightColor.xyz * sceneData.sunlightColor.w;

    vec3 albedo = texture(albedoTexture, inTexCoord).xyz;
    vec3 normal = calculate_pixel_normal();

    // lighting
    vec3 ambient = albedo * sceneData.sunlightColor.w * vec3(0.05);
    vec3 diffuse = albedo * sunlightColor * max(dot(sunlightDirection, normal), 0.0);

    vec3 li = ambient + diffuse;

    vec3 color = reinhard_tone_mapping(li);

    outFragColor = vec4(color, 1.0f);
}
