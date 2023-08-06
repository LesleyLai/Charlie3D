add_library(charlie3d_util
        configuration.hpp
        file.hpp
        )
add_library(charlie3d::util ALIAS charlie3d_util)


target_link_libraries(charlie3d_util
        PUBLIC
        beyond::core
        )