cmake_minimum_required(VERSION 3.15)
project(Globe VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Put the main executable in the project root (not in build/)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR})

find_package(GDAL REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(OpenMP REQUIRED)   # not used anymore but harmless
find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)

include_directories(include)
include_directories(src)

add_executable(Globe
    src/Globe.cpp
    src/Globe_Camera.cpp
    src/Globe_cleanup.cpp
    src/Globe_FrameSync.cpp
    src/Globe_Grid.cpp
    src/Globe_Helpers.cpp
    src/Globe_Mesh.cpp
    src/Globe_Offscreen.cpp
    src/Globe_Pipeline.cpp
    src/Globe_Settings.cpp
    src/Globe_SwapChain.cpp
    src/Globe_Texture.cpp
    src/Globe_Uniforms.cpp
    src/Globe_VulkanSetup.cpp
    src/Globe_Window.cpp
)

target_link_libraries(Globe
    PRIVATE
    GDAL::GDAL
    nlohmann_json::nlohmann_json
    OpenMP::OpenMP_CXX
    Vulkan::Vulkan
    glfw
)

install(TARGETS Globe RUNTIME DESTINATION bin)