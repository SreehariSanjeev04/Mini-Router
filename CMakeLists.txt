cmake_minimum_required(VERSION 3.10)

project(mini-router)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB_RECURSE SOURCES "src/*.cpp")

add_executable(Router ${SOURCES})

target_include_directories(Router PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_options(Router PRIVATE -Wall -Wextra -Wpedantic)

