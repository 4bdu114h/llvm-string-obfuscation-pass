cmake_minimum_required(VERSION 3.20)

project(StringObfuscationPass)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

add_library(
    StringObfuscationPass
    MODULE
    StringObfuscationPass.cpp
)

set_target_properties(
    StringObfuscationPass
    PROPERTIES
    PREFIX ""
)

llvm_map_components_to_libnames(
    LLVM_LIBS
    Core
    Support
    Passes
    IRReader
)

target_link_libraries(
    StringObfuscationPass
    PRIVATE
    ${LLVM_LIBS}
)