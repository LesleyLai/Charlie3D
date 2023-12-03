#include "thread_pool.hpp"
#include "beyond/concurrency/thread_pool.hpp"

#ifdef _WIN32
#include <beyond/utils/assert.hpp>
#include <fmt/core.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <processthreadsapi.h>
#endif

namespace charlie {

void ThreadPool::enqueue(Task<void>& task)
{
  queue_.push(task.get_coroutine_handle());
}

void ThreadPool::wait()
{
  queue_.done();
  for (auto& worker : workers_) { worker.join(); }
}

auto TaskQueue::pop() -> std::coroutine_handle<>
{
  std::unique_lock lock{mutex_};
  while (tasks_.empty() && !is_done_) cv_.wait(lock);
  if (tasks_.empty()) return nullptr;
  auto result = tasks_.front();
  tasks_.pop();
  return result;
}

ThreadPool::ThreadPool(std::string_view name, size_t thread_count)
{
  workers_.reserve(thread_count);

  for (size_t t = 0; t < thread_count; ++t) {
    workers_.emplace_back([this]() {
      while (true) {
        std::coroutine_handle<> task = queue_.pop();
        if (task) {
          if (not task.done()) {
            task.resume();
            queue_.push(task);
          }
        } else {
          // No task in queue
          return;
        }
      }
    });
#ifdef _WIN32
    HANDLE native_handle = workers_[t].native_handle();
    std::wstring thread_name;
    fmt::format_to(std::back_inserter(thread_name), "{} Worker {}", name, t);
    const HRESULT r = SetThreadDescription(native_handle, thread_name.c_str());
    BEYOND_ENSURE(SUCCEEDED(r));
#endif
  }
}

ThreadPool::~ThreadPool()
{
  queue_.done();
}

} // namespace charlie
