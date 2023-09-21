add_library(charlie3d_utils INTERFACE)
add_library(charlie3d::utils ALIAS charlie3d_utils)

find_package(spdlog)
find_package(tracy REQUIRED)

target_link_libraries(charlie3d_utils
        INTERFACE
        beyond::core
        spdlog::spdlog
        Tracy::TracyClient
        )