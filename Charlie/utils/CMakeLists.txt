add_library(charlie3d_utils
        framerate_counter.cpp framerate_counter.hpp
        prelude.hpp
        file.hpp
        file_watcher.hpp
        $<$<BOOL:${WIN32}>:file_watcher_impl_win32.cpp>
        $<$<BOOL:${LINUX}>:file_watcher_impl_linux.cpp>
        string_map.hpp asset_path.cpp asset_path.hpp
        background_tasks.cpp
        background_tasks.hpp)

add_library(charlie3d::utils ALIAS charlie3d_utils)

find_package(spdlog REQUIRED)
find_package(Tracy CONFIG REQUIRED)

target_link_libraries(charlie3d_utils
        PUBLIC
        beyond::core
        spdlog::spdlog
        Tracy::TracyClient
)
