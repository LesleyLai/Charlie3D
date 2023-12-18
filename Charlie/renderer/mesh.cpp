#include "mesh.hpp"

namespace charlie {

void destroy_submesh(vkh::Context& context, SubMesh& submesh)
{
  vkh::destroy_buffer(context, submesh.position_buffer);
  vkh::destroy_buffer(context, submesh.vertex_buffer);
  vkh::destroy_buffer(context, submesh.index_buffer);
}

} // namespace charlie
