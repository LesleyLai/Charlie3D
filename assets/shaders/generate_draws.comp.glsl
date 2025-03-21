#version 460

#include "prelude.h.glsl"
#include "draw_data.h.glsl"

// Output: a draw indirect buffer that can directly feed into the GPU
struct IndirectCommand {
    uint index_count;
    uint instance_count;
    uint first_index;
    int vertex_offset;
    uint first_instance;
};

layout (buffer_reference, scalar) writeonly restrict buffer DrawIndirectBuffer {
    IndirectCommand draws[];
};

layout (push_constant) uniform constants
{
    DrawsBuffer in_draws_buffer;
    DrawIndirectBuffer draw_indirect_buffer;
    uint total_draw_count;
};


layout (local_size_x = 256, local_size_y = 1, local_size_z = 1) in;
void main() {
    uint index = gl_GlobalInvocationID.x;
    if (index >= total_draw_count) {
        return;
    }

    Draw draw = in_draws_buffer.draws[index];

    IndirectCommand indirect_command;
    indirect_command.index_count = draw.index_count;
    indirect_command.instance_count = 1;
    indirect_command.first_index = draw.index_offset;
    indirect_command.vertex_offset = draw.vertex_offset;
    indirect_command.first_instance = index; // Used to get the original draw index

    draw_indirect_buffer.draws[index] = indirect_command;

}