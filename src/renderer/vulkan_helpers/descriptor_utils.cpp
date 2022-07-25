#include "descriptor_utils.hpp"

#include "context.hpp"
#include "descriptor_pool.hpp"

#include <beyond/utils/assert.hpp>

namespace {

[[nodiscard]] auto
create_pool(vkh::Context& context,
            const vkh::DescriptorAllocator::PoolSizes& pool_sizes,
            std::uint32_t count, VkDescriptorPoolCreateFlags flags)
    -> vkh::Expected<VkDescriptorPool>
{
  std::vector<VkDescriptorPoolSize> sizes;
  sizes.reserve(pool_sizes.sizes.size());
  for (auto [type, weight] : pool_sizes.sizes) {
    sizes.push_back({type, uint32_t(weight * count)});
  }

  return vkh::create_descriptor_pool(
      context, vkh::DescriptorPoolCreateInfo{
                   .flags = flags, .max_sets = count, .pool_sizes = sizes});
}

} // anonymous namespace

namespace vkh {

DescriptorAllocator::DescriptorAllocator(Context& context) : context_{&context}
{}

DescriptorAllocator::~DescriptorAllocator()
{
  if (context_) {
    // delete every pool held
    for (VkDescriptorPool p : free_pools_) {
      vkDestroyDescriptorPool(context_->device(), p, nullptr);
    }
    for (VkDescriptorPool p : used_pools_) {
      vkDestroyDescriptorPool(context_->device(), p, nullptr);
    }
  }
}

void DescriptorAllocator::reset_pools()
{
  BEYOND_ENSURE(context_ != nullptr);

  // reset all used pools and add them to the free pools
  for (auto& p : used_pools_) {
    vkResetDescriptorPool(context_->device(), p, 0);
    free_pools_.push_back(p);
  }

  // clear the used pools, since we've put them all in the free pools
  used_pools_.clear();

  // reset the current pool handle back to null
  current_pool_ = VK_NULL_HANDLE;
}

auto DescriptorAllocator::allocate(VkDescriptorSetLayout layout)
    -> Expected<VkDescriptorSet>
{
  BEYOND_ENSURE(context_ != nullptr);
  auto& context = *context_;

  // initialize the current_pool_ handle if it's null
  if (current_pool_ == VK_NULL_HANDLE) {
    current_pool_ = grab_pool();
    used_pools_.push_back(current_pool_);
  }

  const VkDescriptorSetAllocateInfo alloc_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = current_pool_,
      .descriptorSetCount = 1,
      .pSetLayouts = &layout};

  // try to allocate the descriptor set
  VkDescriptorSet set{};
  VkResult alloc_result =
      vkAllocateDescriptorSets(context.device(), &alloc_info, &set);

  switch (alloc_result) {
  case VK_SUCCESS:
    // all good, return
    return set;
  case VK_ERROR_FRAGMENTED_POOL:
  case VK_ERROR_OUT_OF_POOL_MEMORY:
    // Need reallocation, continue
    break;
  default:
    // unrecoverable error
    return Expected<VkDescriptorSet>{beyond::unexpect, alloc_result};
  }

  // allocate a new pool and retry
  current_pool_ = grab_pool();
  used_pools_.push_back(current_pool_);

  alloc_result = vkAllocateDescriptorSets(context.device(), &alloc_info, &set);
  // if it still fails then we have big issues
  if (alloc_result == VK_SUCCESS) { return set; }

  return Expected<VkDescriptorSet>{beyond::unexpect, alloc_result};
}

auto DescriptorAllocator::grab_pool() -> VkDescriptorPool
{
  if (free_pools_.empty()) {
    return create_pool(*context_, descriptor_sizes_, 1000, 0).value();
  } else { // there are reusable pools availible
           // grab pool from the back of the vector and remove it from there.
    VkDescriptorPool pool = free_pools_.back();
    free_pools_.pop_back();
    return pool;
  }
}

DescriptorLayoutCache::DescriptorLayoutCache(VkDevice device) : device_{device}
{}

DescriptorLayoutCache::~DescriptorLayoutCache()
{
  // delete every descriptor layout held
  for (const auto& pair : layout_cache_) {
    vkDestroyDescriptorSetLayout(device_, pair.second, nullptr);
  }
}

auto DescriptorLayoutCache::create_descriptor_layout(
    const VkDescriptorSetLayoutCreateInfo& info) -> VkDescriptorSetLayout
{
  DescriptorLayoutInfo layoutinfo;
  layoutinfo.bindings.resize(info.bindingCount);
  std::copy(info.pBindings, info.pBindings + info.bindingCount,
            layoutinfo.bindings.begin());

  // sort the bindings if they aren't in order
  if (const bool is_sorted = std::ranges::is_sorted(
          layoutinfo.bindings, {}, &VkDescriptorSetLayoutBinding::binding);
      !is_sorted) {
    std::ranges::sort(layoutinfo.bindings, {},
                      &VkDescriptorSetLayoutBinding::binding);
  }

  // try to grab from cache
  if (auto it = layout_cache_.find(layoutinfo); it != layout_cache_.end()) {
    return (*it).second;
  }

  // create a new one (not found)
  VkDescriptorSetLayout layout = VK_NULL_HANDLE;
  VK_CHECK(vkCreateDescriptorSetLayout(device_, &info, nullptr, &layout));

  // add to cache
  layout_cache_[layoutinfo] = layout;
  return layout;
}

auto DescriptorLayoutCache::DescriptorLayoutInfo::operator==(
    const DescriptorLayoutInfo& other) const -> bool
{
  return std::ranges::equal(other.bindings, bindings,
                            [](const VkDescriptorSetLayoutBinding& lhs,
                               const VkDescriptorSetLayoutBinding& rhs) {
                              return lhs.binding == rhs.binding &&
                                     lhs.descriptorType != rhs.descriptorType &&
                                     lhs.descriptorCount ==
                                         rhs.descriptorCount &&
                                     lhs.stageFlags == rhs.stageFlags;
                            });
}

auto DescriptorLayoutCache::DescriptorLayoutInfo::hash() const -> std::size_t
{
  using std::hash;
  using std::size_t;

  size_t result = hash<size_t>()(bindings.size());

  for (const VkDescriptorSetLayoutBinding& b : bindings) {
    // pack the binding data into a single int64. Not fully correct but it's ok
    const size_t binding_hash = b.binding | b.descriptorType << 8 |
                                b.descriptorCount << 16 | b.stageFlags << 24;

    // shuffle the packed binding data and xor it with the main hash
    result ^= hash<size_t>()(binding_hash);
  }

  return result;
}

DescriptorBuilder::DescriptorBuilder(DescriptorLayoutCache& layout_cache,
                                     DescriptorAllocator& allocator)
    : cache_{&layout_cache}, alloc_{&allocator}
{}

auto DescriptorBuilder::bind_buffer(uint32_t binding,
                                    const VkDescriptorBufferInfo& buffer_info,
                                    VkDescriptorType type,
                                    VkShaderStageFlags stage_flags)
    -> DescriptorBuilder&
{
  bindings_.push_back(VkDescriptorSetLayoutBinding{
      .binding = binding,
      .descriptorType = type,
      .descriptorCount = 1,
      .stageFlags = stage_flags,
      .pImmutableSamplers = nullptr,
  });

  writes_.push_back(
      VkWriteDescriptorSet{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                           .dstBinding = binding,
                           .descriptorCount = 1,
                           .descriptorType = type,
                           .pBufferInfo = &buffer_info});

  return *this;
}

auto DescriptorBuilder::bind_image(uint32_t binding,
                                   const VkDescriptorImageInfo& image_info,
                                   VkDescriptorType type,
                                   VkShaderStageFlags stage_flags)
    -> DescriptorBuilder&
{
  bindings_.push_back(VkDescriptorSetLayoutBinding{
      .binding = binding,
      .descriptorType = type,
      .descriptorCount = 1,
      .stageFlags = stage_flags,
      .pImmutableSamplers = nullptr,
  });

  writes_.push_back(VkWriteDescriptorSet{
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstBinding = binding,
      .descriptorCount = 1,
      .descriptorType = type,
      .pImageInfo = &image_info,
  });

  return *this;
}

auto DescriptorBuilder::build() -> Expected<DescriptorBuilderResult>
{
  // build layout first
  const VkDescriptorSetLayoutCreateInfo layout_info{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = static_cast<std::uint32_t>(bindings_.size()),
      .pBindings = bindings_.data(),
  };

  VkDescriptorSetLayout layout = cache_->create_descriptor_layout(layout_info);

  return alloc_
      ->allocate(layout) //
      .map([&](VkDescriptorSet set) {
        // write descriptor
        for (VkWriteDescriptorSet& w : writes_) { w.dstSet = set; }

        vkUpdateDescriptorSets(alloc_->context()->device(),
                               static_cast<std::uint32_t>(writes_.size()),
                               writes_.data(), 0, nullptr);

        return DescriptorBuilderResult{layout, set};
      });
}

} // namespace vkh
