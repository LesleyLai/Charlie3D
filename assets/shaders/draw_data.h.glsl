#ifndef CHARLIE3D_DRAW_DATA_GLSL
#define CHARLIE3D_DRAW_DATA_GLSL

#include "prelude.h.glsl"

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