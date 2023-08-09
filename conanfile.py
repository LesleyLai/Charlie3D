import os

from conan import ConanFile
from conan.tools.cmake import cmake_layout
from conan.tools.files import copy


class CompressorRecipe(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeToolchain", "CMakeDeps"

    def requirements(self):
        self.requires("fmt/10.0.0")
        self.requires("spdlog/1.12.0")
        self.requires("backward-cpp/1.6")
        self.requires("glfw/3.3.8")
        self.requires("vulkan-memory-allocator/3.0.1")
        self.requires("stb/cci.20220909")
        self.requires("tinyobjloader/1.0.7")
        self.requires("imgui/1.89.8")
        
        self.requires("catch2/3.4.0")

    def generate(self):
        copy(self, "*glfw*", os.path.join(self.dependencies["imgui"].package_folder,
                                          "res", "bindings"),
             os.path.join(self.source_folder, "third-party/imgui/bindings"))
        copy(self, "*vulkan*", os.path.join(self.dependencies["imgui"].package_folder,
                                            "res", "bindings"),
             os.path.join(self.source_folder, "third-party/imgui/bindings"))

    def layout(self):
        cmake_layout(self)
