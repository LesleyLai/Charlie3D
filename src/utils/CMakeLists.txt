add_library(charlie3d_utils INTERFACE)
add_library(charlie3d::utils ALIAS charlie3d_utils)

target_link_libraries(charlie3d_utils
        INTERFACE
        beyond::core
        )