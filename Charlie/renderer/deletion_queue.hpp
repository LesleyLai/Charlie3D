#pragma once

#include <vulkan/vulkan.h>

#include <functional>
#include <ranges>
#include <utility>

#include "../vulkan_helpers/context.hpp"

namespace charlie {

class DeletionQueue {
  std::vector<std::function<void(Ref<vkh::Context>)>> deleters_;
  vkh::Context* context_ = nullptr;

public:
  DeletionQueue() = delete;
  explicit DeletionQueue(vkh::Context& context) : context_{&context} {}

  ~DeletionQueue() { flush(); }

  DeletionQueue(const DeletionQueue&) = delete;
  auto operator=(const DeletionQueue&) & -> DeletionQueue& = delete;
  DeletionQueue(DeletionQueue&& other) noexcept
      : deleters_(std::exchange(other.deleters_, {})), context_(std::exchange(other.context_, {}))
  {
  }
  auto operator=(DeletionQueue&& other) & noexcept -> DeletionQueue&
  {
    if (this != &other) {
      deleters_ = std::exchange(other.deleters_, {});
      context_ = std::exchange(other.context_, {});
    }
    return *this;
  }

  template <class Func> void push(Func&& function)
  {
    deleters_.push_back(std::forward<Func&&>(function));
  }

  void flush()
  {
    for (auto& deleter : std::ranges::reverse_view(deleters_)) { deleter(ref(*context_)); }

    deleters_.clear();
  }
};

} // namespace charlie
