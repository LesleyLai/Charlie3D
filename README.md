# Charlie3D

Charlie 3D is my personal R&D rendering framework in Vulkan. It is constructed on top of
my [beyond-core](https://github.com/Beyond-Engine/Core) library.

![Amazon Bistro](images/Bistro.png)

## Goals and no-goals

Goals

- Flexibility for personal projects and graphics experiments
- Modular and reusable implementation of various rendering techniques
- As platform-agnostic as possible

No goals

- Serving as a game engine
- Maintaining "clean" code and well-designed software architecture; this is intended for hacking and prototyping
- Support GPUs and old drivers that miss critical features

## Features

### Main Features

- **Shader hot reloading**: Set up shader modules and pipeline like normal and shader files will be automatically
  watched for changes.

### Vulkan-related

- [Vulkan helpers](Charlie/vulkan_helpers) that simplifies object creation and debug name setting

### Rendering

- Shadow mapping with Percentage-Closer Filtering (PCF)
  and [Percentage-Closer Soft Shadows (PCSS)](https://developer.download.nvidia.cn/shaderlibrary/docs/shadow_PCSS.pdf)
- Normal map support
- Occlusion map support
- [Reverse-Z](https://developer.nvidia.com/blog/visualizing-depth-precision/) for better precision

### Asset

- GLTF and Obj file loading: Wrapping around [tinyobjloader](https://github.com/tinyobjloader/tinyobjloader)
  and [fastgltf](https://github.com/spnda/fastgltf)

### Interactions

- A versatile camera implementation that support both arcball and first-person control
- [Dear Imgui](https://github.com/ocornut/imgui) integration

### Utilities

- [Tracy](https://github.com/wolfpld/tracy) Profiler integration
