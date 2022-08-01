#version 460

layout (location = 0) in vec3 vPosition;
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec2 vTexCoord;

layout(set = 0, binding = 0) uniform CameraBuffer {
    mat4 view;
    mat4 proj;
    mat4 viewProj;
} cameraData;

struct ObjectData{
    mat4 model;
};

layout(std140, set = 1, binding = 0) readonly buffer ObjectBuffer {
    ObjectData objects[];
} objectBuffer;

layout (location = 0) out vec2 outTexCoord;

void main()
{
    mat4 model = objectBuffer.objects[gl_BaseInstance].model;
    mat4 transformMatrix = cameraData.viewProj * model;
    gl_Position = transformMatrix * vec4(vPosition, 1.0f);
    outTexCoord = vTexCoord;
}
