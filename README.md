# Charlie3D

Personal rendering framework/engine in Vulkan. It is built on top of
my [beyond-core](https://github.com/Beyond-Engine/Core) library.

## Goals and no-goals

Goals

- Be flexible enough for my personal projects and graphics experiments
- Implement different rendering techniques in a modular and reusable fashion
- As platform-agnostic as possible

No goals

- Having elegant and well-designed code. This is a more hacking/prototyping thing.
- Support GPUs and old drivers that miss critical features

## Features

### Vulkan-related

- [Vulkan helpers](src/renderer/vulkan_helpers) that simplifies object creation and debug name setting
- [A descriptor allocator and layout cache](src/renderer/vulkan_helpers/descriptor_utils.hpp)

### Rendering

- Runtime shader compilation

### Asset

- Obj file loading

### Utilities

- A versatile camera implementation that support both arcball and first-person controll
- [Dear Imgui](https://github.com/ocornut/imgui) integration
- [Tracy](https://github.com/wolfpld/tracy) Profiler integration
