#pragma once

#include <map>
#include <span>
#include <vector>

#include <vulkan/vulkan.h>

#include "error_handling.hpp"

namespace vkh {

class Context;

class DescriptorAllocator {
public:
  struct PoolSizes {
    std::vector<std::pair<VkDescriptorType, float>> sizes = {
        {VK_DESCRIPTOR_TYPE_SAMPLER, 0.5f},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4.f},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 4.f},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1.f},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1.f},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1.f},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2.f},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2.f},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1.f},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1.f},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 0.5f}};
  };

private:
  Context* context_ = nullptr;
  VkDescriptorPool current_pool_ = VK_NULL_HANDLE;
  PoolSizes descriptor_sizes_;
  std::vector<VkDescriptorPool> used_pools_;
  std::vector<VkDescriptorPool> free_pools_;

public:
  explicit DescriptorAllocator(Context& context);
  ~DescriptorAllocator();
  DescriptorAllocator(const DescriptorAllocator&) = delete;
  auto operator=(const DescriptorAllocator&) & -> DescriptorAllocator& = delete;
  DescriptorAllocator(DescriptorAllocator&&) noexcept = delete;
  auto operator=(DescriptorAllocator&&) & noexcept -> DescriptorAllocator& = delete;

  void reset_pools();
  [[nodiscard]] auto allocate(VkDescriptorSetLayout layout) -> Expected<VkDescriptorSet>;

  [[nodiscard]] auto context() -> Context*
  {
    return context_;
  };

private:
  [[nodiscard]] auto grab_pool() -> VkDescriptorPool;
};

class DescriptorLayoutCache {
public:
  explicit DescriptorLayoutCache(VkDevice new_device);
  ~DescriptorLayoutCache();

  DescriptorLayoutCache(const DescriptorLayoutCache&) = delete;
  auto operator=(const DescriptorLayoutCache&) = delete;

  auto create_descriptor_layout(const VkDescriptorSetLayoutCreateInfo& info)
      -> VkDescriptorSetLayout;

  struct DescriptorLayoutInfo {
    // good idea to turn this into a inlined array
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    [[nodiscard]] auto operator==(const DescriptorLayoutInfo& other) const -> bool;

    [[nodiscard]] auto hash() const -> std::size_t;
  };

private:
  struct DescriptorLayoutHash {
    [[nodiscard]] auto operator()(const DescriptorLayoutInfo& k) const -> std::size_t
    {
      return k.hash();
    }
  };

  std::unordered_map<DescriptorLayoutInfo, VkDescriptorSetLayout, DescriptorLayoutHash>
      layout_cache_;
  VkDevice device_;
};

struct DescriptorBuilderResult {
  VkDescriptorSetLayout layout;
  VkDescriptorSet set;
};

class DescriptorBuilder {
  std::vector<VkWriteDescriptorSet> writes_;
  std::vector<VkDescriptorSetLayoutBinding> bindings_;

  DescriptorLayoutCache* cache_ = nullptr;
  DescriptorAllocator* alloc_ = nullptr;

public:
  DescriptorBuilder(DescriptorLayoutCache& layout_cache, DescriptorAllocator& allocator);

  auto bind_buffer(uint32_t binding, const VkDescriptorBufferInfo& buffer_info,
                   VkDescriptorType type, VkShaderStageFlags stage_flags) -> DescriptorBuilder&;

  auto bind_image(uint32_t binding, const VkDescriptorImageInfo& image_info, VkDescriptorType type,
                  VkShaderStageFlags stage_flags) -> DescriptorBuilder&;

  auto build() -> Expected<DescriptorBuilderResult>;
};

} // namespace vkh
