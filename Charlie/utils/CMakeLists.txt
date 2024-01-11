add_library(charlie3d_utils
        configuration.hpp
        framerate_counter.cpp framerate_counter.hpp
        prelude.hpp
        file.hpp
        file_watcher.hpp
        $<$<BOOL:${WIN32}>:file_watcher_impl_win32.cpp>
        string_map.hpp)

add_library(charlie3d::utils ALIAS charlie3d_utils)

find_package(spdlog REQUIRED)
find_package(tracy REQUIRED)

target_link_libraries(charlie3d_utils
        PUBLIC
        beyond::core
        spdlog::spdlog
        Tracy::TracyClient
        )