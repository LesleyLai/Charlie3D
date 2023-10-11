#ifndef CHARLIE3D_THREAD_POOL_HPP
#define CHARLIE3D_THREAD_POOL_HPP

#include <coroutine>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "task.hpp"

#include <fmt/format.h>

namespace charlie {

// A queue that maintain tasks in FIFO order
class TaskQueue {
  std::queue<std::coroutine_handle<>> tasks_;
  bool is_done_ = false;
  std::mutex mutex_;
  std::condition_variable cv_;

public:
  auto pop() -> std::coroutine_handle<>;

  auto push(std::coroutine_handle<> coroutine_handle)
  {
    std::unique_lock lock{mutex_};
    tasks_.emplace(coroutine_handle);
    cv_.notify_one();
  }

  [[nodiscard]] auto is_done() const -> bool { return is_done_; }

  void done()
  {
    {
      std::unique_lock lock{mutex_};
      is_done_ = true;
    }
    cv_.notify_all();
  }
};

/**
 *
 * When shutting down. The thread pool will stop accepting new tasks, but will wait for all tasks to
 * finish
 */
class ThreadPool {
public:
  explicit ThreadPool(std::string_view name = "",
                      size_t thread_count = std::jthread::hardware_concurrency());
  ~ThreadPool();
  ThreadPool(const ThreadPool&) = delete;
  auto operator=(const ThreadPool&) -> ThreadPool& = delete;

  void enqueue(Task<void>& task);
  void wait();

  [[nodiscard]] auto schedule() noexcept { return std::suspend_always{}; }

private:
  std::vector<std::jthread> workers_;
  TaskQueue queue_;
};

} // namespace charlie

#endif // CHARLIE3D_THREAD_POOL_HPP
