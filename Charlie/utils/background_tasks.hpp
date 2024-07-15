#ifndef CHARLIE3D_BACKGROUND_TASKS_HPP
#define CHARLIE3D_BACKGROUND_TASKS_HPP

#include <beyond/concurrency/thread_pool.hpp>

// Gets the thread pool for low-priority background tasks
namespace charlie {

auto background_thread_pool() -> beyond::ThreadPool&;

}

#endif // CHARLIE3D_BACKGROUND_TASKS_HPP
