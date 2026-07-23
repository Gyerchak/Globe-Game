cmake_minimum_required(VERSION 3.15)
project(Projekt-001 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(GDAL REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(OpenMP REQUIRED)

include_directories(include)

add_executable(projekt_001
    src/main.cpp
    src/MosaicBuilder.cpp
)

target_link_libraries(projekt_001
    PRIVATE
    GDAL::GDAL
    nlohmann_json::nlohmann_json
    OpenMP::OpenMP_CXX
)

install(TARGETS projekt_001 RUNTIME DESTINATION bin)
