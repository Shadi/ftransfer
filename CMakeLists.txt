cmake_minimum_required(VERSION 3.20)
project(ftransfer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(ZSTD REQUIRED libzstd)

add_executable(ftransfer
    src/main.cpp
    src/net.cpp
    src/sender.cpp
    src/receiver.cpp
    src/compress.cpp
)

target_include_directories(ftransfer PRIVATE include)
target_link_libraries(ftransfer PRIVATE ${ZSTD_LIBRARIES})
target_include_directories(ftransfer PRIVATE ${ZSTD_INCLUDE_DIRS})
