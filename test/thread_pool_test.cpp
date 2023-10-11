#include <catch2/catch_test_macros.hpp>

#include "../src/utils/thread_pool.hpp"

namespace charlie {

class FireOnceEvent {
  std::atomic_flag flag_;

public:
  void set()
  {
    flag_.test_and_set();
    flag_.notify_all();
  }

  void wait() { flag_.wait(false); }
};

template <typename T = void> class SyncWaitTaskPromise {};

template <> struct SyncWaitTaskPromise<void> {
  using CoroutineType = std::coroutine_handle<SyncWaitTaskPromise>;

  [[nodiscard]] auto initial_suspend() const noexcept { return std::suspend_always{}; }
  [[nodiscard]] auto final_suspend() const noexcept
  {
    struct awaiter {
      [[nodiscard]] auto await_ready() const noexcept -> bool { return false; }
      void await_suspend(CoroutineType coro) const noexcept
      {
        FireOnceEvent* event = coro.promise().event_;
        if (event) { event->set(); }
      }

      void await_resume() noexcept {}
    };
    return awaiter();
  }

  FireOnceEvent* event_ = nullptr;

  auto get_return_object() noexcept { return CoroutineType::from_promise(*this); }
  void return_void() {}

  [[noreturn]] void unhandled_exception() { throw; }
};

template <typename T = void> class [[nodiscard]] SyncWaitTask {
  std::coroutine_handle<SyncWaitTaskPromise<T>> handle_;

public:
  using promise_type = SyncWaitTaskPromise<T>;

  explicit(false) SyncWaitTask(std::coroutine_handle<SyncWaitTaskPromise<T>> handle)
      : handle_(handle)
  {
  }
  SyncWaitTask(const SyncWaitTask&) = delete;
  auto operator=(const SyncWaitTask&) -> SyncWaitTask& = delete;

  ~SyncWaitTask()
  {
    if (handle_) { handle_.destroy(); }
  }

  void run(FireOnceEvent& event);
};

template <typename T> void SyncWaitTask<T>::run(FireOnceEvent& event)
{
  handle_.promise().event_ = &event;
  handle_.resume();
}

template <typename T> [[nodiscard]] auto make_sync_wait_task(Task<T>&& t) -> SyncWaitTask<T>
{
  co_await t;
}

template <typename T> void sync_wait(Task<T>&& t)
{
  FireOnceEvent event;
  auto wait_task = make_sync_wait_task(std::forward<Task<T>>(t));
  wait_task.run(event);
  event.wait();
}

} // namespace charlie

TEST_CASE("sync_wait Task<void>")
{
  const std::string expected = "hello world from a long enough string\n";
  std::string output;

  auto func = [&]() -> charlie::Task<> {
    output = expected;
    co_return;
  };

  charlie::sync_wait(func());
  REQUIRE(output == expected);
}

TEST_CASE("co_await a Task<void>")
{
  int result = 0;
  auto func = [&]() -> charlie::Task<> {
    result = 42;
    co_return;
  };

  auto func2 = [&]() -> charlie::Task<> {
    co_await func();
    co_return;
  };
  charlie::sync_wait(func2());
  REQUIRE(result == 42);
}

TEST_CASE("co_await a Task<int>")
{
  auto func = []() -> charlie::Task<int> { co_return 42; };

  int result = 0;
  auto func2 = [&]() -> charlie::Task<> {
    result = co_await func();
    co_return;
  };
  charlie::sync_wait(func2());
  REQUIRE(result == 42);
}

TEST_CASE("Thread pool tests")
{
  SECTION("An empty thread pool won't stuck")
  {
    [[maybe_unused]] charlie::ThreadPool thread_pool;
  }
}