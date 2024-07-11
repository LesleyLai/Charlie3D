#include "mesh.hpp"

namespace charlie {

void destroy_mesh(vkh::Context& context, Mesh& mesh)
{
  vkh::destroy_buffer(context, mesh.position_buffer);
  vkh::destroy_buffer(context, mesh.vertex_buffer);
  vkh::destroy_buffer(context, mesh.index_buffer);
}

} // namespace charlie
