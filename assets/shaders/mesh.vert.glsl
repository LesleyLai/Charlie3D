#version 460

layout (location = 0) in vec3 vPosition;
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec2 vTexCoord;
layout (location = 3) in vec4 vTangent;

layout (set = 0, binding = 0) uniform CameraBuffer {
    mat4 view;
    mat4 proj;
    mat4 viewProj;
} cameraData;

struct ObjectData {
    mat4 model;
};

layout (std140, set = 1, binding = 0) readonly buffer ObjectBuffer {
    ObjectData objects[];
} objectBuffer;

layout (location = 0) out vec2 outTexCoord;
layout (location = 1) out vec3 outNormal;
layout (location = 2) out vec3 outTangent;
layout (location = 3) out vec3 outBiTangent;

void main()
{
    mat4 model = objectBuffer.objects[gl_BaseInstance].model;
    mat4 transformMatrix = cameraData.viewProj * model;
    gl_Position = transformMatrix * vec4(vPosition, 1.0f);
    outTexCoord = vTexCoord;

    outNormal = normalize(inverse(mat3(model)) * vNormal);
    outTangent = normalize(mat3(model) * vTangent.xyz);
    outBiTangent = cross(vNormal, outTangent) * vTangent.w;
}
