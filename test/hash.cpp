#include <catch2/catch_test_macros.hpp>

#include "../Charlie/renderer/sampler_cache.hpp"

TEST_CASE("VkSamplerCreateInfo hash")
{
  const VkSamplerCreateInfo sampler_info = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                            .magFilter = VK_FILTER_LINEAR,
                                            .minFilter = VK_FILTER_LINEAR,
                                            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                                            .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                            .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                            .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                            .maxLod = VK_LOD_CLAMP_NONE};
  [[maybe_unused]]
  const auto hash = std::hash<VkSamplerCreateInfo>{}(sampler_info);
}