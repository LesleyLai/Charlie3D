set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_executable(charlie3d_test main.cpp dummy_test.cpp)

find_package(Catch2 REQUIRED)
target_link_libraries(charlie3d_test
        PRIVATE charlie3d::lib Catch2::Catch2)
enable_testing()

add_test(NAME charlie3d_test COMMAND "${CMAKE_BINARY_DIR}/bin/charlie3d_test")
