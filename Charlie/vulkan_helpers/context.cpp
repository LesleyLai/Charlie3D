#include "context.hpp"

#include "buffer.hpp"
#include "error_handling.hpp"

#include "VkBootstrap.h"
#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <SDL_vulkan.h>

#include <tracy/Tracy.hpp>

#include <vulkan/vk_enum_string_helper.h>

namespace {

[[nodiscard]] auto debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
                                  VkDebugUtilsMessageTypeFlagsEXT /*message_types*/,
                                  const VkDebugUtilsMessengerCallbackDataEXT* p_callback_data,
                                  void* /*p_user_data*/) -> VkBool32
{
  const auto log_level = [&]() {
    if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
      return spdlog::level::err;
    } else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
      return spdlog::level::warn;
    } else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
      return spdlog::level::info;
    } else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
      return spdlog::level::trace;
    }

    beyond::panic("Unknown severity level");
  }();

  spdlog::log(log_level, "{}", p_callback_data->pMessage);

  if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    beyond::panic("Vulkan Validation error!");
  }

  return 0;
}

} // namespace

namespace vkh {

Context::Context(charlie::Window& window)
{
  ZoneScoped;

  VK_CHECK(volkInitialize());

  auto instance_ret = []() {
    ZoneScopedN("vkCreateInstance");
    return vkb::InstanceBuilder{}
        .require_api_version(1, 3, 0)
        .set_debug_callback(debug_callback)
        .request_validation_layers()
        .add_validation_feature_enable(VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT)
        .enable_extension("VK_EXT_debug_utils")
        .build();
  }();

  if (!instance_ret) { beyond::panic(instance_ret.error().message()); }
  instance_ = instance_ret->instance;
  debug_messenger_ = instance_ret->debug_messenger;

  volkLoadInstance(instance_);

  {
    ZoneScopedN("SDL_Vulkan_CreateSurface");
    if (SDL_FALSE == SDL_Vulkan_CreateSurface(window.raw_window(), instance_, &surface_)) {
      beyond::panic(SDL_GetError());
    }
  }

  vkb::PhysicalDeviceSelector phys_device_selector(instance_ret.value());

  auto phys_device_ret = [&]() {
    ZoneScopedN("Select physical device");
    return phys_device_selector.set_surface(surface_)
        .allow_any_gpu_device_type(false)
        .prefer_gpu_device_type(vkb::PreferredDeviceType::discrete)
        .add_required_extension(VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME)
        .add_required_extension(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME)
        .set_required_features({
            .fillModeNonSolid = true,
        })
        .set_required_features_11({.shaderDrawParameters = true})
        .set_required_features_12({
            .descriptorIndexing = true,
            .shaderSampledImageArrayNonUniformIndexing = true,
            .descriptorBindingSampledImageUpdateAfterBind = true,
            .descriptorBindingPartiallyBound = true,
            .runtimeDescriptorArray = true,
        })
        .set_required_features_13({.synchronization2 = true, .dynamicRendering = true})
        .select();
  }();

  if (!phys_device_ret) { beyond::panic(phys_device_ret.error().message()); }

  vkb::PhysicalDevice vkb_physical_device = phys_device_ret.value();
  physical_device_ = vkb_physical_device.physical_device;
  gpu_properties_ = vkb_physical_device.properties;

  SPDLOG_INFO("Physical device name {}", vkb_physical_device.name);

  const vkb::DeviceBuilder device_builder{vkb_physical_device};
  const auto device_ret = [&]() {
    ZoneScopedN("vkCreateDevice");
    return device_builder.build();
  }();
  if (!device_ret) { beyond::panic(device_ret.error().message()); }
  auto vkb_device = device_ret.value();
  device_ = vkb_device.device;

  volkLoadDevice(device_);

  graphics_queue_ = vkb_device.get_queue(vkb::QueueType::graphics).value();
  graphics_queue_family_index_ = vkb_device.get_queue_index(vkb::QueueType::graphics).value();
  compute_queue_ = vkb_device.get_queue(vkb::QueueType::compute).value();
  compute_queue_family_index_ = vkb_device.get_queue_index(vkb::QueueType::compute).value();

  //  transfer_queue_ = vkb_device.get_queue(vkb::QueueType::transfer).value();
  //  transfer_queue_family_index_ =
  //      vkb_device.get_queue_index(vkb::QueueType::transfer).value();

  present_queue_ = vkb_device.get_queue(vkb::QueueType::present).value();

  const VmaVulkanFunctions vma_vulkan_func{
      .vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties,
      .vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties,
      .vkAllocateMemory = vkAllocateMemory,
      .vkFreeMemory = vkFreeMemory,
      .vkMapMemory = vkMapMemory,
      .vkUnmapMemory = vkUnmapMemory,
      .vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges,
      .vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges,
      .vkBindBufferMemory = vkBindBufferMemory,
      .vkBindImageMemory = vkBindImageMemory,
      .vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements,
      .vkGetImageMemoryRequirements = vkGetImageMemoryRequirements,
      .vkCreateBuffer = vkCreateBuffer,
      .vkDestroyBuffer = vkDestroyBuffer,
      .vkCreateImage = vkCreateImage,
      .vkDestroyImage = vkDestroyImage,
      .vkCmdCopyBuffer = vkCmdCopyBuffer,
  };

  const VmaAllocatorCreateInfo allocator_create_info{
      .physicalDevice = physical_device_,
      .device = device_,
      .pVulkanFunctions = &vma_vulkan_func,
      .instance = instance_,
  };

  {
    ZoneScopedN("vmaCreateAllocator");
    VK_CHECK(vmaCreateAllocator(&allocator_create_info, &allocator_));
  }
} // namespace vkh

Context::~Context()
{
  if (!instance_) return;

  vmaDestroyAllocator(allocator_);

  vkDestroyDevice(device_, nullptr);
  vkDestroySurfaceKHR(instance_, surface_, nullptr);
  vkb::destroy_debug_utils_messenger(instance_, debug_messenger_, nullptr);
  vkDestroyInstance(instance_, nullptr);
}

Context::Context(vkh::Context&& other) noexcept
    : instance_{std::exchange(other.instance_, {})},
      debug_messenger_{std::exchange(other.debug_messenger_, {})},
      surface_{std::exchange(other.surface_, {})},
      physical_device_{std::exchange(other.physical_device_, {})},
      device_{std::exchange(other.device_, {})},
      graphics_queue_{std::exchange(other.graphics_queue_, {})},
      compute_queue_{std::exchange(other.compute_queue_, {})},
      transfer_queue_{std::exchange(other.transfer_queue_, {})},
      present_queue_{std::exchange(other.present_queue_, {})},
      graphics_queue_family_index_{std::exchange(other.graphics_queue_family_index_, {})},
      compute_queue_family_index_{std::exchange(other.compute_queue_family_index_, {})},
      transfer_queue_family_index_{std::exchange(other.transfer_queue_family_index_, {})},
      allocator_{std::exchange(other.allocator_, {})}
{
}

auto Context::operator=(Context&& other) & noexcept -> Context&
{
  if (this != &other) {
    this->~Context();
    instance_ = std::exchange(other.instance_, {});
    debug_messenger_ = std::exchange(other.debug_messenger_, {});
    surface_ = std::exchange(other.surface_, {});
    physical_device_ = std::exchange(other.physical_device_, {});
    device_ = std::exchange(other.device_, {});
    graphics_queue_ = std::exchange(other.graphics_queue_, {});
    compute_queue_ = std::exchange(other.compute_queue_, {});
    transfer_queue_ = std::exchange(other.transfer_queue_, {});
    present_queue_ = std::exchange(other.present_queue_, {});
    graphics_queue_family_index_ = std::exchange(other.graphics_queue_family_index_, {});
    compute_queue_family_index_ = std::exchange(other.compute_queue_family_index_, {});
    transfer_queue_family_index_ = std::exchange(other.transfer_queue_family_index_, {});
    allocator_ = std::exchange(other.allocator_, {});
  }
  return *this;
}

auto Context::map_impl(const AllocatedBuffer& buffer) -> Expected<void*>
{
  void* ptr = nullptr;
  VKH_TRY(vmaMapMemory(allocator_, buffer.allocation, &ptr));
  return ptr;
}

void Context::unmap(const AllocatedBuffer& buffer)
{
  vmaUnmapMemory(allocator_, buffer.allocation);
}

} // namespace vkh