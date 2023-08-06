add_library(charlie3d_util INTERFACE)
add_library(charlie3d::util ALIAS charlie3d_util)

target_link_libraries(charlie3d_util
        INTERFACE
        beyond::core
        )