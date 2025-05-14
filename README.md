# Charlie3D

Charlie 3D is my personal R&D rendering framework in Vulkan. It is constructed on top of
my [beyond-core](https://github.com/Beyond-Engine/Core) library.

**Note: This repository has been archived. Further development will continue on [Codeberg](https://codeberg.org/Lesley/Charlie3D)**.

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

## Build Instruction

> [!IMPORTANT]
> This project uses [git-submodules](https://git-scm.com/book/en/v2/Git-Tools-Submodules). And it also
> uses [git-lfs](https://git-lfs.com/) for asset management. Please follow the setup instructions carefully to ensure
> everything is initialized correctly.

### Install git-lfs if you haven't

If you haven't done this before, make sure to [install git-lfs](https://git-lfs.com/) and run `git lfs install` before
cloning the repository to ensure all large assets are correctly downloaded.

If you've already cloned the repository without git-lfs, then install it and run:

```sh
git lfs fetch --all
git lfs pull
```

to download the missing assets.

### Install vcpkg

If you haven’t already installed vcpkg. Goto a new directory where you want to install `vcpkg`, and do:

```sh
git clone https://github.com/microsoft/vcpkg.git
./vcpkg/bootstrap-vcpkg.sh  # or bootstrap-vcpkg.bat on Windows
```

### Clone the repository

```sh
git clone https://github.com/LesleyLai/Charlie3D.git --recurse-submodules
cd Charlie3D
```

If you forget the `--recurse-submodules` argument, do

```sh
git submodule update --init --recursive
```

### Configure CMake and Build

At this point, you can either use your IDE’s CMake integration or run the following commands from the terminal. The key
is to provide the vcpkg toolchain file to CMake:

```sh
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build
```

> [!TIP]
> Replace `/path/to/vcpkg` with the actual path to the local vcpkg installation.
