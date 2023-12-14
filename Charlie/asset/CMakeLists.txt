add_library(charlie3d_asset
        cpu_mesh.hpp cpu_scene.hpp
        obj_loader.hpp obj_loader.cpp
        gltf_loader.hpp gltf_loader.cpp
        cpu_image.cpp cpu_image.hpp)

add_library(charlie3d::asset ALIAS charlie3d_asset)

find_package(meshoptimizer REQUIRED)
find_package(tinyobjloader REQUIRED)
#find_package(fastgltf REQUIRED)
find_package(Stb REQUIRED)

target_link_libraries(charlie3d_asset
        PUBLIC
        charlie3d::utils
        beyond::core
        PRIVATE
        tinyobjloader::tinyobjloader
        meshoptimizer::meshoptimizer
        fastgltf::fastgltf
        )
target_include_directories(charlie3d_asset PRIVATE ${Stb_INCLUDE_DIR})