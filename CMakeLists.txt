cmake_minimum_required(VERSION 3.15)
project(Projekt-001 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Put the main executable in the project root (not in build/)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR})

find_package(GDAL REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(OpenMP REQUIRED)   # not used anymore but harmless

include_directories(include)

add_executable(projekt_001
    Main.cpp
    src/MapBuilder.cpp
)

target_link_libraries(projekt_001
    PRIVATE
    GDAL::GDAL
    nlohmann_json::nlohmann_json
    OpenMP::OpenMP_CXX
)

install(TARGETS projekt_001 RUNTIME DESTINATION bin)