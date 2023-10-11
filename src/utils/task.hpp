#ifndef CHARLIE3D_TASK_HPP
#define CHARLIE3D_TASK_HPP

#include <coroutine>

#include <beyond/types/optional.hpp>

namespace charlie {

/**
 * @brief A Task is an asynchronous computation that is executed lazily
 */
template <typename T = void> struct Task;

namespace detail {

struct TaskPromiseBase {
  std::coroutine_handle<> continuation_ = std::noop_coroutine();

  struct FinalAwaitable {
    [[nodiscard]] auto await_ready() const noexcept -> bool { return false; }
    template <typename promise_type>
    [[nodiscard]] auto await_suspend(std::coroutine_handle<promise_type> coro) noexcept
        -> std::coroutine_handle<>
    {
      return coro.promise().continuation_;
    }
    void await_resume() noexcept {}
  };

  auto initial_suspend() noexcept -> std::suspend_always { return {}; }
  auto final_suspend() noexcept { return FinalAwaitable{}; }

  [[noreturn]] void unhandled_exception() { throw; }
};

template <typename T> struct TaskPromise : detail::TaskPromiseBase {
private:
  beyond::optional<T> value_;

public:
  void return_value(T value) { value_ = std::move(value); }
  auto get_return_object() -> Task<T>;

  auto value() -> T
  {
    if (value_.has_value()) {
      return *value_;
    } else {
      beyond::panic("The coroutine value is never set");
    }
  }
};

template <> struct TaskPromise<void> : detail::TaskPromiseBase {
  auto get_return_object() -> Task<>;

  void return_void() {}
};

} // namespace detail

template <typename T> struct Task {
  using promise_type = detail::TaskPromise<T>;

  using Handle = std::coroutine_handle<promise_type>;

  explicit Task(Handle handle) : handle_{handle} {}

  ~Task()
  {
    if (handle_) { handle_.destroy(); }
  }
  Task(const Task&) = delete;
  auto operator=(const Task&) -> Task& = delete;
  Task(Task&& other) noexcept : handle_{std::exchange(other.handle_, nullptr)} {}
  auto operator=(Task&& other) & noexcept -> Task&
  {

    if (std::addressof(other) != this) {
      if (handle_) { handle_.destroy(); }
      handle_ = std::exchange(other.handle_, nullptr);
    }
  }

  auto get_coroutine_handle() -> Handle { return handle_; }

  auto operator co_await() const noexcept
  {
    struct Awaiter {
      [[nodiscard]] auto await_ready() const noexcept -> bool { return !handle_ || handle_.done(); }
      [[nodiscard]] auto await_suspend(std::coroutine_handle<> awaiting_coroutine) noexcept
          -> std::coroutine_handle<>
      {
        handle_.promise().continuation_ = awaiting_coroutine;
        return handle_;
      }
      auto await_resume() noexcept -> T
      {
        if constexpr (not std::is_void_v<T>) { return this->handle_.promise().value(); }
      }

      std::coroutine_handle<promise_type> handle_;
    };
    return Awaiter{handle_};
  }

private:
  std::coroutine_handle<promise_type> handle_;
};

namespace detail {

template <typename T> auto TaskPromise<T>::get_return_object() -> Task<T>
{
  return Task<T>{std::coroutine_handle<TaskPromise<T>>::from_promise(*this)};
}

inline auto TaskPromise<void>::get_return_object() -> Task<>
{
  return Task<void>{std::coroutine_handle<TaskPromise<void>>::from_promise(*this)};
}
} // namespace detail

} // namespace charlie

#endif // CHARLIE3D_TASK_HPP
