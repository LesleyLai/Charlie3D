add_library(charlie3d_asset
        cpu_mesh.hpp cpu_scene.hpp
        obj_loader.hpp obj_loader.cpp
        gltf_loader.hpp gltf_loader.cpp
        header_only_libs_impl.cpp)

add_library(charlie3d::asset ALIAS charlie3d_asset)

find_package(meshoptimizer REQUIRED)
find_package(tinyobjloader REQUIRED)
find_package(tinyobjloader REQUIRED)
find_package(fastgltf REQUIRED)
find_package(stb REQUIRED)

target_link_libraries(charlie3d_asset
        PUBLIC
        charlie3d::utils
        beyond::core
        PRIVATE
        stb::stb
        tinyobjloader::tinyobjloader
        meshoptimizer::meshoptimizer
        fastgltf::fastgltf
        )