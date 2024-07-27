#ifndef CHARLIE3D_DRAW_DATA_GLSL
#define CHARLIE3D_DRAW_DATA_GLSL

#extension GL_EXT_buffer_reference: require
#extension GL_EXT_scalar_block_layout: require
#extension GL_EXT_nonuniform_qualifier: require

// All draws in the scene before culling
struct Draw {
    int vertex_offset;
    uint index_count;
    uint index_offset;
    uint material_index;
    uint node_index;
};

layout (buffer_reference, scalar) readonly restrict buffer DrawsBuffer {
    Draw draws[];
};

#endif // CHARLIE3D_DRAW_DATA_GLSL