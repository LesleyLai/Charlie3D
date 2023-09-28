#ifndef CHARLIE3D_THREAD_POOL_HPP
#define CHARLIE3D_THREAD_POOL_HPP

#include <coroutine>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace charlie {

/**
 * An asynchronous computation that is executed lazily
 */
template <typename T> struct Task {};

template <> struct Task<void> {
  // clang-format off
  struct promise_type {
    auto initial_suspend() noexcept -> std::suspend_always { return {}; }
    auto final_suspend() noexcept -> std::suspend_always { return {}; }
    auto get_return_object() -> Task {
      return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
    }
    void return_void() {}
    [[noreturn]] void unhandled_exception() { throw; }
  };
  using Handle = std::coroutine_handle<promise_type>;

  auto get_coroutine_handle() -> Handle { return handle_;}
  // clang-format on

private:
  std::coroutine_handle<promise_type> handle_;

  explicit Task(std::coroutine_handle<promise_type> handle) : handle_{handle} {}
};

// A queue that maintain tasks in FIFO order
class TaskQueue {
  std::queue<std::coroutine_handle<>> tasks_;
  bool done_ = false;
  std::mutex mutex_;
  std::condition_variable cv_;

public:
  auto pop() -> std::coroutine_handle<>
  {
    std::unique_lock lock{mutex_};
    while (tasks_.empty() && !done_) cv_.wait(lock);
    if (tasks_.empty()) return nullptr;
    auto result = tasks_.front();
    tasks_.pop();
    return result;
  }

  auto push(std::coroutine_handle<> coroutine_handle)
  {
    std::unique_lock lock{mutex_};
    tasks_.emplace(coroutine_handle);
    cv_.notify_one();
  }

  void done()
  {
    {
      std::unique_lock lock{mutex_};
      done_ = true;
    }
    cv_.notify_all();
  }
};

class ThreadPool {
public:
  explicit ThreadPool(size_t thread_count = std::jthread::hardware_concurrency());

  void enqueue(Task<void> task);
  void wait();

  [[nodiscard]] auto schedule() noexcept
  {
    return std::suspend_always{};
  }

private:
  std::vector<std::jthread> workers_;
  TaskQueue queue_;

  bool stop_{false};

  size_t total_tasks_{0};
  std::atomic<size_t> finished_{0};

  void process_(std::coroutine_handle<> task);
};

ThreadPool::ThreadPool(size_t thread_count)
{
  workers_.reserve(thread_count);

  for (size_t t = 0; t < thread_count; ++t) {
    workers_.emplace_back([this]() {
      while (true) {
        std::coroutine_handle<> task = queue_.pop();
        if (stop_) { return; }
        if (task) { process_(task); }
      }
    });
  }
}

void ThreadPool::enqueue(Task<void> task)
{
  ++total_tasks_;
  queue_.push(task.get_coroutine_handle());
}

void ThreadPool::process_(std::coroutine_handle<> task)
{
  if (not task.done()) {
    task.resume();
    queue_.push(task);
  } else {
    task.destroy();
    ++finished_;

    if (finished_.load() >= total_tasks_) {
      {
        queue_.done();
        stop_ = true;
      }
    }
  }
}

void ThreadPool::wait()
{

  for (auto& worker : workers_) { worker.join(); }
}

} // namespace charlie

#endif // CHARLIE3D_THREAD_POOL_HPP
