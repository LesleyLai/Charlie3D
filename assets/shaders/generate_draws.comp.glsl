#version 460

#extension GL_EXT_buffer_reference: require
#extension GL_EXT_scalar_block_layout: require

struct IndirectCommand {
    int index_count;
    int instance_count;
    int first_index;
    int vertex_offset;
    int first_instance;
};

layout (buffer_reference, scalar) readonly restrict buffer InIndirectBuffer {
    IndirectCommand draws[];
};

layout (buffer_reference, scalar) writeonly restrict buffer OutIndirectBuffer {
    IndirectCommand draws[];
};

layout (push_constant) uniform constants
{
    InIndirectBuffer initial_draws_buffer;
    OutIndirectBuffer final_draws_buffer;
    uint total_draw_count;
};

layout (local_size_x = 256, local_size_y = 1, local_size_z = 1) in;
void main() {
    uint index = gl_GlobalInvocationID.x;
    if (index >= total_draw_count) {
        return;
    }

    IndirectCommand draw = initial_draws_buffer.draws[index];

    final_draws_buffer.draws[index] = draw;

}