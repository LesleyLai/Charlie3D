#include "background_tasks.hpp"

namespace charlie {

auto background_thread_pool() -> beyond::ThreadPool&
{
  static beyond::ThreadPool pool;
  return pool;
}

} // namespace charlie
