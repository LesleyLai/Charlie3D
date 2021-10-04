add_subdirectory(beyond-core)

include(FetchContent)
FetchContent_Declare(
        fetch_vk_bootstrap
        GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap
        GIT_TAG 95b82a95378c2900f7d1d7908cee322b5f3bf120
)
FetchContent_MakeAvailable(fetch_vk_bootstrap)