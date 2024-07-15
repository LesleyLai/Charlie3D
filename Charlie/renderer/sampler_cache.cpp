#include "sampler_cache.hpp"

#include "../vulkan_helpers/debug_utils.hpp"

#include <volk.h>

namespace charlie {

SamplerCache::SamplerCache(VkDevice device) : device_{device}
{
  static constexpr VkSamplerCreateInfo default_sampler_info = {
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .magFilter = VK_FILTER_LINEAR,
      .minFilter = VK_FILTER_LINEAR,
      .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
      .maxLod = VK_LOD_CLAMP_NONE};
  default_sampler_ = create_sampler(default_sampler_info);
  VK_CHECK(vkh::set_debug_name(device, default_sampler_, "Default Sampler"));
}

SamplerCache::~SamplerCache()
{
  for (auto pair : map_) { vkDestroySampler(device_, pair.second, nullptr); }
}

auto SamplerCache::create_sampler(const VkSamplerCreateInfo& create_info) -> VkSampler
{
  if (const auto itr = map_.find(create_info); itr != map_.end()) {
    return itr->second;
  } else {
    VkSampler sampler = VK_NULL_HANDLE;
    VK_CHECK(vkCreateSampler(device_, &create_info, nullptr, &sampler));
    map_.emplace(create_info, sampler);
    return sampler;
  }
}
auto SamplerCache::default_sampler() const -> VkSampler
{
  return default_sampler_;
}

} // namespace charlie