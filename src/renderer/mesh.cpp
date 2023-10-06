#include "mesh.hpp"

namespace charlie {

void destroy_mesh(vkh::Context& context, const Mesh& mesh)
{
  vkh::destroy_buffer(context, mesh.position_buffer);
  vkh::destroy_buffer(context, mesh.normal_buffer);
  vkh::destroy_buffer(context, mesh.uv_buffer);
  vkh::destroy_buffer(context, mesh.tangent_buffer);
  vkh::destroy_buffer(context, mesh.index_buffer);
}

} // namespace charlie
