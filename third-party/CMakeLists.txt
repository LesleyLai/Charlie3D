add_subdirectory(beyond-core)

include(FetchContent)
FetchContent_Declare(
        fetch_vk_bootstrap
        GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap
        GIT_TAG 61f77612c70dd49a59157fe139a7d248a90e206a
)
FetchContent_MakeAvailable(fetch_vk_bootstrap)

add_subdirectory(imgui)
