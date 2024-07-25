struct ObjectData {
    mat4 model;
};

layout (std430, set = 1, binding = 0) readonly restrict buffer ObjectBuffer {
    ObjectData objects[];
} object_buffer;