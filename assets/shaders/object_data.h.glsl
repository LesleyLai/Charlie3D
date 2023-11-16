struct ObjectData {
    mat4 model;
};

layout (std430, set = 1, binding = 0) readonly buffer ObjectBuffer {
    ObjectData objects[];
} object_buffer;

layout (std430, set = 1, binding = 1) readonly buffer MaterialId {
    int material_ids[];
} material_index_buffer;