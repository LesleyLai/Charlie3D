# Charlie3D

Charlie 3D is my personal R&D rendering framework in Vulkan. It is built on top of
my [beyond-core](https://github.com/Beyond-Engine/Core) library.

## Goals and no-goals

Goals

- Be flexible enough for my personal projects and graphics experiments
- Implement different rendering techniques in a modular and reusable fashion
- As platform-agnostic as possible

No goals

- Having elegant and well-designed code. This is a more hacking/prototyping thing
- Support GPUs and old drivers that miss critical features

## Features

### Main Features

- **Shader hot reloading**: Set up shader modules and pipeline like normal and shader files will be automatically
  watched for changes.

### Vulkan-related

- [Vulkan helpers](Charlie/vulkan_helpers) that simplifies object creation and debug name setting

### Rendering

- Shadow mapping with Percentage-Closer Filtering
- Normal map support
- Occlusion map support

### Asset

- GLTF and Obj file loading: Wrapping around [tinyobjloader](https://github.com/tinyobjloader/tinyobjloader)
  and [fastgltf](https://github.com/spnda/fastgltf)

### Utilities

- A versatile camera implementation that support both arcball and first-person control
- [Dear Imgui](https://github.com/ocornut/imgui) integration
- [Tracy](https://github.com/wolfpld/tracy) Profiler integration
