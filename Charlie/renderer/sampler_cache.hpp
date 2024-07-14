#ifndef CHARLIE3D_SAMPLER_CACHE_HPP
#define CHARLIE3D_SAMPLER_CACHE_HPP

#include "../utils/prelude.hpp"
#include "../vulkan_helpers/error_handling.hpp"

// #include <vulkan/vulkan_core.h>

#include <volk.h>

#include <beyond/utils/copy_move.hpp>
#include <beyond/utils/hash.hpp>

#include <unordered_map>

namespace std {

template <> struct hash<VkSamplerCreateInfo> {
  [[nodiscard]] auto
  operator()(const VkSamplerCreateInfo& create_info) const noexcept -> std::size_t
  {
    BEYOND_ENSURE(create_info.sType == VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO);
    BEYOND_ENSURE_MSG(create_info.pNext == nullptr,
                      "VkSamplerCreateInfo with a pNext is currently not supported");

    return beyond::hash_combine(
        create_info.flags, create_info.magFilter, create_info.minFilter, create_info.mipmapMode,
        create_info.addressModeU, create_info.addressModeV, create_info.addressModeW,
        create_info.mipLodBias, create_info.anisotropyEnable, create_info.maxAnisotropy,
        create_info.compareEnable, create_info.compareOp, create_info.minLod, create_info.maxLod,
        create_info.borderColor, create_info.unnormalizedCoordinates);
  }
};

} // namespace std

namespace charlie {

struct SamplerEqualTo {
  [[nodiscard]]
  auto operator()(const VkSamplerCreateInfo& create_info1,
                  const VkSamplerCreateInfo& create_info2) const -> bool
  {
    BEYOND_ASSERT(create_info1.sType == VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO);
    BEYOND_ASSERT(create_info2.sType == VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO);
    BEYOND_ENSURE_MSG(create_info1.pNext == nullptr,
                      "VkSamplerCreateInfo with a pNext is currently not supported");
    BEYOND_ENSURE_MSG(create_info2.pNext == nullptr,
                      "VkSamplerCreateInfo with a pNext is currently not supported");

    return create_info1.flags == create_info2.flags &&
           create_info1.magFilter == create_info2.magFilter &&
           create_info1.minFilter == create_info2.minFilter &&
           create_info1.mipmapMode == create_info2.mipmapMode &&
           create_info1.addressModeU == create_info2.addressModeU &&
           create_info1.addressModeV == create_info2.addressModeV &&
           create_info1.addressModeW == create_info2.addressModeW &&
           create_info1.mipLodBias == create_info2.mipLodBias &&
           create_info1.anisotropyEnable == create_info2.anisotropyEnable &&
           create_info1.maxAnisotropy == create_info2.maxAnisotropy &&
           create_info1.compareEnable == create_info2.compareEnable &&
           create_info1.compareOp == create_info2.compareOp &&
           create_info1.minLod == create_info2.minLod &&
           create_info1.maxLod == create_info2.maxLod &&
           create_info1.borderColor == create_info2.borderColor &&
           create_info1.unnormalizedCoordinates == create_info2.unnormalizedCoordinates;
  }
};

class SamplerCache {
  VkDevice device_ = nullptr;
  std::unordered_map<VkSamplerCreateInfo, VkSampler, std::hash<VkSamplerCreateInfo>, SamplerEqualTo>
      map_;

public:
  explicit SamplerCache(VkDevice device) : device_{device} {}
  ~SamplerCache()
  {
    for (auto pair : map_) { vkDestroySampler(device_, pair.second, nullptr); }
  }

  BEYOND_DELETE_COPY(SamplerCache);
  BEYOND_DELETE_MOVE(SamplerCache);

  [[nodiscard]] auto create_sampler(const VkSamplerCreateInfo& create_info) -> VkSampler
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
};

} // namespace charlie

#endif // CHARLIE3D_SAMPLER_CACHE_HPP
