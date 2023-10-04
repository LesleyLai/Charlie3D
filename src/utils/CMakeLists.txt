add_library(charlie3d_utils
        configuration.hpp
        framerate_counter.cpp framerate_counter.hpp
        file.hpp
        thread_pool.hpp)
add_library(charlie3d::utils ALIAS charlie3d_utils)

find_package(spdlog)
find_package(tracy REQUIRED)

target_link_libraries(charlie3d_utils
        PUBLIC
        beyond::core
        spdlog::spdlog
        Tracy::TracyClient
        )